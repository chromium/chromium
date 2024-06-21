# Copyright (C) 2011 Google Inc. All rights reserved.
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

import hashlib
import logging
import re
from typing import Optional

from blinkpy.web_tests.port.base import FuzzyRange
from blinkpy.web_tests.port.driver import DeviceFailure, DriverInput, DriverOutput
from blinkpy.web_tests.models import test_failures, testharness_results
from blinkpy.web_tests.models.test_results import TestResult, build_test_result

_log = logging.getLogger(__name__)


def run_single_test(port, options, results_directory, worker_name, driver,
                    test_input):
    runner = SingleTestRunner(port, options, results_directory, worker_name,
                              driver, test_input)
    try:
        test_result = runner.run()
        if not options.no_expectations:
            test_result.create_artifacts()
        return test_result
    except DeviceFailure as error:
        _log.error('device failed: %s', error)
        return TestResult(
            test_input.test_name, device_failed=True,
            failures=[test_failures.FailureEarlyExit()])


class SingleTestRunner(object):
    def __init__(self, port, options, results_directory, worker_name, driver,
                 test_input):
        self._port = port
        self._filesystem = port.host.filesystem
        self._options = options
        self._results_directory = results_directory
        self._driver = driver
        self._timeout_ms = test_input.timeout_ms
        self._worker_name = worker_name
        self._test_name = test_input.test_name
        self._reference_files = test_input.reference_files
        self._retry_attempt = test_input.retry_attempt

        test_failures.AbstractTestResultType.port = port
        test_failures.AbstractTestResultType.test_name = test_input.test_name
        test_failures.AbstractTestResultType.result_directory = results_directory
        test_failures.AbstractTestResultType.filesystem = self._filesystem
        TestResult.repeat_tests = (self._options.watch
                                   or self._options.repeat_each > 1
                                   or self._options.iterations > 1)
        TestResult.results_directory = self._results_directory

    def _expected_driver_output(self):
        return DriverOutput(
            self._port.expected_text(self._test_name),
            self._port.expected_image(self._test_name),
            self._port.expected_checksum(self._test_name),
            self._port.expected_audio(self._test_name))

    def _should_fetch_expected_checksum(self):
        return not self._options.reset_results

    def _driver_input(self):
        # The image hash is used to avoid doing an image dump if the
        # checksums match, so it should be set to a blank value if we
        # are generating a new baseline.  (Otherwise, an image from a
        # previous run will be copied into the baseline.)
        image_hash = None
        if self._should_fetch_expected_checksum():
            image_hash = self._port.expected_checksum(self._test_name)
            if image_hash:
                image_hash = image_hash.decode("ascii", "replace")

        args = self._port.args_for_test(self._test_name)
        test_name = self._port.name_for_test(self._test_name)
        wpt_print_mode = self._port.is_wpt_print_reftest(self._test_name)
        trace_file = self._port.trace_file_for_test(self._test_name)
        startup_trace_file = self._port.startup_trace_file_for_test(
            self._test_name)
        return DriverInput(test_name, self._timeout_ms, image_hash,
                           wpt_print_mode, trace_file, startup_trace_file,
                           args)

    def run(self):
        # WPT crash tests do not have baselines, so even when re-baselining we
        # run them as normal.
        if (self._options.enable_sanitizer
                or self._port.is_wpt_crash_test(self._test_name)):
            return self._run_crash_test()
        if self._options.reset_results or self._options.copy_baselines:
            return self._run_rebaseline()
        if self._reference_files:
            return self._run_reftest()
        return self._run_compare_test()

    def _run_crash_test(self):
        # running a crash test means that we ignore the actual test output and just look
        # for timeouts and crashes (real or forced by the driver). Most crashes should
        # indicate problems found by a sanitizer (ASAN, LSAN, etc.), but we will report
        # on other crashes and timeouts as well.
        driver_output = self._driver.run_test(self._driver_input())
        expected_driver_output = self._expected_driver_output()
        failures = self._handle_error(driver_output)
        return self._build_test_result(driver_output, failures)

    def _run_compare_test(self):
        """Runs the single test and returns test result."""
        driver_output = self._driver.run_test(self._driver_input())
        expected_driver_output = self._expected_driver_output()
        failures = self._compare_output(expected_driver_output, driver_output)
        return self._build_test_result(driver_output, failures)

    def _run_rebaseline(self):
        """Similar to _run_compare_test(), but has the side effect of updating
        or adding baselines. This is called when --reset-results and/or
        --copy-baselines are specified in the command line. If --reset-results,
        in the returned result we treat baseline mismatch as success."""
        driver_output = self._driver.run_test(self._driver_input())
        expected_driver_output = self._expected_driver_output()

        all_failures = self._compare_output(expected_driver_output,
                                            driver_output)

        if self._options.reset_results:
            # Only report leaks, timeouts and crashes and treat all other
            # failures as successes
            reported_failures = self._handle_error(driver_output)
        else:
            # Return comparison failures between the baseline and the actual
            # output as well as leaks, timeouts and crashes
            reported_failures = all_failures

        self._update_or_add_new_baselines(driver_output, all_failures)
        return self._build_test_result(driver_output, reported_failures)

    def _build_test_result(self, driver_output, failures, **kwargs):
        kwargs.setdefault('test_run_time', driver_output.test_time)
        kwargs.setdefault('pid', driver_output.pid)
        kwargs.setdefault('crash_site', driver_output.crash_site)
        return build_test_result(driver_output,
                                 self._test_name,
                                 retry_attempt=self._retry_attempt,
                                 failures=failures,
                                 typ_host=self._port.typ_host(),
                                 **kwargs)

    def _convert_to_str(self, data):
        if data:
            return data.decode('utf8', 'replace')
        else:
            return ''

    def _update_or_add_new_baselines(self, driver_output, failures):
        """Updates or adds new baselines for the test if necessary."""
        if (test_failures.has_failure_type(test_failures.FailureTimeout,
                                           failures)
                or test_failures.has_failure_type(test_failures.FailureCrash,
                                                  failures)):
            return
        # We usually don't want to create a new baseline if there isn't one
        # existing (which usually means this baseline isn't necessary, e.g.
        # an image-only test without text expectation files). However, in the
        # following cases, we do:
        # 1. The failure is MISSING; a baseline is apparently needed.
        # 2. A testharness.js test fails assertions: testharness.js tests
        #    without baselines are implicitly expected to pass all assertions;
        #    if there are failed assertions we need to create a new baseline.
        #    Note that the created baseline might be redundant, but users can
        #    optimize them later with optimize-baselines.
        if self._is_all_pass_testharness_text_not_needing_baseline(
                self._convert_to_str(driver_output.text)):
            driver_output.text = None
        self._save_baseline_data(
            driver_output.text, '.txt',
            test_failures.has_failure_type(test_failures.FailureMissingResult,
                                           failures)
            or test_failures.has_failure_type(
                test_failures.FailureTestHarnessAssertion, failures))
        self._save_baseline_data(
            driver_output.audio, '.wav',
            test_failures.has_failure_type(test_failures.FailureMissingAudio,
                                           failures))

        expected_png = driver_output.image
        if self._reference_files:
            _log.warning('Can not rebaseline the image baseline of reftest %s',
                         self._test_name)
            # Let _save_baseline_data remove the '-expected.png' if it exists.
            expected_png = None
        self._save_baseline_data(
            expected_png, '.png',
            test_failures.has_failure_type(test_failures.FailureMissingImage,
                                           failures))

    def _save_baseline_data(self, data, extension, force_create_new_baseline):
        port = self._port
        fs = self._filesystem

        # Do not create a new baseline unless we are specifically told so.
        current_expected_path = port.expected_filename(
            self._test_name, extension, return_default=False)
        if not current_expected_path and not force_create_new_baseline:
            return

        flag_specific_dir = port.baseline_flag_specific_dir()
        if flag_specific_dir:
            output_dir = fs.join(flag_specific_dir,
                                 fs.dirname(self._test_name))
        elif self._options.copy_baselines:
            output_dir = fs.join(port.baseline_version_dir(),
                                 fs.dirname(self._test_name))
        else:
            output_dir = fs.dirname(
                port.expected_filename(
                    self._test_name,
                    extension,
                    fallback_base_for_virtual=False))

        fs.maybe_make_directory(output_dir)
        output_basename = fs.basename(
            port.output_filename(self._test_name, port.BASELINE_SUFFIX,
                                 extension))
        output_path = fs.join(output_dir, output_basename)

        # Remove |output_path| if it exists and is not the generic expectation to
        # avoid extra baseline if the new baseline is the same as the fallback baseline.
        generic_dir = fs.join(
            port.web_tests_dir(),
            fs.dirname(
                port.lookup_virtual_test_base(self._test_name)
                or self._test_name))
        if (not data or output_dir != generic_dir) and fs.exists(output_path):
            _log.info('Removing the current baseline "%s"',
                      port.relative_test_filename(output_path))
            fs.remove(output_path)

        # Note that current_expected_path may change because of the above file removal.
        current_expected_path = port.expected_filename(
            self._test_name, extension, return_default=False)
        data = data or b''
        if (current_expected_path and fs.sha1(current_expected_path) ==
                hashlib.sha1(data).hexdigest()):
            if self._options.reset_results:
                _log.info(
                    'Not writing new baseline "%s" because it is the same as the current baseline',
                    port.relative_test_filename(output_path))
            else:
                _log.info(
                    'Not copying baseline to "%s" because the actual result is the same as the current baseline',
                    port.relative_test_filename(output_path))
            return

        if not data and not current_expected_path:
            _log.info(
                'Not creating new baseline because the test does not need it')
            return
        # If the data is empty and the fallback exists, we'll continue to create
        # an empty baseline file to override the fallback baseline.

        if self._options.reset_results:
            _log.info('Writing new baseline "%s"',
                      port.relative_test_filename(output_path))
            port.update_baseline(output_path, data)
        else:
            _log.info('Copying baseline to "%s"',
                      port.relative_test_filename(output_path))
            if fs.exists(current_expected_path):
                fs.copyfile(current_expected_path, output_path)
            else:
                _log.error(
                    'Could not copy baseline to "%s" from "%s" because the source file does not exist',
                    port.relative_test_filename(output_path),
                    current_expected_path)

    def _handle_error(self, driver_output, reference_filename=None):
        """Returns test failures if some unusual errors happen in driver's run.

        Args:
          driver_output: The output from the driver.
          reference_filename: The full path to the reference file which produced
               the driver_output. This arg is optional and should be used only in
               reftests until we have a better way to know which html file
               is used for producing the driver_output.
        """
        failures = []
        if driver_output.timeout:
            failures.append(
                test_failures.FailureTimeout(driver_output,
                                             bool(reference_filename)))

        if reference_filename:
            testname = self._port.relative_test_filename(reference_filename)
        else:
            testname = self._test_name

        if driver_output.crash:
            failures.append(
                test_failures.FailureCrash(
                    driver_output,
                    is_reftest=bool(reference_filename),
                    process_name=driver_output.crashed_process_name,
                    pid=driver_output.crashed_pid,
                    has_log=self._port.output_contains_sanitizer_messages(
                        driver_output.crash_log)))
            if driver_output.error:
                _log.debug('%s %s crashed, (stderr lines):', self._worker_name,
                           testname)
            else:
                _log.debug('%s %s crashed, (no stderr)', self._worker_name,
                           testname)
        elif driver_output.leak:
            failures.append(
                test_failures.FailureLeak(driver_output,
                                          bool(reference_filename)))
            _log.debug('%s %s leaked', self._worker_name, testname)
        elif driver_output.error:
            _log.debug('%s %s output stderr lines:', self._worker_name,
                       testname)
        for line in driver_output.error.splitlines():
            _log.debug('  %s', line)
        return failures

    def _compare_output(self, expected_driver_output, driver_output):
        failures = []
        failures.extend(self._handle_error(driver_output))

        if driver_output.crash:
            # Don't continue any more if we already have a crash.
            # In case of timeouts, we continue since we still want
            # to see the text and image output.
            return failures

        failures.extend(
            self._check_extra_and_missing_baselines(expected_driver_output,
                                                    driver_output))

        testharness_completed, testharness_failures = self._compare_testharness_test(
            expected_driver_output, driver_output)
        if testharness_completed:
            failures.extend(testharness_failures)
        else:
            failures.extend(
                self._compare_text(expected_driver_output, driver_output))
            failures.extend(
                self._compare_image(expected_driver_output, driver_output))
            failures.extend(
                self._compare_audio(expected_driver_output, driver_output))
        return failures

    def _report_extra_baseline(self, driver_output, extension, message):
        """If the baseline file exists, logs an error and returns True."""
        if driver_output.crash or driver_output.timeout:
            return False
        # If the baseline overrides a fallback one, we need the empty file to
        # match the empty result.
        if self._port.fallback_expected_filename(self._test_name, extension):
            return False

        expected_file = self._port.expected_filename(
            self._test_name, extension, return_default=False)
        if expected_file:
            _log.error(
                '%s %s, but has an extra baseline file. Please remove %s' %
                (self._test_name, message, expected_file))
            return True
        return False

    def _is_all_pass_testharness_text_not_needing_baseline(self, text_result):
        return (
            text_result
            and testharness_results.is_all_pass_test_result(text_result) and
            # An all-pass testharness test doesn't need the test baseline unless
            # if it is overriding a fallback one.
            not self._port.fallback_expected_filename(self._test_name, '.txt'))

    def _check_extra_and_missing_baselines(self, expected_driver_output,
                                           driver_output):
        failures = []

        if driver_output.text:
            if self._is_all_pass_testharness_text_not_needing_baseline(
                    self._convert_to_str(driver_output.text)):
                if self._report_extra_baseline(
                        driver_output, '.txt',
                        'is an all-pass testharness test'):
                    # TODO(wangxianzhu): Make this a failure.
                    pass
            elif testharness_results.is_testharness_output(
                    self._convert_to_str(driver_output.text)):
                # We only need -expected.txt for a testharness test when we
                # expect it to fail or produce additional console output (when
                # -expected.txt is optional), so don't report missing
                # -expected.txt for testharness tests.
                pass
            elif not expected_driver_output.text:
                failures.append(
                    test_failures.FailureMissingResult(driver_output,
                                                       expected_driver_output))
        elif self._report_extra_baseline(driver_output, '.txt',
                                         'does not produce text result'):
            failures.append(
                test_failures.FailureTextNotGenerated(driver_output,
                                                      expected_driver_output))

        if driver_output.image_hash:
            if self._reference_files:
                if self._report_extra_baseline(driver_output, '.png',
                                               'is a reftest'):
                    # TODO(wangxianzhu): Make this a failure.
                    pass
            else:
                if not expected_driver_output.image:
                    failures.append(
                        test_failures.FailureMissingImage(
                            driver_output, expected_driver_output))
                elif not expected_driver_output.image_hash:
                    failures.append(
                        test_failures.FailureMissingImageHash(
                            driver_output, expected_driver_output))
        elif self._report_extra_baseline(driver_output, '.png',
                                         'does not produce image result'):
            failures.append(
                test_failures.FailureImageHashNotGenerated(
                    driver_output, expected_driver_output))

        if driver_output.audio:
            if not expected_driver_output.audio:
                failures.append(
                    test_failures.FailureMissingAudio(driver_output,
                                                      expected_driver_output))
        elif self._report_extra_baseline(driver_output, '.wav',
                                         'does not produce audio result'):
            failures.append(
                test_failures.FailureAudioNotGenerated(driver_output,
                                                       expected_driver_output))

        return failures

    def _compare_testharness_test(self, expected_driver_output, driver_output):
        """Returns (testharness_completed, testharness_failures)."""
        if not driver_output.text:
            return False, []
        if not testharness_results.is_testharness_output(
                self._convert_to_str(driver_output.text)):
            return False, []
        if (self._options.ignore_testharness_expected_txt
                # Skip the text comparison when expecting an abbreviated
                # all-pass result.
                or self._convert_to_str(expected_driver_output.text)
                == testharness_results.ABBREVIATED_ALL_PASS):
            expected_driver_output.text = b''
        elif expected_driver_output.text:
            # Will compare text if there is expected text that is not all-pass
            # (e.g., has "interesting output").
            return False, []
        if not testharness_results.is_test_output_passing(
                self._convert_to_str(driver_output.text)):
            return True, [
                test_failures.FailureTestHarnessAssertion(
                    driver_output, expected_driver_output)
            ]
        return True, []

    def _compare_text(self, expected_driver_output, driver_output):

        if expected_driver_output.text:
            driver_output.test_type.add('text')
        if not expected_driver_output.text or not driver_output.text:
            return []

        expected_text = expected_driver_output.text.decode('utf8', 'replace')
        actual_text = driver_output.text.decode('utf8', 'replace')

        normalized_actual_text = self._get_normalized_output_text(actual_text)
        # Assuming expected_text is already normalized.
        if not self._port.do_text_results_differ(expected_text,
                                                 normalized_actual_text):
            return []

        # Determine the text mismatch type

        def remove_chars(text, chars):
            for char in chars:
                text = text.replace(char, '')
            return text

        # General text mismatch
        if self._port.do_text_results_differ(
                remove_chars(expected_text, ' \t\n'),
                remove_chars(normalized_actual_text, ' \t\n')):
            return [
                test_failures.FailureTextMismatch(driver_output,
                                                  expected_driver_output)
            ]

        # Space-only mismatch
        if not self._port.do_text_results_differ(
                remove_chars(expected_text, ' \t'),
                remove_chars(normalized_actual_text, ' \t')):
            return [
                test_failures.FailureSpacesAndTabsTextMismatch(
                    driver_output, expected_driver_output)
            ]

        # Newline-only mismatch
        if not self._port.do_text_results_differ(
                remove_chars(expected_text, '\n'),
                remove_chars(normalized_actual_text, '\n')):
            return [
                test_failures.FailureLineBreaksTextMismatch(
                    driver_output, expected_driver_output)
            ]

        # Spaces and newlines
        return [
            test_failures.FailureSpaceTabLineBreakTextMismatch(
                driver_output, expected_driver_output)
        ]

    def _compare_audio(self, expected_driver_output, driver_output):
        if expected_driver_output.audio:
            driver_output.test_type.add('audio')
        if not expected_driver_output.audio or not driver_output.audio:
            return []
        if self._port.do_audio_results_differ(expected_driver_output.audio,
                                              driver_output.audio):
            return [
                test_failures.FailureAudioMismatch(driver_output,
                                                   expected_driver_output)
            ]
        return []

    def _get_normalized_output_text(self, output):
        """Returns the normalized text output, i.e. the output in which
        the end-of-line characters are normalized to "\n".
        """
        # Running tests on Windows produces "\r\n".  The "\n" part is helpfully
        # changed to "\r\n" by our system (Python/Cygwin), resulting in
        # "\r\r\n", when, in fact, we wanted to compare the text output with
        # the normalized text expectation files.
        return output.replace('\r\r\n', '\r\n').replace('\r\n', '\n')

    def _compare_image(self, expected_driver_output, driver_output):
        if expected_driver_output.image and expected_driver_output.image_hash:
            driver_output.test_type.add('image')
        if not expected_driver_output.image or not expected_driver_output.image_hash:
            return []
        # The presence of an expected image, but a lack of an outputted image
        # does not signify an error. content::WebTestControlHost checks the
        # image_hash, and upon a match simply skips recording the outputted
        # image. This even occurs when results_directory is set.
        if not driver_output.image or not driver_output.image_hash:
            return []

        max_channel_diff, max_pixels_diff = self._port.get_wpt_fuzzy_metadata(
            self._test_name)
        if (driver_output.image_hash != expected_driver_output.image_hash or
                not _allows_exact_matches(max_channel_diff, max_pixels_diff)):
            diff, stats, err_str = self._port.diff_image(
                expected_driver_output.image,
                driver_output.image,
                max_channel_diff=max_channel_diff,
                max_pixels_diff=max_pixels_diff)

            if diff:
                driver_output.image_diff = diff
            if stats:
                driver_output.image_diff_stats = stats

            if err_str:
                _log.error('  %s : %s', self._test_name, err_str)
                driver_output.error = (driver_output.error
                                       or b'') + err_str.encode(
                                           'utf8', 'replace')

            if diff or err_str:
                return [
                    test_failures.FailureImageHashMismatch(
                        driver_output, expected_driver_output)
                ]
            elif not max_pixels_diff:
                # See https://bugs.webkit.org/show_bug.cgi?id=69444 for why this
                # isn't a full failure.
                _log.warning('  %s -> pixel hash failed (but diff passed)',
                             self._test_name)

        return []

    def _run_reftest(self):
        test_output = self._driver.run_test(self._driver_input())
        total_test_time = test_output.test_time

        expected_text = self._port.expected_text(self._test_name)
        expected_text_output = DriverOutput(
            text=expected_text, image=None, image_hash=None, audio=None)
        # This _compare_output compares text if expected text exists, ignores
        # image, checks for extra baselines, and generates crash or timeout
        # failures if needed.
        compare_text_failures = self._compare_output(expected_text_output,
                                                     test_output)
        # If the test crashed, or timed out,  or a leak was detected, there's no point
        # in running the reference at all. This can save a lot of execution time if we
        # have a lot of crashes or timeouts.
        if test_output.crash or test_output.timeout or test_output.leak:
            return self._build_test_result(test_output, compare_text_failures)

        # A reftest can have multiple match references and multiple mismatch references;
        # the test fails if any mismatch matches and all of the matches don't match.
        # To minimize the number of references we have to check, we run all of the
        # mismatches first, then the matches, and short-circuit out as soon as we can.
        # Note that sorting by the expectation sorts "!=" before "==" so this is easy
        # to do.
        expected_output = None
        reference_test_names = []
        reftest_failures = []
        wpt_print_mode = self._port.is_wpt_print_reftest(self._test_name)
        args = self._port.args_for_test(self._test_name)
        # sort self._reference_files to put mismatch tests first
        for expectation, reference_filename in sorted(self._reference_files):
            if reference_filename.startswith('about:'):
                reference_test_name = reference_filename
            else:
                reference_test_name = self._port.relative_test_filename(
                    reference_filename)
            reference_test_names.append(reference_test_name)
            driver_input = DriverInput(reference_test_name,
                                       self._timeout_ms,
                                       image_hash=test_output.image_hash,
                                       wpt_print_mode=wpt_print_mode,
                                       trace_file=None,
                                       startup_trace_file=None,
                                       args=args)
            expected_output = self._driver.run_test(driver_input)
            total_test_time += expected_output.test_time
            reftest_failures = self._compare_output_with_reference(
                expected_output, test_output, reference_filename,
                expectation == '!=')

            if ((expectation == '!=' and reftest_failures)
                    or (expectation == '==' and not reftest_failures)):
                break

        assert expected_output
        test_output.test_type.add('image')

        # Combine compare_text_result and test_result
        expected_output.text = expected_text_output.text
        failures = reftest_failures + compare_text_failures

        # FIXME: We don't really deal with a mix of reftest types properly. We
        # pass in a set() to reftest_type and only really handle the first of
        # the references in the result.
        reftest_type = list(
            set([
                reference_file[0] for reference_file in self._reference_files
            ]))

        return self._build_test_result(test_output,
                                       failures,
                                       test_run_time=total_test_time,
                                       reftest_type=reftest_type,
                                       references=reference_test_names)

    def _compare_output_with_reference(self, reference_driver_output,
                                       actual_driver_output,
                                       reference_filename, mismatch):
        failures = []
        # Don't continue any more if we already have crash, timeout, or leak.
        # The caller should report `actual_driver_output` errors.
        if (actual_driver_output.crash or actual_driver_output.timeout
                or actual_driver_output.leak):
            return failures
        failures.extend(
            self._handle_error(
                reference_driver_output,
                reference_filename=reference_filename))
        if failures:
            return failures

        max_channel_diff, max_pixels_diff = self._port.get_wpt_fuzzy_metadata(
            self._test_name)
        if not actual_driver_output.image_hash:
            failures.append(
                test_failures.FailureReftestNoImageGenerated(
                    actual_driver_output, reference_driver_output,
                    reference_filename))
        elif not reference_driver_output.image_hash:
            failures.append(
                test_failures.FailureReftestNoReferenceImageGenerated(
                    actual_driver_output, reference_driver_output,
                    reference_filename))
        elif mismatch:
            if reference_driver_output.image_hash == actual_driver_output.image_hash:
                failures.append(
                    test_failures.FailureReftestMismatchDidNotOccur(
                        actual_driver_output, reference_driver_output,
                        reference_filename))
        elif (reference_driver_output.image_hash !=
              actual_driver_output.image_hash
              or not _allows_exact_matches(max_channel_diff, max_pixels_diff)):
            # When either fuzzy parameter has a nonzero minimum, a diff is
            # expected. Always diff the images in that case, even if the image
            # hashes match.
            diff, stats, err_str = self._port.diff_image(
                reference_driver_output.image,
                actual_driver_output.image,
                max_channel_diff=max_channel_diff,
                max_pixels_diff=max_pixels_diff)
            if diff:
                actual_driver_output.image_diff = diff
            if stats:
                actual_driver_output.image_diff_stats = stats

            if err_str:
                _log.error(err_str)
                actual_driver_output.error = (actual_driver_output.error
                                              or b'') + err_str.encode(
                                                  'utf8', 'replace')

            if diff or err_str:
                failures.append(
                    test_failures.FailureReftestMismatch(
                        actual_driver_output, reference_driver_output,
                        reference_filename))
            elif err_str:
                # TODO(weizhong) Should we include this error message in the
                # artifacts ?
                _log.error('  %s : %s', self._test_name, err_str)
            elif not max_pixels_diff:
                _log.warning(
                    "  %s -> ref test hashes didn't match but diff passed",
                    self._test_name)

        return failures


def _allows_exact_matches(max_channel_diff: Optional[FuzzyRange],
                          max_pixels_diff: Optional[FuzzyRange]) -> bool:
    return (not max_channel_diff or max_channel_diff[0] == 0
            or not max_pixels_diff or max_pixels_diff[0] == 0)
