# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

import argparse
import base64
import json
import logging
import optparse

import mozinfo
import six

from blinkpy.common import path_finder
from blinkpy.common.html_diff import html_diff
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.common.unified_diff import unified_diff
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.typ_types import (
    Artifacts,
    Result,
    ResultSinkReporter,
)

path_finder.bootstrap_wpt_imports()
from wptrunner import manifestexpected, wptmanifest

_log = logging.getLogger(__name__)


def _html_diff(expected_text, actual_text, encoding='utf-8'):
    """A Unicode-safe wrapper around `html_diff`.

    The `html_diff` library requires `str` arguments and returns a `str` value.
    This function accepts and returns Unicode instead.
    """
    # The coercions will have no effect on Python 3, where `str` is already
    # Unicode. In Python 2, where `str` is binary, these coercions are
    # encode/decode calls.
    diff_content = html_diff(
        six.ensure_str(expected_text, encoding),
        six.ensure_str(actual_text, encoding),
    )
    return six.ensure_text(diff_content, encoding)


def _remove_query_params(test_name):
    index = test_name.rfind('?')
    return test_name if index == -1 else test_name[:index]


class WPTResultsProcessor(object):
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

        # wptrunner test names exclude the "external/wpt" prefix. Here, we
        # reintroduce the prefix to create valid paths relative to the web test
        # root directory in the Chromium source tree.
        tests = results['tests']
        prefix_components = self.path_finder.wpt_prefix().split(self.fs.sep)
        for component in reversed(prefix_components):
            if component:
                tests = {component: tests}
        results['tests'] = tests
        metadata = results.get('metadata') or {}
        test_names = self._extract_artifacts(
            results['tests'],
            delim=results['path_delimiter'],
            # Unlike the "external/wpt" prefix, this prefix does not actually
            # exist on disk and only affects how the results are reported.
            test_name_prefix=metadata.get('test_name_prefix', ''))
        _log.info('Extracted artifacts for %d tests', len(test_names))

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

    def _extract_artifacts(self,
                           current_node,
                           current_path='',
                           delim='/',
                           test_name_prefix=''):
        """Recursively extract artifacts from the test results trie.

        The JSON results represent tests as the leaves of a trie (nested
        objects). The trie's structure corresponds to the WPT directory
        structure on disk. This method will traverse the trie's nodes, writing
        files to the artifacts directory at leaf nodes and uploading them to
        ResultSink.

        Arguments:
            current_node: The node in the trie to be processed.
            current_path (str): The path constructed so far, which will become
                a test name at a leaf node. An empty path represents the WPT
                root directory.
            delim (str): Delimiter between components in test names. In
                practice, the value is the POSIX directory separator.
            test_name_prefix (str): Test name prefix to prepend to the generated
                path when uploading results.

        Returns:
            list[str]: A list of test names found.
        """
        if test_name_prefix and not test_name_prefix.endswith(delim):
            test_name_prefix += delim
        if 'actual' in current_node:
            # Leaf node detected.
            if 'artifacts' not in current_node:
                return []
            artifacts = current_node['artifacts']
            artifacts.pop('wpt_actual_status', None)
            artifacts.pop('wpt_subtest_failure', None)
            test_passed = current_node['actual'] == 'PASS'
            self._maybe_write_text_results(artifacts, current_path,
                                           test_passed)
            diff_stats = self._maybe_write_screenshots(artifacts, current_path)
            if diff_stats:
                current_node["image_diff_stats"] = diff_stats
            self._maybe_write_logs(artifacts, current_path)
            # Required by blinkpy/web_tests/results.html to show stderr.
            if 'stderr' in artifacts:
                current_node['has_stderr'] = True
            self._add_result_to_sink(current_node, current_path,
                                     test_name_prefix)
            _log.debug('Extracted artifacts for %s: %s', current_path,
                       ', '.join(artifacts) if artifacts else '(none)')
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
                    self._extract_artifacts(child_node, child_path, delim,
                                            test_name_prefix))
            return test_names

    def _read_expected_metadata(self, test_name):
        """Try to locate the expected output of this test, if it exists.

        The expected output of a test is checked in to the source tree beside
        the test itself with a ".ini" extension. Not all tests have expected
        output. This could be print-reftests (which are unsupported by the
        blinkpy manifest) or ".any.js" tests (which appear in the output even
        though they do not actually run). Instead, they have corresponding
        tests like ".any.worker.html" that are covered here.

        Raises:
            ValueError: If the expected metadata was unreadable or unparsable.
        """
        # When looking into the WPT manifest, we omit "external/wpt" from the
        # web test name, since that part of the path is only relevant in
        # Chromium.
        wpt_name = self.path_finder.strip_wpt_path(test_name)
        # TODO(crbug.com/1299650): Support virtual tests and metadata fallback.
        test_file_subpath = self.wpt_manifest.file_path_for_test_url(wpt_name)
        if not test_file_subpath:
            raise ValueError('test ID did not resolve to a file')
        metadata_root = self.path_finder.wpt_tests_dir()
        manifest = manifestexpected.get_manifest(metadata_root,
                                                 test_file_subpath, '/',
                                                 self.run_info)
        if not manifest:
            raise ValueError('unable to read ".ini" file from disk')
        test_manifest = manifest.get_test('/' + wpt_name)
        if not test_manifest:
            raise ValueError('test ID does not exist')
        return wptmanifest.serialize(test_manifest.node)

    def _maybe_write_text_results(self, artifacts, test_name, test_passed):
        """Write actual, expected, and diff text outputs to disk, if possible.

        If either the actual or expected output is missing, this method cannot
        produce diffs.

        Arguments:
            artifacts (dict[str, Any]): Mapping from artifact names to their
                contents.
            test_name (str): Web test name (a path).
            test_passed (bool): Whether the actual test status is a "PASS". If
                the test passed, there are no artifacts to output. Note that if
                a baseline file (*.ini) exists, an "actual" of "PASS" means that
                the test matched the baseline, not that the test itself passed.
                As such, we still correctly produce artifacts in the case where
                someone fixes a baselined test.
        """
        actual_metadata = artifacts.pop('wpt_actual_metadata', None)
        if not actual_metadata or test_passed:
            return
        actual_text = '\n'.join(actual_metadata)
        actual_subpath = self._write_artifact(
            actual_text,
            test_name,
            test_failures.FILENAME_SUFFIX_ACTUAL,
        )
        artifacts['actual_text'] = [actual_subpath]

        try:
            expected_text = self._read_expected_metadata(test_name)
        except (ValueError, KeyError, wptmanifest.parser.ParseError) as error:
            _log.error('Unable to read metadata for %s: %s', test_name, error)
            return
        expected_subpath = self._write_artifact(
            expected_text,
            test_name,
            test_failures.FILENAME_SUFFIX_EXPECTED,
        )
        artifacts['expected_text'] = [expected_subpath]

        diff_content = unified_diff(
            expected_text,
            actual_text,
            expected_subpath,
            actual_subpath,
        )
        diff_subpath = self._write_artifact(
            diff_content,
            test_name,
            test_failures.FILENAME_SUFFIX_DIFF,
        )
        artifacts['text_diff'] = [diff_subpath]

        html_diff_content = _html_diff(expected_text, actual_text)
        html_diff_subpath = self._write_artifact(
            html_diff_content,
            test_name,
            test_failures.FILENAME_SUFFIX_HTML_DIFF,
            extension='.html',
        )
        artifacts['pretty_text_diff'] = [html_diff_subpath]

    def _maybe_write_screenshots(self, artifacts, test_name):
        """Write actual, expected, and diff screenshots to disk, if possible.
        Returns the diff stats if the screenshots are different.

        The raw "screenshots" artifact is a list of strings, each of which has
        the format "<url>:<base64-encoded PNG>". Each URL-PNG pair is a
        screenshot of either the test result, or one of its refs. We can
        identify which screenshot is for the test by comparing the URL to the
        test name.
        """
        # Remember the two images so we can diff them later.
        actual_image_bytes = b''
        expected_image_bytes = b''

        screenshots = artifacts.pop('screenshots', None)
        if not screenshots:
            return
        for screenshot in screenshots:
            url, printable_image = screenshot.rsplit(':', 1)

            # The URL produced by wptrunner will have a leading "/", which we
            # trim away for easier comparison to the WPT name below.
            if url.startswith('/'):
                url = url[1:]
            image_bytes = base64.b64decode(printable_image.strip())

            # When comparing the test name to the image URL, we omit
            # "external/wpt" from the test name, since that part of the path is
            # only relevant in Chromium.
            wpt_name = self.path_finder.strip_wpt_path(test_name)
            screenshot_key = 'expected_image'
            file_suffix = test_failures.FILENAME_SUFFIX_EXPECTED
            if wpt_name == url:
                screenshot_key = 'actual_image'
                file_suffix = test_failures.FILENAME_SUFFIX_ACTUAL
                actual_image_bytes = image_bytes
            else:
                expected_image_bytes = image_bytes

            screenshot_subpath = self._write_png_artifact(
                image_bytes,
                test_name,
                file_suffix,
            )
            artifacts[screenshot_key] = [screenshot_subpath]

        diff_bytes, stats, error = self.port.diff_image(
            expected_image_bytes, actual_image_bytes)
        if error:
            _log.error(
                'Error creating diff image for %s '
                '(error: %s, diff_bytes is None: %s)', test_name, error,
                diff_bytes is None)
        elif diff_bytes:
            diff_subpath = self._write_png_artifact(
                diff_bytes,
                test_name,
                test_failures.FILENAME_SUFFIX_DIFF,
            )
            artifacts['image_diff'] = [diff_subpath]

        return stats

    def _maybe_write_logs(self, artifacts, test_name):
        """Write WPT logs to disk, if possible."""
        log = artifacts.pop('wpt_log', None)
        if log:
            log_subpath = self._write_artifact(
                '\n'.join(log),
                test_name,
                test_failures.FILENAME_SUFFIX_STDERR,
            )
            artifacts['stderr'] = [log_subpath]

        crash_log = artifacts.pop('wpt_crash_log', None)
        if crash_log:
            crash_log_subpath = self._write_artifact(
                '\n'.join(crash_log),
                test_name,
                test_failures.FILENAME_SUFFIX_CRASH_LOG,
            )
            artifacts['crash_log'] = [crash_log_subpath]

    def _write_artifact(self,
                        contents,
                        test_name,
                        suffix,
                        extension='.txt',
                        text=True):
        """Write an artifact to disk.

        Arguments:
            contents: The file contents of the artifact to write.
            test_name (str): The name of the test that generated this artifact.
            suffix (str): The suffix of the artifact to write. Usually
                determined from the artifact name.
            extension (str): Filename extension to use. Defaults to ".txt" for
                text files.
            text (bool): Whether to write the contents as text or binary. Make
                sure to pass a compatible argument to `contents`.

        Returns:
            str: The path to the artifact file that was written relative to the
                top-level results directory.
        """
        filename = self.port.output_filename(test_name, suffix, extension)
        full_path = self.fs.join(self.artifacts_dir, filename)
        parent_dir = self.fs.dirname(full_path)
        if not self.fs.exists(parent_dir):
            self.fs.maybe_make_directory(parent_dir)
        if text:
            write_file = self.fs.write_text_file
        else:
            write_file = self.fs.write_binary_file
        write_file(full_path, contents)
        return self.fs.relpath(full_path, self.results_dir)

    def _write_png_artifact(self, artifact, test_name, suffix):
        return self._write_artifact(
            artifact,
            test_name,
            suffix,
            extension='.png',
            text=False,
        )


    def _add_result_to_sink(self, node, test_name, test_name_prefix=''):
        """Add test results to the result sink."""
        actual_statuses = node['actual'].split()
        flaky = len(set(actual_statuses)) > 1
        expected = set(node['expected'].split())
        durations = node.get('times') or [0] * len(actual_statuses)

        artifacts = Artifacts(
            output_dir=self.results_dir,
            host=self.sink.host,
            artifacts_base_dir=self.fs.relpath(self.artifacts_dir,
                                               self.results_dir),
        )
        for name, paths in (node.get('artifacts') or {}).items():
            for path in paths:
                artifacts.AddArtifact(name, path)
        test_path = self.fs.join(self.web_tests_dir,
                                 _remove_query_params(test_name))

        for iteration, (actual,
                        duration) in enumerate(zip(actual_statuses,
                                                   durations)):
            # Test timeouts are a special case of aborts. We must report "ABORT"
            # to result sink for tests that timed out.
            if actual == 'TIMEOUT':
                actual = 'ABORT'

            result = Result(
                name=test_name,
                actual=actual,
                started=self.host.time() - duration,
                took=duration,
                worker=0,
                expected=expected,
                unexpected=actual not in expected,
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

    def _get_wpt_revision(self):
        version_path = self.fs.join(self.web_tests_dir, 'external', 'Version')
        target = 'Version:'
        with self.fs.open_text_file_for_reading(version_path) as version_file:
            for line in version_file:
                if line.startswith(target):
                    rev = line[len(target):].strip()
                    return rev
        return None

    def process_wpt_report(self, report_path):
        """Process and upload a wpt report to result sink."""
        with self.fs.open_text_file_for_reading(report_path) as report_file:
            report = json.load(report_file)
        rev = self._get_wpt_revision()
        # Update with upstream revision
        if rev:
            report['run_info']['revision'] = rev
        report_filename = self.fs.basename(report_path)
        artifact_path = self.fs.join(self.artifacts_dir, report_filename)
        with self.fs.open_text_file_for_writing(artifact_path) as report_file:
            json.dump(report, report_file)

        self.run_info.update(report['run_info'])
        _log.info('Processed wpt report (%s -> %s)', report_path,
                  artifact_path)
        artifact = {
            report_filename: {
                'filePath': artifact_path,
            },
        }
        self.sink.report_invocation_level_artifacts(artifact)
        return artifact_path
