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

import collections
import enum
import logging
import time
from typing import Optional

from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


class InterruptReason(enum.Enum):
    TOO_MANY_FAILURES = enum.auto()
    EXTERNAL_SIGNAL = enum.auto()
    ALL_WORKERS_FAILED = enum.auto()


class TestRunException(Exception):
    def __init__(self, code, msg):
        self.code = code
        self.msg = msg


class TestRunResults(object):
    def __init__(self, expectations, num_tests, result_sink):
        self.total = num_tests
        self.remaining = self.total
        self.expectations = expectations
        self.result_sink = result_sink

        # Various counters:
        self.expected = 0
        self.expected_failures = 0
        self.expected_skips = 0
        self.total_failures = 0
        self.unexpected = 0
        self.unexpected_crashes = 0
        self.unexpected_failures = 0
        self.unexpected_timeouts = 0

        # The wall clock time spent running the tests (web_test_runner.run()).
        self.run_time = 0

        # Map of test name to the *last* result for the test.
        self.results_by_name = {}

        # Map of test name to the *last* unexpected result for the test.
        self.unexpected_results_by_name = {}

        # All results from a run except SKIP, including all iterations.
        self.all_results = []

        # Map of test name to the *last* failures for the test.
        self.failures_by_name = {}

        self.tests_by_expectation = {}
        for expected_result in \
            test_expectations.EXPECTATION_DESCRIPTIONS.keys():
            self.tests_by_expectation[expected_result] = set()

        self.slow_tests = set()
        self.interrupt_reason: Optional[InterruptReason] = None

    @property
    def interrupted(self) -> bool:
        return self.interrupt_reason is not None

    def add(self, test_result, expected, test_is_slow):
        result_type_for_stats = test_result.type
        self.tests_by_expectation[result_type_for_stats].add(
            test_result.test_name)
        if self.result_sink:
            self.result_sink.sink(expected, test_result, self.expectations)

        self.results_by_name[test_result.test_name] = test_result
        if test_result.type != ResultType.Skip:
            self.all_results.append(test_result)
        self.remaining -= 1
        if len(test_result.failures):
            self.total_failures += 1
            self.failures_by_name[test_result.test_name] = test_result.failures
        if expected:
            self.expected += 1
            if test_result.type == ResultType.Skip:
                self.expected_skips += 1
            elif test_result.type != ResultType.Pass:
                self.expected_failures += 1
        else:
            self.unexpected_results_by_name[test_result.test_name] = \
                test_result
            self.unexpected += 1
            if len(test_result.failures):
                self.unexpected_failures += 1
            if test_result.type == ResultType.Crash:
                self.unexpected_crashes += 1
            elif test_result.type == ResultType.Timeout:
                self.unexpected_timeouts += 1
        if test_is_slow:
            self.slow_tests.add(test_result.test_name)


class RunDetails(object):
    def __init__(self,
                 exit_code,
                 summarized_full_results=None,
                 summarized_failing_results=None,
                 initial_results=None,
                 all_retry_results=None):
        self.exit_code = exit_code
        self.summarized_full_results = summarized_full_results
        self.summarized_failing_results = summarized_failing_results
        self.initial_results = initial_results
        self.all_retry_results = all_retry_results or []


def _interpret_test_failures(failures):
    test_dict = {}
    failure_types = [type(failure) for failure in failures]
    # FIXME: get rid of all this is_* values once there is a 1:1 map between
    # TestFailure type and test_expectations.EXPECTATION.
    if test_failures.FailureMissingAudio in failure_types:
        test_dict['is_missing_audio'] = True

    if test_failures.FailureMissingResult in failure_types:
        test_dict['is_missing_text'] = True

    if (test_failures.FailureMissingImage in failure_types
            or test_failures.FailureMissingImageHash in failure_types
            or test_failures.FailureReftestNoImageGenerated in failure_types
            or test_failures.FailureReftestNoReferenceImageGenerated in
            failure_types):
        test_dict['is_missing_image'] = True

    if test_failures.FailureTestHarnessAssertion in failure_types:
        test_dict['is_testharness_test'] = True

    return test_dict


def summarize_results(port_obj,
                      options,
                      expectations,
                      initial_results,
                      all_retry_results,
                      only_include_failing=False):
    """Returns a dictionary containing a summary of the test runs, with the following fields:
        'version': a version indicator
        'fixable': The number of fixable tests (NOW - PASS)
        'skipped': The number of skipped tests (NOW & SKIPPED)
        'num_regressions': The number of non-flaky failures
        'num_flaky': The number of flaky failures
        'num_passes': The number of expected and unexpected passes
        'tests': a dict of tests -> {'expected': '...', 'actual': '...'}
    """
    results = {}
    results['version'] = 3
    all_retry_results = all_retry_results or []

    tbe = initial_results.tests_by_expectation

    results['skipped'] = len(tbe[ResultType.Skip])

    # TODO(dpranke): Some or all of these counters can be removed.
    num_passes = 0
    num_flaky = 0
    num_regressions = 0

    # Calculate the number of failures by types (only in initial results).
    num_failures_by_type = {}
    for expected_result in initial_results.tests_by_expectation:
        tests = initial_results.tests_by_expectation[expected_result]
        num_failures_by_type[expected_result] = len(tests)
    results['num_failures_by_type'] = num_failures_by_type

    # Combine all iterations and retries together into a dictionary with the
    # following structure:
    #    { test_name: [ (result, is_unexpected), ... ], ... }
    # where result is a single TestResult, is_unexpected is a boolean
    # representing whether the result is unexpected in that run.
    merged_results_by_name = collections.defaultdict(list)
    for test_run_results in [initial_results] + all_retry_results:
        # all_results does not include SKIP, so we need results_by_name.
        for test_name, result in test_run_results.results_by_name.items():
            if result.type == ResultType.Skip:
                is_unexpected = test_name in test_run_results.unexpected_results_by_name
                merged_results_by_name[test_name].append((result,
                                                          is_unexpected))

        # results_by_name only includes the last result, so we need all_results.
        for result in test_run_results.all_results:
            test_name = result.test_name
            is_unexpected = test_name in test_run_results.unexpected_results_by_name
            merged_results_by_name[test_name].append((result, is_unexpected))

    # Finally, compute the tests dict.
    tests = {}
    for test_name, merged_results in merged_results_by_name.items():
        initial_result = merged_results[0][0]

        if only_include_failing and initial_result.type == ResultType.Skip:
            continue
        exp = expectations.get_expectations(test_name)
        expected_results, bugs = exp.results, exp.reason
        expected = ' '.join(expected_results)
        actual = []
        actual_types = []
        crash_sites = []

        all_pass = True
        has_expected = False
        has_unexpected = False
        has_unexpected_pass = False
        has_stderr = False
        for result, is_unexpected in merged_results:
            actual.append(result.type)
            actual_types.append(result.type)
            crash_sites.append(result.crash_site)

            if result.type != ResultType.Pass:
                all_pass = False
            if result.has_stderr:
                has_stderr = True
            if is_unexpected:
                has_unexpected = True
                if result.type == ResultType.Pass:
                    has_unexpected_pass = True
            else:
                has_expected = True

        # TODO(crbug.com/855255): This code calls a test flaky if it has both
        # expected and unexpected runs (NOT pass and failure); this is generally
        # wrong (really it should just be if there are multiple kinds of results),
        # but this works in the normal case because a test will only be retried
        # if a result is unexpected, and if you get an expected result on the
        # retry, then you did get multiple results. This fails if you get
        # one kind of unexpected failure initially and another kind of
        # unexpected failure on the retry (e.g., TIMEOUT CRASH), or if you
        # explicitly run a test multiple times and get multiple expected results.
        is_flaky = has_expected and has_unexpected

        test_dict = {}
        test_dict['expected'] = expected
        test_dict['actual'] = ' '.join(actual)

        if hasattr(options, 'shard_index'):
            test_dict['shard'] = options.shard_index

        # If a flag was added then add flag specific test expectations to the per test field
        flag_exp = expectations.get_flag_expectations(test_name)
        if flag_exp:
            base_exp = expectations.get_base_expectations(test_name)
            test_dict['flag_expectations'] = list(flag_exp.results)
            test_dict['base_expectations'] = list(base_exp.results)

        # Fields below are optional. To avoid bloating the output results json
        # too much, only add them when they are True or non-empty.

        if is_flaky:
            num_flaky += 1
            test_dict['is_flaky'] = True
        elif all_pass or has_unexpected_pass:
            # We count two situations as a "pass":
            # 1. All test runs pass (which is obviously non-flaky, but does not
            #    imply whether the runs are expected, e.g. they can be all
            #    unexpected passes).
            # 2. The test isn't flaky and has at least one unexpected pass
            #    (which implies all runs are unexpected). One tricky example
            #    that doesn't satisfy #1 is that if a test is expected to
            #    crash but in fact fails and then passes, it will be counted
            #    as "pass".
            num_passes += 1
            if not has_stderr and only_include_failing:
                continue
        elif has_unexpected:
            # Either no retries or all retries failed unexpectedly.
            num_regressions += 1

        rounded_run_time = round(initial_result.test_run_time, 1)
        if rounded_run_time:
            test_dict['time'] = rounded_run_time

        if exp.is_slow_test:
            test_dict['is_slow_test'] = True

        if has_stderr:
            test_dict['has_stderr'] = True

        if bugs:
            test_dict['bugs'] = bugs.split()

        if initial_result.reftest_type:
            test_dict.update(reftest_type=list(initial_result.reftest_type))

        crash_sites = [site for site in crash_sites if site]
        if len(crash_sites) > 0:
            test_dict['crash_site'] = crash_sites[0]

        if test_failures.has_failure_type(test_failures.FailureTextMismatch,
                                          initial_result.failures):
            for failure in initial_result.failures:
                if isinstance(failure, test_failures.FailureTextMismatch):
                    test_dict['text_mismatch'] = \
                        failure.text_mismatch_category()
                    break

        for failure in initial_result.failures:
            if isinstance(failure, test_failures.FailureImageHashMismatch):
                test_dict['image_diff_stats'] = \
                    failure.actual_driver_output.image_diff_stats
                break

        # Note: is_unexpected and is_regression are intended to reflect the
        # *last* result. In the normal use case (stop retrying failures
        # once they pass), this is equivalent to saying that all of the
        # results were unexpected failures.
        last_result = actual_types[-1]
        if not expectations.matches_an_expected_result(test_name, last_result):
            test_dict['is_unexpected'] = True
            if last_result != ResultType.Pass:
                test_dict['is_regression'] = True

        if initial_result.has_repaint_overlay:
            test_dict['has_repaint_overlay'] = True

        test_dict.update(_interpret_test_failures(initial_result.failures))
        for retry_result, is_unexpected in merged_results[1:]:
            # TODO(robertma): Why do we only update unexpected retry failures?
            if is_unexpected:
                test_dict.update(
                    _interpret_test_failures(retry_result.failures))

        for test_result, _ in merged_results:
            for artifact_name, artifacts in \
                test_result.artifacts.artifacts.items():
                artifact_dict = test_dict.setdefault('artifacts', {})
                artifact_dict.setdefault(artifact_name, []).extend(artifacts)

        convert_to_hierarchical_view(tests, test_name, test_dict)

    results['tests'] = tests
    results['num_passes'] = num_passes
    results['num_flaky'] = num_flaky
    results['num_regressions'] = num_regressions
    # Does results.html have enough information to compute this itself? (by
    # checking total number of results vs. total number of tests?)
    results['interrupted'] = initial_results.interrupted
    results['layout_tests_dir'] = port_obj.web_tests_dir()
    results['seconds_since_epoch'] = int(time.time())

    if hasattr(options, 'build_number'):
        results['build_number'] = options.build_number
    if hasattr(options, 'builder_name'):
        results['builder_name'] = options.builder_name
    if getattr(options, 'order', None) == 'random' and hasattr(
            options, 'seed'):
        results['random_order_seed'] = options.seed
    results['path_delimiter'] = '/'

    # If there is a flag name then add the flag name field
    if expectations.flag_name:
        results['flag_name'] = expectations.flag_name

    # Don't do this by default since it takes >100ms.
    # It's only used for rebaselining and uploading data to the flakiness dashboard.
    results['chromium_revision'] = ''
    if getattr(options, 'builder_name', None):
        path = port_obj.repository_path()
        git = port_obj.host.git(path=path)
        if git:
            results['chromium_revision'] = str(git.commit_position(path))
        else:
            _log.warning(
                'Failed to determine chromium commit position for %s, '
                'leaving "chromium_revision" key blank in full_results.json.',
                path)

    return results


def convert_to_hierarchical_view(tests, test_name, test_dict):
    # Store test hierarchically by directory. e.g.
    # foo/bar/baz.html: test_dict
    # foo/bar/baz1.html: test_dict
    #
    # becomes
    # foo: {
    #     bar: {
    #         baz.html: test_dict,
    #         baz1.html: test_dict
    #     }
    # }
    parts = test_name.split('/')
    current_map = tests
    for i, part in enumerate(parts):
        if i == (len(parts) - 1):
            current_map[part] = test_dict
            break
        if part not in current_map:
            current_map[part] = {}
        current_map = current_map[part]


def _worker_number(worker_name):
    return int(worker_name.split('/')[1]) if worker_name else -1


def _test_result_as_dict(result, **kwargs):
    ret = {
        'test_name': result.test_name,
        'test_run_time': result.test_run_time,
        'has_stderr': result.has_stderr,
        'reftest_type': result.reftest_type[:],
        'pid': result.pid,
        'has_repaint_overlay': result.has_repaint_overlay,
        'crash_site': result.crash_site,
        'retry_attempt': result.retry_attempt,
        'type': result.type,
        'worker_name': result.worker_name,
        'worker_number': _worker_number(result.worker_name),
        'shard_name': result.shard_name,
        'start_time': result.start_time,
        'total_run_time': result.total_run_time,
        'test_number': result.test_number,
        **kwargs,
    }
    if result.failures:
        failures = []
        for failure in result.failures:
            try:
                failures.append({'message': failure.message()})
            except NotImplementedError:
                failures.append({'message': type(failure).__name__})
        ret['failures'] = failures
    if result.failure_reason:
        ret['failure_reason'] = {
            'primary_error_message':
            result.failure_reason.primary_error_message
        }
    for artifact_name, artifacts in result.artifacts.artifacts.items():
        artifact_dict = ret.setdefault('artifacts', {})
        artifact_dict.setdefault(artifact_name, []).extend(artifacts)
    return ret


def test_run_histories(options, expectations, initial_results,
                       all_retry_results):
    """Returns a dictionary containing a flattened list of all test runs, with
    the following fields:
        'version': a version indicator.
        'run_histories': a list of TestResult joined with expectations info.
        'random_order_seed': if order set to random and seed specified.

    Comparing to `summarized_results` this method dumps all the running
    histories for the tests (instead of `last_result`).
    """
    ret = {}
    ret['version'] = 1
    if getattr(options, 'order', None) == 'random' and hasattr(
            options, 'seed'):
        ret['random_order_seed'] = options.seed

    run_histories = []
    for test_run_results in [initial_results] + all_retry_results:
        # all_results does not include SKIP, so we need results_by_name.
        for test_name, result in test_run_results.results_by_name.items():
            if result.type != ResultType.Skip:
                continue
            exp = expectations.get_expectations(test_name)
            run_histories.append(
                _test_result_as_dict(result,
                                     expected_results=list(exp.results),
                                     bugs=exp.reason))

        # results_by_name only includes the last result, so we need all_results.
        for result in test_run_results.all_results:
            exp = expectations.get_expectations(result.test_name)
            run_histories.append(
                _test_result_as_dict(result,
                                     expected_results=list(exp.results),
                                     bugs=exp.reason))
    ret['run_histories'] = run_histories

    return ret
