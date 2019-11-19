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

import cPickle

from blinkpy.web_tests.models import test_failures, test_expectations

from blinkpy.common import path_finder

path_finder.add_typ_dir_to_sys_path()

from typ.artifacts import Artifacts

def build_test_result(
    driver_output, test_name, retry_attempt=0,
        failures=None, test_run_time=None, reftest_type=None,
        pid=None, references=None, device_failed=False, crash_site=None):
    failures = failures or []
    if not failures and driver_output.error:
        failures.append(test_failures.PassWithStderr(driver_output))
    return TestResult(
        test_name, retry_attempt=retry_attempt,
        failures=failures, test_run_time=test_run_time,
        reftest_type=reftest_type, pid=pid, references=references,
        device_failed=device_failed, crash_site=crash_site)


class TestResult(object):
    """Data object containing the results of a single test."""
    repeat_tests = True
    results_directory = ''
    filesystem = None

    @staticmethod
    def loads(string):
        return cPickle.loads(string)

    def __init__(self, test_name, retry_attempt=0,  failures=None,
                 test_run_time=None, reftest_type=None,
                 pid=None, references=None, device_failed=False, crash_site=None):
        self.test_name = test_name
        self.failures = failures or []
        self.test_run_time = test_run_time or 0  # The time taken to execute the test itself.
        self.has_stderr = any(failure.has_stderr for failure in self.failures)
        self.reftest_type = reftest_type or []
        self.pid = pid
        self.references = references or []
        self.device_failed = device_failed
        self.has_repaint_overlay = any(
            failure.has_repaint_overlay for failure in self.failures)
        self.crash_site = crash_site
        self.retry_attempt = retry_attempt

        results = set([f.result for f in self.failures] or [test_expectations.PASS])
        assert len(results) <= 2, (
            'single_test_runner.py incorrectly reported results %s for test %s' %
            (', '.join(results), test_name))
        if len(results) == 2:
            assert test_expectations.TIMEOUT in results and test_expectations.FAIL in results, (
                'The only combination of 2 results allowable is TIMEOUT and FAIL. '
                'Test %s reported the following results %s' % (test_name, ', '.join(results)))
            self.type = test_expectations.TIMEOUT
        else:
            # FIXME: Setting this in the constructor makes this class hard to mutate.
            self.type = results.pop()

        # These are set by the worker, not by the driver, so they are not passed to the constructor.
        self.worker_name = ''
        self.shard_name = ''
        self.total_run_time = 0  # The time taken to run the test plus any references, compute diffs, etc.
        self.test_number = None
        self.artifacts = Artifacts(
            self.results_directory, self.filesystem, retry_attempt,
            repeat_tests=self.repeat_tests)

    def create_artifacts(self):
        for failure in self.failures:
            failure.create_artifacts(self.artifacts)

    def __eq__(self, other):
        return (self.test_name == other.test_name and
                self.failures == other.failures and
                self.test_run_time == other.test_run_time and
                self.retry_attempt == other.retry_attempt and
                self.results_directory == other.results_directory)

    def __ne__(self, other):
        return not (self == other)

    def dumps(self):
        return cPickle.dumps(self)
