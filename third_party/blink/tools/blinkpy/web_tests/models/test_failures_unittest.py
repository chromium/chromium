# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.models.typ_types import Artifacts
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.port.driver import DriverOutput

from blinkpy.web_tests.models.test_failures import (ALL_FAILURE_CLASSES,
                                                    PassWithStderr,
                                                    FailureCrash,
                                                    FailureTimeout,
                                                    TestFailure, FailureText)


class TestFailuresTest(unittest.TestCase):
    def setUp(self):
        self._actual_output = DriverOutput(
            text=None, image=None, image_hash=None, audio=None)
        self._expected_output = DriverOutput(
            text=None, image=None, image_hash=None, audio=None)

    def assert_loads(self, cls):
        failure_obj = cls(self._actual_output, self._expected_output)
        s = failure_obj.dumps()
        new_failure_obj = TestFailure.loads(s)
        self.assertIsInstance(new_failure_obj, cls)

        self.assertEqual(failure_obj, new_failure_obj)

        # Also test that != is implemented.
        self.assertFalse(failure_obj != new_failure_obj)

    def test_message_is_virtual(self):
        failure_obj = TestFailure(self._actual_output, self._expected_output)
        with self.assertRaises(NotImplementedError):
            failure_obj.message()

    def test_loads(self):
        for c in ALL_FAILURE_CLASSES:
            self.assert_loads(c)

    def test_equals(self):
        self.assertEqual(FailureCrash(self._actual_output),
                         FailureCrash(self._actual_output))
        self.assertNotEqual(FailureCrash(self._actual_output),
                            FailureTimeout(self._actual_output))
        crash_set = set([
            FailureCrash(self._actual_output),
            FailureCrash(self._actual_output)
        ])
        self.assertEqual(len(crash_set), 1)
        # The hash happens to be the name of the class, but sets still work:
        crash_set = set([FailureCrash(self._actual_output), 'FailureCrash'])
        self.assertEqual(len(crash_set), 2)

    def test_crashes(self):
        self.assertEqual(
            FailureCrash(self._actual_output).message(),
            'content_shell crashed')
        self.assertEqual(
            FailureCrash(
                self._actual_output,
                process_name='foo',
                pid=1234).message(), 'foo crashed [pid=1234]')

    def test_repeated_test_artifacts(self):
        host = MockSystemHost()
        port = Port(host, 'baseport')
        artifacts = Artifacts('/dir', host.filesystem, repeat_tests=True)

        def init_test_failure(test_failure):
            test_failure.port = port
            test_failure.filesystem = host.filesystem
            test_failure.test_name = 'foo.html'
            test_failure.result_directory = '/dir'

        pass_with_stderr = PassWithStderr(
            DriverOutput(None, None, None, None, error=b'pass with stderr'))
        init_test_failure(pass_with_stderr)
        crash = FailureCrash(
            DriverOutput(None,
                         None,
                         None,
                         None,
                         crash=True,
                         error=b'crash stderr'))
        init_test_failure(crash)
        timeout = FailureTimeout(
            DriverOutput(None, None, None, None, error=b'timeout with stderr'))
        init_test_failure(timeout)

        pass_with_stderr.create_artifacts(artifacts)
        self.assertEqual('pass with stderr',
                         host.filesystem.read_text_file('/dir/foo-stderr.txt'))

        crash.create_artifacts(artifacts)
        self.assertEqual('crash stderr',
                         host.filesystem.read_text_file('/dir/foo-stderr.txt'))

        timeout.create_artifacts(artifacts)
        self.assertEqual('timeout with stderr',
                         host.filesystem.read_text_file('/dir/foo-stderr.txt'))

        pass_with_stderr.create_artifacts(artifacts)
        self.assertEqual('timeout with stderr',
                         host.filesystem.read_text_file('/dir/foo-stderr.txt'))

    def test_failure_reason_crash(self):
        # stderr tell us the cause of the crash.
        error_log = """[722:259:ERROR:other_file.cc(123)] Unrelated message.
[722:259:FATAL:multiplex_router.cc(181)] Check failed: !client_.
#0 0x55b31e3271d9 base::debug::CollectStackTrace()
"""
        self._actual_output.error = error_log.encode('utf8')

        failure = FailureCrash(self._actual_output)
        failure_reason = failure.failure_reason()

        self.assertIsNotNone(failure_reason)
        self.assertEqual(failure_reason.primary_error_message,
                         'multiplex_router.cc(181): Check failed: !client_.')

    def test_failure_reason_crash_none(self):
        # stderr does not tell us the cause of the crash.
        error_log = """[722:259:ERROR:other_file.cc(123)] Unrelated message.
722:259:ERROR:other_file.cc(123)] Unrelated message 2.
"""

        self._actual_output.error = error_log.encode('utf8')

        failure_text = FailureCrash(self._actual_output)
        failure_reason = failure_text.failure_reason()

        self.assertIsNone(failure_reason)

    def test_failure_reason_testharness_js(self):
        expected_text = ''
        actual_text = """Content-Type: text/plain
This is a testharness.js-based test.
FAIL Tests that the document gets overscroll event with right deltaX/Y attributes. promise_test: Unhandled rejection with value: "Document did not receive scrollend event."
Harness: the test ran to completion."""

        self._actual_output.text = actual_text.encode('utf8')
        self._expected_output.text = expected_text.encode('utf8')

        failure_text = FailureText(self._actual_output, self._expected_output)
        failure_reason = failure_text.failure_reason()
        self.assertIsNotNone(failure_reason)
        self.assertEqual(
            failure_reason.primary_error_message,
            'Tests that the document gets overscroll event with right'
            ' deltaX/Y attributes. promise_test: Unhandled rejection with'
            ' value: "Document did not receive scrollend event."')

    def test_failure_reason_text_diff(self):
        expected_text = """retained line 1
deleted line 1
deleted line 2
retained line 2
"""

        actual_text = """retained line 1
new line 1
retained line 2
new line 2
"""

        self._actual_output.text = actual_text.encode('utf8')
        self._expected_output.text = expected_text.encode('utf8')

        failure_text = FailureText(self._actual_output, self._expected_output)
        failure_reason = failure_text.failure_reason()
        self.assertIsNotNone(failure_reason)
        self.assertEqual(
            failure_reason.primary_error_message,
            'Unexpected Diff (+got, -want):\n'
            '+new line 1\n'
            '-deleted line 1\n'
            '-deleted line 2')

    def test_failure_reason_empty_text_diff(self):
        # Construct a scenario in which the difference between the actual
        # and expected text does not provide a useful failure reason.
        expected_text = ''
        actual_text = '\n'

        self._actual_output.text = actual_text.encode('utf8')
        self._expected_output.text = expected_text.encode('utf8')

        failure_text = FailureText(self._actual_output, self._expected_output)
        failure_reason = failure_text.failure_reason()
        self.assertIsNone(failure_reason)
