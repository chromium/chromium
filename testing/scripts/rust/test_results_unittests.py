#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_results import TestResult
from test_results import _build_json_data


class TestResultTests(unittest.TestCase):
    def test_equality_and_hashing(self):
        a1 = TestResult('foo', 'PASS', 'FAIL')
        a2 = TestResult('foo', 'PASS', 'FAIL')
        b = TestResult('bar', 'PASS', 'FAIL')
        c = TestResult('foo', 'FAIL', 'FAIL')
        d = TestResult('foo', 'PASS', 'PASS')
        self.assertEqual(a1, a2)
        self.assertEqual(hash(a1), hash(a2))
        self.assertNotEqual(a1, b)
        self.assertNotEqual(a1, c)
        self.assertNotEqual(a1, d)

    def test_pass_expected_repr(self):
        pass_expected_repr = repr(TestResult('foo', 'PASS'))
        self.assertIn('foo', pass_expected_repr)
        self.assertIn('PASS', pass_expected_repr)
        self.assertNotIn('FAIL', pass_expected_repr)
        self.assertIn('TestResult', pass_expected_repr)

    def test_fail_expected_repr(self):
        fail_expected_repr = repr(TestResult('foo', 'PASS', 'FAIL'))
        self.assertIn('foo', fail_expected_repr)
        self.assertIn('PASS', fail_expected_repr)
        self.assertIn('FAIL', fail_expected_repr)
        self.assertIn('TestResult', fail_expected_repr)


class BuildJsonDataTests(unittest.TestCase):
    def test_grouping_of_tests(self):
        t1 = TestResult('group1//foo', 'PASS')
        t2 = TestResult('group1//bar', 'FAIL')
        t3 = TestResult('group2//baz', 'FAIL')
        actual_result = _build_json_data([t1, t2, t3], 123)
        # yapf: disable
        expected_result = {
            'interrupted': False,
            'path_delimiter': '//',
            'seconds_since_epoch': 123,
            'version': 3,
            'tests': {
                'group1': {
                    'foo': {
                        'expected': 'PASS',
                        'actual': 'PASS'
                    },
                    'bar': {
                        'expected': 'PASS',
                        'actual': 'FAIL'
                    }},
                'group2': {
                    'baz': {
                        'expected': 'PASS',
                        'actual': 'FAIL'
                    }}},
            'num_failures_by_type': {
                'PASS': 1,
                'FAIL': 2
            }
        }
        # yapf: enable
        self.assertEqual(actual_result, expected_result)


if __name__ == '__main__':
    unittest.main()
