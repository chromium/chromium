# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import unittest

import result_sink_util


class UnitTest(unittest.TestCase):

  def test_compose_test_result(self):
    """Tests compose_test_result function."""
    # Test a test result without log_path.
    test_result = result_sink_util.compose_test_result('TestCase/testSomething',
                                                       'PASS', True)
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'tags': [],
    }
    self.assertEqual(test_result, expected)
    # Tests a test result with log_path.
    test_result = result_sink_util.compose_test_result('TestCase/testSomething',
                                                       'PASS', True,
                                                       'Some logs.')
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'summaryHtml': '<pre>Some logs.</pre>',
        'tags': [],
    }
    self.assertEqual(test_result, expected)

  def test_long_test_log(self):
    """Tests long test log is reported as expected."""
    len_32_str = 'This is a string in length of 32'
    self.assertEqual(len(len_32_str), 32)
    len_4128_str = (4 * 32 + 1) * len_32_str
    self.assertEqual(len(len_4128_str), 4128)
    expected_summary_html = ('<pre>' + len_32_str * 126 + 'This is a stri' +
                             '...Full output in "Test Log" Artifact.</pre>')

    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'PASS',
        'expected': True,
        'summaryHtml': expected_summary_html,
        'artifacts': {
            'Test Log': {
                'contents': base64.b64encode(len_4128_str)
            },
        },
        'tags': [],
    }
    test_result = result_sink_util.compose_test_result('TestCase/testSomething',
                                                       'PASS', True,
                                                       len_4128_str)
    self.assertEqual(test_result, expected)

  def test_compose_test_result_assertions(self):
    """Tests invalid status is rejected"""
    with self.assertRaises(AssertionError):
      test_result = result_sink_util.compose_test_result(
          'TestCase/testSomething', 'SOME_INVALID_STATUS', True)

    with self.assertRaises(AssertionError):
      test_result = result_sink_util.compose_test_result(
          'TestCase/testSomething', 'PASS', True, tags=('a', 'b'))

    with self.assertRaises(AssertionError):
      test_result = result_sink_util.compose_test_result(
          'TestCase/testSomething',
          'PASS',
          True,
          tags=[('a', 'b', 'c'), ('d', 'e')])

    with self.assertRaises(AssertionError):
      test_result = result_sink_util.compose_test_result(
          'TestCase/testSomething', 'PASS', True, tags=[('a', 'b'), ('c', 3)])

  def test_composed_with_tags(self):
    """Tests tags is in correct format."""
    expected = {
        'testId': 'TestCase/testSomething',
        'status': 'SKIP',
        'expected': True,
        'tags': [{
            'key': 'disabled_test',
            'value': 'true',
        }]
    }
    test_result = result_sink_util.compose_test_result(
        'TestCase/testSomething',
        'SKIP',
        True,
        tags=[('disabled_test', 'true')])
    self.assertEqual(test_result, expected)


if __name__ == '__main__':
  unittest.main()
