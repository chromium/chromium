# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

import base64
import collections
import contextlib
import functools
import json
import io
import logging
import math
import os
import posixpath
import queue
import re
import shlex
import signal
import textwrap
import threading
import time
from dataclasses import dataclass, field
from typing import (
    Any,
    ClassVar,
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
from urllib.parse import quote_plus, urlsplit, urlunsplit

import mozinfo

from blinkpy.common import path_finder
from blinkpy.common.html_diff import html_diff
from blinkpy.common.lru import LRUMapping
from blinkpy.common.memoized import memoized
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.unified_diff import unified_diff
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port.driver import TestURIMapper
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.testharness_results import (
    format_testharness_baseline,
    LineType,
    make_all_pass_baseline,
    parse_testharness_baseline,
    Status,
    TestharnessLine,
)
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.test_run_results import convert_to_hierarchical_view
from blinkpy.web_tests.models.typ_types import (
    Artifacts,
    Expectation,
    ExpectationType,
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
_WPT_DOC_URL: str = ('https://chromium.googlesource.com/chromium/src/+/HEAD'
                     '/docs/testing/run_web_platform_tests.md')
_WPT_BASE_FYI_URL: str = urlsplit(
    'https://wpt.fyi/results/?label=experimental&label=master&aligned')

RunInfo = Dict[str, Any]
TestType = Literal[tuple(wpttest.manifest_test_cls)]


def wptrunner_to_chromium_status(status: str) -> str:
    return _status_mapping[status]


def wpt_fyi_url(test: str) -> Optional[str]:
    prefix = 'external/wpt/'
    if not test.startswith(prefix):
        return None
    scheme, netloc, path, query, fragment = _WPT_BASE_FYI_URL
    path = posixpath.join(path, quote_plus(test[len(prefix):]))
    return urlunsplit((scheme, netloc, path, query, fragment))


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
    STATUSES: ClassVar[List[str]] = [
        # Sorted from least to most "interesting" statuses. A status is more
        # "interesting" when it indicates the test did not run to completion.
        ResultType.Pass,
        ResultType.Failure,
        ResultType.Skip,
        ResultType.Timeout,
        ResultType.Crash,
    ]

    def __init__(self,
                 *args,
                 test_type: Optional[str] = None,
                 exp_line: Optional[ExpectationType] = None,
                 baseline: Optional[List[TestharnessLine]] = None,
                 **kwargs):
        kwargs.setdefault('expected', exp_line.results)
        super().__init__(*args, **kwargs)
        self.testharness_results = []
        self.test_type = test_type
        self._exp_line = exp_line or Expectation()
        self._baseline = baseline or []
        self.image_diff_stats = None
        # TODO(crbug.com/1521922): Populate `self.failure_reason` like
        # `run_web_tests.py` does to help LUCI cluster failures.

    @property
    def has_stderr(self) -> bool:
        return 'stderr' in self.artifacts

    @functools.cached_property
    def can_have_subtests(self) -> bool:
        return self.test_type in {'testharness', 'wdspec'}

    def _maybe_add_testharness_result(self,
                                      status: str,
                                      message: Optional[str] = None,
                                      subtest: Optional[str] = None):
        if not self.can_have_subtests:
            return
        try:
            status = Status[status]
        except KeyError:
            # `OK` (and other statuses not recorded in baselines) aren't
            # contained in `Status` and take this early out.
            return
        line_type = LineType.SUBTEST if subtest else LineType.HARNESS_ERROR
        result = TestharnessLine(line_type, frozenset([status]), message,
                                 subtest)
        if subtest:
            self.testharness_results.append(result)
        else:
            self.testharness_results.insert(0, result)

    def update_from_subtest(self,
                            subtest: str,
                            status: str,
                            message: Optional[str] = None):
        self._maybe_add_testharness_result(status, message, subtest)

    def update_from_test(self, status: str, message: Optional[str] = None):
        self._maybe_add_testharness_result(status, message)
        self.actual = wptrunner_to_chromium_status(status)
        if self.can_have_subtests and self.actual in {
                ResultType.Pass,
                ResultType.Failure,
        }:
            if self._baseline_matches():
                self.actual = ResultType.Pass
            else:
                self.actual = ResultType.Failure
        self.unexpected = self.actual not in self.expected
        self.is_regression = self.actual != ResultType.Pass and self.unexpected

    def _baseline_matches(self) -> bool:
        # Even though `run_web_tests.py` cares about subtest order, harness
        # messages, and non-testharness output (e.g., console messages), ignore
        # them here to match wptrunner's pass/fail evaluation.
        expected_harness_error, expected_subtests = self._group_results(
            self._baseline)
        actual_harness_error, actual_subtests = self._group_results(
            self.testharness_results)
        for subtest, actual_subtest in actual_subtests.items():
            default_subtest = TestharnessLine(LineType.SUBTEST,
                                              frozenset([Status.PASS]),
                                              subtest=subtest)
            expected_subtest = expected_subtests.get(subtest, default_subtest)
            (actual_status, ) = actual_subtest.statuses
            if actual_status not in expected_subtest.statuses:
                return False
            # Another difference with `run_web_tests.py` is that messages are
            # only checked for subtest `FAIL` or `PRECONDITION_FAILED`:
            # https://github.com/web-platform-tests/wpt/blob/4ee1931a/tools/wptrunner/wptrunner/testrunner.py#L699-L712
            is_subtest_fail = actual_subtest.statuses & {
                Status.FAIL,
                Status.PRECONDITION_FAILED,
            }
            if (is_subtest_fail and expected_subtest.message
                    and actual_subtest.message
                    != expected_subtest.message.strip()):
                return False
        unknown_subtests = set(expected_subtests) - set(actual_subtests)
        if unknown_subtests:
            # Because tests that have unused expectations look like they ran
            # expectedly and aren't retried, we need this log to indicate why
            # the shard may have exited with a nonzero exit code.
            _log.warning(
                f'Marking {self.name!r} as a failure because its '
                f'*-expected.txt contains {len(unknown_subtests)} subtests '
                "that didn't run.")
            return False
        return expected_harness_error == actual_harness_error

    def _group_results(
        self,
        results: List[TestharnessLine],
    ) -> Tuple[Optional[TestharnessLine], Dict[str, TestharnessLine]]:
        harness_error, subtests_by_name = None, {}
        for line in results:
            if line.line_type == LineType.HARNESS_ERROR:
                if harness_error:
                    raise EventProcessingError(
                        f'{self.name!r} baseline cannot have more than one '
                        'harness error')
                harness_error = TestharnessLine(line.line_type, line.statuses)
            elif line.line_type == LineType.SUBTEST:
                if line.subtest in subtests_by_name:
                    raise EventProcessingError(
                        f'duplicate subtest {line.subtest!r} in '
                        f'{self.name!r} baseline')
                subtests_by_name[line.subtest] = line
        return harness_error, subtests_by_name

    def format_baseline(self) -> str:
        header = (LineType.TESTHARNESS_HEADER if self.test_type
                  == 'testharness' else LineType.WDSPEC_HEADER)
        if all(result.statuses <= {Status.PASS}
               for result in self.testharness_results):
            return make_all_pass_baseline(header)
        return format_testharness_baseline([
            TestharnessLine(header),
            *self.testharness_results,
            TestharnessLine(LineType.FOOTER),
        ])

    def summarize(self, product: str) -> Optional[str]:
        """Generate a summary of this test result as sanitized HTML.

        See [1] for ResultDB-specific markup features.

        [1]: https://source.chromium.org/chromium/_/chromium/infra/luci/luci-go/+/1f5926756ec235cc63fbed7c7e603e86335998d3:resultdb/proto/v1/test_result.proto;l=99-112
        """
        if not self.is_regression:
            return None
        # TODO(crbug.com/1521922): Unify result sink reporting with
        # `blinkpy.web_tests.*`.
        summary = textwrap.dedent(f"""\
            <p><strong>This WPT was run against <code>{product}</code> using
            <code>chromedriver</code>. See <a href="{_WPT_DOC_URL}">these
            instructions</a> about running these tests locally and triaging
            failures.</strong></p>""").replace('\n', ' ')
        url = wpt_fyi_url(self.name)
        if url:
            summary += f'<p><a href="{url}">Latest wpt.fyi results</a></p>'
        for name in ['stderr', 'crash_log']:
            if name in self.artifacts:
                summary += f'<h3>{name}</h3>'
                summary += f'<p><text-artifact artifact-id="{name}"/></p>'
        return summary


class Event(NamedTuple):
    action: str
    time: int
    thread: str
    pid: int
    source: str

    @classmethod
    def make_raw_event(cls, action: str, **extra: Any) -> Dict[str, Any]:
        """Create a raw synthetic `mozlog` event."""
        return {
            'action': action,
            'time': time.time() * 1000,  # Time since epoch in ms
            'thread': threading.current_thread().name,
            'pid': os.getpid(),
            'source': __name__,
            **extra,  # Action-specific fields
        }


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

    @staticmethod
    def decode_image(screenshot: 'ReftestScreenshot') -> bytes:
        return base64.b64decode(screenshot['screenshot'].strip())


@dataclass
class BrowserOutput:
    """Output from a live browser process.

    Attributes:
        command: The base command to start the browser (i.e., does not contain
            test URI).
        log: A running buffer of output (usually stderr) from that browser.
    """
    # TODO(crbug.com/41494889): Consider sharing a base class with
    # `DriverOutput` in `web_tests`.
    command: List[str] = field(default_factory=list)
    log: io.StringIO = field(default_factory=io.StringIO)


class WPTResultsProcessor:
    # Executables that wptrunner can start and whose output should go into the
    # crash log.
    _executables = [
        'chromedriver',
        'logcat',
        'content_shell',
    ]
    _cmd_log_pattern: re.Pattern = re.compile(
        'Launching \w+: (?P<command>.*?)(\s+data:\S+)?$')

    def __init__(self,
                 fs: FileSystem,
                 port: Port,
                 artifacts_dir: str = '',
                 sink: Optional[ResultSinkReporter] = None,
                 test_name_prefix: str = '',
                 failure_threshold: Optional[int] = None,
                 crash_timeout_threshold: Optional[int] = None,
                 reset_results: bool = False,
                 repeat_each: int = 1,
                 processes: Optional[int] = None):
        self.fs = fs
        self.port = port
        self.artifacts_dir = artifacts_dir
        self.sink = sink or ResultSinkReporter(host=port.typ_host())
        # This prefix does not actually exist on disk and only affects how the
        # results are reported.
        if test_name_prefix and not test_name_prefix.endswith('/'):
            test_name_prefix += '/'
        self.test_name_prefix = test_name_prefix
        self.path_finder = path_finder.PathFinder(self.fs)
        self._test_uri_mapper = TestURIMapper(self.port)
        # Provide placeholder properties until the `suite_start` events are
        # processed.
        self.run_info = dict(mozinfo.info)

        self._iteration: int = 0
        self._results: Dict[str, WPTResult] = {}
        # Map PIDs of running browsers to their output. The mapping's capacity
        # should be limited to `wpt run --processes`.
        #
        # Since wptrunner doesn't communicate browser lifecycles, use
        # `LRUMapping` to avoid accumulating too many logs that will never be
        # retrieved by a `test_end` event. Dead browser processes stop
        # producing logs, which will eventually be purged by output produced by
        # restarted browsers.
        self.browser_outputs: Dict[int, BrowserOutput] = LRUMapping(
            processes or port.default_child_processes())
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
        self.repeat_each = repeat_each
        assert self.failure_threshold > 0
        assert self.crash_timeout_threshold > 0

        # Collects the number of failures by status (only from initial run).
        self._num_failures_by_status = collections.defaultdict(int)
        # Results includes retries, used for computing full_results.json
        self._results_by_name = collections.defaultdict(list)

    @functools.cached_property
    def _expectations(self):
        return TestExpectations(self.port)

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
        return final_result

    @contextlib.contextmanager
    def stream_results(
            self,
            timeout: Optional[float] = None) -> Iterator[queue.SimpleQueue]:
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
        assert timeout is None or timeout >= 0, timeout
        events = queue.SimpleQueue()
        worker = threading.Thread(target=self._consume_events,
                                  args=(events, ),
                                  name='results-stream-worker',
                                  daemon=True)
        worker.start()
        try:
            yield events
        except KeyboardInterrupt:
            # Use a greater default timeout on CI so that the processor has
            # enough time to report all incomplete tests. Slow exit is less of a
            # concern.
            if timeout is None and self.sink.resultdb_supported:
                timeout = 15
            raise
        finally:
            # Keep the default exit fast for local or completed runs.
            if timeout is None:
                timeout = 3
            # Send a shutdown event, if one has not been sent already, to tell
            # the worker to exit.
            deadline = time.time() + timeout
            events.put(Event.make_raw_event('shutdown'), timeout=timeout)
            worker.join(timeout=max(0, deadline - time.time()))
            if worker.is_alive():
                _log.warning('Results stream worker did not stop.')
            else:
                _log.info('Stopped results stream worker.')

    def _consume_events(self, events: queue.SimpleQueue):
        while True:
            event = events.get()
            try:
                self.process_event(event)
            except StreamShutdown:
                return
            except Exception as error:
                _log.exception('Unable to process event %r: %s', event, error)

    def process_event(self, raw_event: Dict[str, Any]):
        raw_event = dict(raw_event)
        event = Event(raw_event.pop('action'), raw_event.pop('time'),
                      raw_event.pop('thread'), raw_event.pop('pid'),
                      raw_event.pop('source'))
        test = raw_event.pop('test', None)
        subsuite = raw_event.pop('subsuite', '')
        if test:
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
                    tests: Optional[Dict[str, List[str]]] = None,
                    **_):
        if run_info:
            self.run_info.update(run_info)
        if self._iteration > 0:
            return
        # Register tests that will run so that they can be reported as
        # "unexpectedly skipped" if they have no results on shutdown.
        for group_key, group_tests in (tests or {}).items():
            subsuite, _, _ = group_key.rpartition(':')
            for test_url in group_tests:
                test = self._get_chromium_test_name(test_url, subsuite)
                self._results_by_name[test] = []

    def suite_end(self, event: Event, **_):
        self._iteration += 1

    def _get_chromium_test_name(self, test: str, subsuite: str) -> str:
        test = test[1:] if test.startswith('/') else test
        wpt_dir, _ = self.port.split_wpt_dir(test)
        if wpt_dir != 'wpt_internal':
            test = self.path_finder.wpt_prefix() + test
        if subsuite:
            test = f'virtual/{subsuite}/{test}'
        return test

    def test_start(self, event: Event, test: str, **_):
        expected_text = self.port.expected_text(test)
        if expected_text:
            baseline = parse_testharness_baseline(expected_text.decode())
        else:
            baseline = []
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
            exp_line=self._expectations.get_expectations(test),
            baseline=baseline)

    def get_path_from_test_root(self, test: str) -> str:
        wpt_dir, url_from_wpt_dir = self.port.split_wpt_dir(test)
        manifest = self.port.wpt_manifest(wpt_dir)
        return manifest.file_path_for_test_url(url_from_wpt_dir)

    @memoized
    def _file_path_for_test(self, test: str) -> str:
        _, test = self.port.get_suite_name_and_base_test(test)
        path_from_test_root = self.get_path_from_test_root(test)
        if not path_from_test_root:
            raise EventProcessingError(
                'Test ID %r does not exist in the manifest' % test)
        wpt_dir, _ = self.port.split_wpt_dir(test)
        return self.path_finder.path_from_web_tests(*posixpath.split(wpt_dir),
                                                    path_from_test_root)

    def get_test_type(self, test: str) -> str:
        _, test = self.port.get_suite_name_and_base_test(test)
        wpt_dir, url_from_wpt_dir = self.port.split_wpt_dir(test)
        manifest = self.port.wpt_manifest(wpt_dir)
        return manifest.get_test_type(url_from_wpt_dir)

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
        result.update_from_subtest(subtest, status, message)

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
        result.pid = (extra or {}).get('browser_pid', 0)
        result.update_from_test(status, message)
        artifacts, image_diff_stats = self._extract_artifacts(
            result, message, extra or {})
        result.artifacts = artifacts.artifacts
        result.image_diff_stats = image_diff_stats
        if result.unexpected:
            self._handle_unexpected_result(result)
        product = self.port.get_option('product', '(unknown)')
        self.sink.report_individual_test_result(
            test_name_prefix=self.test_name_prefix,
            result=result,
            artifact_output_dir=self.fs.dirname(self.artifacts_dir),
            expectations=None,
            test_file_location=result.file_path,
            html_summary=result.summarize(product))
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
        incomplete_tests = {
            test
            for test, results in self._results_by_name.items() if not results
        }
        if incomplete_tests:
            _log.warning(f'{len(incomplete_tests)} test(s) never completed.')
            with self.sink.batch_results():
                # Using the same timestamp for both events produces the desired
                # test duration of zero.
                start_event = Event(**Event.make_raw_event('test_start'))
                end_event = Event('test_end', *start_event[1:])
                for test in incomplete_tests:
                    expected = chromium_to_wptrunner_statuses(
                        self._expectations.get_expectations(test).results,
                        self.get_test_type(test))
                    self.test_start(start_event, test)
                    self.test_end(end_event, test, 'SKIP', expected)

        for test, results in self._results_by_name.items():
            assert results, test
        raise StreamShutdown

    def create_final_results(self):
        # compute the tests dict
        tests = {}
        num_passes = num_regressions = 0
        shard_index = self.port.get_option('shard_index')
        for test_name, results in self._results_by_name.items():
            # TODO: the expected result calculated this way could change each time
            expected = ' '.join(results[0].expected)
            actual = [result.actual for result in results]
            is_pass = results[-1].actual == ResultType.Pass
            if is_pass:
                num_passes += 1

            test_dict = {}
            test_dict['expected'] = expected
            test_dict['actual'] = ' '.join(actual)

            # Fields below are optional. To avoid bloating the output results json
            # too much, only add them when they are True or non-empty.
            if len(set(actual)) > 1:
                test_dict['is_flaky'] = True
            if shard_index is not None:
                test_dict['shard'] = shard_index

            rounded_run_time = round(results[0].took, 1)
            if rounded_run_time:
                test_dict['time'] = rounded_run_time

            if self.port.is_slow_wpt_test(test_name):
                test_dict['is_slow_test'] = True
            if results[-1].unexpected:
                test_dict['is_unexpected'] = True
            if results[-1].is_regression:
                test_dict['is_regression'] = True
                num_regressions += 1
            if results[0].image_diff_stats:
                test_dict['image_diff_stats'] = results[0].image_diff_stats

            has_stderr = any(result.has_stderr for result in results)
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

    def process_output(self, event: Event, command: str, data: Any,
                       process: str, **_):
        if not any(executable in command for executable in self._executables):
            return
        if not isinstance(data, str):
            data = json.dumps(data, sort_keys=True)
        # TODO(crbug.com/333782826): Remove after addressing non-integer values
        # by wptrunner's Android drivers:
        # https://github.com/web-platform-tests/wpt/blob/073f56c2/tools/wptrunner/wptrunner/browsers/chrome_android.py#L126
        #
        # which does not adhere to:
        # https://firefox-source-docs.mozilla.org/mozbase/mozlog.html
        #
        # which says that `process` is a PID.
        try:
            pid = int(process)
        except ValueError:
            return
        output = self.browser_outputs.get(pid)
        if not output:
            output = self.browser_outputs[pid] = BrowserOutput()
        output.log.write(f'{data}\n')

        if output.command:
            # In wptrunner, there's a 1-1 correspondence between `chromedriver`
            # PID and session [0], so there's no need to set the command more than
            # once.
            #
            # [0]: https://github.com/web-platform-tests/wpt/blob/f95204fd/tools/wptrunner/wptrunner/executors/executorwebdriver.py#L478-L498
            return
        cmd_match = self._cmd_log_pattern.search(data)
        if cmd_match:
            output.command = self._parse_command(cmd_match['command'])

    def _parse_command(self, raw_command: str) -> List[str]:
        tokens = []
        for raw_token in shlex.split(raw_command):
            # Unfortunately, whitespace in switch values is not preserved in
            # the logged output. To produce a `command` artifact that's
            # copy-paste runnable in a shell, try to reconstruct the value with
            # this special hardcoded rule. It relies on the assumption that
            # all arguments after the initial binary are switches prefixed with
            # `--`.
            if (tokens and tokens[-1].startswith('--host-resolver-rules=')
                    and not raw_token.startswith('--')):
                tokens[-1] += f' {raw_token}'
            # Don't keep any headless option, since it will hide the browser
            # locally.
            elif not raw_token.startswith('--headless'):
                tokens.append(raw_token)
        return tokens

    def _write_text_results(self, result: WPTResult, artifacts: Artifacts):
        """Write actual, expected, and diff text outputs to disk, if possible.

        If either the actual or expected output is missing (i.e., all-pass), no
        diffs are produced.

        Arguments:
            result: WPT test result.
            artifacts: Artifact manager (note that this is not the artifact ID
                to paths mapping itself).
        """
        assert result.can_have_subtests, (
            f'{result.name!r} cannot have a text baseline')
        actual_subpath = self.port.output_filename(
            result.name, test_failures.FILENAME_SUFFIX_ACTUAL, '.txt')
        expected_subpath = self.port.output_filename(
            result.name, test_failures.FILENAME_SUFFIX_EXPECTED, '.txt')
        actual_text = result.format_baseline()
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

        if not actual_text:
            return
        expected_text = expected_text or ''
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
                           screenshot1: ReftestScreenshot,
                           screenshot2: ReftestScreenshot):
        """Write actual, expected, and diff screenshots to disk, if possible.

        Arguments:
            test_name: Web test name (a URL whose path is relative to
                `web_tests/`).
            artifacts: Artifact manager.
            screenshot1: A screenshot of either the test page or one of its
                references that failed comparison.
            screenshot2: The screenshot compared against `screenshot1`.

        The screenshots may be in either order and may represent any compatible
        comparison (test against reference, or match ref against mismatch ref).

        Returns:
            The diff stats if the screenshots are different.
        """
        _, base_test = self.port.get_suite_name_and_base_test(test_name)
        test_url = self.path_finder.strip_wpt_path(base_test)

        # `test_url` is the globally mounted URL (i.e., its canonical ID),
        # whereas `url_from_wpt_dir`'s path part is relative to the test root
        # (i.e., `external/wpt` or `wpt_internal`). These URLs happen to be
        # identical for `external/wpt`, which wptserve mounts to `/`.
        wpt_dir, url_from_wpt_dir = self.port.split_wpt_dir(base_test)
        assert wpt_dir, f'{base_test!r} is not a WPT'
        manifest = self.port.wpt_manifest(wpt_dir)
        references = manifest.extract_reference_list(url_from_wpt_dir)
        relation_by_ref = {url: relation for relation, url in references}
        assert set(relation_by_ref.values()) <= {'==', '!='}

        test_screenshot = match_screenshot = mismatch_screenshot = None
        for screenshot in [screenshot1, screenshot2]:
            # Compare URLs with a leading `/` to follow the convention
            # wptrunner uses. There can only be up to one of each type of
            # screenshot.
            if screenshot['url'] == f'/{test_url}':
                assert not test_screenshot
                test_screenshot = screenshot
            elif relation_by_ref[screenshot['url']] == '==':
                assert not match_screenshot
                match_screenshot = screenshot
            else:
                assert not mismatch_screenshot
                mismatch_screenshot = screenshot

        # Because fuzzy rules allow matches without pixel-by-pixel equality,
        # the screenshots in a mismatch failure may be slightly different, so we
        # still extract both.
        if mismatch_screenshot:
            expected = mismatch_screenshot
            # For tests with both match and mismatch references, the references
            # may be compared if the match reference is found to be equivalent
            # to the test page.
            actual = test_screenshot or match_screenshot
        else:
            expected, actual = match_screenshot, test_screenshot

        assert expected
        expected_image = ReftestScreenshot.decode_image(expected)
        expected_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_EXPECTED, '.png')
        artifacts.CreateArtifact('expected_image', expected_subpath,
                                 expected_image)

        assert actual
        actual_image = ReftestScreenshot.decode_image(actual)
        actual_subpath = self.port.output_filename(
            test_name, test_failures.FILENAME_SUFFIX_ACTUAL, '.png')
        artifacts.CreateArtifact('actual_image', actual_subpath, actual_image)

        diff_bytes, stats, error = self.port.diff_image(
            expected_image, actual_image)
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
                   artifact_id: str, suffix: str, contents: str):
        log_subpath = self.port.output_filename(test_name, suffix, '.txt')
        artifacts.CreateArtifact(artifact_id, log_subpath, contents.encode())

    def _extract_artifacts(self, result: WPTResult, message: Optional[str],
                           extra: Dict[str, Any]) -> Tuple[Artifacts, str]:
        # Ensure `artifacts_base_dir` (i.e., `layout-test-results`) is prepended
        # to `full_results_jsonp.js` paths so that `results.html` can correctly
        # fetch artifacts.
        artifacts = Artifacts(self.fs.dirname(self.artifacts_dir),
                              self.sink.host,
                              iteration=self._iteration,
                              artifacts_base_dir=self.fs.basename(
                                  self.artifacts_dir),
                              repeat_tests=(self.repeat_each > 1))
        image_diff_stats = None
        # Dump output for `--reset-results`, even if the test passes, as the
        # current port may fall back to a failing port.
        if self.reset_results or result.actual == ResultType.Failure:
            if result.can_have_subtests:
                self._write_text_results(result, artifacts)
            screenshots = extra.get('reftest_screenshots') or []
            if screenshots:
                # Remove the relation operator `==` or `!=` between the
                # screenshot objects.
                screenshot1, _, screenshot2 = screenshots
                image_diff_stats = self._write_screenshots(
                    result.name, artifacts, screenshot1, screenshot2)

        if message:
            self._write_log(result.name, artifacts, 'crash_log',
                            test_failures.FILENAME_SUFFIX_CRASH_LOG, message)

        if leak_counters := extra.get('leak_counters'):
            leak_log = json.dumps(leak_counters, separators=(',', ':'))
            self._write_log(result.name, artifacts, 'leak_log',
                            test_failures.FILENAME_SUFFIX_LEAK_LOG, leak_log)

        # If the browser process isn't restarted, it's possible for that process
        # to continue producing stdio that will be dumped into the log for the
        # next test that browser runs.
        output = self.browser_outputs.get(result.pid)
        if output:
            self._write_log(result.name, artifacts, 'stderr',
                            test_failures.FILENAME_SUFFIX_STDERR,
                            output.log.getvalue())
            output.log = io.StringIO()

        if output and output.command:
            test_name = (self.port.lookup_virtual_test_base(result.name)
                         or result.name)
            uri = self._test_uri_mapper.test_to_uri(test_name)
            command = shlex.join([*output.command, uri])
            self._write_log(result.name, artifacts, 'command',
                            test_failures.FILENAME_SUFFIX_CMD, command)

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
