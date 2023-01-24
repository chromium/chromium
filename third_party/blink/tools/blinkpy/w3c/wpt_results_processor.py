# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

import argparse
import base64
import json
import logging
import optparse
from typing import List

import mozinfo

from blinkpy.common import path_finder
from blinkpy.common.html_diff import html_diff
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.common.unified_diff import unified_diff
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.typ_types import (
    Artifacts,
    Result,
    ResultType,
    ResultSinkReporter,
)

path_finder.bootstrap_wpt_imports()
from wptrunner import manifestexpected, wptmanifest
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends.base import ManifestItem

_log = logging.getLogger(__name__)


def _remove_query_params(test_name):
    index = test_name.rfind('?')
    return test_name if index == -1 else test_name[:index]


def update_with_static_expectations(test_or_subtest: ManifestItem):
    """Update a (sub)test's metadata with evaluated expectations.

    wptrunner manages test expectations with a high-level API (i.e.,
    manifestexpected) calling low-level ones dealing with the abstract syntax
    tree (i.e., wptmanifest). This function transfers the expectations evaluated
    against run info from the high-level `TestNode` object to the low-level AST.

    Note:
        This function destructively modifies the AST.
    """
    if test_or_subtest.node:
        for child_node in test_or_subtest.node.children:
            if (isinstance(child_node, wptnode.KeyValueNode)
                    and child_node.data == 'expected'):
                # Overwrite any branches or default values with the statically
                # evaluated expectation.
                for status_or_condition in list(child_node.children):
                    status_or_condition.remove()
                try:
                    statuses = test_or_subtest.get('expected')
                except KeyError:
                    # Remove the `expected` key with no value
                    child_node.remove()
                    continue
                if isinstance(statuses, str):
                    child_node.append(wptnode.ValueNode(statuses))
                else:
                    assert isinstance(statuses, list)
                    statuses_node = wptnode.ListNode()
                    for status in statuses:
                        statuses_node.append(wptnode.ValueNode(status))
                    child_node.append(statuses_node)
    for child_item in test_or_subtest.iterchildren():
        update_with_static_expectations(child_item)


class WPTResultsProcessor:
    def __init__(self,
                 host,
                 port=None,
                 web_tests_dir='',
                 artifacts_dir='',
                 results_dir='',
                 sink=None):
        self.host = host
        self.fs = self.host.filesystem
        self.port = port or self.host.port_factory.get()
        self.web_tests_dir = web_tests_dir
        self.artifacts_dir = artifacts_dir
        self.results_dir = results_dir
        self.sink = sink or ResultSinkReporter()
        self.wpt_manifest = self.port.wpt_manifest('external/wpt')
        self.internal_manifest = self.port.wpt_manifest('wpt_internal')
        self.path_finder = path_finder.PathFinder(self.fs)
        # Provide placeholder properties until the wptreport is processed.
        self.run_info = dict(mozinfo.info)

    @classmethod
    def from_options(cls, host, options):
        logging_level = logging.DEBUG if options.verbose else logging.INFO
        configure_logging(logging_level=logging_level, include_time=True)

        port_options = optparse.Values()
        # The factory will read the configuration ("Debug" or "Release")
        # automatically from //src/<target>.
        port_options.ensure_value('configuration', None)
        port_options.ensure_value('target', options.target)
        port_options.ensure_value('manifest_update', False)

        port = host.port_factory.get(options=port_options)

        results_dir = host.filesystem.dirname(options.wpt_results)
        return WPTResultsProcessor(host, port, options.web_tests_dir,
                                   options.artifacts_dir, results_dir)

    def main(self, options):
        self._recreate_artifacts_dir()
        if options.wpt_report:
            self.process_wpt_report(options.wpt_report)
        else:
            _log.debug('No wpt report to process')
        self.process_wpt_results(options.wpt_results)
        self._copy_results_viewer()

    @classmethod
    def parse_args(cls, argv=None):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='log extra details helpful for debugging',
        )
        parser.add_argument(
            '-t',
            '--target',
            default='Release',
            help='target build subdirectory under //out',
        )
        parser.add_argument(
            '--web-tests-dir',
            required=True,
            help='path to the web tests root',
        )
        parser.add_argument(
            '--artifacts-dir',
            required=True,
            help='path to a directory to write artifacts to',
        )
        parser.add_argument(
            '--wpt-results',
            required=True,
            help=('path to the JSON test results file '
                  '(created with `wpt run --log-chromium=...`)'),
        )
        parser.add_argument(
            '--wpt-report',
            help=('path to the wptreport file '
                  '(created with `wpt run --log-wptreport=...`)'),
        )
        return parser.parse_args(argv)

    def _recreate_artifacts_dir(self):
        if self.fs.exists(self.artifacts_dir):
            self.fs.rmtree(self.artifacts_dir)
        self.fs.maybe_make_directory(self.artifacts_dir)
        _log.info('Recreated artifacts directory (%s)', self.artifacts_dir)

    def _copy_results_viewer(self):
        source = self.path_finder.path_from_blink_tools(
            'blinkpy', 'web_tests', 'results.html')
        destination = self.fs.join(self.artifacts_dir, 'results.html')
        self.fs.copyfile(source, destination)
        _log.info('Copied results viewer (%s -> %s)', source, destination)

    def process_wpt_results(self,
                            raw_results_path,
                            full_results_json=None,
                            full_results_jsonp=None,
                            failing_results_jsonp=None):
        """Postprocess the results generated by wptrunner.

        Arguments:
            raw_results_path (str): Path to a JSON results file, which contains
                raw contents or points to artifacts that will be extracted into
                their own files. These fields are removed from the test results
                tree to avoid duplication. This method will overwrite the
                original JSON file with the processed results.
            full_results_json (str): Path to write processed JSON results to.
            full_results_jsonp (str): Path to write processed JSONP results to.
            failing_results_jsonp (str): Path to write failing JSONP results to.

        See Also:
            https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/json_test_results_format.md
        """
        full_results_json = full_results_json or self.fs.join(
            self.artifacts_dir, 'full_results.json')
        full_results_jsonp = full_results_jsonp or self.fs.join(
            self.artifacts_dir, 'full_results_jsonp.js')
        # NOTE: Despite the name, this is actually a JSONP file.
        # https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/controllers/manager.py;l=636;drc=3b93609b2498af0e9dc298f64e2b4f6204af68fa
        failing_results_jsonp = failing_results_jsonp or self.fs.join(
            self.artifacts_dir, 'failing_results.json')

        # results is modified in place throughout this function.
        with self.fs.open_text_file_for_reading(
                raw_results_path) as results_file:
            results = json.load(results_file)

        metadata = results.get('metadata') or {}
        if 'num_failures_by_type' in results and 'PASS' in results[
                'num_failures_by_type']:
            num_passes = results['num_failures_by_type']['PASS']
            results['num_passes'] = num_passes

        test_names = self._update_tests_trie(
            results['tests'],
            delim=results['path_delimiter'],
            # This prefix does not actually exist on disk and only affects
            # how the results are reported.
            test_name_prefix=metadata.get('test_name_prefix', ''))
        _log.info('Extracted artifacts for %d tests', len(test_names))

        results['num_regressions'] = self._count_regressions(results['tests'])

        results_serialized = json.dumps(results)
        self.fs.write_text_file(full_results_json, results_serialized)
        self.fs.copyfile(full_results_json, raw_results_path)

        # JSONP paddings need to be the same as:
        # https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/controllers/manager.py;l=629;drc=3b93609b2498af0e9dc298f64e2b4f6204af68fa
        with self.fs.open_text_file_for_writing(full_results_jsonp) as dest:
            dest.write('ADD_FULL_RESULTS(')
            dest.write(results_serialized)
            dest.write(');')

        self._trim_to_regressions(results['tests'])
        with self.fs.open_text_file_for_writing(failing_results_jsonp) as dest:
            dest.write('ADD_RESULTS(')
            json.dump(results, dest)
            dest.write(');')

    def _update_tests_trie(self,
                           current_node,
                           current_path: str = '',
                           delim: str = '/',
                           test_name_prefix: str = '') -> List[str]:
        """Recursively update the test results trie.

        The JSON results represent tests as the leaves of a trie (nested
        objects). The trie's structure corresponds to the WPT directory
        structure on disk. This method will traverse the trie's nodes, writing
        files to the artifacts directory at leaf nodes and uploading them to
        ResultSink.

        Arguments:
            current_node: The node in the trie to be processed.
            current_path: The path constructed so far, which will become a test
                name at a leaf node. An empty path represents the WPT root URL.
            delim: Delimiter between components in test names. In practice, the
                value is the POSIX directory separator.
            test_name_prefix: Test name prefix to prepend to the generated
                path when uploading results.

        Returns:
            A list of test names found.
        """
        if test_name_prefix and not test_name_prefix.endswith(delim):
            test_name_prefix += delim
        if 'actual' in current_node:
            # Leaf node detected.
            self._add_result_to_sink(current_node, current_path,
                                     test_name_prefix)
            return [current_path]
        else:
            test_names = []
            for component, child_node in current_node.items():
                if current_path:
                    child_path = current_path + delim + component
                else:
                    # At the web test root, do not include a leading slash.
                    child_path = component
                test_names.extend(
                    self._update_tests_trie(child_node, child_path, delim,
                                            test_name_prefix))
            return test_names

    def _read_expected_metadata(self, test_name):
        """Try to locate the expected output of this test, if it exists.

        The expected output of a test is checked in to the source tree beside
        the test itself with a ".ini" extension. The absence of such a file
        implies the test is expected to be all-PASS.

        Raises:
            ValueError: If the expected metadata was unreadable or unparsable.
        """
        if self.path_finder.is_wpt_internal_path(test_name):
            test_file_subpath = self.internal_manifest.file_path_for_test_url(
                test_name[len('wpt_internal/'):])
            metadata_root = self.path_finder.path_from_web_tests(
                'wpt_internal')
        else:
            # TODO(crbug.com/1299650): Support virtual tests and metadata fallback.
            test_file_subpath = self.wpt_manifest.file_path_for_test_url(
                test_name)
            metadata_root = self.path_finder.path_from_web_tests(
                'external', 'wpt')
        if not test_file_subpath:
            raise ValueError('test ID did not resolve to a file')
        manifest = manifestexpected.get_manifest(metadata_root,
                                                 test_file_subpath,
                                                 self.run_info)
        if not manifest:
            raise ValueError('unable to read ".ini" file from disk')
        test_manifest = manifest.get_test(test_name.rpartition('/')[-1])
        if not test_manifest:
            raise ValueError('test ID does not exist')
        update_with_static_expectations(test_manifest)
        return wptmanifest.serialize(test_manifest.node)

    def _write_text_results(self, test_name: str, artifacts: Artifacts,
                            actual_text: str):
        """Write actual, expected, and diff text outputs to disk, if possible.

        If the expected output (WPT metadata) is missing, this method will not
        produce diffs.

        Arguments:
            test_name: Web test name (a path).
            artifacts: Artifact manager (note that this is not the artifact ID
                to paths mapping itself).
            actual_text: (Sub)test results in the WPT metadata format. There
                should be no conditions (i.e., no `if <expr>: <value>`).
        """
        actual_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_ACTUAL, '.txt')
        artifacts.CreateArtifact('actual_text',
                                 actual_subpath,
                                 actual_text,
                                 write_as_text=True)

        try:
            expected_text = self._read_expected_metadata(test_name)
        except (ValueError, KeyError, wptmanifest.parser.ParseError) as error:
            _log.error('Unable to read metadata for %s: %s', test_name, error)
            return
        expected_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_EXPECTED, '.txt')
        artifacts.CreateArtifact('expected_text',
                                 expected_subpath,
                                 expected_text,
                                 write_as_text=True)

        diff_content = unified_diff(
            expected_text,
            actual_text,
            expected_subpath,
            actual_subpath,
        )
        diff_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_DIFF, '.txt')
        artifacts.CreateArtifact('text_diff',
                                 diff_subpath,
                                 diff_content,
                                 write_as_text=True)

        html_diff_content = html_diff(expected_text, actual_text)
        html_diff_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_HTML_DIFF, '.html')
        artifacts.CreateArtifact('pretty_text_diff',
                                 html_diff_subpath,
                                 html_diff_content,
                                 write_as_text=True)

    def _write_screenshots(self, test_name: str, artifacts: Artifacts,
                           screenshots: List[str]):
        """Write actual, expected, and diff screenshots to disk, if possible.

        Arguments:
            test_name: Web test name (a path).
            artifacts: Artifact manager.
            screenshots: Each element represents a screenshot of either the test
                result or one of its references and has the format
                `<url>:<base64-encoded PNG>`. If `<url>` matches the test name,
                that screenshot is of the page under test.

        Returns:
            The diff stats if the screenshots are different.
        """
        # Remember the two images so we can diff them later.
        actual_image_bytes = b''
        expected_image_bytes = b''

        for screenshot in screenshots:
            url, printable_image = screenshot.rsplit(':', 1)

            # The URL produced by wptrunner will have a leading "/", which we
            # trim away for easier comparison to the WPT name below.
            if url.startswith('/'):
                url = url[1:]
            image_bytes = base64.b64decode(printable_image.strip())

            screenshot_key = 'expected_image'
            file_suffix = test_failures.FILENAME_SUFFIX_EXPECTED
            if test_name == url:
                screenshot_key = 'actual_image'
                file_suffix = test_failures.FILENAME_SUFFIX_ACTUAL
                actual_image_bytes = image_bytes
            else:
                expected_image_bytes = image_bytes

            screenshot_subpath = self.port.output_filename(
                test_name, file_suffix, '.png')
            artifacts.CreateArtifact(screenshot_key, screenshot_subpath,
                                     image_bytes)

        diff_bytes, stats, error = self.port.diff_image(
            expected_image_bytes, actual_image_bytes)
        if error:
            _log.error(
                'Error creating diff image for %s '
                '(error: %s, diff_bytes is None: %s)', test_name, error,
                diff_bytes is None)
        elif diff_bytes:
            diff_subpath = self.port.output_filename(
                test_name, test_failures.FILENAME_SUFFIX_DIFF, '.png')
            artifacts.CreateArtifact('image_diff', diff_subpath, diff_bytes)

        return stats

    def _write_log(self, test_name: str, artifacts: Artifacts,
                   artifact_id: str, suffix: str, lines: List[str]):
        log_subpath = self.port.output_filename(test_name, suffix, '.txt')
        # Each line should already end in a newline.
        artifacts.CreateArtifact(artifact_id,
                                 log_subpath,
                                 ''.join(lines),
                                 write_as_text=True)

    def _add_result_to_sink(self, node, test_name, test_name_prefix=''):
        """Add test results to the result sink."""
        actual_statuses = node['actual'].split()
        flaky = len(set(actual_statuses)) > 1
        expected = set(node['expected'].split())
        durations = node.get('times') or [0] * len(actual_statuses)
        artifacts = self._extract_artifacts(test_name, node)

        if self.path_finder.is_wpt_internal_path(test_name):
            test_path = self.fs.join(self.web_tests_dir,
                                     _remove_query_params(test_name))
        else:
            test_path = self.fs.join(self.web_tests_dir, 'external', 'wpt',
                                     _remove_query_params(test_name))

        for iteration, (actual,
                        duration) in enumerate(zip(actual_statuses,
                                                   durations)):
            if (self.run_info.get('sanitizer_enabled')
                    and actual == ResultType.Failure):
                # `--enable-sanitizer` is equivalent to running every test as a
                # crashtest. It suffices for a crashtest to not suffer a timeout
                # or low-level crash to pass:
                #   https://web-platform-tests.org/writing-tests/crashtest.html
                actual = ResultType.Pass
            result = Result(
                name=test_name,
                actual=actual,
                started=(self.host.time() - duration),
                took=duration,
                worker=0,
                # The expected statuses here are actually the web test/wptrunner
                # ones, unlike `actual`, which is a ResultDB status.
                expected=expected,
                unexpected=(actual not in expected),
                flaky=flaky,
                # TODO(crbug/1314847): wptrunner merges output from all runs
                # together. Until it outputs per-test-run artifacts instead, we
                # just upload the artifacts on the first result. No need to
                # upload the same artifacts multiple times.
                artifacts=(artifacts.artifacts if iteration == 0 else {}),
            )
            self.sink.report_individual_test_result(
                test_name_prefix=test_name_prefix,
                result=result,
                artifact_output_dir=self.results_dir,
                expectations=None,
                test_file_location=test_path)

    def _extract_artifacts(self, test_name: str, node) -> Artifacts:
        artifact_contents = node.get('artifacts') or {}
        nonflaky_pass = node['actual'] == 'PASS'
        # TODO(crbug.com/1314847): Reenable the overwrite check after the bug is
        # fixed by removing `repeat_tests=True`.
        artifacts = Artifacts(output_dir=self.results_dir,
                              host=self.sink.host,
                              artifacts_base_dir=self.fs.relpath(
                                  self.artifacts_dir, self.results_dir),
                              repeat_tests=True)

        artifact_contents.pop('wpt_actual_status', None)
        artifact_contents.pop('wpt_subtest_failure', None)

        actual_metadata = artifact_contents.pop('wpt_actual_metadata', None)
        if not nonflaky_pass and actual_metadata:
            self._write_text_results(test_name, artifacts,
                                     '\n'.join(actual_metadata))
        screenshots = artifact_contents.pop('screenshots', None)
        if screenshots:
            diff_stats = self._write_screenshots(test_name, artifacts,
                                                 screenshots)
            if diff_stats:
                node['image_diff_stats'] = diff_stats

        log_lines = artifact_contents.pop('wpt_log', None)
        if log_lines:
            self._write_log(test_name, artifacts, 'stderr',
                            test_failures.FILENAME_SUFFIX_STDERR, log_lines)
            # Required by `blinkpy/web_tests/results.html` to show as stderr.
            node['has_stderr'] = True
        crash_log_lines = artifact_contents.pop('wpt_crash_log', None)
        if crash_log_lines:
            self._write_log(test_name, artifacts, 'crash_log',
                            test_failures.FILENAME_SUFFIX_CRASH_LOG,
                            crash_log_lines)

        # Write back the map with paths to files, not the contents of the files
        # themselves.
        node['artifacts'] = artifacts.artifacts
        _log.debug(
            'Extracted artifacts for %s: %s', test_name, ', '.join(
                artifacts.artifacts) if artifacts.artifacts else '(none)')
        return artifacts

    def _count_regressions(self, current_node) -> int:
        """Recursively count number of regressions from test results trie."""
        if current_node.get('actual'):
            return int(current_node.get('is_regression', 0))

        return sum(map(self._count_regressions, current_node.values()))

    def _trim_to_regressions(self, current_node):
        """Recursively remove non-regressions from the test results trie.

        Returns:
            bool: Whether to remove `current_node` from the trie.
        """
        if 'actual' in current_node:
            # Found a leaf. Delete it if it's not a regression (unexpected
            # failure).
            return not current_node.get('is_regression')

        # Not a leaf, recurse into the subtree. Note that we make a copy of the
        # items since we delete from the node during the loop.
        for component, child_node in list(current_node.items()):
            if self._trim_to_regressions(child_node):
                del current_node[component]

        # Delete the current node if empty.
        return len(current_node) == 0

    def process_wpt_report(self, report_path):
        """Process and upload a wpt report to result sink."""
        with self.fs.open_text_file_for_reading(report_path) as report_file:
            report = json.loads(next(report_file))
            for retry_report in map(json.loads, report_file):
                report['results'].extend(retry_report['results'])
        report_filename = self.fs.basename(report_path)
        artifact_path = self.fs.join(self.artifacts_dir, report_filename)
        if not report['run_info'].get('used_upstream'):
            report['results'] = self._compact_wpt_results(report['results'])
        with self.fs.open_text_file_for_writing(artifact_path) as report_file:
            json.dump(report, report_file, separators=(',', ':'))
        self.run_info.update(report['run_info'])
        _log.info('Processed wpt report (%s -> %s)', report_path,
                  artifact_path)
        self.sink.report_invocation_level_artifacts({
            report_filename: {
                'filePath': artifact_path,
            },
        })

    def _compact_wpt_results(self, results):
        """Remove nonessential fields from wptreport (sub)tests.

        Fields unnecessary for updating metadata include:
           * 'message': Informational messages like stack traces.
           * 'expected': When omitted, implies the test ran as expected.
             Expected results are still included because the updater removes
             stale expectations by default.
           * 'known_intermittent': When omitted, no intermittent statuses are
              expected.
           * 'duration': Time taken to run the test.

        See Also:
            https://github.com/web-platform-tests/wpt/blob/131b8a541ba98afcef35ae757e4fb2f805714230/tools/wptrunner/wptrunner/metadata.py#L439-L450
            https://github.com/web-platform-tests/wpt.fyi/blob/8bf23a6f68d18acab002aa6a613fc5660afb0a85/webapp/components/test-file-results-table.js#L240-L283
        """
        compact_results = []
        for result in results:
            compact_result = {'status': result['status']}
            expected = result.get('expected')
            if expected and expected != result['status']:
                compact_result['expected'] = expected
            intermittent = result.get('known_intermittent')
            if intermittent:
                compact_result['known_intermittent'] = intermittent
            test_id = result.get('test')
            if test_id:
                compact_result['test'] = test_id
                compact_result['subtests'] = self._compact_wpt_results(
                    result['subtests'])
            else:
                compact_result['name'] = result['name']  # Subtest detected
            compact_results.append(compact_result)
        return compact_results
