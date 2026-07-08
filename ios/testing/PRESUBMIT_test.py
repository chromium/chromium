#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi, MockFile
import PRESUBMIT


class CheckTestExpectationsTest(unittest.TestCase):

    def setUp(self):
        self.mock_input = MockInputApi()
        self.mock_output = MockOutputApi()
        self.mock_input.change.RepositoryRoot = lambda: os.path.join(
            os.path.dirname(__file__), '..', '..')
        self.mock_input.PresubmitLocalPath = lambda: os.path.dirname(__file__)

    def testValidExpectations(self):
        lines = [
            '# Comment line should be ignored.',
            '',
            'crbug.com/123 [ ios18 simulator ] MyTestCase/testFoo [ Failure ]',
            'http://crbug.com/789012 [ device ] MyTestSuite [ Pass Crash ]',
            'https://crbug.com/345678 AnotherTest [ Skip ] # trailing comment',
            'crbug.com/chromium/123456 AnotherTest [ Failure ]',
        ]
        self.mock_input.files = [
            MockFile('ios/testing/test_expectations.txt', lines)
        ]
        results = PRESUBMIT.CheckTestExpectations(
            self.mock_input, self.mock_output)
        self.assertEqual(len(results), 0)

    def testInvalidSyntaxMissingBug(self):
        lines = [
            'MyTestCase/testFoo [ Failure ]',
        ]
        self.mock_input.files = [
            MockFile('ios/testing/test_expectations.txt', lines)
        ]
        results = PRESUBMIT.CheckTestExpectations(
            self.mock_input, self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('error', results[0].type)
        self.assertIn('Missing or invalid bug field', results[0].message)

    def testInvalidSyntaxMalformed(self):
        lines = [
            'This line has no expectation brackets at all',
        ]
        self.mock_input.files = [
            MockFile('ios/testing/test_expectations.txt', lines)
        ]
        results = PRESUBMIT.CheckTestExpectations(
            self.mock_input, self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('error', results[0].type)
        self.assertIn(
            'Line does not respect prescribed syntax', results[0].message)

    def testInvalidExpectationWord(self):
        lines = [
            'crbug.com/123456 [ ios ] MyTestCase/testFoo [ Bogus ]',
        ]
        self.mock_input.files = [
            MockFile('ios/testing/test_expectations.txt', lines)
        ]
        results = PRESUBMIT.CheckTestExpectations(
            self.mock_input, self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('error', results[0].type)
        self.assertIn("Invalid expectation 'Bogus'", results[0].message)

    def testInvalidBugUrlFormat(self):
        lines = [
            'crbug.com/ [ ios ] MyTestCase/testFoo [ Failure ]',
            'http://google.com/123456 [ ios ] MyTestCase/testFoo [ Failure ]',
            'notabug [ ios ] MyTestCase/testFoo [ Failure ]',
        ]
        self.mock_input.files = [
            MockFile('ios/testing/test_expectations.txt', lines)
        ]
        results = PRESUBMIT.CheckTestExpectations(
            self.mock_input, self.mock_output)
        self.assertEqual(len(results), 3)
        for res in results:
            self.assertEqual('error', res.type)
            self.assertIn('Missing or invalid bug field', res.message)

    def testInvalidWildcardInTestName(self):
        lines = [
            'crbug.com/123456 [ ios ] MyTestCase/* [ Failure ]',
        ]
        self.mock_input.files = [
            MockFile('ios/testing/test_expectations.txt', lines)
        ]
        results = PRESUBMIT.CheckTestExpectations(
            self.mock_input, self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('error', results[0].type)
        self.assertIn(
            "Wildcards ('*') are not supported in test names",
            results[0].message)


if __name__ == '__main__':
    unittest.main()
