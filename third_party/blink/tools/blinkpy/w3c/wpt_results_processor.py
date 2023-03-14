# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

import base64
import collections
import contextlib
import json
import logging
import queue
import threading
from typing import (
    Any,
    Dict,
    Iterator,
    List,
    NamedTuple,
    Optional,
    Set,
    TypedDict,
)
from urllib.parse import urlsplit

import mozinfo

from blinkpy.common import path_finder
from blinkpy.common.html_diff import html_diff
from blinkpy.common.memoized import memoized
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.unified_diff import unified_diff
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.typ_types import (
    Artifacts,
    Result,
    ResultSinkReporter,
    ResultType,
)

path_finder.bootstrap_wpt_imports()
from wptrunner import manifestexpected, wptmanifest
from wptrunner.wptmanifest import node as wptnode
from wptrunner.wptmanifest.backends.base import ManifestItem

_log = logging.getLogger(__name__)


class WPTResult(Result):
    """A container for a wptrunner test result.

    This class extends the typ `Result` type with wptrunner-specific
    functionality:
     1. Maps more specific wptrunner statuses to web test ones (which are then
        mapped to ResultDB ones within typ).
     2. Handles subtests. See below for an explanation of status priority.
     3. Format (sub)test statuses and messages into WPT metadata or logs.
    """

    _wptrunner_to_chromium_statuses = {
        'OK': ResultType.Pass,
        'PASS': ResultType.Pass,
        'FAIL': ResultType.Failure,
        'ERROR': ResultType.Failure,
        'PRECONDITION_FAILED': ResultType.Failure,
        'TIMEOUT': ResultType.Timeout,
        'EXTERNAL-TIMEOUT': ResultType.Timeout,
        'CRASH': ResultType.Crash,
        'INTERNAL-ERROR': ResultType.Crash,
        'SKIP': ResultType.Skip,
        'NOTRUN': ResultType.Skip,
    }

    _status_priority = [
        # Sorted from least to most "interesting" statuses. A status is more
        # "interesting" when it indicates the test did not run to completion.
        ResultType.Pass,
        ResultType.Failure,
        ResultType.Skip,
        ResultType.Timeout,
        ResultType.Crash,
    ]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.messages = []
        self._test_section = wptnode.DataNode(_test_basename(self.name))

    def _add_expected_status(self, section: wptnode.DataNode, status: str):
        expectation = wptnode.KeyValueNode('expected')
        expectation.append(wptnode.ValueNode(status))
        section.append(expectation)

    def _maybe_set_statuses(self, status: str, expected: Set[str]):
        """Set this result's actual/expected statuses.

        A `testharness.js` test may have subtests with their own statuses and
        expectations, in addition to the test-level harness status/expectation.
        As a result, there isn't a singular status to report to ResultDB.

        This method resolves this conflict by reporting the most "interesting"
        status among all tests/subtests. Given two statuses with the same
        priority, tiebreak by favoring the unexpected status, followed by the
        latest status. The order tiebreaker ensures a test-level status
        overrides a subtest-level status when they have the same priority.
        """
        actual = self._wptrunner_to_chromium_statuses[status]
        expected = {
            self._wptrunner_to_chromium_statuses[status]
            for status in expected
        }
        unexpected = actual not in expected
        priority = (self._status_priority.index(actual), unexpected)
        # pylint: disable=access-member-before-definition
        # `actual` and `unexpected` are set in `Result`'s constructor.
        if priority >= (self._status_priority.index(
                self.actual), self.unexpected):
            self.actual, self.expected = actual, expected
            self.unexpected = unexpected

    def update_from_subtest(self,
                            subtest: str,
                            status: str,
                            expected: Set[str],
                            message: Optional[str] = None):
        if message:
            self.messages.append('%s: %s\n' % (subtest, message))
        subtest_section = wptnode.DataNode(subtest)
        self._add_expected_status(subtest_section, status)
        self._test_section.append(subtest_section)
        # Tentatively promote "interesting" statuses to the test level.
        self._maybe_set_statuses(status, expected)

    def update_from_test(self,
                         status: str,
                         expected: Set[str],
                         message: Optional[str] = None):
        if message:
            self.messages.insert(0, 'Harness: %s\n' % message)
        self._add_expected_status(self._test_section, status)
        self._maybe_set_statuses(status, expected)

    @property
    def actual_metadata(self):
        return wptmanifest.serialize(self._test_section)


def _test_basename(test_id: str) -> str:
    # The test "basename" is test path + query string + fragment
    path_parts = urlsplit(test_id).path.rsplit('/', maxsplit=1)
    if len(path_parts) == 1:
        return test_id
    return test_id[len(path_parts[0]) + 1:]


class Event(NamedTuple):
    action: str
    time: int
    thread: str
    pid: int
    source: str


class EventProcessingError(ValueError):
    """Non-fatal exception raised when an event cannot be processed.

    Examples of bad inputs:
      * Events delivered out-of-order (e.g., `test_end` before `test_start`).
      * Referencing a test that doesn't exist.
    """


class StreamShutdown(Exception):
    """Exception to halt event processing."""


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


class ReftestScreenshot(TypedDict):
    """A screenshot of either a test page or one of its references.

    If the URL matches the test page, the screenshot is for the page under test.
    """
    url: str
    screenshot: str


class WPTResultsProcessor:
    # Executables that wptrunner can start and whose output should go into the
    # crash log.
    _executables = [
        'chromedriver',
        'logcat',
        'content_shell',
    ]

    def __init__(self,
                 fs: FileSystem,
                 port: Port,
                 artifacts_dir: str = '',
                 sink: Optional[ResultSinkReporter] = None,
                 test_name_prefix: str = ''):
        self.fs = fs
        self.port = port
        self.artifacts_dir = artifacts_dir
        self.sink = sink or ResultSinkReporter()
        # This prefix does not actually exist on disk and only affects how the
        # results are reported.
        if test_name_prefix and not test_name_prefix.endswith('/'):
            test_name_prefix += '/'
        self.test_name_prefix = test_name_prefix
        self.wpt_manifest = self.port.wpt_manifest('external/wpt')
        self.internal_manifest = self.port.wpt_manifest('wpt_internal')
        self.path_finder = path_finder.PathFinder(self.fs)
        # Provide placeholder properties until the `suite_start` events are
        # processed.
        self.run_info = dict(mozinfo.info)

        self._iteration: int = 0
        self._results: Dict[str, WPTResult] = {}
        self._leaves: Dict[str, Dict[str, Any]] = collections.defaultdict(dict)
        self._crash_log: List[str] = []
        self._event_handlers = {
            'suite_start': self.suite_start,
            'test_start': self.test_start,
            'test_status': self.test_status,
            'test_end': self.test_end,
            'suite_end': self.suite_end,
            'shutdown': self.shutdown,
            'process_output': self.process_output,
        }
        self.has_regressions: bool = False

    def recreate_artifacts_dir(self):
        if self.fs.exists(self.artifacts_dir):
            self.fs.rmtree(self.artifacts_dir)
        self.fs.maybe_make_directory(self.artifacts_dir)
        self._copy_results_viewer()
        _log.info('Recreated artifacts directory (%s)', self.artifacts_dir)

    def _copy_results_viewer(self):
        files_to_copy = ['results.html', 'results.html.version']
        for file in files_to_copy:
            source = self.path_finder.path_from_blink_tools(
                'blinkpy', 'web_tests', file)
            destination = self.fs.join(self.artifacts_dir, file)
            self.fs.copyfile(source, destination)
            if file == 'results.html':
                _log.info('Copied results viewer (%s -> %s)', source,
                          destination)

    def process_results_json(self,
                             raw_results_path,
                             full_results_json=None,
                             full_results_jsonp=None,
                             failing_results_jsonp=None):
        """Postprocess the results JSON generated by wptrunner.

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

        self._update_tests_trie(results['tests'],
                                delim=results.get('path_delimiter', '/'))

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

    @contextlib.contextmanager
    def stream_results(self,
                       timeout: float = 3) -> Iterator[queue.SimpleQueue]:
        """Asynchronously handle wptrunner test results.

        This context manager starts and cleans up a worker thread to write
        artifacts to disk and, in LUCI, report results to ResultSink.

        Yields:
            A queue that the caller should put `mozlog` events into. The worker
            will consume events from the queue.

        Raises:
            TimeoutError: If the worker thread fails to join within the given
                timeout. This probably indicates the event queue was backlogged
                before this manager exited; a well-behaved caller should avoid
                this.
        """
        self.recreate_artifacts_dir()
        events = queue.SimpleQueue()
        worker = threading.Thread(target=self._consume_events,
                                  args=(events, ),
                                  name='results-stream-worker',
                                  daemon=True)
        worker.start()
        try:
            yield events
        finally:
            # Send a shutdown event, if one has not been sent already, to tell
            # the worker to exit.
            events.put({'action': 'shutdown'}, timeout=timeout)
            worker.join(timeout=timeout)

    def _consume_events(self, events: queue.SimpleQueue):
        while True:
            event = events.get()
            try:
                self.process_event(event)
            except StreamShutdown:
                _log.info('Stopping results stream worker thread.')
                return
            except Exception as error:
                _log.error('Unable to process event %r: %s', event, error)

    def process_event(self, raw_event: Dict[str, Any]):
        raw_event = dict(raw_event)
        event = Event(raw_event.pop('action'), raw_event.pop('time'),
                      raw_event.pop('thread'), raw_event.pop('pid'),
                      raw_event.pop('source'))
        test = raw_event.pop('test', None)
        if test:
            raw_event['test'] = test[1:] if test.startswith('/') else test
        status = raw_event.get('status')
        if status:
            expected = {raw_event.get('expected', status)}
            expected.update(raw_event.pop('known_intermittent', []))
            raw_event['expected'] = expected
        handler = self._event_handlers.get(event.action)
        if handler:
            handler(event, **raw_event)
        elif event.action != 'log':
            _log.warning(
                "%r event received, but not handled (event: %r, "
                'extra: %r)', event.action, event, raw_event)

    def suite_start(self,
                    event: Event,
                    run_info: Optional[Dict[str, Any]] = None,
                    **_):
        if run_info:
            self.run_info.update(run_info)

    def suite_end(self, event: Event, **_):
        self._iteration += 1

    def test_start(self, event: Event, test: str, **_):
        self._results[test] = WPTResult(
            test,
            # Placeholder status that has the lowest priority possible.
            actual=ResultType.Pass,
            unexpected=False,
            started=event.time,
            took=0,
            worker=0,
            file_path=self._file_path_for_test(test),
            pid=event.pid)

    @memoized
    def _file_path_for_test(self, test: str) -> str:
        if test.startswith('wpt_internal/'):
            prefix = 'wpt_internal'
            path_from_test_root = self.internal_manifest.file_path_for_test_url(
                test[len('wpt_internal/'):])
        else:
            prefix = self.path_finder.wpt_prefix()
            path_from_test_root = self.wpt_manifest.file_path_for_test_url(
                test)
        if not path_from_test_root:
            raise EventProcessingError(
                'Test ID %r does not exist in the manifest' % test)
        return self.path_finder.path_from_web_tests(prefix,
                                                    path_from_test_root)

    def test_status(self,
                    event: Event,
                    test: str,
                    subtest: str,
                    status: str,
                    expected: Set[str],
                    message: Optional[str] = None,
                    **_):
        result = self._results.get(test)
        if not result:
            raise EventProcessingError('Test not started: %s' % test)
        result.update_from_subtest(subtest, status, expected, message)

    def test_end(self,
                 event: Event,
                 test: str,
                 status: str,
                 expected: Set[str],
                 message: Optional[str] = None,
                 extra: Optional[Dict[str, Any]] = None,
                 **_):
        result = self._results.pop(test, None)
        if not result:
            raise EventProcessingError('Test not started: %s' % test)
        result.took = max(0, event.time - result.started) / 1000
        result.update_from_test(status, expected, message)
        result.artifacts = self._extract_artifacts(result, extra).artifacts
        if result.unexpected:
            if (self.run_info.get('sanitizer_enabled')
                    and result.actual == ResultType.Failure):
                # `--enable-sanitizer` is equivalent to running every test as a
                # crashtest. It suffices for a crashtest to not suffer a timeout
                # or low-level crash to pass:
                #   https://web-platform-tests.org/writing-tests/crashtest.html
                result.actual = ResultType.Pass
                result.unexpected = False
            if result.actual not in {ResultType.Pass, ResultType.Skip}:
                self.has_regressions = True
        self.sink.report_individual_test_result(
            test_name_prefix=self.test_name_prefix,
            result=result,
            artifact_output_dir=self.fs.dirname(self.artifacts_dir),
            expectations=None,
            test_file_location=result.file_path)
        _log.debug(
            'Reported result for %s, iteration %d (actual: %s, '
            'expected: %s, artifacts: %s)', result.name, self._iteration,
            result.actual, ', '.join(sorted(result.expected)), ', '.join(
                sorted(result.artifacts)) if result.artifacts else '<none>')

    def shutdown(self, event: Event, **_):
        if self._results:
            _log.warning('Some tests have unreported results:')
            for test in sorted(self._results):
                _log.warning('  %s', test)
        raise StreamShutdown

    def process_output(self, event: Event, command: str, data: Any, **_):
        if not any(executable in command for executable in self._executables):
            return
        if not isinstance(data, str):
            data = json.dumps(data, sort_keys=True)
        self._crash_log.append(data + '\n')

    def _update_tests_trie(self,
                           current_node,
                           current_path: str = '',
                           delim: str = '/'):
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
        """
        if 'actual' in current_node:
            # Leaf node detected.
            current_node.update(self._leaves.get(current_path, {}))
        else:
            for component, child_node in current_node.items():
                if current_path:
                    child_path = current_path + delim + component
                else:
                    # At the web test root, do not include a leading slash.
                    child_path = component
                self._update_tests_trie(child_node, child_path, delim)

    def _read_expected_metadata(self, test_name: str, file_path: str):
        """Try to locate the expected output of this test, if it exists.

        The expected output of a test is checked in to the source tree beside
        the test itself with a ".ini" extension. The absence of such a file
        implies the test is expected to be all-PASS.

        Raises:
            ValueError: If the expected metadata was unreadable or unparsable.
        """
        if self.path_finder.is_wpt_internal_path(test_name):
            metadata_root = self.path_finder.path_from_web_tests(
                'wpt_internal')
        else:
            # TODO(crbug.com/1299650): Support virtual tests and metadata fallback.
            metadata_root = self.path_finder.path_from_web_tests(
                'external', 'wpt')
        test_file_subpath = self.fs.relpath(file_path, metadata_root)
        manifest = manifestexpected.get_manifest(metadata_root,
                                                 test_file_subpath,
                                                 self.run_info)
        if not manifest:
            raise FileNotFoundError
        test_manifest = manifest.get_test(_test_basename(test_name))
        if not test_manifest:
            raise ValueError('test ID does not exist')
        update_with_static_expectations(test_manifest)
        return wptmanifest.serialize(test_manifest.node)

    def _write_text_results(self, test_name: str, artifacts: Artifacts,
                            actual_text: str, file_path: str):
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
        artifacts.CreateArtifact('actual_text', actual_subpath,
                                 actual_text.encode())

        try:
            expected_text = self._read_expected_metadata(test_name, file_path)
        except FileNotFoundError:
            _log.debug('".ini" file for "%s" does not exist.', file_path)
            return
        except (ValueError, KeyError, wptmanifest.parser.ParseError) as error:
            _log.warning('Unable to parse metadata for %s: %s', test_name,
                         error)
            return
        expected_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_EXPECTED, '.txt')
        artifacts.CreateArtifact('expected_text', expected_subpath,
                                 expected_text.encode())

        diff_content = unified_diff(
            expected_text,
            actual_text,
            expected_subpath,
            actual_subpath,
        )
        diff_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_DIFF, '.txt')
        artifacts.CreateArtifact('text_diff', diff_subpath,
                                 diff_content.encode())

        html_diff_content = html_diff(expected_text, actual_text)
        html_diff_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_HTML_DIFF, '.html')
        artifacts.CreateArtifact('pretty_text_diff', html_diff_subpath,
                                 html_diff_content.encode())

    def _write_screenshots(self, test_name: str, artifacts: Artifacts,
                           screenshots: List[ReftestScreenshot]):
        """Write actual, expected, and diff screenshots to disk, if possible.

        Arguments:
            test_name: Web test name (a path).
            artifacts: Artifact manager.
            screenshots: Each element represents a screenshot of either the test
                result or one of its references.

        Returns:
            The diff stats if the screenshots are different.
        """
        # Remember the two images so we can diff them later.
        actual_image_bytes = b''
        expected_image_bytes = b''

        for screenshot in screenshots:
            if not isinstance(screenshot, dict):
                # Skip the relation string, like '!=' or '=='.
                continue
            # The URL produced by wptrunner will have a leading "/", which we
            # trim away for easier comparison to the WPT name below.
            url = screenshot['url']
            if url.startswith('/'):
                url = url[1:]
            image_bytes = base64.b64decode(screenshot['screenshot'].strip())

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
        artifacts.CreateArtifact(artifact_id, log_subpath,
                                 ''.join(lines).encode())
        lines.clear()

    def _extract_artifacts(self, result: WPTResult, extra) -> Artifacts:
        # Ensure `artifacts_base_dir` (i.e., `layout-test-results`) is prepended
        # to `full_results_jsonp.js` paths so that `results.html` can correctly
        # fetch artifacts.
        artifacts = Artifacts(self.fs.dirname(self.artifacts_dir),
                              self.sink.host,
                              iteration=self._iteration,
                              artifacts_base_dir=self.fs.basename(
                                  self.artifacts_dir))
        leaf = self._leaves[result.name]
        if result.actual != ResultType.Pass:
            self._write_text_results(result.name, artifacts,
                                     result.actual_metadata, result.file_path)
            screenshots = (extra or {}).get('reftest_screenshots') or []
            if screenshots:
                diff_stats = self._write_screenshots(result.name, artifacts,
                                                     screenshots)
                leaf['image_diff_stats'] = diff_stats

        if result.messages:
            self._write_log(result.name, artifacts, 'stderr',
                            test_failures.FILENAME_SUFFIX_STDERR,
                            result.messages)
            # Required by blinkpy/web_tests/results.html to show stderr.
            leaf['has_stderr'] = True
        if self._crash_log:
            self._write_log(result.name, artifacts, 'crash_log',
                            test_failures.FILENAME_SUFFIX_CRASH_LOG,
                            self._crash_log)

        artifacts_across_retries = leaf.setdefault('artifacts', {})
        for artifact_id, paths in artifacts.artifacts.items():
            artifacts_across_retries.setdefault(artifact_id, []).extend(paths)
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

        See Also:
            https://github.com/web-platform-tests/wpt/blob/131b8a541ba98afcef35ae757e4fb2f805714230/tools/wptrunner/wptrunner/metadata.py#L439-L450
            https://github.com/web-platform-tests/wpt.fyi/blob/8bf23a6f68d18acab002aa6a613fc5660afb0a85/webapp/components/test-file-results-table.js#L240-L283
        """
        compact_results = []
        for result in results:
            compact_result = {'status': result['status']}
            duration = result.get('duration')
            if duration:
                compact_result['duration'] = duration
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
