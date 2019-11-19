# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2011 Apple Inc. All rights reserved.
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

import json
import os
import re
import StringIO
import sys
import unittest

from blinkpy.common import exit_codes
from blinkpy.common import path_finder
from blinkpy.common.host import Host
from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import WEB_TESTS_LAST_COMPONENT
from blinkpy.common.system.path import abspath_to_uri
from blinkpy.common.system.system_host import SystemHost

from blinkpy.web_tests import run_web_tests
from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.port import test
from blinkpy.web_tests.views.printing import Printer

_MOCK_ROOT = os.path.join(
    path_finder.get_chromium_src_dir(), 'third_party', 'pymock')
sys.path.insert(0, _MOCK_ROOT)
import mock  # pylint: disable=wrong-import-position


def parse_args(extra_args=None, tests_included=False):
    extra_args = extra_args or []
    args = []
    if not '--platform' in extra_args:
        args.extend(['--platform', 'test'])

    if not {'--jobs', '-j', '--child-processes'}.intersection(set(args)):
        args.extend(['--jobs', 1])
    args.extend(extra_args)
    if not tests_included:
        # We use the glob to test that globbing works.
        args.extend(['passes',
                     'http/tests',
                     'websocket/tests',
                     'failures/expected/*'])
    return run_web_tests.parse_args(args)


def passing_run(extra_args=None, port_obj=None, tests_included=False, host=None, shared_port=True):
    options, parsed_args = parse_args(extra_args, tests_included)
    if not port_obj:
        host = host or MockHost()
        port_obj = host.port_factory.get(port_name=options.platform, options=options)

    if shared_port:
        port_obj.host.port_factory.get = lambda *args, **kwargs: port_obj

    printer = Printer(host, options, StringIO.StringIO())
    run_details = run_web_tests.run(port_obj, options, parsed_args, printer)
    return run_details.exit_code == 0


def logging_run(extra_args=None, port_obj=None, tests_included=False, host=None, shared_port=True):
    options, parsed_args = parse_args(extra_args=extra_args,
                                      tests_included=tests_included)
    host = host or MockHost()
    if not port_obj:
        port_obj = host.port_factory.get(port_name=options.platform, options=options)

    run_details, output = run_and_capture(port_obj, options, parsed_args, shared_port)
    return (run_details, output, host.user)


def run_and_capture(port_obj, options, parsed_args, shared_port=True):
    if shared_port:
        port_obj.host.port_factory.get = lambda *args, **kwargs: port_obj
    logging_stream = StringIO.StringIO()
    printer = Printer(port_obj.host, options, logging_stream)
    run_details = run_web_tests.run(port_obj, options, parsed_args, printer)
    return (run_details, logging_stream)


def get_tests_run(args, host=None, port_obj=None):
    results = get_test_results(args, host=host, port_obj=port_obj)
    return [result.test_name for result in results]


def get_test_batches(args, host=None):
    results = get_test_results(args, host)
    batches = []
    batch = []
    current_pid = None
    for result in results:
        if batch and result.pid != current_pid:
            batches.append(batch)
            batch = []
        batch.append(result.test_name)
    if batch:
        batches.append(batch)
    return batches


def get_test_results(args, host=None, port_obj=None):
    options, parsed_args = parse_args(args, tests_included=True)

    host = host or MockHost()
    port_obj = port_obj or host.port_factory.get(port_name=options.platform, options=options)

    printer = Printer(host, options, StringIO.StringIO())
    run_details = run_web_tests.run(port_obj, options, parsed_args, printer)

    all_results = []
    if run_details.initial_results:
        all_results.extend(run_details.initial_results.all_results)

    for retry_results in run_details.all_retry_results:
        all_results.extend(retry_results.all_results)
    return all_results


def parse_full_results(full_results_text):
    json_to_eval = full_results_text.replace('ADD_RESULTS(', '').replace(');', '')
    compressed_results = json.loads(json_to_eval)
    return compressed_results


class StreamTestingMixin(object):

    def assert_contains(self, stream, string):
        self.assertIn(string, stream.getvalue())

    def assert_not_empty(self, stream):
        self.assertTrue(stream.getvalue())


class RunTest(unittest.TestCase, StreamTestingMixin):

    def setUp(self):
        # A real PlatformInfo object is used here instead of a
        # MockPlatformInfo because we need to actually check for
        # Windows and Mac to skip some tests.
        self._platform = SystemHost().platform

    def test_basic(self):
        options, args = parse_args(
            extra_args=['--json-failing-test-results', '/tmp/json_failing_test_results.json'],
            tests_included=True)
        logging_stream = StringIO.StringIO()
        host = MockHost()
        port_obj = host.port_factory.get(options.platform, options)
        printer = Printer(host, options, logging_stream)
        details = run_web_tests.run(port_obj, options, args, printer)

        # These numbers will need to be updated whenever we add new tests.
        self.assertEqual(details.initial_results.total, test.TOTAL_TESTS)
        self.assertEqual(details.initial_results.expected_skips, test.TOTAL_SKIPS)
        self.assertEqual(
            len(details.initial_results.unexpected_results_by_name),
            test.UNEXPECTED_PASSES + test.UNEXPECTED_FAILURES)
        self.assertEqual(details.exit_code, test.UNEXPECTED_FAILURES)
        self.assertEqual(details.all_retry_results[0].total, test.UNEXPECTED_FAILURES)

        expected_tests = (
            details.initial_results.total
            - details.initial_results.expected_skips
            - len(details.initial_results.unexpected_results_by_name))
        expected_summary_str = ''
        if details.initial_results.expected_failures > 0:
            expected_summary_str = " (%d passed, %d didn't)" % (
                expected_tests - details.initial_results.expected_failures,
                details.initial_results.expected_failures)
        one_line_summary = "%d tests ran as expected%s, %d didn't:\n" % (
            expected_tests,
            expected_summary_str,
            len(details.initial_results.unexpected_results_by_name))
        self.assertIn(one_line_summary, logging_stream.buflist)

        # Ensure the results were summarized properly.
        self.assertEqual(details.summarized_failing_results['num_regressions'], details.exit_code)

        # Ensure the results were written out and displayed.
        failing_results_text = host.filesystem.read_text_file('/tmp/layout-test-results/failing_results.json')
        json_to_eval = failing_results_text.replace('ADD_RESULTS(', '').replace(');', '')
        self.assertEqual(json.loads(json_to_eval), details.summarized_failing_results)

        full_results_text = host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json')
        self.assertEqual(json.loads(full_results_text), details.summarized_full_results)

        self.assertEqual(host.user.opened_urls, [abspath_to_uri(MockHost().platform, '/tmp/layout-test-results/results.html')])

    def test_max_locked_shards(self):
        # Tests for the default of using one locked shard even in the case of more than one child process.
        _, regular_output, _ = logging_run(['--debug-rwt-logging', '--jobs', '2'], shared_port=False)
        self.assertTrue(any('1 locked' in line for line in regular_output.buflist))

    def test_child_processes_2(self):
        _, regular_output, _ = logging_run(
            ['--debug-rwt-logging', '--jobs', '2'], shared_port=False)
        self.assertTrue(any(['Running 2 ' in line for line in regular_output.buflist]))

    def test_child_processes_min(self):
        _, regular_output, _ = logging_run(
            ['--debug-rwt-logging', '--jobs', '2', '-i', 'passes/virtual_passes', 'passes'],
            tests_included=True, shared_port=False)
        self.assertTrue(any(['Running 1 ' in line for line in regular_output.buflist]))

    def test_dryrun(self):
        tests_run = get_tests_run(['--dry-run'])
        self.assertEqual(tests_run, [])

        tests_run = get_tests_run(['-n'])
        self.assertEqual(tests_run, [])

    def test_enable_sanitizer(self):
        self.assertTrue(passing_run(['--enable-sanitizer', '--order', 'natural', 'failures/expected/text.html']))

    def test_exception_raised(self):
        # Exceptions raised by a worker are treated differently depending on
        # whether they are in-process or out. inline exceptions work as normal,
        # which allows us to get the full stack trace and traceback from the
        # worker. The downside to this is that it could be any error, but this
        # is actually useful in testing.
        #
        # Exceptions raised in a separate process are re-packaged into
        # WorkerExceptions (a subclass of BaseException), which have a string capture of the stack which can
        # be printed, but don't display properly in the unit test exception handlers.
        with self.assertRaises(BaseException):
            logging_run(['failures/expected/exception.html', '--jobs', '1'], tests_included=True)

        with self.assertRaises(BaseException):
            logging_run(
                ['--jobs', '2', '--skipped=ignore', 'failures/expected/exception.html', 'passes/text.html'],
                tests_included=True,
                shared_port=False)

    def test_device_failure(self):
        # Test that we handle a device going offline during a test properly.
        details, regular_output, _ = logging_run(['failures/expected/device_failure.html'], tests_included=True)
        self.assertEqual(details.exit_code, 0)
        self.assertTrue('worker/0 has failed' in regular_output.getvalue())

    def test_keyboard_interrupt(self):
        # Note that this also tests running a test marked as SKIP if
        # you specify it explicitly.
        details, _, _ = logging_run(['failures/expected/keyboard.html', '--jobs', '1'], tests_included=True)
        self.assertEqual(details.exit_code, exit_codes.INTERRUPTED_EXIT_STATUS)

        _, regular_output, _ = logging_run(
            ['failures/expected/keyboard.html', 'passes/text.html', '--jobs', '2', '--skipped=ignore'],
            tests_included=True, shared_port=False)
        self.assertTrue(any(['Interrupted, exiting' in line for line in regular_output.buflist]))

    def test_no_tests_found(self):
        details, err, _ = logging_run(['resources'], tests_included=True)
        self.assertEqual(details.exit_code, exit_codes.NO_TESTS_EXIT_STATUS)
        self.assert_contains(err, 'No tests to run.\n')

    def test_no_tests_found_2(self):
        details, err, _ = logging_run(['foo'], tests_included=True)
        self.assertEqual(details.exit_code, exit_codes.NO_TESTS_EXIT_STATUS)
        self.assert_contains(err, 'No tests to run.\n')

    def test_no_tests_found_3(self):
        details, err, _ = logging_run(['--shard-index', '4', '--total-shards', '400', 'foo/bar.html'], tests_included=True)
        self.assertEqual(details.exit_code, exit_codes.NO_TESTS_EXIT_STATUS)
        self.assert_contains(err, 'No tests to run.\n')

    def test_no_tests_found_with_ok_flag(self):
        details, err, _ = logging_run(
            ['resources', '--zero-tests-executed-ok'], tests_included=True)
        self.assertEqual(details.exit_code, exit_codes.OK_EXIT_STATUS)
        self.assert_contains(err, 'No tests to run.\n')

    def test_no_tests_found_with_ok_flag_shards(self):
        details, err, _ = logging_run(
            ['--shard-index', '4', '--total-shards', '40', 'foo/bar.html', '--zero-tests-executed-ok'], tests_included=True)
        self.assertEqual(details.exit_code, exit_codes.OK_EXIT_STATUS)
        self.assert_contains(err, 'No tests to run.\n')

    def test_natural_order(self):
        tests_to_run = [
            'passes/audio.html',
            'failures/expected/text.html',
            'failures/unexpected/missing_text.html',
            'passes/args.html'
        ]
        tests_run = get_tests_run(['--order=natural'] + tests_to_run)
        self.assertEqual([
            'failures/expected/text.html',
            'failures/unexpected/missing_text.html',
            'passes/args.html',
            'passes/audio.html'
        ], tests_run)

    def test_natural_order_test_specified_multiple_times(self):
        tests_to_run = ['passes/args.html', 'passes/audio.html', 'passes/audio.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=natural'] + tests_to_run)
        self.assertEqual(['passes/args.html', 'passes/args.html', 'passes/audio.html', 'passes/audio.html'], tests_run)

    def test_random_order(self):
        tests_to_run = [
            'passes/audio.html',
            'failures/expected/text.html',
            'failures/unexpected/missing_text.html',
            'passes/args.html'
        ]
        tests_run = get_tests_run(['--order=random'] + tests_to_run)
        self.assertEqual(sorted(tests_to_run), sorted(tests_run))

    def test_random_order_with_seed(self):
        tests_to_run = [
            'failures/expected/text.html',
            'failures/unexpected/missing_text.html',
            'passes/args.html',
            'passes/audio.html',
        ]
        tests_run = get_tests_run(['--order=random', '--seed=5'] + sorted(tests_to_run))
        expected_order = [
            'failures/expected/text.html',
            'failures/unexpected/missing_text.html',
            'passes/audio.html',
            'passes/args.html',
        ]

        self.assertEqual(tests_run, expected_order)

    def test_random_order_with_timestamp_seed(self):
        tests_to_run = sorted([
            'failures/unexpected/missing_text.html',
            'failures/expected/text.html',
            'passes/args.html',
            'passes/audio.html',
        ])

        run_1 = get_tests_run(['--order=random'] + tests_to_run, host=MockHost(time_return_val=10))
        run_2 = get_tests_run(['--order=random'] + tests_to_run, host=MockHost(time_return_val=10))
        self.assertEqual(run_1, run_2)

        run_3 = get_tests_run(['--order=random'] + tests_to_run, host=MockHost(time_return_val=20))
        self.assertNotEqual(run_1, run_3)

    def test_random_order_test_specified_multiple_times(self):
        tests_to_run = ['passes/args.html', 'passes/audio.html', 'passes/audio.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=random'] + tests_to_run)
        self.assertEqual(tests_run.count('passes/audio.html'), 2)
        self.assertEqual(tests_run.count('passes/args.html'), 2)

    def test_no_order(self):
        tests_to_run = [
            'passes/audio.html',
            'failures/expected/text.html',
            'failures/unexpected/missing_text.html',
            'passes/args.html'
        ]
        tests_run = get_tests_run(['--order=none'] + tests_to_run)
        self.assertEqual(tests_to_run, tests_run)

    def test_no_order_test_specified_multiple_times(self):
        tests_to_run = ['passes/args.html', 'passes/audio.html', 'passes/audio.html', 'passes/args.html']
        tests_run = get_tests_run(['--order=none'] + tests_to_run)
        self.assertEqual(tests_to_run, tests_run)

    def test_no_order_with_directory_entries_in_natural_order(self):
        tests_to_run = ['http/tests/ssl', 'perf/foo', 'http/tests/passes']
        tests_run = get_tests_run(['--order=none'] + tests_to_run)
        self.assertEqual(tests_run, ['http/tests/ssl/text.html', 'perf/foo/test.html',
                                     'http/tests/passes/image.html', 'http/tests/passes/text.html'])

    def test_repeat_each(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--repeat-each', '2', '--order', 'natural'] + tests_to_run)
        self.assertEqual(tests_run, ['passes/image.html', 'passes/image.html', 'passes/text.html', 'passes/text.html'])

    def test_gtest_repeat(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--gtest_repeat', '2', '--order', 'natural'] + tests_to_run)
        self.assertEqual(tests_run, ['passes/image.html', 'passes/text.html', 'passes/image.html', 'passes/text.html'])

    def test_gtest_repeat_overrides_iterations(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--iterations', '4', '--gtest_repeat', '2', '--order', 'natural'] + tests_to_run)
        self.assertEqual(tests_run, ['passes/image.html', 'passes/text.html', 'passes/image.html', 'passes/text.html'])

    def test_ignore_flag(self):
        # Note that passes/image.html is expected to be run since we specified it directly.
        tests_run = get_tests_run(['-i', 'passes', 'passes/image.html'])
        self.assertNotIn('passes/text.html', tests_run)
        self.assertIn('passes/image.html', tests_run)

    def test_skipped_flag(self):
        tests_run = get_tests_run(['passes'])
        self.assertNotIn('passes/skipped/skip.html', tests_run)
        num_tests_run_by_default = len(tests_run)

        # Check that nothing changes when we specify skipped=default.
        self.assertEqual(len(get_tests_run(['--skipped=default', 'passes'])),
                         num_tests_run_by_default)

        # Now check that we run one more test (the skipped one).
        tests_run = get_tests_run(['--skipped=ignore', 'passes'])
        self.assertIn('passes/skipped/skip.html', tests_run)
        self.assertEqual(len(tests_run), num_tests_run_by_default + 1)

        # Now check that we only run the skipped test.
        self.assertEqual(get_tests_run(['--skipped=only', 'passes']), ['passes/skipped/skip.html'])

        # Now check that we don't run anything.
        self.assertEqual(get_tests_run(['--skipped=always', 'passes/skipped/skip.html']), [])

    def test_isolated_script_test_also_run_disabled_tests(self):
        self.assertEqual(
            sorted(get_tests_run(['--isolated-script-test-also-run-disabled-tests', 'passes'])),
            sorted(get_tests_run(['--skipped=ignore', 'passes']))
        )

    def test_gtest_also_run_disabled_tests(self):
        self.assertEqual(
            sorted(get_tests_run(['--gtest_also_run_disabled_tests', 'passes'])),
            sorted(get_tests_run(['--skipped=ignore', 'passes']))
        )

    def test_iterations(self):
        tests_to_run = ['passes/image.html', 'passes/text.html']
        tests_run = get_tests_run(['--iterations', '2', '--order', 'natural'] + tests_to_run)
        self.assertEqual(tests_run, ['passes/image.html', 'passes/text.html', 'passes/image.html', 'passes/text.html'])

    def test_repeat_each_iterations_num_tests(self):
        # The total number of tests should be: number_of_tests *
        # repeat_each * iterations
        host = MockHost()
        _, err, _ = logging_run(
            ['--iterations', '2', '--repeat-each', '4', '--debug-rwt-logging', 'passes/text.html', 'failures/expected/text.html'],
            tests_included=True, host=host)
        self.assert_contains(err, "All 16 tests ran as expected (8 passed, 8 didn't).\n")

    def test_skip_failing_tests(self):
        # This tests that we skip both known failing and known flaky tests. Because there are
        # no known flaky tests in the default test_expectations, we add additional expectations.
        host = MockHost()
        host.filesystem.write_text_file('/tmp/overrides.txt', 'Bug(x) passes/image.html [ Failure Pass ]\n')

        batches = get_test_batches(['--skip-failing-tests', '--additional-expectations', '/tmp/overrides.txt'], host=host)
        has_passes_text = False
        for batch in batches:
            self.assertNotIn('failures/expected/text.html', batch)
            self.assertNotIn('passes/image.html', batch)
            has_passes_text = has_passes_text or ('passes/text.html' in batch)
        self.assertTrue(has_passes_text)

    def test_single_file(self):
        tests_run = get_tests_run(['passes/text.html'])
        self.assertEqual(tests_run, ['passes/text.html'])

    def test_single_file_with_prefix(self):
        tests_run = get_tests_run([WEB_TESTS_LAST_COMPONENT + '/passes/text.html'])
        self.assertEqual(['passes/text.html'], tests_run)

    def test_stderr_is_saved(self):
        host = MockHost()
        self.assertTrue(passing_run(host=host))
        self.assertEqual(
            host.filesystem.read_text_file('/tmp/layout-test-results/passes/error-stderr.txt'),
            'stuff going to stderr')
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['passes']['error.html']
        self.assertEqual(test_results['artifacts']['stderr'], ['passes/error-stderr.txt'])

    def test_crash_log_is_saved(self):
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/crash.html', '--num-retries', '1'],
            tests_included=True, host=host))
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        self.assertEqual(
            host.filesystem.read_text_file(
                '/tmp/layout-test-results/failures/unexpected/crash-crash-log.txt'),
                'crash log')
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['crash.html']
        self.assertEqual(test_results['artifacts']['crash_log'], [
            'failures/unexpected/crash-crash-log.txt',
            'retry_1/failures/unexpected/crash-crash-log.txt'])

    def test_crash_log_is_saved_after_delay(self):
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/crash-with-delayed-log.html',
            '--num-retries', '1'],
            tests_included=True, host=host))
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        self.assertEqual(
            host.filesystem.read_text_file(
                '/tmp/layout-test-results/failures/unexpected/crash-with-delayed-log-crash-log.txt'),
                'delayed crash log')
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['crash-with-delayed-log.html']
        self.assertEqual(test_results['artifacts']['crash_log'], [
            'failures/unexpected/crash-with-delayed-log-crash-log.txt',
            'retry_1/failures/unexpected/crash-with-delayed-log-crash-log.txt'])

    def test_reftest_mismatch_with_text_mismatch_only_writes_stderr_once(self):
        # test that there is no exception when two failure types, FailureTextMismatch and
        # FailureReftestMismatch both have the same stderr to print out.
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/reftest-mismatch-with-text-mismatch-with-stderr.html',],
            tests_included=True, host=host))

    @unittest.skip('Need to make subprocesses use mock filesystem')
    def test_crash_log_is_saved_after_delay_using_multiple_jobs(self):
        # TODO(rmhasan): When web_test_runner.run() spawns multiple jobs it uses
        # the non mock file system. We should figure out how to make all subprocesses
        # use the mock file system.
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/crash-with-delayed-log.html',
            'passes/args.html', '-j', '2'],
            tests_included=True, host=host))
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['crash-with-delayed-log.html']
        self.assertEqual(test_results['artifacts']['crash_log'], [
            'failures/unexpected/crash-with-delayed-log-crash-log.txt'])

    def test_crash_sample_file_is_saved(self):
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/crash-with-sample.html',
             '--num-retries', '1'],
            tests_included=True, host=host))
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        self.assertEqual(
            host.filesystem.read_text_file(
                '/tmp/layout-test-results/failures/unexpected/crash-with-sample-sample.txt'),
                'crash sample file')
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['crash-with-sample.html']
        self.assertEqual(test_results['artifacts']['sample_file'], [
            'failures/unexpected/crash-with-sample-sample.txt',
            'retry_1/failures/unexpected/crash-with-sample-sample.txt'])

    @unittest.skip('Need to make subprocesses use mock filesystem')
    def test_crash_sample_file_is_saved_multiple_jobs(self):
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/crash-with-sample.html',
             'passes/image.html', '--num-retries', '1', '-j', '2'],
            tests_included=True, host=host))
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['crash-with-sample.html']
        self.assertEqual(test_results['artifacts']['sample_file'], [
            'failures/unexpected/crash-with-sample-sample.txt',
            'retry_1/failures/unexpected/crash-with-sample-sample.txt'])

    def test_reftest_crash_log_is_saved(self):
        host = MockHost()
        self.assertTrue(logging_run(
            ['--order', 'natural', 'failures/unexpected/crash-reftest.html', '--num-retries', '1'],
            tests_included=True, host=host))
        self.assertEqual(
            host.filesystem.read_text_file(
                '/tmp/layout-test-results/failures/unexpected/crash-reftest-crash-log.txt'),
            'reftest crash log')
        self.assertEqual(
            host.filesystem.read_text_file(
                '/tmp/layout-test-results/retry_1/failures/unexpected/crash-reftest-crash-log.txt'),
            'reftest crash log')
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['crash-reftest.html']
        self.assertEqual(test_results['artifacts']['crash_log'], [
            'failures/unexpected/crash-reftest-crash-log.txt',
            'retry_1/failures/unexpected/crash-reftest-crash-log.txt'])

    def test_test_list(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], host=host)
        self.assertEqual(['passes/text.html'], tests_run)
        host.filesystem.remove(filename)
        details, err, _ = logging_run(['--test-list=%s' % filename], tests_included=True, host=host)
        self.assertEqual(details.exit_code, exit_codes.NO_TESTS_EXIT_STATUS)
        self.assert_not_empty(err)

    def test_test_list_with_prefix(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, WEB_TESTS_LAST_COMPONENT + '/passes/text.html')
        tests_run = get_tests_run(['--test-list=%s' % filename], host=host)
        self.assertEqual(['passes/text.html'], tests_run)

    def test_isolated_script_test_filter(self):
        host = MockHost()
        tests_run = get_tests_run(
            ['--isolated-script-test-filter=passes/text.html::passes/image.html', 'passes/error.html'],
            host=host
        )
        self.assertEqual(sorted(tests_run), [])

        tests_run = get_tests_run(
            ['--isolated-script-test-filter=passes/error.html::passes/image.html', 'passes/error.html'],
            host=host
        )
        self.assertEqual(sorted(tests_run), ['passes/error.html'])

        tests_run = get_tests_run(
            ['--isolated-script-test-filter=-passes/error.html::passes/image.html'],
            host=host
        )
        self.assertEqual(sorted(tests_run), ['passes/image.html'])

        tests_run = get_tests_run(
            ['--isolated-script-test-filter=passes/error.html::passes/image.html',
             '--isolated-script-test-filter=-passes/error.html'],
            host=host
        )
        self.assertEqual(sorted(tests_run), ['passes/image.html'])

    def test_gtest_filter(self):
        host = MockHost()
        tests_run = get_tests_run(['--gtest_filter=passes/text.html:passes/image.html', 'passes/error.html'], host=host)
        self.assertEqual(sorted(tests_run), ['passes/error.html', 'passes/image.html', 'passes/text.html'])

    def test_sharding_even(self):
        # Test that we actually select the right part
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']

        with mock.patch('__builtin__.hash', len):
            # Shard 0 of 2
            tests_run = get_tests_run(['--shard-index', '0', '--total-shards', '2', '--order', 'natural'] + tests_to_run)
            self.assertEqual(tests_run, ['passes/platform_image.html', 'passes/text.html'])
            # Shard 1 of 2
            tests_run = get_tests_run(['--shard-index', '1', '--total-shards', '2', '--order', 'natural'] + tests_to_run)
            self.assertEqual(tests_run, ['passes/error.html', 'passes/image.html'])

    def test_sharding_uneven(self):
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html',
                        'perf/foo/test.html']

        with mock.patch('__builtin__.hash', len):
            # Shard 0 of 3
            tests_run = get_tests_run(['--shard-index', '0', '--total-shards', '3', '--order', 'natural'] + tests_to_run)
            self.assertEqual(tests_run, ['perf/foo/test.html'])
            # Shard 1 of 3
            tests_run = get_tests_run(['--shard-index', '1', '--total-shards', '3', '--order', 'natural'] + tests_to_run)
            self.assertEqual(tests_run, ['passes/text.html'])
            # Shard 2 of 3
            tests_run = get_tests_run(['--shard-index', '2', '--total-shards', '3', '--order', 'natural'] + tests_to_run)
            self.assertEqual(tests_run, ['passes/error.html', 'passes/image.html', 'passes/platform_image.html'])

    def test_sharding_incorrect_arguments(self):
        with self.assertRaises(ValueError):
            get_tests_run(['--shard-index', '3'])
        with self.assertRaises(ValueError):
            get_tests_run(['--total-shards', '3'])
        with self.assertRaises(ValueError):
            get_tests_run(['--shard-index', '3', '--total-shards', '3'])

    def test_sharding_environ(self):
        tests_to_run = ['passes/error.html', 'passes/image.html', 'passes/platform_image.html', 'passes/text.html']
        host = MockHost()

        with mock.patch('__builtin__.hash', len):
            host.environ['GTEST_SHARD_INDEX'] = '0'
            host.environ['GTEST_TOTAL_SHARDS'] = '2'
            shard_0_tests_run = get_tests_run(['--order', 'natural'] + tests_to_run, host=host)
            self.assertEqual(shard_0_tests_run, ['passes/platform_image.html', 'passes/text.html'])

            host.environ['GTEST_SHARD_INDEX'] = '1'
            host.environ['GTEST_TOTAL_SHARDS'] = '2'
            shard_1_tests_run = get_tests_run(['--order', 'natural'] + tests_to_run, host=host)
            self.assertEqual(shard_1_tests_run, ['passes/error.html', 'passes/image.html'])

    def test_smoke_test(self):
        host = MockHost()
        smoke_test_filename = test.WEB_TEST_DIR + '/SmokeTests'
        host.filesystem.write_text_file(smoke_test_filename, 'passes/text.html\n')

        # Test the default smoke testing.
        tests_run = get_tests_run(['--smoke', '--order', 'natural'], host=host)
        self.assertEqual(['passes/text.html'], tests_run)

        # Test running the smoke tests plus some manually-specified tests.
        tests_run = get_tests_run(['--smoke', 'passes/image.html', '--order', 'natural'], host=host)
        self.assertEqual(['passes/image.html', 'passes/text.html'], tests_run)

        # Test running the smoke tests plus some manually-specified tests.
        tests_run = get_tests_run(['--no-smoke', 'passes/image.html', '--order', 'natural'], host=host)
        self.assertEqual(['passes/image.html'], tests_run)

        # Test that we don't run just the smoke tests by default on a normal test port.
        tests_run = get_tests_run(['--order', 'natural'], host=host)
        self.assertNotEqual(['passes/text.html'], tests_run)

        # Create a port that does run only the smoke tests by default, and verify that works as expected.
        port_obj = host.port_factory.get('test')
        port_obj.default_smoke_test_only = lambda: True
        tests_run = get_tests_run(['--order', 'natural'], host=host, port_obj=port_obj)
        self.assertEqual(['passes/text.html'], tests_run)

        # Verify that --no-smoke continues to work on a smoke-by-default port.
        tests_run = get_tests_run(['--no-smoke', 'passes/image.html', '--order', 'natural'],
                                  host=host, port_obj=port_obj)
        self.assertNotIn('passes/text.html', tests_run)

    def test_smoke_test_default_retry(self):
        host = MockHost()
        smoke_test_filename = test.WEB_TEST_DIR + '/SmokeTests'
        host.filesystem.write_text_file(
            smoke_test_filename, 'failures/unexpected/text-image-checksum.html\n')

        # Retry if additional tests are given.
        _, err, __ = logging_run(['--smoke', 'passes/image.html'], host=host, tests_included=True)
        self.assertIn('Retrying', err.getvalue())

    def test_missing_and_unexpected_results(self):
        # Test that we update expectations in place. If the expectation
        # is missing, update the expected generic location.
        host = MockHost()
        details, _, _ = logging_run(['--no-show-results',
                                     'failures/unexpected/missing_text.html',
                                     'failures/unexpected/text-image-checksum.html'],
                                    tests_included=True, host=host)
        self.assertEqual(details.exit_code, 2)
        results = json.loads(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        results['tests']['failures']['unexpected']['text-image-checksum.html'].pop('artifacts')
        self.assertEqual(
            results['tests']['failures']['unexpected']['text-image-checksum.html'],
            {
                'expected': 'PASS',
                'actual': 'FAIL',
                'is_unexpected': True,
                'is_regression': True,
                'text_mismatch': 'general text mismatch',
            })
        results['tests']['failures']['unexpected']['missing_text.html'].pop('artifacts')
        self.assertEqual(
            results['tests']['failures']['unexpected']['missing_text.html'],
            {
                'expected': 'PASS',
                'actual': 'FAIL',
                'is_unexpected': True,
                'is_regression': True,
                'is_missing_text': True,
            })
        self.assertEqual(results['num_regressions'], 2)
        self.assertEqual(results['num_flaky'], 0)

    def test_different_failure_on_retry(self):
        # This tests that if a test fails two different ways -- both unexpected
        # -- we treat it as a failure rather than a flaky result.  We use the
        # initial failure for simplicity and consistency w/ the flakiness
        # dashboard, even if the second failure is worse.

        details, _, _ = logging_run(['--num-retries=3', 'failures/unexpected/text_then_crash.html'], tests_included=True)
        self.assertEqual(details.exit_code, 1)
        self.assertEqual(details.summarized_failing_results['tests']['failures']['unexpected']['text_then_crash.html']['actual'],
                         'FAIL CRASH CRASH CRASH')

        # If we get a test that fails two different ways -- but the second one is expected --
        # we should treat it as a flaky result and report the initial unexpected failure type
        # to the dashboard. However, the test should be considered passing.
        details, _, _ = logging_run(['--num-retries=3', 'failures/expected/crash_then_text.html'], tests_included=True)
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(details.summarized_failing_results['tests']['failures']['expected']['crash_then_text.html']['actual'],
                         'CRASH FAIL')

    def test_watch(self):
        host = MockHost()
        host.user.set_canned_responses(['r', 'r', 'q'])
        _, output, _ = logging_run(['--watch', 'failures/unexpected/text.html'], tests_included=True, host=host)
        output_string = output.getvalue()
        self.assertIn(
            'Link to pretty diff:\nfile:///tmp/layout-test-results/failures/unexpected/text-pretty-diff.html', output_string)
        self.assertEqual(output_string.count('[1/1] failures/unexpected/text.html failed unexpectedly (text diff)'), 3)

    def test_crash_with_stderr(self):
        host = MockHost()
        logging_run(['failures/unexpected/crash-with-stderr.html'], tests_included=True, host=host)
        full_results = json.loads(host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        self.assertEqual(full_results['tests']['failures']['unexpected']['crash-with-stderr.html']['has_stderr'], True)

    def test_no_image_failure_with_image_diff(self):
        host = MockHost()
        logging_run(
            ['failures/unexpected/checksum-with-matching-image.html'], tests_included=True, host=host)
        self.assertTrue(host.filesystem.read_text_file(
            '/tmp/layout-test-results/full_results.json').find('"num_regressions":0') != -1)

    def test_exit_after_n_failures_upload(self):
        host = MockHost()
        details, regular_output, _ = logging_run(
            ['failures/unexpected/text-image-checksum.html', 'passes/text.html',
             '--exit-after-n-failures', '1', '--order', 'natural'],
            tests_included=True, host=host)

        # By returning False, we know that the incremental results were generated and then deleted.
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/incremental_results.json'))

        self.assertEqual(details.exit_code, exit_codes.EARLY_EXIT_STATUS)

        # This checks that passes/text.html is considered Skip-ped.
        self.assertIn('"skipped":1', host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))

        # This checks that we told the user we bailed out.
        self.assertTrue('Exiting early after 1 failures. 1 tests run.\n' in regular_output.getvalue())

        # This checks that neither test ran as expected.
        # FIXME: This log message is confusing; tests that were skipped should be called out separately.
        self.assertTrue('0 tests ran as expected, 2 didn\'t:\n' in regular_output.getvalue())

    def test_exit_after_n_failures(self):
        # Unexpected failures should result in tests stopping.
        tests_run = get_tests_run(['failures/unexpected/text-image-checksum.html',
                                   'passes/text.html', '--exit-after-n-failures', '1',
                                   '--order', 'natural'])
        self.assertEqual(['failures/unexpected/text-image-checksum.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run(['failures/expected/text.html', 'passes/text.html',
                                   '--exit-after-n-failures', '1',
                                   '--order', 'natural'])
        self.assertEqual(['failures/expected/text.html', 'passes/text.html'], tests_run)

    def test_exit_after_n_crashes(self):
        # Unexpected crashes should result in tests stopping.
        tests_run = get_tests_run(['--order', 'natural', 'failures/unexpected/crash.html',
                                   'passes/text.html', '--exit-after-n-crashes-or-timeouts', '1'])
        self.assertEqual(['failures/unexpected/crash.html'], tests_run)

        # Same with timeouts.
        tests_run = get_tests_run(['failures/unexpected/timeout.html', 'passes/text.html',
                                   '--exit-after-n-crashes-or-timeouts', '1',
                                   '--order', 'natural'])
        self.assertEqual(['failures/unexpected/timeout.html'], tests_run)

        # But we'll keep going for expected ones.
        tests_run = get_tests_run(['failures/expected/crash.html', 'passes/text.html',
                                   '--exit-after-n-crashes-or-timeouts', '1',
                                   '--order', 'natural'])
        self.assertEqual(['failures/expected/crash.html', 'passes/text.html'], tests_run)

    def test_results_directory_absolute(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        host = MockHost()
        with host.filesystem.mkdtemp() as tmpdir:
            _, _, user = logging_run(['--results-directory=' + str(tmpdir), '--order', 'natural'],
                                     tests_included=True, host=host)
            self.assertEqual(user.opened_urls, [abspath_to_uri(host.platform, host.filesystem.join(tmpdir, 'results.html'))])

    def test_results_directory_default(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.

        # This is the default location.
        _, _, user = logging_run(tests_included=True)
        self.assertEqual(user.opened_urls, [abspath_to_uri(MockHost().platform, '/tmp/layout-test-results/results.html')])

    def test_results_directory_relative(self):
        # We run a configuration that should fail, to generate output, then
        # look for what the output results url was.
        host = MockHost()
        host.filesystem.maybe_make_directory('/tmp/cwd')
        host.filesystem.chdir('/tmp/cwd')
        _, _, user = logging_run(['--results-directory=foo'], tests_included=True, host=host)
        self.assertEqual(user.opened_urls, [abspath_to_uri(host.platform, '/tmp/cwd/foo/results.html')])

    def test_retrying_default_value(self):
        # Do not retry when the test list is explicit.
        host = MockHost()
        details, err, _ = logging_run(['failures/unexpected/text-image-checksum.html'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertNotIn('Retrying', err.getvalue())

        # Retry 3 times by default when the test list is not explicit.
        host = MockHost()
        details, err, _ = logging_run(['failures/unexpected'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, test.UNEXPECTED_NON_VIRTUAL_FAILURES)
        self.assertIn('Retrying', err.getvalue())
        self.assertTrue(
            host.filesystem.exists('/tmp/layout-test-results/retry_1/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(
            host.filesystem.exists('/tmp/layout-test-results/retry_2/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(
            host.filesystem.exists('/tmp/layout-test-results/retry_3/failures/unexpected/text-image-checksum-actual.txt'))

    def test_retrying_default_value_test_list(self):
        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'failures/unexpected/text-image-checksum.html\nfailures/unexpected/crash.html')
        details, err, _ = logging_run(['--test-list=%s' % filename, '--order', 'natural'],
                                      tests_included=True, host=host)
        self.assertEqual(details.exit_code, 2)
        self.assertIn('Retrying', err.getvalue())

        host = MockHost()
        filename = '/tmp/foo.txt'
        host.filesystem.write_text_file(filename, 'failures')
        details, err, _ = logging_run(['--test-list=%s' % filename], tests_included=True, host=host)
        self.assertEqual(details.exit_code, test.UNEXPECTED_NON_VIRTUAL_FAILURES)
        self.assertIn('Retrying', err.getvalue())

    def test_retrying_and_flaky_tests(self):
        host = MockHost()
        details, err, _ = logging_run(['--num-retries=3', 'failures/flaky'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 0)
        self.assertIn('Retrying', err.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/flaky/text-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retry_1/failures/flaky/text-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retry_2/failures/flaky/text-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retry_3/failures/flaky/text-actual.txt'))

    def test_retrying_crashed_tests(self):
        host = MockHost()
        details, err, _ = logging_run(['--num-retries=3', 'failures/unexpected/crash.html'], tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertIn('Retrying', err.getvalue())

    def test_retrying_leak_tests(self):
        host = MockHost()
        details, err, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/leak.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertIn('Retrying', err.getvalue())
        self.assertEqual(host.filesystem.read_text_file(
            '/tmp/layout-test-results/failures/unexpected/leak-leak-log.txt'),
            'leak detected')
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['leak.html']
        self.assertEqual(test_results['artifacts']['leak_log'], [
            'failures/unexpected/leak-leak-log.txt',
            'retry_1/failures/unexpected/leak-leak-log.txt'])

    def test_unexpected_text_mismatch(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/text-mismatch-overlay.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['text-mismatch-overlay.html']
        self.assertEqual(test_results['artifacts']['actual_text'], [
            'failures/unexpected/text-mismatch-overlay-actual.txt',
            'retry_1/failures/unexpected/text-mismatch-overlay-actual.txt'])
        self.assertEqual(test_results['artifacts']['expected_text'], [
            'failures/unexpected/text-mismatch-overlay-expected.txt',
            'retry_1/failures/unexpected/text-mismatch-overlay-expected.txt'])
        self.assertEqual(test_results['artifacts']['text_diff'], [
            'failures/unexpected/text-mismatch-overlay-diff.txt',
            'retry_1/failures/unexpected/text-mismatch-overlay-diff.txt'])
        self.assertEqual(test_results['artifacts']['pretty_text_diff'], [
            'failures/unexpected/text-mismatch-overlay-pretty-diff.html',
            'retry_1/failures/unexpected/text-mismatch-overlay-pretty-diff.html'])
        self.assertEqual(test_results['artifacts']['overlay'], [
            'failures/unexpected/text-mismatch-overlay-overlay.html',
            'retry_1/failures/unexpected/text-mismatch-overlay-overlay.html'])

    def test_unexpected_no_text_baseline(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/no-text-baseline.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['no-text-baseline.html']
        self.assertEqual(test_results['artifacts']['actual_text'], [
            'failures/unexpected/no-text-baseline-actual.txt',
            'retry_1/failures/unexpected/no-text-baseline-actual.txt'])
        self.assertNotIn('expected_text', test_results['artifacts'])
        self.assertEqual(test_results['artifacts']['text_diff'], [
            'failures/unexpected/no-text-baseline-diff.txt',
            'retry_1/failures/unexpected/no-text-baseline-diff.txt'])
        self.assertEqual(test_results['artifacts']['pretty_text_diff'], [
            'failures/unexpected/no-text-baseline-pretty-diff.html',
            'retry_1/failures/unexpected/no-text-baseline-pretty-diff.html'])
        self.assertNotIn('overlay', test_results['artifacts'])

    def test_unexpected_no_text_generated(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/no-text-generated.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['no-text-generated.html']
        self.assertEqual(test_results['artifacts']['expected_text'], [
            'failures/unexpected/no-text-generated-expected.txt',
            'retry_1/failures/unexpected/no-text-generated-expected.txt'])
        self.assertNotIn('actual_text', test_results['artifacts'])
        self.assertEqual(test_results['artifacts']['text_diff'], [
            'failures/unexpected/no-text-generated-diff.txt',
            'retry_1/failures/unexpected/no-text-generated-diff.txt'])
        self.assertEqual(test_results['artifacts']['pretty_text_diff'], [
            'failures/unexpected/no-text-generated-pretty-diff.html',
            'retry_1/failures/unexpected/no-text-generated-pretty-diff.html'])
        self.assertNotIn('overlay', test_results['artifacts'])

    def test_reftest_mismatching_image(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/reftest.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['reftest.html']
        self.assertEqual(test_results['artifacts']['actual_image'], [
            'failures/unexpected/reftest-actual.png',
            'retry_1/failures/unexpected/reftest-actual.png'])
        self.assertEqual(test_results['artifacts']['expected_image'], [
            'failures/unexpected/reftest-expected.png',
            'retry_1/failures/unexpected/reftest-expected.png'])
        self.assertEqual(test_results['artifacts']['image_diff'], [
            'failures/unexpected/reftest-diff.png',
            'retry_1/failures/unexpected/reftest-diff.png'])
        self.assertEqual(test_results['artifacts']['pretty_image_diff'], [
            'failures/unexpected/reftest-diffs.html',
            'retry_1/failures/unexpected/reftest-diffs.html'])
        self.assertEqual(test_results['artifacts']['reference_file_mismatch'],  [
            'failures/unexpected/reftest-expected.html',
            'retry_1/failures/unexpected/reftest-expected.html'])

    def test_reftest_failure_matching_image(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['failures/unexpected/mismatch.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['mismatch.html']
        self.assertIn('reference_file_match', test_results['artifacts'])
        self.assertEqual(test_results['artifacts']['reference_file_match'],
                         ['failures/unexpected/mismatch-expected-mismatch.html'])

    def test_unexpected_image_mismatch(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/image-mismatch.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['image-mismatch.html']
        self.assertEqual(test_results['artifacts']['actual_image'], [
            'failures/unexpected/image-mismatch-actual.png',
            'retry_1/failures/unexpected/image-mismatch-actual.png'])
        self.assertEqual(test_results['artifacts']['expected_image'], [
            'failures/unexpected/image-mismatch-expected.png',
            'retry_1/failures/unexpected/image-mismatch-expected.png'])
        self.assertEqual(test_results['artifacts']['image_diff'], [
            'failures/unexpected/image-mismatch-diff.png',
            'retry_1/failures/unexpected/image-mismatch-diff.png'])
        self.assertEqual(test_results['artifacts']['pretty_image_diff'], [
            'failures/unexpected/image-mismatch-diffs.html',
            'retry_1/failures/unexpected/image-mismatch-diffs.html'])

    def test_unexpected_no_image_generated(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/no-image-generated.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['no-image-generated.html']
        self.assertNotIn('actual_image', test_results['artifacts'])
        self.assertEqual(test_results['artifacts']['expected_image'], [
            'failures/unexpected/no-image-generated-expected.png',
            'retry_1/failures/unexpected/no-image-generated-expected.png'])
        self.assertNotIn('image_diff', test_results['artifacts'])
        self.assertNotIn('pretty_image_diff', test_results['artifacts'])

    def test_unexpected_no_image_baseline(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/no-image-baseline.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['no-image-baseline.html']
        self.assertNotIn('expected_image', test_results['artifacts'])
        self.assertEqual(test_results['artifacts']['actual_image'], [
            'failures/unexpected/no-image-baseline-actual.png',
            'retry_1/failures/unexpected/no-image-baseline-actual.png'])
        self.assertNotIn('image_diff', test_results['artifacts'])
        self.assertNotIn('pretty_image_diff', test_results['artifacts'])

    def test_unexpected_audio_mismatch(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/audio-mismatch.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['audio-mismatch.html']
        self.assertEqual(test_results['artifacts']['actual_audio'], [
            'failures/unexpected/audio-mismatch-actual.wav',
            'retry_1/failures/unexpected/audio-mismatch-actual.wav'])
        self.assertEqual(test_results['artifacts']['expected_audio'], [
            'failures/unexpected/audio-mismatch-expected.wav',
            'retry_1/failures/unexpected/audio-mismatch-expected.wav'])

    def test_unexpected_audio_missing_baseline(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/no-audio-baseline.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['no-audio-baseline.html']
        self.assertEqual(test_results['artifacts']['actual_audio'], [
            'failures/unexpected/no-audio-baseline-actual.wav',
            'retry_1/failures/unexpected/no-audio-baseline-actual.wav'])

    def test_unexpected_no_audio_generated(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--num-retries=1', 'failures/unexpected/no-audio-generated.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        results = json.loads(
            host.filesystem.read_text_file('/tmp/layout-test-results/full_results.json'))
        test_results = results['tests']['failures']['unexpected']['no-audio-generated.html']
        self.assertEqual(test_results['artifacts']['expected_audio'], [
            'failures/unexpected/no-audio-generated-expected.wav',
            'retry_1/failures/unexpected/no-audio-generated-expected.wav'])

    def test_retrying_uses_retry_directories(self):
        host = MockHost()
        details, _, _ = logging_run(['--num-retries=3', 'failures/unexpected/text-image-checksum.html'],
                                    tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(
            host.filesystem.exists('/tmp/layout-test-results/retry_1/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(
            host.filesystem.exists('/tmp/layout-test-results/retry_2/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertTrue(
            host.filesystem.exists('/tmp/layout-test-results/retry_3/failures/unexpected/text-image-checksum-actual.txt'))

    def test_retrying_alias_flag(self):
        host = MockHost()
        _, err, __ = logging_run(['--test-launcher-retry-limit=3', 'failures/unexpected/crash.html'],
                                 tests_included=True, host=host)
        self.assertIn('Retrying', err.getvalue())

    def test_clobber_old_results(self):
        host = MockHost()
        details, _, _ = logging_run(['--num-retries=3', 'failures/unexpected/text-image-checksum.html'],
                                    tests_included=True, host=host)
        # See tests above for what files exist at this point.

        # Now we test that --clobber-old-results does remove the old retries.
        details, err, _ = logging_run(['--no-retry-failures', '--clobber-old-results',
                                       'failures/unexpected/text-image-checksum.html'],
                                      tests_included=True, host=host)
        self.assertEqual(details.exit_code, 1)
        self.assertTrue('Clobbering old results' in err.getvalue())
        self.assertIn('failures/unexpected/text-image-checksum.html', err.getvalue())
        self.assertTrue(host.filesystem.exists('/tmp/layout-test-results/failures/unexpected/text-image-checksum-actual.txt'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retry_1'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retry_2'))
        self.assertFalse(host.filesystem.exists('/tmp/layout-test-results/retry_3'))

    def test_run_order__inline(self):
        # These next tests test that we run the tests in ascending alphabetical
        # order per directory. HTTP tests are sharded separately from other tests,
        # so we have to test both.
        tests_run = get_tests_run(['--order', 'natural', '-i', 'passes/virtual_passes', 'passes'])
        self.assertEqual(tests_run, sorted(tests_run))

        tests_run = get_tests_run(['--order', 'natural', 'http/tests/passes'])
        self.assertEqual(tests_run, sorted(tests_run))

    def test_virtual(self):
        self.assertTrue(passing_run(['--order', 'natural', 'passes/text.html', 'passes/args.html',
                                     'virtual/passes/text.html', 'virtual/passes/args.html']))

    def test_virtual_warns_when_wildcard_used(self):
        virtual_test_warning_msg = ('WARNING: Wildcards in paths are not supported for '
                                    'virtual test suites.')

        run_details, err, _ = logging_run(['passes/args.html', 'virtual/passes/'],
                                          tests_included=True)
        self.assertEqual(len(run_details.summarized_full_results['tests']['passes'].keys()), 1)
        self.assertFalse(virtual_test_warning_msg in err.getvalue())

        run_details, err, _ = logging_run(['passes/args.html', 'virtual/passes/*'],
                                          tests_included=True)
        self.assertEqual(len(run_details.summarized_full_results['tests']['passes'].keys()), 1)
        self.assertTrue(virtual_test_warning_msg in err.getvalue())

    def test_reftest_run(self):
        tests_run = get_tests_run(['passes/reftest.html'])
        self.assertEqual(['passes/reftest.html'], tests_run)

    def test_reftest_expected_html_should_be_ignored(self):
        tests_run = get_tests_run(['passes/reftest-expected.html'])
        self.assertEqual([], tests_run)

    def test_reftest_driver_should_run_expected_html(self):
        tests_run = get_test_results(['passes/reftest.html'])
        self.assertEqual(tests_run[0].references, ['passes/reftest-expected.html'])

    def test_reftest_driver_should_run_expected_mismatch_html(self):
        tests_run = get_test_results(['passes/mismatch.html'])
        self.assertEqual(tests_run[0].references, ['passes/mismatch-expected-mismatch.html'])

    def test_reftest_crash(self):
        test_results = get_test_results(['failures/unexpected/crash-reftest.html'])
        # The list of references should be empty since the test crashed and we didn't run any references.
        self.assertEqual(test_results[0].references, [])

    def test_reftest_with_virtual_reference(self):
        _, err, _ = logging_run(['--details', 'virtual/virtual_passes/passes/reftest.html'], tests_included=True)
        self.assertTrue('ref: virtual/virtual_passes/passes/reftest-expected.html' in err.getvalue())
        self.assertTrue(re.search(r'args: --virtual-arg\s*ref:', err.getvalue()))

    def test_reftest_matching_text_expectation(self):
        test_name = 'passes/reftest-with-text.html'
        host = MockHost()
        run_details, _, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 0)

    def test_reftest_mismatching_text_expectation(self):
        test_name = 'failures/unexpected/reftest-with-mismatching-text.html'
        host = MockHost()
        run_details, _, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 1)
        self.assertEqual(test_result.type, test_expectations.FAIL)

    def test_reftest_mismatching_pixel_matching_text(self):
        test_name = 'failures/unexpected/reftest-with-matching-text.html'
        host = MockHost()
        run_details, _, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 1)
        self.assertEqual(test_result.type, test_expectations.FAIL)

    def test_reftest_mismatching_both_text_and_pixel(self):
        test_name = 'failures/unexpected/reftest.html'
        host = MockHost()
        host.filesystem.write_text_file(test.WEB_TEST_DIR + '/failures/unexpected/reftest-expected.txt', 'mismatch')
        run_details, _, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 2)
        self.assertEqual(test_result.type, test_expectations.FAIL)

    def test_extra_baselines(self):
        host = MockHost()
        extra_txt = test.WEB_TEST_DIR + '/passes/image-expected.txt'
        host.filesystem.write_text_file(extra_txt, 'Extra txt')
        extra_wav = test.WEB_TEST_DIR + '/passes/image-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        test_name = 'passes/image.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 2)
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureTextNotGenerated, test_result.failures))
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureAudioNotGenerated, test_result.failures))
        self.assert_contains(log_stream, 'Please remove %s' % extra_txt)
        self.assert_contains(log_stream, 'Please remove %s' % extra_wav)

    def test_empty_overriding_baselines(self):
        host = MockHost()
        base_baseline = test.WEB_TEST_DIR + '/passes/image-expected.txt'
        host.filesystem.write_text_file(base_baseline, 'Non-empty')
        platform_baseline = test.WEB_TEST_DIR + '/platform/test-mac-mac10.10/passes/image-expected.txt'
        host.filesystem.write_text_file(platform_baseline, '')
        test_name = 'passes/image.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 0)
        self.assertNotIn('Please remove', log_stream)

    def test_reftest_extra_baselines(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/reftest-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        extra_wav = test.WEB_TEST_DIR + '/passes/reftest-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        test_name = 'passes/reftest.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 1)
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureAudioNotGenerated, test_result.failures))
        # For now extra png baseline is only reported in an error message.
        self.assert_contains(log_stream, 'Please remove %s' % extra_png)
        self.assert_contains(log_stream, 'Please remove %s' % extra_wav)

    def test_reftest_with_text_extra_baselines(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/reftest-with-text-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        extra_wav = test.WEB_TEST_DIR + '/passes/reftest-with-text-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        test_name = 'passes/reftest-with-text.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 1)
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureAudioNotGenerated, test_result.failures))
        # For now extra png baseline is only reported in an error message.
        self.assert_contains(log_stream, 'Please remove %s' % extra_png)
        self.assert_contains(log_stream, 'Please remove %s' % extra_wav)

    def test_reftest_extra_png_baseline(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/reftest-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        test_name = 'passes/reftest.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertEqual(run_details.exit_code, 0)
        # For now extra png baseline is only reported in an error message.
        self.assert_contains(log_stream, 'Please remove %s' % extra_png)

    def test_passing_testharness_extra_baselines(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/testharness-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        extra_txt = test.WEB_TEST_DIR + '/passes/testharness-expected.txt'
        host.filesystem.write_text_file(
            extra_txt,
            'This is a testharness.js-based test.\nPASS: bah\nHarness: the test ran to completion.')
        extra_wav = test.WEB_TEST_DIR + '/passes/testharness-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        test_name = 'passes/testharness.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 2)
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureImageHashNotGenerated, test_result.failures))
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureAudioNotGenerated, test_result.failures))
        # For now extra txt baseline for all-pass testharness test is only reported in an error message.
        self.assert_contains(log_stream, 'Please remove %s' % extra_png)
        self.assert_contains(log_stream, 'Please remove %s' % extra_txt)
        self.assert_contains(log_stream, 'Please remove %s' % extra_wav)

    def test_passing_testharness_extra_txt_baseline(self):
        host = MockHost()
        extra_txt = test.WEB_TEST_DIR + '/passes/testharness-expected.txt'
        host.filesystem.write_text_file(
            extra_txt,
            'This is a testharness.js-based test.\nPASS: bah\nHarness: the test ran to completion.')
        test_name = 'passes/testharness.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertEqual(run_details.exit_code, 0)
        # For now extra txt baseline for all-pass testharness test is only reported in an error message.
        self.assert_contains(log_stream, 'Please remove %s' % extra_txt)

    def test_passing_testharness_extra_mismatching_txt_baseline(self):
        host = MockHost()
        extra_txt = test.WEB_TEST_DIR + '/passes/testharness-expected.txt'
        host.filesystem.write_text_file(
            extra_txt,
            'This is a testharness.js-based test.\nFAIL: bah\nHarness: the test ran to completion.')
        test_name = 'passes/testharness.html'
        run_details, log_stream, _ = logging_run([test_name], tests_included=True, host=host)
        self.assertNotEqual(run_details.exit_code, 0)
        self.assertEqual(run_details.initial_results.total, 1)
        test_result = run_details.initial_results.all_results[0]
        self.assertEqual(test_result.test_name, test_name)
        self.assertEqual(len(test_result.failures), 1)
        self.assertTrue(test_failures.has_failure_type(test_failures.FailureTextMismatch, test_result.failures))
        self.assert_contains(log_stream, 'Please remove %s' % extra_txt)

    def test_passing_testharness_overriding_baseline(self):
        # An all-pass testharness text baseline is necessary when it overrides a fallback baseline.
        host = MockHost()
        # The base baseline expects a failure.
        base_baseline = test.WEB_TEST_DIR + '/passes/testharness-expected.txt'
        host.filesystem.write_text_file(base_baseline, 'Failure')
        platform_baseline = test.WEB_TEST_DIR + '/platform/test-mac-mac10.10/passes/testharness-expected.txt'
        host.filesystem.write_text_file(
            platform_baseline,
            'This is a testharness.js-based test.\nPASS: bah\nHarness: the test ran to completion.')
        run_details, log_stream, _ = logging_run(
            ['passes/testharness.html'], tests_included=True, host=host)
        self.assertEqual(run_details.exit_code, 0)
        self.assertNotIn('Please remove', log_stream.getvalue())

    def test_additional_platform_directory(self):
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo', '--order', 'natural']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/../foo', '--order', 'natural']))
        self.assertTrue(passing_run(['--additional-platform-directory', '/tmp/foo', '--additional-platform-directory',
                                     '/tmp/bar', '--order', 'natural']))
        self.assertTrue(passing_run(['--additional-platform-directory', 'foo', '--order', 'natural']))

    def test_additional_expectations(self):
        host = MockHost()
        host.filesystem.write_text_file('/tmp/overrides.txt', 'Bug(x) failures/unexpected/mismatch.html [ Failure ]\n')
        self.assertTrue(passing_run(['--additional-expectations', '/tmp/overrides.txt', 'failures/unexpected/mismatch.html'],
                                    tests_included=True, host=host))

    def test_platform_directories_ignored_when_searching_for_tests(self):
        tests_run = get_tests_run(['--platform', 'test-mac-mac10.10'])
        self.assertNotIn('platform/test-mac-mac10.10/http/test.html', tests_run)
        self.assertNotIn('platform/test-win-win7/http/test.html', tests_run)

    def test_platform_directories_not_searched_for_additional_tests(self):
        tests_run = get_tests_run(['--platform', 'test-mac-mac10.10', 'http'])
        self.assertNotIn('platform/test-mac-mac10.10/http/test.html', tests_run)
        self.assertNotIn('platform/test-win-win7/http/test.html', tests_run)

    def test_output_diffs(self):
        host = MockHost()
        logging_run(['failures/unexpected/text-image-checksum.html'], tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertTrue(any(path.endswith('-diff.txt') for path in written_files.keys()))
        self.assertTrue(any(path.endswith('-pretty-diff.html') for path in written_files.keys()))
        self.assertFalse(any(path.endswith('-wdiff.html') for path in written_files))

    def test_unsupported_platform(self):
        stderr = StringIO.StringIO()
        res = run_web_tests.main(['--platform', 'foo'], stderr)

        self.assertEqual(res, exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)
        self.assertTrue('unsupported platform' in stderr.getvalue())

    def test_verbose_in_child_processes(self):
        # When we actually run multiple processes, we may have to reconfigure logging in the
        # child process (e.g., on win32) and we need to make sure that works and we still
        # see the verbose log output. However, we can't use logging_run() because using
        # output_capture to capture stderr latter results in a nonpicklable host.

        options, parsed_args = parse_args(['--verbose', '--fully-parallel', '--jobs',
                                           '2', 'passes/text.html', 'passes/image.html'], tests_included=True)
        host = MockHost()
        port_obj = host.port_factory.get(port_name=options.platform, options=options)
        logging_stream = StringIO.StringIO()
        printer = Printer(host, options, logging_stream)
        run_web_tests.run(port_obj, options, parsed_args, printer)
        self.assertTrue('text.html passed' in logging_stream.getvalue())
        self.assertTrue('image.html passed' in logging_stream.getvalue())

    def disabled_test_driver_logging(self):
        # FIXME: Figure out how to either use a mock-test port to
        # get output or mack mock ports work again.
        host = Host()
        _, err, _ = logging_run(['--platform', 'mock-win', '--driver-logging', 'fast/harness/results.html'],
                                tests_included=True, host=host)
        self.assertIn('OUT:', err.getvalue())

    def _check_json_test_results(self, host, details):
        self.assertEqual(details.exit_code, 0)
        self.assertTrue(host.filesystem.exists('/tmp/json_results.json'))
        json_failing_test_results = host.filesystem.read_text_file('/tmp/json_results.json')
        self.assertEqual(json.loads(json_failing_test_results), details.summarized_full_results)

    def test_json_test_results(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--json-test-results', '/tmp/json_results.json'], host=host)
        self._check_json_test_results(host, details)

    def test_json_test_results_alias_write_full_results_to(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--write-full-results-to', '/tmp/json_results.json'], host=host)
        self._check_json_test_results(host, details)

    def test_json_test_results_alias_isolated_script_test_output(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--isolated-script-test-output', '/tmp/json_results.json'], host=host)
        self._check_json_test_results(host, details)

    def test_json_failing_test_results(self):
        host = MockHost()
        details, _, _ = logging_run(
            ['--json-failing-test-results', '/tmp/json_failing_results.json'], host=host)
        self.assertEqual(details.exit_code, 0)
        self.assertTrue(host.filesystem.exists('/tmp/json_failing_results.json'))
        json_failing_test_results = host.filesystem.read_text_file('/tmp/json_failing_results.json')
        self.assertEqual(json.loads(json_failing_test_results), details.summarized_failing_results)

    def test_no_default_expectations(self):
        self.assertFalse(passing_run(['--ignore-default-expectations', 'failures/expected/text.html']))


class RebaselineTest(unittest.TestCase, StreamTestingMixin):
    """Tests for flags which cause new baselines to be written.

    When running web tests, there are several flags which write new
    baselines. This is separate from the "blink_tool.py rebaseline" commands,
    which fetch new baselines from elsewhere rather than generating them.
    """

    def assert_baselines(self, written_files, log_stream, expected_file_base, expected_extensions):
        """Asserts that the written_files contains baselines for one test.

        Args:
            written_files: from FileSystem.written_files.
            log_stream: The log stream from the run.
            expected_file_base: Relative path to the baseline,
                without the extension, from the web test directory.
            expected_extensions: Expected extensions which should be written.
        """
        for ext in expected_extensions:
            baseline = '%s-expected%s' % (expected_file_base, ext)
            baseline_full_path = '%s/%s' % (test.WEB_TEST_DIR, baseline)
            self.assertIsNotNone(written_files.get(baseline_full_path))
            baseline_message = 'Writing new baseline "%s"\n' % baseline
            self.assert_contains(log_stream, baseline_message)
        # Assert that baselines with other extensions were not written.
        for ext in ({'.png', '.txt', '.wav'} - set(expected_extensions)):
            baseline = '%s-expected%s' % (expected_file_base, ext)
            baseline_full_path = '%s/%s' % (test.WEB_TEST_DIR, baseline)
            self.assertIsNone(written_files.get(baseline_full_path))

    def test_reset_results_basic(self):
        # Test that we update baselines in place when the test fails
        # (text and image mismatch).
        host = MockHost()
        details, log_stream, _ = logging_run(
            ['--reset-results', 'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        # The run exit code is 0, indicating success; since we're resetting
        # baselines, it's OK for actual results to not match baselines.
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 7)
        self.assert_baselines(
            written_files, log_stream,
            'failures/unexpected/text-image-checksum',
            expected_extensions=['.txt', '.png'])

    def test_no_baselines_are_written_with_no_reset_results_flag(self):
        # This test checks that we're *not* writing baselines when we're not
        # supposed to be (when there's no --reset-results flag).
        host = MockHost()
        details, log_stream, _ = logging_run(
            ['failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        # In a normal test run where actual results don't match baselines, the
        # exit code indicates failure.
        self.assertEqual(details.exit_code, 1)
        self.assert_baselines(
            written_files, log_stream, 'failures/unexpected/text-image-checksum',
            expected_extensions=[])

    def test_reset_results_missing_results(self):
        # Test that we create new baselines at the generic location for
        # if we are missing baselines.
        host = MockHost()
        details, log_stream, _ = logging_run(
            [
                '--reset-results',
                'failures/unexpected/missing_text.html',
                'failures/unexpected/missing_image.html',
                'failures/unexpected/missing_render_tree_dump.html'
            ],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 8)
        self.assert_baselines(
            written_files, log_stream,
            'failures/unexpected/missing_text', ['.txt'])
        self.assert_baselines(
            written_files, log_stream,
            'failures/unexpected/missing_image', ['.png'])
        self.assert_baselines(
            written_files, log_stream,
            'failures/unexpected/missing_render_tree_dump',
            expected_extensions=['.txt'])

    def test_reset_results_testharness_no_baseline(self):
        # Tests that we create new result for a failing testharness test without
        # baselines, but don't create one for a passing one.
        host = MockHost()
        details, log_stream, _ = logging_run(
            [
                '--reset-results',
                'failures/unexpected/testharness.html',
                'passes/testharness.html'
            ],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 6)
        self.assert_baselines(written_files, log_stream, 'failures/unexpected/testharness', ['.txt'])
        self.assert_baselines(written_files, log_stream, 'passes/testharness', [])

    def test_reset_results_testharness_existing_baseline(self):
        # Tests that we update existing baseline for a testharness test.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR + '/failures/unexpected/testharness-expected.txt', 'foo')
        details, log_stream, _ = logging_run(
            [
                '--reset-results',
                'failures/unexpected/testharness.html'
            ],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 0)
        written_files = host.filesystem.written_files
        self.assertEqual(len(written_files.keys()), 6)
        self.assert_baselines(written_files, log_stream, 'failures/unexpected/testharness', ['.txt'])

    def test_reset_results_image_only(self):
        # Tests that we don't create new text results for an image-only test.
        host = MockHost()
        details, log_stream, _ = logging_run(
            [
                '--reset-results',
                'failures/unexpected/image-only.html',
            ],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 0)
        written_files = host.filesystem.written_files
        self.assertEqual(len(written_files.keys()), 6)
        self.assert_baselines(written_files, log_stream, 'failures/unexpected/image-only', ['.png'])

    def test_copy_baselines(self):
        # Test that we update the baselines in the version-specific directories
        # if the new baseline is different from the fallback baseline.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we also
            # check that the text baseline isn't written if it matches.
            'text-image-checksum_fail-txt')
        details, log_stream, _ = logging_run(
            [
                '--copy-baselines',
                'failures/unexpected/text-image-checksum.html'
            ],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 1)
        self.assertEqual(len(written_files.keys()), 11)
        self.assert_contains(
            log_stream,
            'Copying baseline to "platform/test-mac-mac10.10/failures/unexpected/text-image-checksum-expected.png"')
        self.assert_contains(
            log_stream,
            'Not copying baseline to "platform/test-mac-mac10.10/failures/unexpected/text-image-checksum-expected.txt"')

    def test_reset_results_with_copy_baselines(self):
        # Test that we update the baselines in the version-specific directories
        # if the new baseline is different from the fallback baseline.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we also
            # check that the text baseline isn't written if it matches.
            'text-image-checksum_fail-txt')
        details, log_stream, _ = logging_run(
            [
                '--reset-results', '--copy-baselines',
                'failures/unexpected/text-image-checksum.html'
            ],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 7)
        self.assert_baselines(
            written_files, log_stream,
            'platform/test-mac-mac10.10/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])

    def test_reset_results_reftest(self):
        # Test rebaseline of reference tests.
        # Reference tests don't have baselines, so they should be ignored.
        host = MockHost()
        details, log_stream, _ = logging_run(
            ['--reset-results', 'passes/reftest.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 5)
        self.assert_baselines(
            written_files, log_stream, 'passes/reftest', expected_extensions=[])

    def test_reset_results_reftest_with_text(self):
        # In this case, there is a text baseline present; a new baseline is
        # written even though this is a reference test.
        host = MockHost()
        details, log_stream, _ = logging_run(
            ['--reset-results', 'failures/unexpected/reftest-with-mismatching-text.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 6)
        self.assert_baselines(
            written_files, log_stream, 'failures/unexpected/reftest-with-mismatching-text',
            expected_extensions=['.txt'])

    def test_reset_results_remove_extra_baselines(self):
        host = MockHost()
        extra_txt = test.WEB_TEST_DIR + '/failures/unexpected/image-only-expected.txt'
        host.filesystem.write_text_file(extra_txt, 'Extra txt')
        extra_wav = test.WEB_TEST_DIR + '/failures/unexpected/image-only-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        details, log_stream, _ = logging_run(
            ['--reset-results', 'failures/unexpected/image-only.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 8)
        self.assertIsNone(written_files[extra_txt])
        self.assertIsNone(written_files[extra_wav])
        self.assert_baselines(
            written_files, log_stream, 'failures/unexpected/image-only',
            expected_extensions=['.png'])

    def test_reset_results_reftest_remove_extra_baselines(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/reftest-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        extra_wav = test.WEB_TEST_DIR + '/passes/reftest-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        extra_txt = test.WEB_TEST_DIR + '/passes/reftest-expected.txt'
        host.filesystem.write_text_file(extra_txt, 'reftest')
        details, _, _ = logging_run(['--reset-results', 'passes/reftest.html'],
                                    tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 8)
        self.assertIsNone(written_files[extra_png])
        self.assertIsNone(written_files[extra_wav])
        self.assertIsNone(written_files[extra_txt])

    def test_reset_results_reftest_with_text_remove_extra_baselines(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/reftest-with-text-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        extra_wav = test.WEB_TEST_DIR + '/passes/reftest-with-text-expected.wav'
        host.filesystem.write_text_file(extra_wav, 'Extra wav')
        details, _, _ = logging_run(['--reset-results', 'passes/reftest-with-text.html'],
                                    tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 7)
        self.assertIsNone(written_files[extra_png])
        self.assertIsNone(written_files[extra_wav])
        self.assertNotIn(test.WEB_TEST_DIR + '/passes/reftest-with-text-expected.txt', written_files)

    def test_reset_results_passing_testharness_remove_extra_baselines(self):
        host = MockHost()
        extra_png = test.WEB_TEST_DIR + '/passes/testharness-expected.png'
        host.filesystem.write_text_file(extra_png, 'Extra png')
        extra_txt = test.WEB_TEST_DIR + '/passes/testharness-expected.txt'
        host.filesystem.write_text_file(extra_txt, 'Extra txt')
        details, log_stream, _ = logging_run(
            ['--reset-results', 'passes/testharness.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 7)
        self.assertIsNone(written_files[extra_png])
        self.assertIsNone(written_files[extra_txt])
        self.assert_baselines(
            written_files, log_stream, 'passes/testharness',
            expected_extensions=[])

    def test_reset_results_failing_testharness(self):
        host = MockHost()
        details, log_stream, _ = logging_run(
            ['--reset-results', 'failures/unexpected/testharness.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 6)
        self.assert_baselines(
            written_files, log_stream, 'failures/unexpected/testharness',
            expected_extensions=['.txt'])

    def test_new_flag_specific_baseline(self):
        # Test writing new baselines under flag-specific directory if the actual
        # results are different from the current baselines.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we also
            # check that the text baseline isn't written if it matches.
            'text-image-checksum_fail-txt')
        details, log_stream, _ = logging_run(
            ['--additional-driver-flag=--flag',
             '--reset-results',
             'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 7)
        # We should create new image baseline only.
        self.assert_baselines(
            written_files, log_stream,
            'flag-specific/flag/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])

    def test_copy_flag_specific_baseline(self):
        # Test writing new baselines under flag-specific directory if the actual
        # results are different from the current baselines.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we also
            # check that the text baseline isn't written if it matches.
            'text-image-checksum_fail-txt')
        details, log_stream, _ = logging_run(
            ['--additional-driver-flag=--flag',
             '--copy-baselines',
             'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 1)
        self.assertEqual(len(written_files.keys()), 11)
        self.assert_contains(
            log_stream,
            'Copying baseline to "flag-specific/flag/failures/unexpected/text-image-checksum-expected.png"')
        self.assert_contains(
            log_stream,
            'Not copying baseline to "flag-specific/flag/failures/unexpected/text-image-checksum-expected.txt"')

    def test_new_flag_specific_baseline_optimize(self):
        # Test removing existing baselines under flag-specific directory if the
        # actual results are the same as the fallback baselines.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we check
            # that the flag-specific text baseline is removed if the actual
            # result is the same as this fallback baseline.
            'text-image-checksum_fail-txt')
        flag_specific_baseline_txt = (
            test.WEB_TEST_DIR +
            '/flag-specific/flag/failures/unexpected/text-image-checksum-expected.txt')
        host.filesystem.write_text_file(
            flag_specific_baseline_txt, 'existing-baseline-different-from-fallback')

        details, log_stream, _ = logging_run(
            ['--additional-driver-flag=--flag',
             '--reset-results',
             'failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 0)
        self.assertFalse(host.filesystem.exists(flag_specific_baseline_txt))
        written_files = host.filesystem.written_files
        self.assertEqual(len(written_files.keys()), 8)
        # We should create new image baseline only.
        self.assert_baselines(
            written_files, log_stream,
            'flag-specific/flag/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])

    def test_new_virtual_baseline(self):
        # Test writing new baselines under virtual test directory if the actual
        # results are different from the current baselines.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we also
            # check that the text baseline isn't written if it matches.
            'text-image-checksum_fail-txt')
        details, log_stream, _ = logging_run(
            ['--reset-results',
             'virtual/virtual_failures/failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 9)
        # We should create new image baseline only.
        self.assert_baselines(
            written_files, log_stream,
            'virtual/virtual_failures/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])

    def test_new_platform_baseline_with_fallback(self):
        # Test that we update the existing baseline in the platform-specific
        # directory if the new baseline is different, with existing fallback
        # baseline (which should not matter).
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/platform/test-mac-mac10.10/failures/unexpected/text-image-checksum-expected.png',
            'wrong-png-baseline')

        details, log_stream, _ = logging_run(
            [
                '--reset-results',
                'failures/unexpected/text-image-checksum.html'
            ],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 7)
        # We should reset the platform image baseline.
        self.assert_baselines(
            written_files, log_stream,
            'platform/test-mac-mac10.10/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])

    def test_new_platform_baseline_without_fallback(self):
        # Test that we update the existing baseline in the platform-specific
        # directory if the new baseline is different, without existing fallback
        # baseline (which should not matter).
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/platform/test-mac-mac10.10/failures/unexpected/text-image-checksum-expected.png',
            'wrong-png-baseline')
        host.filesystem.remove(
            test.WEB_TEST_DIR + '/failures/unexpected/text-image-checksum-expected.png')

        details, log_stream, _ = logging_run(
            [
                '--reset-results',
                'failures/unexpected/text-image-checksum.html'
            ],
            tests_included=True, host=host)
        written_files = host.filesystem.written_files
        self.assertEqual(details.exit_code, 0)
        self.assertEqual(len(written_files.keys()), 8)
        # We should reset the platform image baseline.
        self.assert_baselines(
            written_files, log_stream,
            'platform/test-mac-mac10.10/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])

    def test_new_virtual_baseline_optimize(self):
        # Test removing existing baselines under flag-specific directory if the
        # actual results are the same as the fallback baselines.
        host = MockHost()
        host.filesystem.write_text_file(
            test.WEB_TEST_DIR +
            '/failures/unexpected/text-image-checksum-expected.txt',
            # This value is the same as actual text result of the test defined
            # in blinkpy.web_tests.port.test. This is added so that we check
            # that the flag-specific text baseline is removed if the actual
            # result is the same as this fallback baseline.
            'text-image-checksum_fail-txt')
        virtual_baseline_txt = (
            test.WEB_TEST_DIR +
            '/virtual/virtual_failures/failures/unexpected/text-image-checksum-expected.txt')
        host.filesystem.write_text_file(
            virtual_baseline_txt, 'existing-baseline-different-from-fallback')

        details, log_stream, _ = logging_run(
            ['--reset-results',
             'virtual/virtual_failures/failures/unexpected/text-image-checksum.html'],
            tests_included=True, host=host)
        self.assertEqual(details.exit_code, 0)
        self.assertFalse(host.filesystem.exists(virtual_baseline_txt))
        written_files = host.filesystem.written_files
        self.assertEqual(len(written_files.keys()), 10)
        # We should create new image baseline only.
        self.assert_baselines(
            written_files, log_stream,
            'virtual/virtual_failures/failures/unexpected/text-image-checksum',
            expected_extensions=['.png'])


class MainTest(unittest.TestCase):

    def test_exception_handling(self):
        orig_run_fn = run_web_tests.run

        # pylint: disable=unused-argument
        def interrupting_run(port, options, args, printer):
            raise KeyboardInterrupt

        def successful_run(port, options, args, printer):

            class FakeRunDetails(object):
                exit_code = exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

            return FakeRunDetails()

        def exception_raising_run(port, options, args, printer):
            assert False

        stderr = StringIO.StringIO()
        try:
            run_web_tests.run = interrupting_run
            res = run_web_tests.main([], stderr)
            self.assertEqual(res, exit_codes.INTERRUPTED_EXIT_STATUS)

            run_web_tests.run = successful_run
            res = run_web_tests.main(['--platform', 'test'], stderr)
            self.assertEqual(res, exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)

            run_web_tests.run = exception_raising_run
            res = run_web_tests.main([], stderr)
            self.assertEqual(res, exit_codes.UNEXPECTED_ERROR_EXIT_STATUS)
        finally:
            run_web_tests.run = orig_run_fn
