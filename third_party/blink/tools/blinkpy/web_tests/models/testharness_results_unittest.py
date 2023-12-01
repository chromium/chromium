# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import textwrap
import unittest

from blinkpy.web_tests.models import testharness_results
from blinkpy.web_tests.models.testharness_results import (
    TestharnessLine,
    LineType,
    Status,
)


class TestResultCheckerTest(unittest.TestCase):
    def test_is_all_pass_test_result_positive_cases(self):
        self.assertTrue(
            testharness_results.is_all_pass_test_result(
                'This is a testharness.js-based test.\n'
                '[PASS] foo bar \n'
                'Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_all_pass_test_result(
                'This is a wdspec test.\n'
                '[PASS] foo bar \n'
                'Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_all_pass_test_result(
                'This is a testharness.js-based test.\n'
                '[PASS] \'grid\' with: grid-template-areas: "a b"\n'
                '"c d";\n'
                'Harness: the test ran to completion.\n'))

    def test_is_all_pass_test_result_negative_cases(self):
        self.assertFalse(
            testharness_results.is_all_pass_test_result(
                'This is a testharness.js-based test.\n'
                'CONSOLE WARNING: This is a warning.\n'
                'Test ran to completion.'))
        self.assertFalse(
            testharness_results.is_all_pass_test_result(
                'This is a testharness.js-based test.\n'
                '[PASS] foo bar \n'
                '[FAIL]  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_all_pass_test_result(
                'This is a testharness.js-based test.\n'
                'Harness Error. harness_status.status = 1\n'
                '[PASS] foo bar\n'
                'Harness: the test ran to completion.'))

    def test_is_testharness_output_positive_cases(self):
        self.assertTrue(
            testharness_results.is_testharness_output(
                'This is a testharness.js-based test.\n'
                'Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_testharness_output(
                '\n'
                ' \r This is a testharness.js-based test. \n'
                ' \r  \n'
                ' \rHarness: the test ran to completion.   \n'
                '\n'))
        self.assertTrue(
            testharness_results.is_testharness_output(
                'This is a testharness.js-based test.\n'
                'Foo bar \n'
                ' Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_testharness_output(
                'This is a testharness.js-based test.\n'
                '[FAIL] bah \n'
                ' Harness: the test ran to completion.\n'
                '\n'
                '\n'))

    def test_is_testharness_output_negative_cases(self):
        self.assertFalse(testharness_results.is_testharness_output('foo'))
        self.assertFalse(testharness_results.is_testharness_output(''))
        self.assertFalse(testharness_results.is_testharness_output('   '))
        self.assertFalse(
            testharness_results.is_testharness_output(
                'This is a testharness.js-based test.  Harness: the test ran to completion.'
            ))
        self.assertFalse(
            testharness_results.is_testharness_output(
                '   This    \n'
                'is a testharness.js-based test.\n'
                'Harness: the test ran to completion.'))

    def test_is_test_output_passing_empty_content(self):
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '   Harness: the test ran to completion.'))

    def test_is_test_output_passing_with_pass_and_random_text(self):
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'RANDOM TEXT.\n'
                'This is a testharness.js-based test.\n'
                '[PASS] things are fine.\n'
                ' Harness: the test ran to completion.\n'
                '\n'))

    def test_is_test_output_passing_basic_examples(self):
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '[PASS] foo bar \n'
                'Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '[PASS] foo bar FAIL  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '[PASS] foo bar \n'
                '[FAIL]  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '[FAIL] bah \n'
                'Harness: the test ran to completion.'))

    def test_is_test_output_passing_with_console_messages(self):
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                ' CONSOLE ERROR: BLAH  \n'
                ' Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                ' CONSOLE WARNING: BLAH  \n'
                '[PASS] some passing method\n'
                'Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'CONSOLE LOG: error.\n'
                'This is a testharness.js-based test.\n'
                '[PASS] things are fine.\n'
                'Harness: the test ran to completion.\n'
                '\n'))
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'CONSOLE ERROR: error.\n'
                'This is a testharness.js-based test.\n'
                '[PASS] things are fine.\n'
                'Harness: the test ran to completion.\n'
                '\n'))
        self.assertTrue(
            testharness_results.is_test_output_passing(
                'CONSOLE WARNING: error.\n'
                'This is a testharness.js-based test.\n'
                '[PASS] things are fine.\n'
                'Harness: the test ran to completion.\n'
                '\n'))

    def test_is_test_output_passing_with_timeout_or_notrun(self):
        self.assertFalse(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '[TIMEOUT] bah \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_test_output_passing(
                'This is a testharness.js-based test.\n'
                '[NOTRUN] bah \n'
                ' Harness: the test ran to completion.'))

    def test_has_other_useful_output_positive_cases(self):
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'This is a testharness.js-based test.\n'
                'CONSOLE ERROR: This is an error.\n'
                'Test ran to completion.'))
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'This is a testharness.js-based test.\n'
                'CONSOLE WARNING: This is a warning.\n'
                'Test ran to completion.'))
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'CONSOLE ERROR: This is an error.\n'
                'Test ran to completion.'))
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'CONSOLE WARNING: This is a warning.\n'
                'Test ran to completion.'))
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'This is a testharness.js-based test.\n'
                'CONSOLE ERROR: This is an error.'))
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'CONSOLE ERROR: This is an error.'))
        self.assertTrue(
            testharness_results.has_other_useful_output(
                'CONSOLE WARNING: This is a warning.'))
        self.assertTrue(
            testharness_results.has_other_useful_output('ALERT: alert!'))
        self.assertTrue(
            testharness_results.has_other_useful_output('CONFIRM: confirm?'))
        self.assertTrue(
            testharness_results.has_other_useful_output('PROMPT: prompt.'))

    def test_has_other_useful_output_negative_cases(self):
        self.assertFalse(
            testharness_results.has_other_useful_output(
                'This is a testharness.js-based test.\n'
                'CONSOLE MESSAGE: This is not error.'))
        self.assertFalse(
            testharness_results.has_other_useful_output(
                'This is a testharness.js-based test.\n'
                'No errors here.'))
        self.assertFalse(
            testharness_results.has_other_useful_output(
                'This is not a CONSOLE ERROR, sorry.'))
        self.assertFalse(
            testharness_results.has_other_useful_output(
                'This is not a CONSOLE WARNING, sorry.'))
        self.assertFalse(
            testharness_results.has_other_useful_output('Not an ALERT'))
        self.assertFalse(
            testharness_results.has_other_useful_output('Not a CONFRIM'))
        self.assertFalse(
            testharness_results.has_other_useful_output('Not a PROMPT'))

    def test_parse_testharness_baseline(self):
        results = testharness_results.parse_testharness_baseline(
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 1 , harness_status.message = ReferenceError: ShadowRealm is not defined
                [PASS] \t Query "geolocation" permission
                [ FAIL  TIMEOUT ] Window interface: attribute\\n\\0\\r\\\\nevent
                  assert_true: property should be enumerable\\n\\0\\r\\\\n  expected true got false

                CONSOLE ERROR: Console error
                Harness: the test ran to completion.
                """))
        self.assertEqual(len(results), 6)

        self.assertIs(results[0].line_type, LineType.TESTHARNESS_HEADER)
        self.assertEqual(results[0].statuses, frozenset())
        self.assertIsNone(results[0].subtest)
        self.assertIsNone(results[0].message)

        self.assertIs(results[1].line_type, LineType.HARNESS_ERROR)
        self.assertEqual(results[1].statuses, {Status.ERROR})
        self.assertIsNone(results[1].subtest)
        self.assertEqual(results[1].message,
                         'ReferenceError: ShadowRealm is not defined')

        self.assertIs(results[2].line_type, LineType.SUBTEST)
        self.assertEqual(results[2].statuses, {Status.PASS})
        self.assertEqual(results[2].subtest,
                         '\t Query "geolocation" permission')
        self.assertIsNone(results[2].message)

        self.assertIs(results[3].line_type, LineType.SUBTEST)
        self.assertEqual(results[3].statuses, {Status.FAIL, Status.TIMEOUT})
        self.assertEqual(results[3].subtest,
                         'Window interface: attribute\n\0\r\\nevent')
        self.assertEqual(
            results[3].message, 'assert_true: property should be enumerable'
            '\n\0\r\\n  expected true got false')

        self.assertIs(results[4].line_type, LineType.CONSOLE_ERROR)
        self.assertEqual(results[4].statuses, frozenset())
        self.assertIsNone(results[4].subtest)
        self.assertEqual(results[4].message, 'Console error')

        self.assertIs(results[5].line_type, LineType.FOOTER)
        self.assertEqual(results[5].statuses, frozenset())
        self.assertIsNone(results[5].subtest)
        self.assertIsNone(results[5].message)

    def test_parse_testharness_baseline_other_newlines(self):
        _, subtest, _ = testharness_results.parse_testharness_baseline(
            textwrap.dedent("""\
                This is a testharness.js-based test.
                [FAIL] not line breaks: \v \f \x1c \x1e \x85
                  assert_true: not line breaks: \u2028 \u2029.
                Harness: the test ran to completion.
                """))
        self.assertEqual(subtest.line_type, LineType.SUBTEST)
        self.assertEqual(subtest.statuses, {Status.FAIL})
        self.assertEqual(subtest.subtest,
                         'not line breaks: \v \f \x1c \x1e \x85')
        self.assertEqual(subtest.message,
                         'assert_true: not line breaks: \u2028 \u2029.')

    def test_format_testharness_baseline(self):
        lines = [
            TestharnessLine(LineType.CONSOLE_WARNING,
                            message='warning before test'),
            TestharnessLine(LineType.TESTHARNESS_HEADER),
            TestharnessLine(LineType.HARNESS_ERROR, {Status.ERROR},
                            'SyntaxError'),
            TestharnessLine(LineType.SUBTEST, {Status.PASS, Status.TIMEOUT},
                            'fake-message\n\r\0\\n', 'subtest-1\n\r\0\\n'),
            TestharnessLine(LineType.SUBTEST, {Status.NOTRUN},
                            subtest='subtest-2'),
            TestharnessLine(LineType.FOOTER),
        ]
        self.assertEqual(
            testharness_results.format_testharness_baseline(lines),
            textwrap.dedent("""\
                CONSOLE WARNING: warning before test
                This is a testharness.js-based test.
                Harness Error. harness_status.status = 1 , harness_status.message = SyntaxError
                [PASS TIMEOUT] subtest-1\\n\\r\\0\\\\n
                  fake-message\\n\\r\\0\\\\n
                [NOTRUN] subtest-2
                Harness: the test ran to completion.
                """))

    def test_format_all_pass_testharness_baseline(self):
        lines = [
            TestharnessLine(LineType.TESTHARNESS_HEADER),
            TestharnessLine(LineType.SUBTEST, {Status.PASS},
                            subtest='subtest'),
            TestharnessLine(LineType.FOOTER),
        ]
        # No failure counts written. Note that it is the caller's responsibility
        # to detect that this is an all-pass baseline, and possibly not write
        # it.
        self.assertEqual(
            testharness_results.format_testharness_baseline(lines),
            textwrap.dedent("""\
                This is a testharness.js-based test.
                Harness: the test ran to completion.
                """))

    def test_format_status_counts(self):
        lines = [
            TestharnessLine(LineType.SUBTEST, {Status.FAIL},
                            subtest=f'subtest-{i}') for i in range(50)
        ]
        lines = [
            TestharnessLine(LineType.TESTHARNESS_HEADER),
            *lines,
            TestharnessLine(LineType.FOOTER),
        ]
        self.assertIn(
            'Found 50 FAIL, 0 TIMEOUT, 0 NOTRUN.',
            testharness_results.format_testharness_baseline(
                lines).splitlines())
