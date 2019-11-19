# Copyright (C) 2010 Google Inc. All rights reserved.
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

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models import test_results
from blinkpy.web_tests.models import test_run_results
from blinkpy.web_tests.port.driver import DriverOutput

def get_result(test_name, result_type=test_expectations.PASS, run_time=0):
    failures = []
    dummy_1, dummy_2 = DriverOutput(None, None, None, None), DriverOutput(None, None, None, None)
    if result_type == test_expectations.TIMEOUT:
        failures = [test_failures.FailureTimeout(dummy_1)]
    elif result_type == test_expectations.CRASH:
        failures = [test_failures.FailureCrash(dummy_1)]
    elif result_type == test_expectations.FAIL:
        failures = [test_failures.TestFailure(dummy_1, dummy_2)]
    return test_results.TestResult(test_name, failures=failures, test_run_time=run_time)


def run_results(port, extra_skipped_tests=None):
    tests = [
        'passes/text.html',
        'failures/expected/timeout.html',
        'failures/expected/crash.html',
        'failures/expected/leak.html',
        'failures/expected/keyboard.html',
        'failures/expected/audio.html',
        'failures/expected/text.html',
        'passes/skipped/skip.html'
    ]
    expectations = test_expectations.TestExpectations(port, tests)
    if extra_skipped_tests:
        expectations.add_extra_skipped_tests(extra_skipped_tests)
    return test_run_results.TestRunResults(expectations, len(tests))


def summarized_results(port, expected, passing, flaky, only_include_failing=False, extra_skipped_tests=None):
    test_is_slow = False

    all_retry_results = []
    initial_results = run_results(port, extra_skipped_tests)
    if expected:
        initial_results.add(get_result('passes/text.html', test_expectations.PASS), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/audio.html', test_expectations.FAIL), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/timeout.html', test_expectations.TIMEOUT), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/crash.html', test_expectations.CRASH), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/leak.html', test_expectations.FAIL), expected, test_is_slow)
    elif passing:
        skipped_result = get_result('passes/skipped/skip.html')
        skipped_result.type = test_expectations.SKIP
        initial_results.add(skipped_result, True, test_is_slow)

        initial_results.add(get_result('passes/text.html', run_time=1), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/audio.html'), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/timeout.html'), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/crash.html'), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/leak.html'), expected, test_is_slow)
    else:
        initial_results.add(get_result('passes/text.html', test_expectations.TIMEOUT, run_time=1), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/audio.html',
                                       test_expectations.CRASH, run_time=0.049), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/timeout.html',
                                       test_expectations.FAIL, run_time=0.05), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/crash.html', test_expectations.TIMEOUT), expected, test_is_slow)
        initial_results.add(get_result('failures/expected/leak.html', test_expectations.TIMEOUT), expected, test_is_slow)

        # we only list keyboard.html here, since normally this is WontFix
        initial_results.add(get_result('failures/expected/keyboard.html', test_expectations.SKIP), expected, test_is_slow)

        initial_results.add(get_result('failures/expected/text.html', test_expectations.FAIL), expected, test_is_slow)

        all_retry_results = [run_results(port, extra_skipped_tests),
                             run_results(port, extra_skipped_tests),
                             run_results(port, extra_skipped_tests)]

        def add_result_to_all_retries(new_result, expected):
            for run_result in all_retry_results:
                run_result.add(new_result, expected, test_is_slow)

        if flaky:
            add_result_to_all_retries(get_result('passes/text.html', test_expectations.PASS), True)
            add_result_to_all_retries(
                get_result('failures/expected/audio.html', test_expectations.FAIL), True)
            add_result_to_all_retries(
                get_result('failures/expected/leak.html', test_expectations.FAIL), True)
            add_result_to_all_retries(
                get_result('failures/expected/timeout.html', test_expectations.FAIL), True)

            all_retry_results[0].add(
                get_result('failures/expected/crash.html', test_expectations.FAIL),
                False, test_is_slow)
            all_retry_results[1].add(
                get_result('failures/expected/crash.html', test_expectations.CRASH),
                True, test_is_slow)
            all_retry_results[2].add(
                get_result('failures/expected/crash.html', test_expectations.FAIL),
                False, test_is_slow)

            all_retry_results[0].add(
                get_result('failures/expected/text.html', test_expectations.FAIL),
                True, test_is_slow)

        else:
            add_result_to_all_retries(
                get_result('passes/text.html', test_expectations.TIMEOUT), False)
            add_result_to_all_retries(
                get_result('failures/expected/audio.html', test_expectations.FAIL), False)
            add_result_to_all_retries(
                get_result('failures/expected/crash.html', test_expectations.TIMEOUT), False)
            add_result_to_all_retries(
                get_result('failures/expected/leak.html', test_expectations.TIMEOUT), False)

            all_retry_results[0].add(
                get_result('failures/expected/timeout.html', test_expectations.FAIL),
                False, test_is_slow)
            all_retry_results[1].add(
                get_result('failures/expected/timeout.html', test_expectations.CRASH),
                False, test_is_slow)
            all_retry_results[2].add(
                get_result('failures/expected/timeout.html', test_expectations.FAIL),
                False, test_is_slow)

    return test_run_results.summarize_results(
        port, initial_results.expectations, initial_results, all_retry_results,
        only_include_failing=only_include_failing)


class InterpretTestFailuresTest(unittest.TestCase):

    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get(port_name='test')
        self._actual_output = DriverOutput(None, None, None, None)
        self._expected_output = DriverOutput(None, None, None, None)

    def test_interpret_test_failures(self):
        test_dict = test_run_results._interpret_test_failures(
            [test_failures.FailureReftestMismatchDidNotOccur(
                self._actual_output, self._expected_output,
                self.port.abspath_for_test('foo/reftest-expected-mismatch.html'))])
        self.assertEqual(len(test_dict), 0)

        test_dict = test_run_results._interpret_test_failures(
            [test_failures.FailureMissingAudio(
                self._actual_output, self._expected_output)])
        self.assertIn('is_missing_audio', test_dict)

        test_dict = test_run_results._interpret_test_failures(
            [test_failures.FailureMissingResult(
                self._actual_output, self._expected_output)])
        self.assertIn('is_missing_text', test_dict)

        test_dict = test_run_results._interpret_test_failures(
            [test_failures.FailureMissingImage(
                self._actual_output, self._expected_output)])
        self.assertIn('is_missing_image', test_dict)

        test_dict = test_run_results._interpret_test_failures(
            [test_failures.FailureMissingImageHash(
                self._actual_output, self._expected_output)])
        self.assertIn('is_missing_image', test_dict)


class SummarizedResultsTest(unittest.TestCase):

    def setUp(self):
        host = MockHost()
        self.port = host.port_factory.get(port_name='test')

    def test_no_chromium_revision(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertNotIn('revision', summary)

    def test_num_failures_by_type(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertEquals(
            summary['num_failures_by_type'],
            {
                'CRASH': 1,
                'PASS': 1,
                'SKIP': 0,
                'SLOW': 0,
                'TIMEOUT': 3,
                'FAIL': 2,
            })

        summary = summarized_results(self.port, expected=True, passing=False, flaky=False)
        self.assertEquals(
            summary['num_failures_by_type'],
            {
                'CRASH': 1,
                'PASS': 1,
                'SKIP': 0,
                'SLOW': 0,
                'TIMEOUT': 1,
                'FAIL': 2,
            })

        summary = summarized_results(self.port, expected=False, passing=True, flaky=False)
        self.assertEquals(
            summary['num_failures_by_type'],
            {
                'CRASH': 0,
                'PASS': 5,
                'SKIP': 1,
                'SLOW': 0,
                'TIMEOUT': 0,
                'FAIL': 0,
            })

    def test_chromium_revision(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertNotEquals(summary['chromium_revision'],
                             '')

    def test_bug_entry(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False)
        self.assertEquals(summary['tests']['passes']['skipped']['skip.html']['bugs'], ['Bug(test)'])

    def test_extra_skipped_tests(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False, extra_skipped_tests=['passes/text.html'])
        self.assertEquals(summary['tests']['passes']['text.html']['expected'], 'NOTRUN')

    def test_summarized_results_wontfix(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertEquals(summary['tests']['failures']['expected']['keyboard.html']['expected'], 'CRASH SKIP')
        self.assertTrue(summary['tests']['passes']['text.html']['is_unexpected'])
        self.assertEqual(summary['num_passes'], 1)
        self.assertEqual(summary['num_regressions'], 6)
        self.assertEqual(summary['num_flaky'], 0)

    def test_summarized_results_expected_pass(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False)
        self.assertTrue(summary['tests']['passes']['text.html'])
        self.assertEqual(summary['num_passes'], 5)
        self.assertEqual(summary['num_regressions'], 0)
        self.assertEqual(summary['num_flaky'], 0)

    def test_summarized_results_expected_only_include_failing(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=True, passing=False, flaky=False, only_include_failing=True)
        self.assertNotIn('passes', summary['tests'])
        self.assertTrue(summary['tests']['failures']['expected']['audio.html'])
        self.assertTrue(summary['tests']['failures']['expected']['timeout.html'])
        self.assertTrue(summary['tests']['failures']['expected']['crash.html'])
        self.assertTrue(summary['tests']['failures']['expected']['leak.html'])
        self.assertEqual(summary['num_passes'], 1)
        self.assertEqual(summary['num_regressions'], 0)
        self.assertEqual(summary['num_flaky'], 0)

    def test_summarized_results_skipped(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False)
        self.assertEquals(summary['tests']['passes']['skipped']['skip.html']['expected'], 'SKIP')

    def test_summarized_results_only_include_failing(self):
        self.port._options.builder_name = 'dummy builder'
        summary = summarized_results(self.port, expected=False, passing=True, flaky=False, only_include_failing=True)
        self.assertTrue('passes' not in summary['tests'])
        self.assertEqual(summary['num_passes'], 5)
        self.assertEqual(summary['num_regressions'], 0)
        self.assertEqual(summary['num_flaky'], 0)

    def test_rounded_run_times(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertEquals(summary['tests']['passes']['text.html']['time'], 1)
        self.assertTrue('time' not in summary['tests']['failures']['expected']['audio.html'])
        self.assertEquals(summary['tests']['failures']['expected']['timeout.html']['time'], 0.1)
        self.assertTrue('time' not in summary['tests']['failures']['expected']['crash.html'])
        self.assertTrue('time' not in summary['tests']['failures']['expected']['leak.html'])

    def test_timeout_then_unexpected_pass(self):
        test_name = 'failures/expected/text.html'
        expectations = test_expectations.TestExpectations(self.port, [test_name])
        initial_results = test_run_results.TestRunResults(expectations, 1)
        initial_results.add(get_result(test_name, test_expectations.TIMEOUT, run_time=1), False, False)
        all_retry_results = [test_run_results.TestRunResults(expectations, 1),
                             test_run_results.TestRunResults(expectations, 1),
                             test_run_results.TestRunResults(expectations, 1)]
        all_retry_results[0].add(get_result(test_name, test_expectations.FAIL, run_time=0.1), False, False)
        all_retry_results[1].add(get_result(test_name, test_expectations.PASS, run_time=0.1), False, False)
        all_retry_results[2].add(get_result(test_name, test_expectations.PASS, run_time=0.1), False, False)
        summary = test_run_results.summarize_results(
            self.port, expectations, initial_results, all_retry_results)
        self.assertIn('is_unexpected', summary['tests']['failures']['expected']['text.html'])
        self.assertEquals(summary['tests']['failures']['expected']['text.html']['expected'], 'FAIL')
        self.assertEquals(summary['tests']['failures']['expected']['text.html']['actual'], 'TIMEOUT FAIL PASS PASS')
        self.assertEquals(summary['num_passes'], 1)
        self.assertEquals(summary['num_regressions'], 0)
        self.assertEquals(summary['num_flaky'], 0)

    def test_summarized_results_flaky(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=True)

        self.assertEquals(summary['tests']['failures']['expected']['crash.html']['expected'], 'CRASH')
        self.assertEquals(summary['tests']['failures']['expected']['crash.html']['actual'], 'TIMEOUT FAIL CRASH FAIL')

        self.assertTrue('is_unexpected' not in summary['tests']['passes']['text.html'])
        self.assertEquals(summary['tests']['passes']['text.html']['expected'], 'PASS')
        self.assertEquals(summary['tests']['passes']['text.html']['actual'], 'TIMEOUT PASS PASS PASS')

        self.assertTrue(summary['tests']['failures']['expected']['timeout.html']['is_unexpected'])
        self.assertEquals(summary['tests']['failures']['expected']['timeout.html']['expected'], 'TIMEOUT')
        self.assertEquals(summary['tests']['failures']['expected']['timeout.html']['actual'], 'FAIL FAIL FAIL FAIL')

        self.assertTrue('is_unexpected' not in summary['tests']['failures']['expected']['leak.html'])
        self.assertEquals(summary['tests']['failures']['expected']['leak.html']['expected'], 'FAIL')
        self.assertEquals(summary['tests']['failures']['expected']['leak.html']['actual'], 'TIMEOUT FAIL FAIL FAIL')

        self.assertTrue('is_unexpected' not in summary['tests']['failures']['expected']['audio.html'])
        self.assertEquals(summary['tests']['failures']['expected']['audio.html']['expected'], 'FAIL')
        self.assertEquals(summary['tests']['failures']['expected']['audio.html']['actual'], 'CRASH FAIL FAIL FAIL')

        self.assertEquals(summary['tests']['failures']['expected']['text.html']['expected'], 'FAIL')
        self.assertTrue('is_unexpected' not in summary['tests']['failures']['expected']['text.html'])

        self.assertEquals(summary['num_flaky'], 6)
        self.assertEquals(summary['num_passes'], 1)  # keyboard.html
        self.assertEquals(summary['num_regressions'], 0)

    def test_summarized_results_flaky_pass_after_first_retry(self):
        test_name = 'passes/text.html'
        expectations = test_expectations.TestExpectations(self.port, [test_name])
        initial_results = test_run_results.TestRunResults(expectations, 1)
        initial_results.add(get_result(test_name, test_expectations.CRASH), False, False)
        all_retry_results = [test_run_results.TestRunResults(expectations, 1),
                             test_run_results.TestRunResults(expectations, 1),
                             test_run_results.TestRunResults(expectations, 1)]
        all_retry_results[0].add(get_result(test_name, test_expectations.TIMEOUT), False, False)
        all_retry_results[1].add(get_result(test_name, test_expectations.PASS), True, False)
        all_retry_results[2].add(get_result(test_name, test_expectations.PASS), True, False)
        summary = test_run_results.summarize_results(
            self.port, expectations, initial_results, all_retry_results)
        self.assertTrue('is_unexpected' not in summary['tests']['passes']['text.html'])
        self.assertEquals(summary['tests']['passes']['text.html']['expected'], 'PASS')
        self.assertEquals(summary['tests']['passes']['text.html']['actual'], 'CRASH TIMEOUT PASS PASS')
        self.assertEquals(summary['num_flaky'], 1)
        self.assertEquals(summary['num_passes'], 0)
        self.assertEquals(summary['num_regressions'], 0)

    def test_summarized_results_with_iterations(self):
        test_name = 'passes/text.html'
        expectations = test_expectations.TestExpectations(self.port, [test_name])
        initial_results = test_run_results.TestRunResults(expectations, 3)
        initial_results.add(get_result(test_name, test_expectations.CRASH), False, False)
        initial_results.add(get_result(test_name, test_expectations.FAIL), False, False)
        initial_results.add(get_result(test_name, test_expectations.TIMEOUT), False, False)
        all_retry_results = [test_run_results.TestRunResults(expectations, 2)]
        all_retry_results[0].add(get_result(test_name, test_expectations.FAIL), False, False)
        all_retry_results[0].add(get_result(test_name, test_expectations.FAIL), False, False)

        summary = test_run_results.summarize_results(
            self.port, expectations, initial_results, all_retry_results)
        self.assertEquals(summary['tests']['passes']['text.html']['expected'], 'PASS')
        self.assertEquals(summary['tests']['passes']['text.html']['actual'], 'CRASH FAIL TIMEOUT FAIL FAIL')
        self.assertEquals(summary['num_flaky'], 0)
        self.assertEquals(summary['num_passes'], 0)
        self.assertEquals(summary['num_regressions'], 1)

    def test_summarized_results_regression(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)

        self.assertTrue(summary['tests']['failures']['expected']['timeout.html']['is_unexpected'])
        self.assertEquals(summary['tests']['failures']['expected']['timeout.html']['expected'], 'TIMEOUT')
        self.assertEquals(summary['tests']['failures']['expected']['timeout.html']['actual'], 'FAIL FAIL CRASH FAIL')

        self.assertTrue(summary['tests']['passes']['text.html']['is_unexpected'])
        self.assertEquals(summary['tests']['passes']['text.html']['expected'], 'PASS')
        self.assertEquals(summary['tests']['passes']['text.html']['actual'], 'TIMEOUT TIMEOUT TIMEOUT TIMEOUT')

        self.assertTrue(summary['tests']['failures']['expected']['crash.html']['is_unexpected'])
        self.assertEquals(summary['tests']['failures']['expected']['crash.html']['expected'], 'CRASH')
        self.assertEquals(summary['tests']['failures']['expected']['crash.html']['actual'], 'TIMEOUT TIMEOUT TIMEOUT TIMEOUT')

        self.assertTrue(summary['tests']['failures']['expected']['leak.html']['is_unexpected'])
        self.assertEquals(summary['tests']['failures']['expected']['leak.html']['expected'], 'FAIL')
        self.assertEquals(summary['tests']['failures']['expected']['leak.html']['actual'], 'TIMEOUT TIMEOUT TIMEOUT TIMEOUT')

        self.assertEquals(summary['tests']['failures']['expected']['audio.html']['expected'], 'FAIL')
        self.assertEquals(summary['tests']['failures']['expected']['audio.html']['actual'], 'CRASH FAIL FAIL FAIL')

        self.assertEquals(summary['num_regressions'], 6)
        self.assertEquals(summary['num_passes'], 1)  # keyboard.html
        self.assertEquals(summary['num_flaky'], 0)

    def test_results_contains_path_delimiter(self):
        summary = summarized_results(self.port, expected=False, passing=False, flaky=False)
        self.assertEqual(summary['path_delimiter'], '/')
