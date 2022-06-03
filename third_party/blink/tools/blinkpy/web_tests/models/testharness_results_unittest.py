# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.web_tests.models import testharness_results


class TestHarnessResultCheckerTest(unittest.TestCase):
    def test_is_all_pass_testharness_result_positive_cases(self):
        self.assertTrue(
            testharness_results.is_all_pass_testharness_result(
                'This is a testharness.js-based test.\n'
                ' PASS: foo bar \n'
                ' Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_all_pass_testharness_result(
                'This is a testharness.js-based test.\n'
                'PASS \'grid\' with: grid-template-areas: "a b"\n'
                '"c d";\n'
                'Harness: the test ran to completion.\n'))

    def test_is_all_pass_testharness_result_negative_cases(self):
        self.assertFalse(
            testharness_results.is_all_pass_testharness_result(
                'This is a testharness.js-based test.\n'
                'CONSOLE WARNING: This is a warning.\n'
                'Test ran to completion.'))
        self.assertFalse(
            testharness_results.is_all_pass_testharness_result(
                'This is a testharness.js-based test.\n'
                ' PASS: foo bar \n'
                'FAIL  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_all_pass_testharness_result(
                'This is a testharness.js-based test.\n'
                'Harness Error. harness_status.status = 1\n'
                'PASS foo bar\n'
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
                'FAIL: bah \n'
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

    def test_is_testharness_output_passing_empty_content(self):
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                '   Harness: the test ran to completion.'))

    def test_is_testharness_output_passing_no_pass(self):
        # If there are no PASS lines, then the test is not considered to pass.
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                '  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' Foo bar \n'
                ' Harness: the test ran to completion.'))

    def test_is_testharness_output_passing_with_pass_and_random_text(self):
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'RANDOM TEXT.\n'
                'This is a testharness.js-based test.\n'
                'PASS: things are fine.\n'
                ' Harness: the test ran to completion.\n'
                '\n'))

    def test_is_testharness_output_passing_basic_examples(self):
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' PASS: foo bar \n'
                ' Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' PASS: foo bar FAIL  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' PASS: foo bar \n'
                'FAIL  \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' FAIL: bah \n'
                ' Harness: the test ran to completion.'))

    def test_is_testharness_output_passing_with_console_messages(self):
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' CONSOLE ERROR: BLAH  \n'
                ' Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' CONSOLE WARNING: BLAH  \n'
                'PASS: some passing method\n'
                ' Harness: the test ran to completion.'))
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'CONSOLE LOG: error.\n'
                'This is a testharness.js-based test.\n'
                'PASS: things are fine.\n'
                'Harness: the test ran to completion.\n'
                '\n'))
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'CONSOLE ERROR: error.\n'
                'This is a testharness.js-based test.\n'
                'PASS: things are fine.\n'
                'Harness: the test ran to completion.\n'
                '\n'))
        self.assertTrue(
            testharness_results.is_testharness_output_passing(
                'CONSOLE WARNING: error.\n'
                'This is a testharness.js-based test.\n'
                'PASS: things are fine.\n'
                'Harness: the test ran to completion.\n'
                '\n'))

    def test_is_testharness_output_passing_with_timeout_or_notrun(self):
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' TIMEOUT: bah \n'
                ' Harness: the test ran to completion.'))
        self.assertFalse(
            testharness_results.is_testharness_output_passing(
                'This is a testharness.js-based test.\n'
                ' NOTRUN: bah \n'
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
