# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import os
import posixpath
import sys
import time

import six

import common

BLINK_TOOLS_DIR = os.path.join(common.SRC_DIR, 'third_party', 'blink', 'tools')
CATAPULT_DIR = os.path.join(common.SRC_DIR, 'third_party', 'catapult')
OUT_DIR = os.path.join(common.SRC_DIR, "out", "{}")
DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT = os.path.join(OUT_DIR, "results.json")
TYP_DIR = os.path.join(CATAPULT_DIR, 'third_party', 'typ')
WEB_TESTS_DIR = os.path.normpath(
    os.path.join(BLINK_TOOLS_DIR, os.pardir, 'web_tests'))
_WPT_ROOT_FRAGMENT = posixpath.join('external', 'wpt', '')
TESTS_ROOT_DIR = os.path.join(WEB_TESTS_DIR, 'external', 'wpt')

if BLINK_TOOLS_DIR not in sys.path:
    sys.path.append(BLINK_TOOLS_DIR)

if TYP_DIR not in sys.path:
    sys.path.append(TYP_DIR)

from blinkpy.common.host import Host
from blinkpy.common.html_diff import html_diff
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.unified_diff import unified_diff
from blinkpy.web_tests.models import test_failures
from typ.artifacts import Artifacts
from typ.json_results import Result
from typ.result_sink import ResultSinkReporter


def strip_wpt_root_prefix(path):
    """Remove a redundant prefix from a WPT path.

    ResultDB identifies WPTs as web tests with the prefix "external/wpt", but
    wptrunner expects paths relative to the WPT root, which already ends in
    "external/wpt". This function transforms a general web test path into a
    WPT path.

    WPT paths are always POSIX-style.
    """
    if path.startswith(_WPT_ROOT_FRAGMENT):
        return posixpath.relpath(path, _WPT_ROOT_FRAGMENT)
    # Path is absolute or does not start with the prefix.
    # Assume the path already points to a valid WPT and pass through.
    return path


class BaseWptScriptAdapter(common.BaseIsolatedScriptArgsAdapter):
    """The base class for script adapters that use wptrunner to execute web
    platform tests. This contains any code shared between these scripts, such
    as integrating output with the results viewer. Subclasses contain other
    (usually platform-specific) logic."""

    def __init__(self, host=None):
        super(BaseWptScriptAdapter, self).__init__()
        if not host:
            host = Host()
        self.fs = host.filesystem
        self.port = host.port_factory.get()
        self.wpt_manifest = self.port.wpt_manifest("external/wpt")
        # Path to the output of the test run. Comes from the args passed to the
        # run, parsed after this constructor. Can be overwritten by tests.
        self.wpt_output = None
        self.wptreport = None
        self.sink = ResultSinkReporter()
        self.layout_test_results_subdir = 'layout-test-results'
        default_wpt_binary = os.path.join(
            common.SRC_DIR, "third_party", "wpt_tools", "wpt", "wpt")
        self.wpt_binary = os.environ.get("WPT_BINARY") or default_wpt_binary

    def maybe_set_default_isolated_script_test_output(self):
        if self.options.isolated_script_test_output:
            return
        default_value = DEFAULT_ISOLATED_SCRIPT_TEST_OUTPUT.format(
            self.options.target)
        print("--isolated-script-test-output not set, defaulting to %s" %
              default_value)
        self.options.isolated_script_test_output = default_value

    def generate_test_output_args(self, output):
        return ['--log-chromium', output]

    def generate_sharding_args(self, total_shards, shard_index):
        return ['--total-chunks=%d' % total_shards,
                # shard_index is 0-based but WPT's this-chunk to be 1-based
                '--this-chunk=%d' % (shard_index + 1),
                # The default sharding strategy is to shard by directory. But
                # we want to hash each test to determine which shard runs it.
                # This allows running individual directories that have few
                # tests across many shards.
                '--chunk-type=hash']

    def do_post_test_run_tasks(self):
        if not self.wpt_output and self.options:
            self.wpt_output = self.options.isolated_script_test_output

        # Move json results into layout-test-results directory
        results_dir = os.path.dirname(self.wpt_output)
        layout_test_results = os.path.join(results_dir,
                                           self.layout_test_results_subdir)
        if self.fs.exists(layout_test_results):
            self.fs.rmtree(layout_test_results)
        self.fs.maybe_make_directory(layout_test_results)

        # Perform post-processing of wptrunner output
        self.process_wptrunner_output(
            os.path.join(layout_test_results, 'full_results.json'),
            os.path.join(layout_test_results, 'full_results_jsonp.js'),
            # NOTE: Despite the name, this is actually a JSONP file.
            # https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/controllers/manager.py;l=636;drc=3b93609b2498af0e9dc298f64e2b4f6204af68fa
            os.path.join(layout_test_results, 'failing_results.json'),
        )

        # Copy layout test results viewer to layout-test-results directory
        self.fs.copyfile(
            os.path.join(WEB_TESTS_DIR, 'fast', 'harness', 'results.html'),
            os.path.join(layout_test_results, 'results.html'))

        if self.wptreport:
            self._process_wpt_report(self.wptreport)

    def get_wpt_revision(self):
        path_to_version = os.path.join(WEB_TESTS_DIR, 'external', 'Version')
        with open(path_to_version) as f:
            for line in f.readlines():
                if line.startswith("Version:"):
                    rev = line[len("Version:"):].strip()
                    return rev
        return None

    def _process_wpt_report(self, wptreport):
        layout_test_results = os.path.join(os.path.dirname(self.wpt_output),
                                           self.layout_test_results_subdir)
        dst = os.path.join(layout_test_results,
                           os.path.basename(wptreport))
        with open(wptreport) as f_src, open(dst, "w") as f_dst:
            data = json.load(f_src)
            # update revision to use upstream revision
            rev = self.get_wpt_revision()
            if rev:
                data["run_info"]["revision"] = rev
            # dump the result to layout-test-results
            json.dump(data, f_dst)

        # upload the report to ResultDB
        artifact = {
            os.path.basename(wptreport): {
                'filePath': dst
            }
        }
        self.sink.report_invocation_level_artifacts(artifact)

    def process_wptrunner_output(self,
                                 full_results_json,
                                 full_results_jsonp,
                                 failing_results_jsonp):
        """Post-process the output generated by wptrunner.

        This output contains a single large json file containing the raw content
        or artifacts which need to be extracted into their own files and removed
        from the json file (to avoid duplication)."""
        # output_json is modified *in place* throughout this function.
        with self.fs.open_text_file_for_reading(self.wpt_output) as f:
            output_json = json.load(f)

        # Wptrunner test names exclude the 'external/wpt' directories, but add
        # them back in at this point to reflect the actual location of tests in
        # Chromium.
        output_json['tests'] = {'external': {'wpt': output_json['tests']}}

        results_dir = os.path.dirname(self.wpt_output)
        self._process_test_leaves(results_dir, output_json['path_delimiter'],
                                  output_json['tests'], '')
        # Write output_json back to the same file after modifying it in memory
        full_results = json.dumps(output_json)
        self.fs.write_text_file(self.wpt_output, full_results)
        # Also copy to full_results.json
        self.fs.copyfile(self.wpt_output, full_results_json)

        # JSONP paddings need to be the same as
        # https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/controllers/manager.py;l=629;drc=3b93609b2498af0e9dc298f64e2b4f6204af68fa
        # Write to full_results.jsonp
        with self.fs.open_text_file_for_writing(full_results_jsonp) as f:
            f.write('ADD_FULL_RESULTS(')
            f.write(full_results)
            f.write(');')
        # Trim the results and write to failing_results.jsonp
        self._trim_to_regressions(output_json['tests'])
        with self.fs.open_text_file_for_writing(failing_results_jsonp) as f:
            f.write('ADD_RESULTS(')
            json.dump(output_json, f)
            f.write(');')

    def _handle_text_outputs(self, actual_metadata, root_node, results_dir,
                             path_so_far):
        """Produces actual, expected and diff text outputs."""
        # If the test passed, there are no artifacts to output. Note that if a
        # baseline file (*.ini file) exists, an actual of PASS means that the
        # test matched the baseline, not that the test itself passed. As such,
        # we still correctly produce artifacts in the case where someone fixes a
        # baselined test.
        if root_node["actual"] == "PASS":
            return

        # Note that the actual_metadata is a list of strings, so we join
        # them on new lines when writing to file.
        actual_text = "\n".join(actual_metadata)
        actual_subpath = self._write_text_artifact(
            test_failures.FILENAME_SUFFIX_ACTUAL,
            results_dir, path_so_far, actual_text)
        root_node["artifacts"]["actual_text"] = [actual_subpath]

        # Try to locate the expected output of this test, if it exists.
        expected_subpath, expected_text = \
            self._maybe_write_expected_output(results_dir, path_so_far)
        if expected_subpath:
            root_node["artifacts"]["expected_text"] = [expected_subpath]
            diff_content = unified_diff(expected_text, actual_text,
                                        expected_subpath, actual_subpath)
            diff_subpath = self._write_text_artifact(
                test_failures.FILENAME_SUFFIX_DIFF, results_dir,
                path_so_far, diff_content)
            root_node["artifacts"]["text_diff"] = [diff_subpath]

            # The html_diff library requires str arguments but the file
            # contents is read-in as unicode. In python3 the contents comes
            # in as a str, but in python2 it remains type unicode, so we
            # have to encode it to get the str version.
            if six.PY2:
                expected_text = expected_text.encode('utf8')
                actual_text = actual_text.encode('utf8')

            html_diff_content = html_diff(expected_text, actual_text)
            if six.PY2:
                # Ensure the diff itself is properly decoded, to avoid
                # UnicodeDecodeErrors when writing to file. This can happen if
                # the diff contains unicode characters but the file is written
                # as ascii because of the default system-level encoding.
                html_diff_content = unicode(html_diff_content, 'utf8')

            html_diff_subpath = self._write_text_artifact(
                test_failures.FILENAME_SUFFIX_HTML_DIFF, results_dir,
                path_so_far, html_diff_content, extension=".html")
            root_node["artifacts"]["pretty_text_diff"] = [html_diff_subpath]

    def _process_test_leaves(self, results_dir, delim, root_node, path_so_far):
        """Finds and processes each test leaf below the specified root.

        This will recursively traverse the trie of results in the json output,
        keeping track of the path to each test and identifying leaves by the
        presence of certain attributes.

        Args:
            results_dir: str path to the dir that results are stored
            delim: str delimiter to be used for test names
            root_node: dict representing the root of the trie we're currently
                looking at
            path_so_far: str the path to the current root_node in the trie
        """
        if "actual" in root_node:
            # Found a leaf, process it
            if "artifacts" not in root_node:
                return

            root_node["artifacts"].pop("wpt_actual_status", None)
            root_node["artifacts"].pop("wpt_subtest_failure", None)

            actual_metadata = root_node["artifacts"].pop(
                "wpt_actual_metadata", None)
            if actual_metadata:
                self._handle_text_outputs(
                    actual_metadata, root_node, results_dir, path_so_far)

            screenshot_artifact = root_node["artifacts"].pop(
                "screenshots", None)
            if screenshot_artifact:
                screenshot_paths_dict = self._write_screenshot_artifact(
                    results_dir, path_so_far, screenshot_artifact)
                for screenshot_key, path in screenshot_paths_dict.items():
                    root_node["artifacts"][screenshot_key] = [path]

            log_artifact = root_node["artifacts"].pop("wpt_log", None)
            if log_artifact:
                artifact_subpath = self._write_text_artifact(
                    test_failures.FILENAME_SUFFIX_STDERR,
                    results_dir, path_so_far, "\n".join(log_artifact))
                if artifact_subpath:
                    # Required by fast/harness/results.html to show stderr.
                    root_node["has_stderr"] = True
                    root_node["artifacts"]["stderr"] = [artifact_subpath]

            crashlog_artifact = root_node["artifacts"].pop(
                "wpt_crash_log", None)
            if crashlog_artifact:
                # Note that the crashlog_artifact is a list of strings, so we
                # join them on new lines when writing to file.
                artifact_subpath = self._write_text_artifact(
                    test_failures.FILENAME_SUFFIX_CRASH_LOG,
                    results_dir, path_so_far, "\n".join(crashlog_artifact))
                if artifact_subpath:
                    root_node["artifacts"]["crash_log"] = [artifact_subpath]

            self._add_result_to_sink(path_so_far, root_node)
            return

        # We're not at a leaf node, continue traversing the trie.
        for key in root_node:
            # Append the key to the path, separated by the delimiter. However if
            # the path is empty, skip the delimiter to avoid a leading slash in
            # the path.
            new_path = path_so_far + delim + key if path_so_far else key
            self._process_test_leaves(results_dir, delim, root_node[key],
                                      new_path)

    def _add_result_to_sink(self, test_name, result_node):
        """Add's test results to results sink

        Args:
          test_name: Name of the test to add to results sink.
          result_node: Dictionary containing the actual result, expected result
              and an artifacts dictionary.
        """

        artifacts = Artifacts(
            output_dir=self.wpt_output,
            host=self.sink.host,
            artifacts_base_dir=self.layout_test_results_subdir)

        assert len(result_node['actual'].split()) == 1, (
            ('There should be only one result, however test %s has the '
             'following results "%s"') % (test_name, result_node['actual']))
        unexpected = result_node['actual'] not in result_node['expected']

        for artifact_name, paths in result_node.get('artifacts', {}).items():
            for path in paths:
                artifacts.AddArtifact(artifact_name, path)

        # Test timeouts are a special case of aborts. We must report ABORT to
        # result sink for tests that timed out.
        result = Result(name=test_name,
                        actual=(result_node['actual']
                                if result_node['actual'] != 'TIMEOUT'
                                else 'ABORT'),
                        started=time.time() - result_node.get('time', 0),
                        took=result_node.get('time', 0),
                        worker=0,
                        expected=set(result_node['expected'].split()),
                        unexpected=unexpected,
                        artifacts=artifacts.artifacts)

        index = test_name.find('?')
        test_path = test_name[:index] if index != -1 else test_name

        self.sink.report_individual_test_result(
            test_name, result, os.path.dirname(self.wpt_output),
            None, os.path.join(WEB_TESTS_DIR, test_path))

    def _maybe_write_expected_output(self, results_dir, test_name):
        """Attempts to create an expected output artifact for the test.

        The expected output of tests is checked-in to the source tree beside the
        test itself, with a .ini extension. Not all tests have expected output.

        Args:
            results_dir: str path to the dir to write the output to
            test_name: str name of the test to write expected output for

        Returns:
            two strings:
            - first is the path to the artifact file that the expected output
              was written to, relative to the directory that the original output
              is located. Returns empty string if there is no expected output
              for this test.
            - second is the text that is written to the file, or empty string if
              there is no expected output for this test.
        """
        # When looking into the WPT manifest, we omit "external/wpt" from the
        # test name, since that part of the path is only in Chromium.
        wpt_test_name = strip_wpt_root_prefix(test_name)
        test_file_subpath = self.wpt_manifest.file_path_for_test_url(
            wpt_test_name)
        if not test_file_subpath:
            # Not all tests in the output have a corresponding test file. This
            # could be print-reftests (which are unsupported by the blinkpy
            # manifest) or .any.js tests (which appear in the output even though
            # they do not actually run - they have corresponding tests like
            # .any.worker.html which are covered here).
            return "", ""

        test_file_path = os.path.join(TESTS_ROOT_DIR, test_file_subpath)
        expected_ini_path = test_file_path + ".ini"
        if not self.fs.exists(expected_ini_path):
            return "", ""

        # This test has checked-in expected output. It needs to be copied to the
        # results viewer directory and renamed from <test>.ini to
        # <test>-expected.txt
        contents = self.fs.read_text_file(expected_ini_path)
        artifact_subpath = self._write_text_artifact(
            test_failures.FILENAME_SUFFIX_EXPECTED, results_dir, test_name,
            contents)
        return artifact_subpath, contents

    def _trim_to_regressions(self, root_node):
        """Finds and processes each test leaf below the specified root.

        This will recursively traverse the trie of results in the json output,
        deleting leaves with is_unexpected=true.

        Args:
            root_node: dict representing the root of the trie we're currently
                looking at

        Returns:
            A boolean, whether to delete the current root_node.
        """
        if "actual" in root_node:
            # Found a leaf. Delete it if it's not a regression (unexpected
            # failures).
            return not root_node.get("is_regression")

        # Not a leaf, recurse into the subtree. Note that we make a copy of the
        # items since we delete from root_node.items() during the loop.
        for key, node in list(root_node.items()):
            if self._trim_to_regressions(node):
                del root_node[key]

        # Delete the current node if empty.
        return len(root_node) == 0


    def _write_text_artifact(self, suffix, results_dir, test_name, artifact,
                             extension=".txt"):
        """Writes a text artifact to disk.

        A text artifact contains some form of text output for a test, such as
        the actual test output, or a diff of the actual and expected outputs.
        It is written to a txt file with a suffix generated from the log type.

        Args:
            suffix: str suffix of the artifact to write, e.g.
                test_failures.FILENAME_SUFFIX_ACTUAL
            results_dir: str path to the directory that results live in
            test_name: str name of the test that this artifact is for
            artifact: string, the text to write for this test.
            extension: str the filename extension to use. Defaults to ".txt" but
                can be changed if needed (eg: to ".html" for pretty-diff)

        Returns:
            string, the path to the artifact file that was written relative
              to the |results_dir|.
        """
        log_artifact_sub_path = (
            os.path.join(self.layout_test_results_subdir,
                         self.port.output_filename(
                             test_name, suffix, extension))
        )
        log_artifact_full_path = os.path.join(results_dir,
                                              log_artifact_sub_path)
        if not self.fs.exists(os.path.dirname(log_artifact_full_path)):
            self.fs.maybe_make_directory(
                os.path.dirname(log_artifact_full_path))
        self.fs.write_text_file(log_artifact_full_path, artifact)

        return log_artifact_sub_path

    def _write_screenshot_artifact(self, results_dir, test_name,
                                   screenshot_artifact):
        """Write screenshot artifact to disk.

        The screenshot artifact is a list of strings, each of which has the
        format <url>:<base64-encoded PNG>. Each url-png pair is a screenshot of
        either the test, or one of its refs. We can identify which screenshot is
        for the test by comparing the url piece to the test name.

        Args:
           results_dir: str path to the directory that results live in
           test:name str name of the test that this artifact is for
           screenshot_artifact: list of strings, each being a url-png pair as
               described above.

        Returns:
             A dict mapping the screenshot key (ie: actual, expected) to the
             path of the file for that screenshot
        """
        result={}
        # Remember the two images so we can diff them later.
        actual_image_bytes = None
        expected_image_bytes = None
        for screenshot_pair in screenshot_artifact:
            screenshot_split = screenshot_pair.split(":")
            url = screenshot_split[0]
            # The url produced by wptrunner will have a leading / which we trim
            # away for easier comparison to the test_name below.
            if url.startswith("/"):
                url = url[1:]
            image_bytes = base64.b64decode(screenshot_split[1].strip())

            screenshot_key = "expected_image"
            file_suffix = test_failures.FILENAME_SUFFIX_EXPECTED
            # When comparing the test name to the image URL, we omit
            # "external/wpt" from the test name, since that part of the path is
            # only in Chromium.
            wpt_test_name = strip_wpt_root_prefix(test_name)
            if wpt_test_name == url:
                screenshot_key = "actual_image"
                file_suffix = test_failures.FILENAME_SUFFIX_ACTUAL

            if screenshot_key == "expected_image":
                expected_image_bytes = image_bytes
            else:
                actual_image_bytes = image_bytes

            screenshot_sub_path = (
                os.path.join(self.layout_test_results_subdir,
                             self.port.output_filename(
                                 test_name, file_suffix, ".png"))
            )
            result[screenshot_key] = screenshot_sub_path

            screenshot_full_path = os.path.join(results_dir,screenshot_sub_path)
            if not self.fs.exists(os.path.dirname(screenshot_full_path)):
                self.fs.maybe_make_directory(
                    os.path.dirname(screenshot_full_path))
            # Note: we are writing raw bytes to this file
            self.fs.write_binary_file(screenshot_full_path, image_bytes)

        # Diff the two images and output the diff file.
        diff_bytes, error = self.port.diff_image(expected_image_bytes,
                                                 actual_image_bytes)
        if diff_bytes and not error:
            diff_sub_path = (
                os.path.join(self.layout_test_results_subdir,
                             self.port.output_filename(
                                 test_name, test_failures.FILENAME_SUFFIX_DIFF,
                                 ".png")))
            result["image_diff"] = diff_sub_path
            diff_full_path = os.path.join(results_dir, diff_sub_path)
            if not self.fs.exists(os.path.dirname(diff_full_path)):
                self.fs.maybe_make_directory(os.path.dirname(diff_full_path))
            # Note: we are writing raw bytes to this file
            self.fs.write_binary_file(diff_full_path, diff_bytes)
        else:
            print("Error creating diff image for test %s. "
                  "Error=%s, diff_bytes is None? %s\n"
                  % (test_name, error, diff_bytes is None))

        return result
