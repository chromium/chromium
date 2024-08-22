# Copyright (C) 2010 Google Inc. All rights reserved.
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
"""The Manager orchestrates the overall process of running web tests.

This includes finding tests to run, reading the test expectations,
starting the required helper servers, deciding the order and way to
run the tests, retrying failed tests, and collecting the test results,
including crash logs and mismatches with expectations.

The Manager object has a constructor and one main method called run.
"""

import fnmatch
import json
import logging
import os
import random
import signal
import sys
import time

from blinkpy.common import exit_codes
from blinkpy.common.path_finder import PathFinder
from blinkpy.tool import grammar
from blinkpy.web_tests.controllers.test_result_sink import CreateTestResultSink
from blinkpy.web_tests.controllers.web_test_finder import WebTestFinder
from blinkpy.web_tests.controllers.web_test_runner import WebTestRunner
from blinkpy.web_tests.layout_package import json_results_generator
from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models import test_run_results
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.models.test_input import TestInput

_log = logging.getLogger(__name__)

TestExpectations = test_expectations.TestExpectations


class Manager(object):
    """A class for managing running a series of web tests."""

    HTTP_SUBDIR = 'http'
    PERF_SUBDIR = 'perf'
    WEBSOCKET_SUBDIR = 'websocket'
    ARCHIVED_RESULTS_LIMIT = 25

    def __init__(self, port, options, printer):
        """Initializes test runner data structures.

        Args:
            port: An object implementing platform-specific functionality.
            options: An options argument which contains command line options.
            printer: A Printer object to record updates to.
        """
        self._port = port
        self._filesystem = port.host.filesystem
        self._options = options
        self._printer = printer

        self._expectations = None
        self._http_server_started = False
        self._wptserve_started = False
        self._websockets_server_started = False

        self._results_directory = self._port.results_directory()
        self._artifacts_directory = self._port.artifacts_directory()
        self._finder = WebTestFinder(self._port, self._options)
        self._path_finder = PathFinder(port.host.filesystem)

        self._sink = CreateTestResultSink(self._port)
        self._runner = WebTestRunner(self._options, self._port, self._printer,
                                     self._results_directory,
                                     self._test_is_slow, self._sink)

    def run(self, args):
        """Runs the tests and return a RunDetails object with the results."""
        start_time = time.time()
        self._printer.write_update('Collecting tests ...')
        running_all_tests = False

        try:
            paths, all_test_names, running_all_tests = self._collect_tests(
                args)
        except IOError:
            # This is raised if --test-list doesn't exist
            return test_run_results.RunDetails(
                exit_code=exit_codes.NO_TESTS_EXIT_STATUS)

        test_names = self._finder.split_into_chunks(all_test_names)
        if self._options.order == 'natural':
            test_names.sort(key=self._port.test_key)
        elif self._options.order == 'random':
            test_names.sort()
            random.Random(self._options.seed).shuffle(test_names)
        elif self._options.order == 'none':
            # Restore the test order to user specified order.
            # base.tests() may change the order as it returns tests in the
            # real, external/wpt, virtual order.
            if paths:
                test_names = self._restore_order(paths, test_names)

        if not self._options.no_expectations:
            self._printer.write_update('Parsing expectations ...')
            self._expectations = test_expectations.TestExpectations(self._port)

        tests_to_run, tests_to_skip = self._prepare_lists(paths, test_names)

        self._printer.print_found(
            len(all_test_names), len(test_names), len(tests_to_run),
            self._options.repeat_each, self._options.iterations)

        # Check to make sure we're not skipping every test.
        if not tests_to_run:
            msg = 'No tests to run.'
            if self._options.zero_tests_executed_ok:
                _log.info(msg)
                # Keep executing to produce valid (but empty) results.
            else:
                _log.critical(msg)
                code = exit_codes.NO_TESTS_EXIT_STATUS
                return test_run_results.RunDetails(exit_code=code)

        exit_code = self._set_up_run(tests_to_run)
        if exit_code:
            return test_run_results.RunDetails(exit_code=exit_code)

        if self._options.num_retries is None:
            # If --test-list is passed, or if no test narrowing is specified,
            # default to 3 retries. Otherwise [e.g. if tests are being passed by
            # name], default to 0 retries.
            if self._options.test_list or len(paths) < len(test_names):
                self._options.num_retries = 3
            else:
                self._options.num_retries = 0

        should_retry_failures = self._options.num_retries > 0

        try:
            self._register_termination_handler()
            self._start_servers(tests_to_run)
            if self._options.watch:
                run_results = self._run_test_loop(tests_to_run, tests_to_skip)
            else:
                run_results = self._run_test_once(tests_to_run, tests_to_skip,
                                                  should_retry_failures)
            initial_results, all_retry_results = run_results
        finally:
            _log.info("Finally stop servers and clean up")
            self._stop_servers()
            self._clean_up_run()

        if self._options.no_expectations:
            return test_run_results.RunDetails(0, [], [], initial_results,
                                               all_retry_results)

        # Some crash logs can take a long time to be written out so look
        # for new logs after the test run finishes.
        self._printer.write_update('Looking for new crash logs ...')
        self._look_for_new_crash_logs(initial_results, start_time)
        for retry_attempt_results in all_retry_results:
            self._look_for_new_crash_logs(retry_attempt_results, start_time)

        self._printer.write_update('Summarizing results ...')
        summarized_full_results = test_run_results.summarize_results(
            self._port, self._options, self._expectations, initial_results,
            all_retry_results)
        summarized_failing_results = test_run_results.summarize_results(
            self._port,
            self._options,
            self._expectations,
            initial_results,
            all_retry_results,
            only_include_failing=True)
        run_histories = test_run_results.test_run_histories(
            self._options, self._expectations, initial_results,
            all_retry_results)

        exit_code = summarized_failing_results['num_regressions']
        if exit_code > exit_codes.MAX_FAILURES_EXIT_STATUS:
            _log.warning('num regressions (%d) exceeds max exit status (%d)',
                         exit_code, exit_codes.MAX_FAILURES_EXIT_STATUS)
            exit_code = exit_codes.MAX_FAILURES_EXIT_STATUS

        if not self._options.dry_run:
            self._write_json_files(summarized_full_results,
                                   summarized_failing_results, initial_results,
                                   running_all_tests, run_histories)

            self._copy_results_html_file(self._artifacts_directory,
                                         'results.html')
            if (initial_results.interrupt_reason is
                    test_run_results.InterruptReason.EXTERNAL_SIGNAL):
                exit_code = exit_codes.INTERRUPTED_EXIT_STATUS
            else:
                if initial_results.interrupted:
                    exit_code = exit_codes.EARLY_EXIT_STATUS
                if (self._options.show_results
                        and (exit_code or initial_results.total_failures)):
                    self._port.show_results_html_file(
                        self._filesystem.join(self._artifacts_directory,
                                              'results.html'))
                self._printer.print_results(time.time() - start_time,
                                            initial_results)

        return test_run_results.RunDetails(exit_code, summarized_full_results,
                                           summarized_failing_results,
                                           initial_results, all_retry_results)

    def _register_termination_handler(self):
        if self._port.host.platform.is_win():
            signum = signal.SIGBREAK
        else:
            signum = signal.SIGTERM
        signal.signal(signum, self._on_termination)

    def _on_termination(self, signum, _frame):
        self._printer.write_update(
            'Received signal "%s" (%d) in %d' %
            (signal.strsignal(signum), signum, os.getpid()))
        raise KeyboardInterrupt

    def _run_test_loop(self, tests_to_run, tests_to_skip):
        # Don't show results in a new browser window because we're already
        # printing the link to diffs in the loop
        self._options.show_results = False

        while True:
            initial_results, all_retry_results = self._run_test_once(
                tests_to_run, tests_to_skip, should_retry_failures=False)
            for name in initial_results.failures_by_name:
                failure = initial_results.failures_by_name[name][0]
                if isinstance(failure, test_failures.FailureTextMismatch):
                    full_test_path = self._filesystem.join(
                        self._artifacts_directory, name)
                    filename, _ = self._filesystem.splitext(full_test_path)
                    pretty_diff_path = 'file://' + filename + '-pretty-diff.html'
                    self._printer.writeln('Link to pretty diff:')
                    self._printer.writeln(pretty_diff_path + '\n')
            self._printer.writeln('Finished running tests')

            user_input = self._port.host.user.prompt(
                'Interactive watch mode: (q)uit (r)etry\n').lower()

            if user_input == 'q' or user_input == 'quit':
                return (initial_results, all_retry_results)

    def _run_test_once(self, tests_to_run, tests_to_skip,
                       should_retry_failures):
        num_workers = int(
            self._port.num_workers(int(self._options.child_processes)))

        initial_results = self._run_tests(
            tests_to_run, tests_to_skip, self._options.repeat_each,
            self._options.iterations, num_workers)

        # Don't retry failures when interrupted by user or failures limit exception.
        should_retry_failures = (should_retry_failures
                                 and not initial_results.interrupted)

        tests_to_retry = self._tests_to_retry(initial_results)
        all_retry_results = []
        if should_retry_failures and tests_to_retry:
            for retry_attempt in range(1, self._options.num_retries + 1):
                if not tests_to_retry:
                    break

                _log.info('')
                _log.info(
                    'Retrying %s, attempt %d of %d...',
                    grammar.pluralize('unexpected failure',
                                      len(tests_to_retry)), retry_attempt,
                    self._options.num_retries)

                retry_results = self._run_tests(
                    tests_to_retry,
                    tests_to_skip=set(),
                    repeat_each=1,
                    iterations=1,
                    num_workers=num_workers,
                    retry_attempt=retry_attempt)
                all_retry_results.append(retry_results)

                tests_to_retry = self._tests_to_retry(retry_results)
        return (initial_results, all_retry_results)

    def _restore_order(self, paths, test_names):
        original_test_names = list(test_names)
        test_names = []
        for path in paths:
            for test in original_test_names:
                if test.startswith(path) or fnmatch.fnmatch(test, path):
                    test_names.append(test)
        test_names += list(set(original_test_names) - set(test_names))
        return test_names

    def _collect_tests(self, args):
        return self._finder.find_tests(
            args,
            test_lists=self._options.test_list,
            filter_files=self._options.isolated_script_test_filter_file,
            inverted_filter_files=self._options.
            inverted_test_launcher_filter_file,
            fastest_percentile=self._options.fastest,
            filters=self._options.isolated_script_test_filter)

    def _is_http_test(self, test):
        return (
            test.startswith(self.HTTP_SUBDIR + self._port.TEST_PATH_SEPARATOR)
            or self._is_websocket_test(test) or self._port.TEST_PATH_SEPARATOR
            + self.HTTP_SUBDIR + self._port.TEST_PATH_SEPARATOR in test)

    def _is_websocket_test(self, test):
        if self._port.should_use_wptserve(test):
            return False

        return self.WEBSOCKET_SUBDIR + self._port.TEST_PATH_SEPARATOR in test

    def _http_tests(self, test_names):
        return set(test for test in test_names if self._is_http_test(test))

    def _is_perf_test(self, test):
        return (self.PERF_SUBDIR == test
                or (self.PERF_SUBDIR + self._port.TEST_PATH_SEPARATOR) in test)

    def _prepare_lists(self, paths, test_names):
        tests_to_skip = self._finder.skip_tests(paths, test_names,
                                                self._expectations)
        tests_to_run = [
            test for test in test_names if test not in tests_to_skip
        ]

        return tests_to_run, tests_to_skip

    def _test_input_for_file(self, test_file, retry_attempt):
        return TestInput(
            test_file,
            self._options.slow_timeout_ms
            if self._test_is_slow(test_file) else self._options.timeout_ms,
            self._test_requires_lock(test_file),
            retry_attempt=retry_attempt)

    def _test_requires_lock(self, test_file):
        """Returns True if the test needs to be locked when running multiple
        instances of this test runner.

        Perf tests are locked because heavy load caused by running other
        tests in parallel might cause some of them to time out.
        """
        return self._is_perf_test(test_file)

    def _test_is_slow(self, test_file):
        if not self._expectations:
            return False
        is_slow_test = self._expectations.get_expectations(
            test_file).is_slow_test
        return is_slow_test or self._port.is_slow_wpt_test(test_file)

    def _needs_servers(self, test_names):
        return any(
            self._is_http_test(test_name) for test_name in test_names)

    def _set_up_run(self, test_names):
        self._printer.write_update('Checking build ...')
        if self._options.build:
            exit_code = self._port.check_build(
                self._needs_servers(test_names), self._printer)
            if exit_code:
                _log.error('Build check failed')
                return exit_code

        if self._options.clobber_old_results:
            self._port.clobber_old_results()
        elif self._filesystem.exists(self._artifacts_directory):
            self._port.limit_archived_results_count()
            # Rename the existing results folder for archiving.
            self._port.rename_results_folder()

        # Create the output directory if it doesn't already exist.
        self._port.host.filesystem.maybe_make_directory(
            self._artifacts_directory)

        exit_code = self._port.setup_test_run()
        if exit_code:
            _log.error('Build setup failed')
            return exit_code

        # Check that the system dependencies (themes, fonts, ...) are correct.
        if not self._options.nocheck_sys_deps:
            self._printer.write_update('Checking system dependencies ...')
            exit_code = self._port.check_sys_deps()
            if exit_code:
                return exit_code

        return exit_codes.OK_EXIT_STATUS

    def _run_tests(self,
                   tests_to_run,
                   tests_to_skip,
                   repeat_each,
                   iterations,
                   num_workers,
                   retry_attempt=0):

        test_inputs = []
        for _ in range(iterations):
            for test in tests_to_run:
                for _ in range(repeat_each):
                    test_inputs.append(
                        self._test_input_for_file(test, retry_attempt))
        return self._runner.run_tests(self._expectations, test_inputs,
                                      tests_to_skip, num_workers,
                                      retry_attempt)

    def _start_servers(self, tests_to_run):
        if any(self._port.is_wpt_test(test) for test in tests_to_run):
            self._printer.write_update('Starting WPTServe ...')
            self._port.start_wptserve()
            self._wptserve_started = True

        if (self._port.requires_http_server()
                or any(self._is_http_test(test) for test in tests_to_run)):
            self._printer.write_update('Starting HTTP server ...')
            self._port.start_http_server(
                additional_dirs={},
                number_of_drivers=self._options.max_locked_shards)
            self._http_server_started = True

        if any(self._is_websocket_test(test) for test in tests_to_run):
            self._printer.write_update('Starting WebSocket server ...')
            self._port.start_websocket_server()
            self._websockets_server_started = True

    def _stop_servers(self):
        if self._wptserve_started:
            self._printer.write_update('Stopping WPTServe ...')
            self._wptserve_started = False
            self._port.stop_wptserve()
        if self._http_server_started:
            self._printer.write_update('Stopping HTTP server ...')
            self._http_server_started = False
            self._port.stop_http_server()
        if self._websockets_server_started:
            self._printer.write_update('Stopping WebSocket server ...')
            self._websockets_server_started = False
            self._port.stop_websocket_server()

    def _clean_up_run(self):
        _log.debug('Flushing stdout')
        sys.stdout.flush()
        _log.debug('Flushing stderr')
        sys.stderr.flush()
        _log.debug('Cleaning up port')
        self._port.clean_up_test_run()
        if self._sink:
            _log.debug('Closing sink')
            self._sink.close()

    def _look_for_new_crash_logs(self, run_results, start_time):
        """Looks for and writes new crash logs, at the end of the test run.

        Since crash logs can take a long time to be written out if the system is
        under stress, do a second pass at the end of the test run.

        Args:
            run_results: The results of the test run.
            start_time: Time the tests started at. We're looking for crash
                logs after that time.
        """
        crashed_processes = []
        test_to_crash_failure = {}

        # reset static variables for Failure type classes
        test_failures.AbstractTestResultType.port = self._port
        test_failures.AbstractTestResultType.result_directory = self._results_directory
        test_failures.AbstractTestResultType.filesystem = self._filesystem

        for test, result in run_results.unexpected_results_by_name.items():
            if result.type != ResultType.Crash:
                continue
            for failure in result.failures:
                if (not isinstance(failure, test_failures.FailureCrash)
                        or failure.has_log):
                    continue
                crashed_processes.append(
                    [test, failure.process_name, failure.pid])
                test_to_crash_failure[test] = failure

        sample_files = self._port.look_for_new_samples(crashed_processes,
                                                       start_time) or {}
        for test, sample_file in sample_files.items():
            test_failures.AbstractTestResultType.test_name = test
            test_result = run_results.unexpected_results_by_name[test]
            artifact_relative_path = self._port.output_filename(
                test, test_failures.FILENAME_SUFFIX_SAMPLE, '.txt')
            artifacts_sub_dir = test_result.artifacts.ArtifactsSubDirectory()
            artifact_abspath = self._filesystem.join(self._results_directory,
                                                     artifacts_sub_dir,
                                                     artifact_relative_path)
            self._filesystem.maybe_make_directory(
                self._filesystem.dirname(artifact_abspath))
            self._filesystem.copyfile(sample_file, artifact_abspath)
            test_result.artifacts.AddArtifact(
                'sample_file',
                self._filesystem.join(artifacts_sub_dir,
                                      artifact_relative_path))

        new_crash_logs = self._port.look_for_new_crash_logs(
            crashed_processes, start_time) or {}
        for test, (crash_log, crash_site) in new_crash_logs.items():
            test_failures.AbstractTestResultType.test_name = test
            failure.crash_log = crash_log
            failure.has_log = self._port.output_contains_sanitizer_messages(
                failure.crash_log)
            test_result = run_results.unexpected_results_by_name[test]
            test_result.crash_site = crash_site
            test_to_crash_failure[test].create_artifacts(
                test_result.artifacts, force_overwrite=True)

    def _tests_to_retry(self, run_results):
        # TODO(ojan): This should also check that result.type != test_expectations.MISSING
        # since retrying missing expectations is silly. But that's a bit tricky since we
        # only consider the last retry attempt for the count of unexpected regressions.
        return [
            result.test_name
            for result in run_results.unexpected_results_by_name.values()
            if result.type != ResultType.Pass
        ]

    def _write_json_files(self, summarized_full_results,
                          summarized_failing_results, initial_results,
                          running_all_tests, run_histories):
        _log.debug("Writing JSON files in %s.", self._artifacts_directory)

        # FIXME: Upload stats.json to the server and delete times_ms.
        times_trie = json_results_generator.test_timings_trie(
            initial_results.results_by_name.values())
        times_json_path = self._filesystem.join(self._artifacts_directory,
                                                'times_ms.json')
        json_results_generator.write_json(self._filesystem, times_trie,
                                          times_json_path)

        # Save out the times data so we can use it for --fastest in the future.
        if running_all_tests:
            bot_test_times_path = self._port.bot_test_times_path()
            self._filesystem.maybe_make_directory(
                self._filesystem.dirname(bot_test_times_path))
            json_results_generator.write_json(self._filesystem, times_trie,
                                              bot_test_times_path)

        stats_trie = self._stats_trie(initial_results)
        stats_path = self._filesystem.join(self._artifacts_directory,
                                           'stats.json')
        self._filesystem.write_text_file(stats_path, json.dumps(stats_trie))

        full_results_path = self._filesystem.join(self._artifacts_directory,
                                                  'full_results.json')
        json_results_generator.write_json(
            self._filesystem, summarized_full_results, full_results_path)

        full_results_jsonp_path = self._filesystem.join(
            self._artifacts_directory, 'full_results_jsonp.js')
        json_results_generator.write_json(
            self._filesystem,
            summarized_full_results,
            full_results_jsonp_path,
            callback='ADD_FULL_RESULTS')
        failing_results_path = self._filesystem.join(self._artifacts_directory,
                                                     'failing_results.json')
        # We write failing_results.json out as jsonp because we need to load it
        # from a file url for results.html and Chromium doesn't allow that.
        json_results_generator.write_json(
            self._filesystem,
            summarized_failing_results,
            failing_results_path,
            callback='ADD_RESULTS')

        if self._options.json_test_results:
            json_results_generator.write_json(self._filesystem,
                                              summarized_full_results,
                                              self._options.json_test_results)
        if self._options.write_run_histories_to:
            json_results_generator.write_json(
                self._filesystem, run_histories,
                self._options.write_run_histories_to)

        _log.debug('Finished writing JSON files.')

    def _copy_results_html_file(self, destination_dir, filename):
        """Copies a file from the template directory to the results directory."""
        files_to_copy = [filename, filename + ".version"]
        template_dir = self._path_finder.path_from_blink_tools(
            'blinkpy', 'web_tests')
        for filename in files_to_copy:
            source_path = self._filesystem.join(template_dir, filename)
            destination_path = self._filesystem.join(destination_dir, filename)
            # Note that the results.html template file won't exist when
            # we're using a MockFileSystem during unit tests, so make sure
            # it exists before we try to copy it.
            if self._filesystem.exists(source_path):
                self._filesystem.copyfile(source_path, destination_path)

    def _stats_trie(self, initial_results):
        def _worker_number(worker_name):
            return int(worker_name.split('/')[1]) if worker_name else -1

        stats = {}
        for result in initial_results.results_by_name.values():
            if result.type != ResultType.Skip:
                stats[result.test_name] = {
                    'results': (_worker_number(result.worker_name),
                                result.test_number, result.pid,
                                int(result.test_run_time * 1000),
                                int(result.total_run_time * 1000))
                }
        stats_trie = {}
        for name, value in stats.items():
            json_results_generator.add_path_to_trie(name, value, stats_trie)
        return stats_trie
