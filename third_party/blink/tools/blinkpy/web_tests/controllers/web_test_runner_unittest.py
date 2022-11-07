# Copyright (C) 2012 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import mock
import sys
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests import run_web_tests
from blinkpy.web_tests.controllers.test_result_sink import CreateTestResultSink
from blinkpy.web_tests.controllers.web_test_runner import (
    WebTestRunner,
    Worker,
    Sharder,
    TestRunInterruptedException,
)
from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.test_run_results import (
    TestRunResults,
    InterruptReason,
)
from blinkpy.web_tests.models.test_input import TestInput
from blinkpy.web_tests.models.test_results import TestResult
from blinkpy.web_tests.port.test import TestPort
from blinkpy.web_tests.port.driver import DriverOutput

TestExpectations = test_expectations.TestExpectations


class FakePrinter(object):
    num_completed = 0
    num_tests = 0

    def print_expected(self, run_results, get_tests_with_result_type):
        pass

    def print_workers_and_shards(self, port, num_workers, num_shards,
                                 num_locked_shards):
        pass

    def print_started_test(self, test_name):
        pass

    def print_finished_test(self, port, result, expected, exp_str, got_str):
        pass

    def write(self, msg):
        pass

    def writeln(self, msg):
        pass

    def write_update(self, msg):
        pass

    def flush(self):
        pass


class LockCheckingRunner(WebTestRunner):
    def __init__(self, port, options, printer, tester, http_lock, sink):
        super(LockCheckingRunner,
              self).__init__(options, port, printer,
                             port.results_directory(), lambda test_name: False,
                             sink)
        self._finished_list_called = False
        self._tester = tester
        self._should_have_http_lock = http_lock


# TODO(crbug.com/926841): Debug running this test on Swarming on Windows.
# Ensure that all child processes are always cleaned up.
@unittest.skipIf(sys.platform == 'win32', 'may not clean up child processes')
class WebTestRunnerTests(unittest.TestCase):
    def setUp(self):
        self._actual_output = DriverOutput(
            text='', image=None, image_hash=None, audio=None)
        self._expected_output = DriverOutput(
            text='', image=None, image_hash=None, audio=None)

    # pylint: disable=protected-access
    def _runner(self, port=None):
        # FIXME: we shouldn't have to use run_web_tests.py to get the options we need.
        options = run_web_tests.parse_args(['--platform',
                                            'test-mac-mac10.11'])[0]
        options.child_processes = '1'

        host = MockHost()
        port = port or host.port_factory.get(options.platform, options=options)
        return LockCheckingRunner(port, options, FakePrinter(), self, True,
                                  CreateTestResultSink(port))

    def _run_tests(self, runner, tests):
        test_inputs = [TestInput(test, timeout_ms=6000) for test in tests]
        expectations = TestExpectations(runner._port, tests)
        runner.run_tests(expectations, test_inputs, set(), num_workers=1)

    def test_interrupt_if_at_failure_limits(self):
        runner = self._runner()
        runner._exit_after_n_failures = 0
        runner._exit_after_n_crashes_or_times = 0
        test_names = ['passes/text.html', 'passes/image.html']
        runner._test_inputs = [
            TestInput(test_name, timeout_ms=6000) for test_name in test_names
        ]

        run_results = TestRunResults(TestExpectations(runner._port),
                                     len(test_names), None)
        run_results.unexpected_failures = 100
        run_results.unexpected_crashes = 50
        run_results.unexpected_timeouts = 50
        # No exception when the exit_after* parameters are 0.
        runner._interrupt_if_at_failure_limits(run_results)

        # No exception when we haven't hit the limit yet.
        runner._exit_after_n_failures = 101
        runner._exit_after_n_crashes_or_timeouts = 101
        runner._interrupt_if_at_failure_limits(run_results)

        # Interrupt if we've exceeded either limit:
        runner._exit_after_n_crashes_or_timeouts = 10
        with self.assertRaises(TestRunInterruptedException):
            runner._interrupt_if_at_failure_limits(run_results)
        self.assertEqual(run_results.results_by_name['passes/text.html'].type,
                         'SKIP')
        self.assertEqual(run_results.results_by_name['passes/image.html'].type,
                         'SKIP')

        runner._exit_after_n_crashes_or_timeouts = 0
        runner._exit_after_n_failures = 10
        with self.assertRaises(TestRunInterruptedException):
            runner._interrupt_if_at_failure_limits(run_results)

    def test_keyboard_interrupt(self):
        runner = self._runner()
        runner._options.derived_batch_size = 1
        runner._options.must_use_derived_batch_size = True
        expectations = TestExpectations(runner._port)
        test_inputs = [TestInput('passes/text.html', timeout_ms=6000)]
        with mock.patch('blinkpy.common.message_pool.get',
                        side_effect=KeyboardInterrupt):
            results = runner.run_tests(expectations,
                                       test_inputs,
                                       tests_to_skip=[],
                                       num_workers=1,
                                       retry_attempt=0)
        self.assertEqual(results.interrupted, True)
        self.assertIs(results.interrupt_reason,
                      InterruptReason.EXTERNAL_SIGNAL)

    def test_update_summary_with_result(self):
        runner = self._runner()
        test = 'failures/expected/reftest.html'
        expectations = TestExpectations(runner._port)
        runner._expectations = expectations

        run_results = TestRunResults(expectations, 1, None)
        result = TestResult(
            test_name=test,
            failures=[
                test_failures.FailureReftestMismatchDidNotOccur(
                    self._actual_output, self._expected_output)
            ],
            reftest_type=['!='])
        runner._update_summary_with_result(run_results, result)
        self.assertEqual(1, run_results.expected)
        self.assertEqual(0, run_results.unexpected)

        run_results = TestRunResults(expectations, 1, None)
        result = TestResult(test_name=test, failures=[], reftest_type=['=='])
        runner._update_summary_with_result(run_results, result)
        self.assertEqual(0, run_results.expected)
        self.assertEqual(1, run_results.unexpected)

    def test_skipped_tests_are_sinked(self):
        runner = self._runner()
        runner._options.derived_batch_size = 1
        runner._options.must_use_derived_batch_size = True
        expectations = TestExpectations(runner._port)
        with mock.patch.object(runner, "_test_result_sink") as rdb:
            runner.run_tests(
                expectations,
                [],
                tests_to_skip=['skips/image.html'],
                num_workers=1,
                retry_attempt=0,
            )
            rdb.sink.assert_called_with(
                True, TestResult(test_name='skips/image.html'), expectations)

    def test_results_are_sinked(self):
        runner = self._runner()
        runner._options.derived_batch_size = 1
        runner._options.must_use_derived_batch_size = True
        test_names = ['passes/text.html', 'passes/image.html']
        test_inputs = [
            TestInput(test_name, timeout_ms=6000) for test_name in test_names
        ]
        with mock.patch.object(runner, "_test_result_sink") as rdb:
            runner.run_tests(
                TestExpectations(runner._port),
                test_inputs,
                tests_to_skip=[],
                num_workers=1,
                retry_attempt=0,
            )
            rdb.sink.assert_has_calls(
                '', [True, TestResult(test_name='passes/text.html')])
            rdb.sink.assert_has_calls(
                '', [True, TestResult(test_name='passes/images.html')])


class SharderTests(unittest.TestCase):

    test_list = [
        "http/tests/websocket/tests/unicode.htm",
        "animations/keyframes.html",
        "http/tests/security/view-source-no-refresh.html",
        "http/tests/websocket/tests/websocket-protocol-ignored.html",
        "fast/css/display-none-inline-style-change-crash.html",
        "http/tests/xmlhttprequest/supported-xml-content-types.html",
        "dom/html/level2/html/HTMLAnchorElement03.html",
        "dom/html/level2/html/HTMLAnchorElement06.html",
        "perf/object-keys.html",
        "virtual/threaded/dir/test.html",
        "virtual/threaded/fast/foo/test.html",
    ]

    def get_test_input(self, test_file):
        return TestInput(
            test_file,
            requires_lock=(test_file.startswith('http')
                           or test_file.startswith('perf')))

    def get_shards(self,
                   num_workers,
                   fully_parallel,
                   run_singly,
                   test_list=None,
                   max_locked_shards=1):
        port = TestPort(MockSystemHost())
        self.sharder = Sharder(port.split_test, max_locked_shards)
        test_list = test_list or self.test_list
        return self.sharder.shard_tests(
            [self.get_test_input(test) for test in test_list], num_workers,
            fully_parallel, False, run_singly)

    def assert_shards(self, actual_shards, expected_shard_names):
        self.assertEqual(len(actual_shards), len(expected_shard_names))
        for i, shard in enumerate(actual_shards):
            expected_shard_name, expected_test_names = expected_shard_names[i]
            self.assertEqual(shard.name, expected_shard_name)
            self.assertEqual(
                [test_input.test_name for test_input in shard.test_inputs],
                expected_test_names)

    def test_shard_by_dir(self):
        locked, unlocked = self.get_shards(
            num_workers=2, fully_parallel=False, run_singly=False)

        # Note that although there are tests in multiple dirs that need locks,
        # they are crammed into a single shard in order to reduce the # of
        # workers hitting the server at once.
        self.assert_shards(locked, [('locked_shard_1', [
            'http/tests/security/view-source-no-refresh.html',
            'http/tests/websocket/tests/unicode.htm',
            'http/tests/websocket/tests/websocket-protocol-ignored.html',
            'http/tests/xmlhttprequest/supported-xml-content-types.html',
            'perf/object-keys.html'
        ])])
        self.assert_shards(
            unlocked,
            [('virtual/threaded/dir', ['virtual/threaded/dir/test.html']),
             ('virtual/threaded/fast/foo',
              ['virtual/threaded/fast/foo/test.html']),
             ('animations', ['animations/keyframes.html']),
             ('dom/html/level2/html', [
                 'dom/html/level2/html/HTMLAnchorElement03.html',
                 'dom/html/level2/html/HTMLAnchorElement06.html'
             ]),
             ('fast/css',
              ['fast/css/display-none-inline-style-change-crash.html'])])

    def test_shard_every_file(self):
        locked, unlocked = self.get_shards(
            num_workers=2,
            fully_parallel=True,
            max_locked_shards=2,
            run_singly=False)
        self.assert_shards(
            locked,
            [('locked_shard_1', [
                'http/tests/websocket/tests/unicode.htm',
                'http/tests/security/view-source-no-refresh.html',
                'http/tests/websocket/tests/websocket-protocol-ignored.html'
            ]),
             ('locked_shard_2', [
                 'http/tests/xmlhttprequest/supported-xml-content-types.html',
                 'perf/object-keys.html'
             ])])
        self.assert_shards(
            unlocked,
            [('virtual/threaded/dir', ['virtual/threaded/dir/test.html']),
             ('virtual/threaded/fast/foo',
              ['virtual/threaded/fast/foo/test.html']),
             ('.', ['animations/keyframes.html']),
             ('.', ['fast/css/display-none-inline-style-change-crash.html']),
             ('.', ['dom/html/level2/html/HTMLAnchorElement03.html']),
             ('.', ['dom/html/level2/html/HTMLAnchorElement06.html'])])

    def test_shard_in_two(self):
        locked, unlocked = self.get_shards(
            num_workers=1, fully_parallel=False, run_singly=False)
        self.assert_shards(locked, [('locked_tests', [
            'http/tests/websocket/tests/unicode.htm',
            'http/tests/security/view-source-no-refresh.html',
            'http/tests/websocket/tests/websocket-protocol-ignored.html',
            'http/tests/xmlhttprequest/supported-xml-content-types.html',
            'perf/object-keys.html'
        ])])
        self.assert_shards(unlocked, [('unlocked_tests', [
            'animations/keyframes.html',
            'fast/css/display-none-inline-style-change-crash.html',
            'dom/html/level2/html/HTMLAnchorElement03.html',
            'dom/html/level2/html/HTMLAnchorElement06.html',
            'virtual/threaded/dir/test.html',
            'virtual/threaded/fast/foo/test.html'
        ])])

    def test_shard_in_two_has_no_locked_shards(self):
        locked, unlocked = self.get_shards(
            num_workers=1,
            fully_parallel=False,
            run_singly=False,
            test_list=['animations/keyframe.html'])
        self.assertEqual(len(locked), 0)
        self.assertEqual(len(unlocked), 1)

    def test_shard_in_two_has_no_unlocked_shards(self):
        locked, unlocked = self.get_shards(
            num_workers=1,
            fully_parallel=False,
            run_singly=False,
            test_list=['http/tests/websocket/tests/unicode.htm'])
        self.assertEqual(len(locked), 1)
        self.assertEqual(len(unlocked), 0)

    def test_multiple_locked_shards(self):
        locked, _ = self.get_shards(
            num_workers=4,
            fully_parallel=False,
            max_locked_shards=2,
            run_singly=False)
        self.assert_shards(
            locked,
            [('locked_shard_1', [
                'http/tests/security/view-source-no-refresh.html',
                'http/tests/websocket/tests/unicode.htm',
                'http/tests/websocket/tests/websocket-protocol-ignored.html'
            ]),
             ('locked_shard_2', [
                 'http/tests/xmlhttprequest/supported-xml-content-types.html',
                 'perf/object-keys.html'
             ])])

        locked, _ = self.get_shards(
            num_workers=4, fully_parallel=False, run_singly=False)
        self.assert_shards(locked, [('locked_shard_1', [
            'http/tests/security/view-source-no-refresh.html',
            'http/tests/websocket/tests/unicode.htm',
            'http/tests/websocket/tests/websocket-protocol-ignored.html',
            'http/tests/xmlhttprequest/supported-xml-content-types.html',
            'perf/object-keys.html'
        ])])

    def test_virtual_shards(self):
        # With run_singly=False, we try to keep all of the tests in a virtual suite together even
        # when fully_parallel=True, so that we don't restart every time the command line args change.
        _, unlocked = self.get_shards(
            num_workers=2,
            fully_parallel=True,
            max_locked_shards=2,
            run_singly=False,
            test_list=['virtual/foo/bar1.html', 'virtual/foo/bar2.html'])
        self.assert_shards(unlocked, [
            ('virtual/foo', ['virtual/foo/bar1.html', 'virtual/foo/bar2.html'])
        ])

        # But, with run_singly=True, we have to restart every time anyway, so we want full parallelism.
        _, unlocked = self.get_shards(
            num_workers=2,
            fully_parallel=True,
            max_locked_shards=2,
            run_singly=True,
            test_list=['virtual/foo/bar1.html', 'virtual/foo/bar2.html'])
        self.assert_shards(unlocked, [('.', ['virtual/foo/bar1.html']),
                                      ('.', ['virtual/foo/bar2.html'])])


class WorkerTests(unittest.TestCase):
    class DummyCaller(object):
        worker_number = 1
        name = 'dummy_caller'

    def test_worker_no_manifest_update(self):
        # pylint: disable=protected-access
        options = run_web_tests.parse_args(['--platform',
                                            'test-mac-mac10.11'])[0]
        worker = Worker(self.DummyCaller(), '/results', options, {})
        self.assertTrue(options.manifest_update)
        self.assertFalse(worker._options.manifest_update)
