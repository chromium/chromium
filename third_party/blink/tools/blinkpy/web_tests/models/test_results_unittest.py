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

import pickle
import unittest

from blinkpy.web_tests.models.test_results import TestResult
from blinkpy.web_tests.port.driver import DriverOutput
from blinkpy.web_tests.models import test_failures


class TestResultsTest(unittest.TestCase):
    def test_defaults(self):
        result = TestResult('foo')
        self.assertEqual(result.test_name, 'foo')
        self.assertEqual(result.failures, [])
        self.assertEqual(result.test_run_time, 0)

    def test_pickling(self):
        """Verify `pickle` can serialize and unserialize a test result.

        `multiprocessing` uses `pickle` to transport test results between
        processes over a queue.
        """
        result = TestResult(test_name='foo', failures=[], test_run_time=1.1)
        buf = pickle.dumps(result)
        unpickled_result = pickle.loads(buf)

        self.assertIsInstance(unpickled_result, TestResult)
        self.assertEqual(unpickled_result, result)
        # Logically the same as the previous assertion, but checks that the `!=`
        # operator is implemented.
        self.assertFalse(unpickled_result != result)

    def test_results_has_stderr(self):
        driver_output = DriverOutput(None, None, None, None, error=b'error')
        failures = [test_failures.FailureCrash(driver_output, None)]
        result = TestResult('foo', failures=failures)
        self.assertTrue(result.has_stderr)

    def test_results_has_repaint_overlay(self):
        text = '"invalidations": ['.encode('utf8')
        driver_output = DriverOutput(text, None, None, None)
        failures = [test_failures.FailureTextMismatch(driver_output, None)]
        result = TestResult('foo', failures=failures)
        self.assertTrue(result.has_repaint_overlay)

    def test_results_multiple(self):
        driver_output = DriverOutput(None, None, None, None)
        failure_crash = [test_failures.FailureCrash(driver_output, None),
                    test_failures.TestFailure(driver_output, None)]
        failure_timeout = [test_failures.FailureTimeout(driver_output, None),
                    test_failures.TestFailure(driver_output, None)]
        failure_early_exit = [test_failures.FailureEarlyExit(driver_output, None),
                    test_failures.TestFailure(driver_output, None)]
        # Should not raise an exception for CRASH and FAIL.
        TestResult('foo', failures=failure_crash)
        # Should not raise an exception for TIMEOUT and FAIL.
        TestResult('foo', failures=failure_timeout)
        with self.assertRaises(AssertionError):
            TestResult('foo', failures=failure_early_exit)
