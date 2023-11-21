# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

import base64
import collections
import contextlib
import json
import logging
import math
import os
import queue
import signal
import threading
import time
from typing import (
    Any,
    Dict,
    FrozenSet,
    Iterator,
    List,
    Literal,
    NamedTuple,
    Optional,
    Set,
    Tuple,
    TypedDict,
)

import mozinfo

from blinkpy.common import path_finder
from blinkpy.common.html_diff import html_diff
from blinkpy.common.memoized import memoized
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.unified_diff import unified_diff
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.testharness_results import (
    LineType,
    Status,
    TestharnessLine,
    format_testharness_baseline,
)
from blinkpy.web_tests.models.test_run_results import convert_to_hierarchical_view
from blinkpy.web_tests.models.typ_types import (
    Artifacts,
    Result,
    ResultSinkReporter,
    ResultType,
)

path_finder.bootstrap_wpt_imports()
from wptrunner import wpttest

_log = logging.getLogger(__name__)
_status_mapping = collections.OrderedDict([
    ('OK', ResultType.Pass),
    ('FAIL', ResultType.Failure),
    ('PASS', ResultType.Pass),
    ('TIMEOUT', ResultType.Timeout),
    ('ERROR', ResultType.Failure),
    ('CRASH', ResultType.Crash),
    ('PRECONDITION_FAILED', ResultType.Failure),
    ('SKIP', ResultType.Skip),
    ('NOTRUN', ResultType.Failure),
])

RunInfo = Dict[str, Any]
TestType = Literal[tuple(wpttest.manifest_test_cls)]


def wptrunner_to_chromium_status(status: str) -> str:
    return _status_mapping[status]


@memoized
def chromium_to_wptrunner_statuses(
    statuses: FrozenSet[str],
    test_type: TestType,
    subtest: bool = False,
) -> Set[str]:
    wptrunner_statuses = {
        wptrunner_status
        for wptrunner_status, chromium_status in _status_mapping.items()
        if chromium_status in statuses
    }
    test_cls = wpttest.manifest_test_cls[test_type]
    if subtest:
        result_cls = test_cls.subtest_result_cls
        assert result_cls, f'{test_type!r} tests cannot have subtests'
    else:
        result_cls = test_cls.result_cls
    return wptrunner_statuses & result_cls.statuses


def normalize_statuses(statuses: Iterator[str]) -> List[str]:
    status_order = list(_status_mapping.keys())
    # Some Chromium statuses may map to more than one wptrunner status (e.g.,
    # ResultType.FAIL -> FAIL, PRECONDITION_FAILED), so return a list of
    # statuses instead of a single status. Also, return them in a well-defined
    # order (generally, most commonly used statuses to least).
    return sorted(set(statuses), key=status_order.index)


class WPTResult(Result):
    """A container for a wptrunner test result.

    This class extends the typ `Result` type with wptrunner-specific
    functionality:
     1. Maps more specific wptrunner statuses to web test ones (which are then
        mapped to ResultDB ones within typ).
     2. Handles subtests. See below for an explanation of status priority.
     3. Format (sub)test statuses and messages into baselines or logs.
    """

    status_priority = [
        # Sorted from least to most "interesting" statuses. A status is more
        # "interesting" when it indicates the test did not run to completion.
        ResultType.Pass,
        ResultType.Failure,
        ResultType.Skip,
        ResultType.Timeout,
        ResultType.Crash,
    ]

    def __init__(self, *args, test_type: Optional[str] = None, **kwargs):
        super().__init__(*args, **kwargs)
        self.testharness_results = []
        self.messages = []
        self.test_type = test_type
        self.has_stderr = False
        self.image_diff_stats = None

    def _maybe_add_testharness_result(self,
                                      status: str,
                                      message: Optional[str] = None,
                                      subtest: Optional[str] = None):
        if self.test_type not in ['testharness', 'wdspec']:
            return

        try:
            status = Status[status]
        except KeyError:
            return
        # The optimizer removes unnecessary PASS lines later.
        line_type = LineType.SUBTEST if subtest else LineType.HARNESS_ERROR
        result = TestharnessLine(line_type, frozenset([status]), message,
                                 subtest)
        if subtest:
            self.testharness_results.append(result)
        else:
            self.testharness_results.insert(0, result)

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
        unexpected = status not in expected
        actual = wptrunner_to_chromium_status(status)
        expected = set(map(wptrunner_to_chromium_status, expected))
        # Converting wptrunner to ResultDB statuses is lossy, so it's possible
        # for the wptrunner result to be unexpected, but ResultDB status
        # `actual` maps to a member of `expected`. Removing the common status
        # forces `typ` to report this test result as unexpected.
        if unexpected:
            expected.discard(actual)
        # pylint: disable=access-member-before-definition
        # `actual` and `unexpected` are set in `Result`'s constructor.
        priority = self._result_priority(actual, unexpected)
        if priority >= self._result_priority(self.actual, self.unexpected):
            self.actual, self.expected = actual, expected
            self.unexpected = unexpected

    def _result_priority(self, status: str,
                         unexpected: bool) -> Tuple[bool, bool, int]:
        incomplete = status in {ResultType.Timeout, ResultType.Crash}
        return (incomplete, unexpected, self.status_priority.index(status))

    def update_from_subtest(self,
                            subtest: str,
                            status: str,
                            expected: Set[str],
                            message: Optional[str] = None):
        if message:
            self.messages.append('%s: %s\n' % (subtest, message))
            self.has_stderr = True
        self._maybe_add_testharness_result(status, message, subtest)
        # Any result against a subtest not expected to run is considered an
        # unexpected pass (and therefore won't cause a build failure).
        if status != 'NOTRUN' and 'NOTRUN' in expected:
            status = 'PASS'
        # Tentatively promote "interesting" statuses to the test level.
        self._maybe_set_statuses(status, expected)

    def update_from_test(self,
                         status: str,
                         expected: Set[str],
                         message: Optional[str] = None):
        if message:
            self.messages.insert(0, 'Harness: %s\n' % message)
            self.has_stderr = True
        self._maybe_add_testharness_result(status, message)
        self._maybe_set_statuses(status, expected)

    def get_result(self):
        header = (LineType.TESTHARNESS_HEADER if self.test_type
                  == 'testharness' else LineType.WDSPEC_HEADER)
        return [
            TestharnessLine(header),
            *self.testharness_results,
            TestharnessLine(LineType.FOOTER),
        ]


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
                 test_name_prefix: str = '',
                 failure_threshold: Optional[int] = None,
                 crash_timeout_threshold: Optional[int] = None,
                 reset_results: bool = False):
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
        self.failure_threshold = failure_threshold or math.inf
        self.crash_timeout_threshold = crash_timeout_threshold or math.inf
        self.reset_results = reset_results
        assert self.failure_threshold > 0
        assert self.crash_timeout_threshold > 0

        # Collects the number of failures by status (only from initial run).
        self._num_failures_by_status = collections.defaultdict(int)
        # Results includes retries, used for computing full_results.json
        self._results_by_name = collections.defaultdict(list)

    @property
    def num_initial_failures(self) -> int:
        failure_statuses = [
            ResultType.Failure, ResultType.Crash, ResultType.Timeout
        ]
        return sum(self._num_failures_by_status[status]
                   for status in failure_statuses)

    def copy_results_viewer(self):
        files_to_copy = ['results.html', 'results.html.version']
        for file in files_to_copy:
            source = self.path_finder.path_from_blink_tools(
                'blinkpy', 'web_tests', file)
            destination = self.fs.join(self.artifacts_dir, file)
            self.fs.copyfile(source, destination)

    def process_results_json(self, json_test_results=None):
        """Postprocess the results JSON generated by wptrunner.

        Arguments:
            json_test_results (str): An optional parameter which specifies path
                to a JSON results file. This is specified by command line arg
                '--json-test-results' and contains exact same data as full_results.json.

        See Also:
            https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/json_test_results_format.md
        """
        final_result = self.create_final_results()
        results_serialized = json.dumps(final_result)
        full_results_json = self.fs.join(self.artifacts_dir,
                                         'full_results.json')
        self.fs.write_text_file(full_results_json, results_serialized)
        if json_test_results:
            self.fs.copyfile(full_results_json, json_test_results)

        # JSONP paddings need to be the same as:
        # https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/controllers/manager.py;l=629;drc=3b93609b2498af0e9dc298f64e2b4f6204af68fa
        full_results_jsonp = self.fs.join(self.artifacts_dir,
                                          'full_results_jsonp.js')
        with self.fs.open_text_file_for_writing(full_results_jsonp) as dest:
            dest.write('ADD_FULL_RESULTS(')
            dest.write(results_serialized)
            dest.write(');')

        self.trim_to_regressions(final_result['tests'])
        # NOTE: Despite the name, this is actually a JSONP file.
        # https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/tools/blinkpy/web_tests/controllers/manager.py;l=636;drc=3b93609b2498af0e9dc298f64e2b4f6204af68fa
        failing_results_jsonp = self.fs.join(self.artifacts_dir,
                                             'failing_results.json')

        with self.fs.open_text_file_for_writing(failing_results_jsonp) as dest:
            dest.write('ADD_RESULTS(')
            json.dump(final_result, dest)
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
            _log.error('Send shutdown event to stop the workers...')
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
                _log.exception('Unable to process event %r: %s', event, error)

    def process_event(self, raw_event: Dict[str, Any]):
        raw_event = dict(raw_event)
        event = Event(raw_event.pop('action'), raw_event.pop('time'),
                      raw_event.pop('thread'), raw_event.pop('pid'),
                      raw_event.pop('source'))
        if (event.action in ['test_start', 'test_status', 'test_end']
                and self.run_info.get('used_upstream')):
            # Do not process test related events when run with
            # --use-upstream-wpt. Only Wpt report is needed for such case.
            return
        test = raw_event.pop('test', None)
        subsuite = raw_event.pop('subsuite', '')
        if test:
            test = test[1:] if test.startswith('/') else test
            test = self._get_chromium_test_name(test, subsuite)
            raw_event['test'] = test
        status = raw_event.get('status')
        if status:
            expected = {raw_event.get('expected', status)}
            expected.update(raw_event.pop('known_intermittent', []))
            raw_event['expected'] = expected
        handler = self._event_handlers.get(event.action)
        if handler:
            handler(event, **raw_event)
        elif event.action not in ['log', 'add_subsuite']:
            _log.warning(
                "%r event received, but not handled (event: %r, "
                'extra: %r)', event.action, event, raw_event)

    def suite_start(self,
                    event: Event,
                    run_info: Optional[RunInfo] = None,
                    **_):
        if run_info:
            self.run_info.update(run_info)

    def suite_end(self, event: Event, **_):
        self._iteration += 1

    def _get_chromium_test_name(self, test: str, subsuite: str):
        if not self.path_finder.is_wpt_internal_path(test):
            test = self.path_finder.wpt_prefix() + test
        if subsuite:
            test = f'virtual/{subsuite}/{test}'
        return test

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
            test_type=self.get_test_type(test),
            pid=event.pid)

    def get_path_from_test_root(self, test: str) -> str:
        if self.path_finder.is_wpt_internal_path(test):
            path_from_test_root = self.internal_manifest.file_path_for_test_url(
                test[len('wpt_internal/'):])
        else:
            test = self.path_finder.strip_wpt_path(test)
            path_from_test_root = self.wpt_manifest.file_path_for_test_url(
                test)
        return path_from_test_root

    @memoized
    def _file_path_for_test(self, test: str) -> str:
        _, test = self.port.get_suite_name_and_base_test(test)
        path_from_test_root = self.get_path_from_test_root(test)
        if not path_from_test_root:
            raise EventProcessingError(
                'Test ID %r does not exist in the manifest' % test)
        if self.path_finder.is_wpt_internal_path(test):
            prefix = 'wpt_internal'
        else:
            prefix = self.fs.join('external', 'wpt')
        return self.path_finder.path_from_web_tests(prefix,
                                                    path_from_test_root)

    def get_test_type(self, test: str) -> str:
        _, test = self.port.get_suite_name_and_base_test(test)
        test_path = self.get_path_from_test_root(test)
        if self.path_finder.is_wpt_internal_path(test):
            return self.internal_manifest.get_test_type(test_path)
        else:
            return self.wpt_manifest.get_test_type(test_path)

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
        artifacts, image_diff_stats = self._extract_artifacts(result, extra)
        result.artifacts = artifacts.artifacts
        result.image_diff_stats = image_diff_stats
        if result.unexpected:
            self._handle_unexpected_result(result)
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

        if self._iteration == 0:
            self._num_failures_by_status[result.actual] += 1

        self._results_by_name[test].append(result)

    def _handle_unexpected_result(self, result: WPTResult):
        if result.actual == ResultType.Failure:
            self.failure_threshold -= 1
        elif result.actual in {ResultType.Crash, ResultType.Timeout}:
            self.crash_timeout_threshold -= 1
        if self.failure_threshold <= 0 or self.crash_timeout_threshold <= 0:
            statuses = ('failures'
                        if self.failure_threshold <= 0 else 'crashes/timeouts')
            _log.error('Exiting early after exceeding threshold '
                       f'for unexpected {statuses}.')
            if self.port.host.platform.is_win():
                signum = signal.CTRL_BREAK_EVENT
            else:
                signum = signal.SIGTERM
            os.kill(os.getpid(), signum)

    def shutdown(self, event: Event, **_):
        if self._results:
            _log.warning('Some tests have unreported results:')
            for test in sorted(self._results):
                _log.warning('  %s', test)

        raise StreamShutdown

    def create_final_results(self):
        # compute the tests dict
        tests = {}
        num_passes = num_regressions = 0
        for test_name, results in self._results_by_name.items():
            # TODO: the expected result calculated this way could change each time
            expected = ' '.join(results[0].expected)
            actual = [result.actual for result in results]
            is_pass = results[-1].actual == ResultType.Pass
            is_unexpected = results[-1].unexpected
            if is_pass:
                num_passes += 1

            test_dict = {}
            test_dict['expected'] = expected
            test_dict['actual'] = ' '.join(actual)
            test_dict['shard'] = self.port.get_option('shard_index')

            # Fields below are optional. To avoid bloating the output results json
            # too much, only add them when they are True or non-empty.
            if len(set(actual)) > 1:
                test_dict['is_flaky'] = True

            rounded_run_time = round(results[0].took, 1)
            if rounded_run_time:
                test_dict['time'] = rounded_run_time

            if self.port.is_slow_wpt_test(test_name):
                test_dict['is_slow_test'] = True

            if is_unexpected:
                test_dict['is_unexpected'] = True
                if not is_pass:
                    test_dict['is_regression'] = True
                    num_regressions += 1

            if results[0].image_diff_stats:
                test_dict['image_diff_stats'] = results[0].image_diff_stats

            has_stderr = any([result.has_stderr for result in results])
            artifacts_across_retries = test_dict.setdefault('artifacts', {})
            for result in results:
                for artifact_id, paths in result.artifacts.items():
                    artifacts_across_retries.setdefault(artifact_id,
                                                        []).extend(paths)

            if has_stderr:
                test_dict['has_stderr'] = True
            # TODO: handle bugs, crash_site, has_repaint_overlay
            convert_to_hierarchical_view(tests, test_name, test_dict)

        # Create the final result dictionary
        final_results = {
            # There are some required fields that we just hard-code.
            'version': 3,

            # TODO: change this to the actual value
            'interrupted': False,
            'path_delimiter': '/',
            'seconds_since_epoch': int(time.time()),
            'layout_tests_dir': self.port.web_tests_dir(),
            "flag_name": self.port.flag_specific_config_name() or '',
            'num_failures_by_type': self._num_failures_by_status,
            'num_passes': num_passes,
            'skipped': self._num_failures_by_status[ResultType.Skip],
            'num_regressions': num_regressions,
            'tests': tests,
        }
        return final_results

    def process_output(self, event: Event, command: str, data: Any, **_):
        if not any(executable in command for executable in self._executables):
            return
        if not isinstance(data, str):
            data = json.dumps(data, sort_keys=True)
        self._crash_log.append(data + '\n')

    def _write_text_results(self, result: WPTResult, artifacts: Artifacts):
        """Write actual, expected, and diff text outputs to disk, if possible.

        If either the actual or expected output is missing (i.e., all-pass), no
        diffs are produced.

        Arguments:
            result: WPT test result.
            artifacts: Artifact manager (note that this is not the artifact ID
                to paths mapping itself).
        """
        actual_text = None
        actual_subpath = self.port.output_filename(
            result.name, test_failures.FILENAME_SUFFIX_ACTUAL, '.txt')
        expected_subpath = self.port.output_filename(
            result.name, test_failures.FILENAME_SUFFIX_EXPECTED, '.txt')
        if result.testharness_results:
            actual_text = format_testharness_baseline(result.get_result())
            artifacts.CreateArtifact('actual_text', actual_subpath,
                                     actual_text.encode())
            if self.reset_results and self._iteration == 0 and result.actual not in {
                    ResultType.Crash,
                    ResultType.Timeout,
            }:
                source = self.fs.join(self.artifacts_dir, actual_subpath)
                dest = self.fs.join(self.port.baseline_version_dir(),
                                    expected_subpath)
                self.fs.maybe_make_directory(self.fs.dirname(dest))
                self.fs.copyfile(source, dest)

        expected_text = self.port.expected_text(result.name)
        if expected_text:
            expected_text = expected_text.decode().strip() + '\n'
            artifacts.CreateArtifact('expected_text', expected_subpath,
                                     expected_text.encode())

        if not actual_text or not expected_text:
            return
        diff_content = unified_diff(expected_text, actual_text,
                                    expected_subpath, actual_subpath)
        diff_subpath = self.port.output_filename(
            result.name, test_failures.FILENAME_SUFFIX_DIFF, '.txt')
        artifacts.CreateArtifact('text_diff', diff_subpath,
                                 diff_content.encode())
        html_diff_content = html_diff(expected_text, actual_text)
        html_diff_subpath = self.port.output_filename(
            result.name, test_failures.FILENAME_SUFFIX_HTML_DIFF, '.html')
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
        _, test_url = self.port.get_suite_name_and_base_test(test_name)
        test_url = self.path_finder.strip_wpt_path(test_url)
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
            if url == test_url:
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

    def _extract_artifacts(self, result: WPTResult, extra) -> (Artifacts, str):
        # Ensure `artifacts_base_dir` (i.e., `layout-test-results`) is prepended
        # to `full_results_jsonp.js` paths so that `results.html` can correctly
        # fetch artifacts.
        artifacts = Artifacts(self.fs.dirname(self.artifacts_dir),
                              self.sink.host,
                              iteration=self._iteration,
                              artifacts_base_dir=self.fs.basename(
                                  self.artifacts_dir))
        image_diff_stats = None
        # Dump output for `--reset-results`, even if the test passes, as the
        # current port may fall back to a failing port.
        if self.reset_results or result.actual not in [
                ResultType.Pass, ResultType.Skip
        ]:
            if result.test_type in {'testharness', 'wdspec'}:
                self._write_text_results(result, artifacts)
            screenshots = (extra or {}).get('reftest_screenshots') or []
            if screenshots:
                image_diff_stats = self._write_screenshots(
                    result.name, artifacts, screenshots)

        if result.messages:
            self._write_log(result.name, artifacts, 'stderr',
                            test_failures.FILENAME_SUFFIX_STDERR,
                            result.messages)
        if self._crash_log:
            self._write_log(result.name, artifacts, 'crash_log',
                            test_failures.FILENAME_SUFFIX_CRASH_LOG,
                            self._crash_log)

        return artifacts, image_diff_stats

    def trim_to_regressions(self, current_node):
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
            if self.trim_to_regressions(child_node):
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
        with self.fs.open_text_file_for_writing(artifact_path) as report_file:
            json.dump(report, report_file, separators=(',', ':'))
        self.sink.report_invocation_level_artifacts({
            report_filename: {
                'filePath': artifact_path,
            },
        })
