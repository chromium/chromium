#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for test_result_util.py."""

import collections
import copy
import mock
import unittest

import test_result_util
from test_result_util import TestResult, TestStatus, ResultCollection
import test_runner_test

FAKE_TEST_LOC = {'repo': 'https://test', 'fileName': '//test.cc'}
PASSED_RESULT = TestResult(
    'passed/test', TestStatus.PASS, duration=1233, test_log='Logs')
PASSED_RESULT_WITH_LOC = TestResult(
    'passed/test',
    TestStatus.PASS,
    duration=1233,
    test_log='Logs',
    test_loc=FAKE_TEST_LOC)
FAILED_RESULT = TestResult(
    'failed/test', TestStatus.FAIL, duration=1233, test_log='line1\nline2')
FAILED_RESULT_DUPLICATE = TestResult(
    'failed/test', TestStatus.FAIL, test_log='line3\nline4')
DISABLED_RESULT = TestResult(
    'disabled/test',
    TestStatus.SKIP,
    expected_status=TestStatus.SKIP,
    attachments={'name': '/path/to/name'})
UNEXPECTED_SKIPPED_RESULT = TestResult('unexpected/skipped_test',
                                       TestStatus.SKIP)
CRASHED_RESULT = TestResult('crashed/test', TestStatus.CRASH)
FLAKY_PASS_RESULT = TestResult('flaky/test', TestStatus.PASS)
FLAKY_FAIL_RESULT = TestResult(
    'flaky/test', TestStatus.FAIL, test_log='line1\nline2')
ABORTED_RESULT = TestResult('aborted/test', TestStatus.ABORT)


class UtilTest(test_runner_test.TestCase):
  """Tests util methods in test_result_util module."""

  def test_validate_kwargs(self):
    """Tests _validate_kwargs."""
    with self.assertRaises(AssertionError) as context:
      TestResult('name', TestStatus.PASS, unknown='foo')
    expected_message = ("Invalid keyword argument(s) in")
    self.assertTrue(expected_message in str(context.exception))
    with self.assertRaises(AssertionError) as context:
      ResultCollection(test_log='foo')
    expected_message = ("Invalid keyword argument(s) in")
    self.assertTrue(expected_message in str(context.exception))

  def test_validate_test_status(self):
    """Tests exception raised from validation."""
    with self.assertRaises(TypeError) as context:
      test_result_util._validate_test_status('TIMEOUT')
    expected_message = ('Invalid test status: TIMEOUT. Should be one of')
    self.assertTrue(expected_message in str(context.exception))

  def test_to_standard_json_literal(self):
    """Tests _to_standard_json_literal."""
    status = test_result_util._to_standard_json_literal(TestStatus.FAIL)
    self.assertEqual(status, 'FAIL')
    status = test_result_util._to_standard_json_literal(TestStatus.ABORT)
    self.assertEqual(status, 'TIMEOUT')


class TestResultTest(test_runner_test.TestCase):
  """Tests TestResult class APIs."""

  def test_init(self):
    """Tests class initialization."""
    test_result = PASSED_RESULT
    self.assertEqual(test_result.name, 'passed/test')
    self.assertEqual(test_result.status, TestStatus.PASS)
    self.assertEqual(test_result.expected_status, TestStatus.PASS)
    self.assertEqual(test_result.test_log, 'Logs')

  def test_compose_result_sink_tags(self):
    """Tests _compose_result_sink_tags."""
    disabled_test_tags = [('test_name', 'disabled/test'),
                          ('disabled_test', 'true')]
    unexpected_skip_test_tags = [('test_name', 'unexpected/skipped_test'),
                                 ('disabled_test', 'false')]
    not_skip_test_tags = [('test_name', 'passed/test')]

    not_skip_test_result = PASSED_RESULT
    self.assertEqual(not_skip_test_tags,
                     not_skip_test_result._compose_result_sink_tags())

    disabled_test_result = DISABLED_RESULT
    self.assertEqual(disabled_test_tags,
                     disabled_test_result._compose_result_sink_tags())

    unexpected_skip_test_result = UNEXPECTED_SKIPPED_RESULT
    self.assertEqual(unexpected_skip_test_tags,
                     unexpected_skip_test_result._compose_result_sink_tags())

  @mock.patch('result_sink_util.ResultSinkClient.post')
  def test_report_to_result_sink(self, mock_post):
    disabled_test_result = DISABLED_RESULT
    client = mock.MagicMock()
    disabled_test_result.report_to_result_sink(client)
    client.post.assert_called_with(
        'disabled/test',
        'SKIP',
        True,
        duration=None,
        test_log='',
        tags=[('test_name', 'disabled/test'), ('disabled_test', 'true')],
        test_loc=None,
        file_artifacts={'name': '/path/to/name'})
    # Duplicate calls will only report once.
    disabled_test_result.report_to_result_sink(client)
    self.assertEqual(client.post.call_count, 1)
    disabled_test_result.report_to_result_sink(client)
    self.assertEqual(client.post.call_count, 1)

    faileded_result = FAILED_RESULT
    client = mock.MagicMock()
    faileded_result.report_to_result_sink(client)
    client.post.assert_called_with(
        'failed/test',
        'FAIL',
        False,
        duration=1233,
        file_artifacts={},
        tags=[('test_name', 'failed/test')],
        test_loc=None,
        test_log='line1\nline2')

    passed_result = PASSED_RESULT_WITH_LOC
    client = mock.MagicMock()
    passed_result.report_to_result_sink(client)
    client.post.assert_called_with(
        'passed/test',
        'PASS',
        True,
        duration=1233,
        file_artifacts={},
        tags=[('test_name', 'passed/test')],
        test_loc=FAKE_TEST_LOC,
        test_log='Logs')


class ResultCollectionTest(test_runner_test.TestCase):
  """Tests ResultCollection class APIs."""

  def setUp(self):
    super(ResultCollectionTest, self).setUp()
    self.full_collection = ResultCollection(test_results=[
        PASSED_RESULT, FAILED_RESULT, FAILED_RESULT_DUPLICATE, DISABLED_RESULT,
        UNEXPECTED_SKIPPED_RESULT, CRASHED_RESULT, FLAKY_PASS_RESULT,
        FLAKY_FAIL_RESULT, ABORTED_RESULT
    ])

  def test_init(self):
    """Tests class initialization."""
    collection = ResultCollection(
        test_results=[
            PASSED_RESULT, DISABLED_RESULT, UNEXPECTED_SKIPPED_RESULT
        ],
        crashed=True)
    self.assertTrue(collection.crashed)
    self.assertEqual(collection.crash_message, '')
    self.assertEqual(
        collection.test_results,
        [PASSED_RESULT, DISABLED_RESULT, UNEXPECTED_SKIPPED_RESULT])

  def test_add_result(self):
    """Tests add_test_result."""
    collection = ResultCollection(test_results=[FAILED_RESULT])
    collection.add_test_result(DISABLED_RESULT)
    self.assertEqual(collection.test_results, [FAILED_RESULT, DISABLED_RESULT])

  def test_add_result_collection_default(self):
    """Tests add_result_collection default (merge crash info)."""
    collection = ResultCollection(test_results=[FAILED_RESULT])
    self.assertFalse(collection.crashed)
    collection.append_crash_message('Crash1')

    crashed_collection = ResultCollection(
        test_results=[PASSED_RESULT], crashed=True)
    crashed_collection.append_crash_message('Crash2')

    collection.add_result_collection(crashed_collection)
    self.assertTrue(collection.crashed)
    self.assertEqual(collection.crash_message, 'Crash1\nCrash2')
    self.assertEqual(collection.test_results, [FAILED_RESULT, PASSED_RESULT])

  def test_add_result_collection_overwrite(self):
    """Tests add_result_collection overwrite."""
    collection = ResultCollection(test_results=[FAILED_RESULT], crashed=True)
    self.assertTrue(collection.crashed)
    collection.append_crash_message('Crash1')

    crashed_collection = ResultCollection(test_results=[PASSED_RESULT])

    collection.add_result_collection(crashed_collection, overwrite_crash=True)
    self.assertFalse(collection.crashed)
    self.assertEqual(collection.crash_message, '')
    self.assertEqual(collection.test_results, [FAILED_RESULT, PASSED_RESULT])

  def test_add_result_collection_ignore(self):
    """Tests add_result_collection overwrite."""
    collection = ResultCollection(test_results=[FAILED_RESULT])
    self.assertFalse(collection.crashed)

    crashed_collection = ResultCollection(
        test_results=[PASSED_RESULT], crashed=True)
    crashed_collection.append_crash_message('Crash2')

    collection.add_result_collection(crashed_collection, ignore_crash=True)
    self.assertFalse(collection.crashed)
    self.assertEqual(collection.crash_message, '')
    self.assertEqual(collection.test_results, [FAILED_RESULT, PASSED_RESULT])

  def test_add_results(self):
    """Tests add_results."""
    collection = ResultCollection(test_results=[PASSED_RESULT])
    collection.add_results([FAILED_RESULT, DISABLED_RESULT])
    self.assertEqual(collection.test_results,
                     [PASSED_RESULT, FAILED_RESULT, DISABLED_RESULT])

  def test_add_name_prefix_to_tests(self):
    """Tests add_name_prefix_to_tests."""
    passed = copy.copy(PASSED_RESULT)
    disabeld = copy.copy(DISABLED_RESULT)
    collection = ResultCollection(test_results=[passed, disabeld])
    some_prefix = 'Some/prefix'
    collection.add_name_prefix_to_tests(some_prefix)
    for test_result in collection.test_results:
      self.assertTrue(test_result.name.startswith(some_prefix))

  def test_add_test_names_status(self):
    """Tests add_test_names_status."""
    test_names = ['test1', 'test2', 'test3']
    collection = ResultCollection(test_results=[PASSED_RESULT])
    collection.add_test_names_status(test_names, TestStatus.SKIP)
    disabled_test_names = ['test4', 'test5', 'test6']
    collection.add_test_names_status(
        disabled_test_names, TestStatus.SKIP, expected_status=TestStatus.SKIP)
    self.assertEqual(collection.test_results[0], PASSED_RESULT)
    unexpected_skipped = collection.tests_by_expression(
        lambda t: not t.expected() and t.status == TestStatus.SKIP)
    self.assertEqual(unexpected_skipped, set(['test1', 'test2', 'test3']))
    self.assertEqual(collection.disabled_tests(),
                     set(['test4', 'test5', 'test6']))

  @mock.patch('test_result_util.TestResult.report_to_result_sink')
  @mock.patch('result_sink_util.ResultSinkClient.close')
  @mock.patch('result_sink_util.ResultSinkClient.__init__', return_value=None)
  def test_add_and_report_test_names_status(self, mock_sink_init,
                                            mock_sink_close, mock_report):
    """Tests add_test_names_status."""
    test_names = ['test1', 'test2', 'test3']
    collection = ResultCollection(test_results=[PASSED_RESULT])
    collection.add_and_report_test_names_status(test_names, TestStatus.SKIP)
    self.assertEqual(collection.test_results[0], PASSED_RESULT)
    unexpected_skipped = collection.tests_by_expression(
        lambda t: not t.expected() and t.status == TestStatus.SKIP)
    self.assertEqual(unexpected_skipped, set(['test1', 'test2', 'test3']))
    self.assertEqual(1, len(mock_sink_init.mock_calls))
    self.assertEqual(3, len(mock_report.mock_calls))
    self.assertEqual(1, len(mock_sink_close.mock_calls))

  def testappend_crash_message(self):
    """Tests append_crash_message."""
    collection = ResultCollection(test_results=[PASSED_RESULT])
    collection.append_crash_message('Crash message 1.')
    self.assertEqual(collection.crash_message, 'Crash message 1.')
    collection.append_crash_message('Crash message 2.')
    self.assertEqual(collection.crash_message,
                     'Crash message 1.\nCrash message 2.')

  def test_tests_by_expression(self):
    """Tests tests_by_expression."""
    collection = self.full_collection
    exp = lambda result: result.status == TestStatus.SKIP
    skipped_tests = collection.tests_by_expression(exp)
    self.assertEqual(skipped_tests,
                     set(['unexpected/skipped_test', 'disabled/test']))

  def test_get_spcific_tests(self):
    """Tests getting sets of tests of specific status."""
    collection = self.full_collection
    self.assertEqual(
        collection.all_test_names(),
        set([
            'passed/test', 'disabled/test', 'failed/test',
            'unexpected/skipped_test', 'crashed/test', 'flaky/test',
            'aborted/test'
        ]))
    self.assertEqual(collection.crashed_tests(), set(['crashed/test']))
    self.assertEqual(collection.disabled_tests(), set(['disabled/test']))
    self.assertEqual(collection.expected_tests(),
                     set(['passed/test', 'disabled/test', 'flaky/test']))
    self.assertEqual(
        collection.unexpected_tests(),
        set([
            'failed/test', 'unexpected/skipped_test', 'crashed/test',
            'flaky/test', 'aborted/test'
        ]))
    self.assertEqual(collection.passed_tests(),
                     set(['passed/test', 'flaky/test']))
    self.assertEqual(collection.failed_tests(),
                     set(['failed/test', 'flaky/test']))
    self.assertEqual(collection.flaky_tests(), set(['flaky/test']))
    self.assertEqual(
        collection.never_expected_tests(),
        set([
            'failed/test', 'unexpected/skipped_test', 'crashed/test',
            'aborted/test'
        ]))
    self.assertEqual(collection.pure_expected_tests(),
                     set(['passed/test', 'disabled/test']))

  def test_add_and_report_crash(self):
    """Tests add_and_report_crash."""
    collection = copy.copy(self.full_collection)

    collection.set_crashed_with_prefix('Prefix Line')
    self.assertEqual(collection.crash_message, 'Prefix Line\n')
    self.assertTrue(collection.crashed)

  @mock.patch('test_result_util.TestResult.report_to_result_sink')
  @mock.patch('result_sink_util.ResultSinkClient.close')
  @mock.patch('result_sink_util.ResultSinkClient.__init__', return_value=None)
  def test_report_to_result_sink(self, mock_sink_init, mock_sink_close,
                                 mock_report):
    """Tests report_to_result_sink."""
    collection = copy.copy(self.full_collection)
    collection.report_to_result_sink()

    mock_sink_init.assert_called_once()
    self.assertEqual(len(collection.test_results), len(mock_report.mock_calls))
    mock_sink_close.assert_called()

  @mock.patch('shard_util.gtest_shard_index', return_value=0)
  @mock.patch('time.time', return_value=10000)
  def test_standard_json_output(self, *args):
    """Tests standard_json_output."""
    passed_test_value = {
        'expected': 'PASS',
        'actual': 'PASS',
        'shard': 0,
        'is_unexpected': False
    }
    failed_test_value = {
        'expected': 'PASS',
        'actual': 'FAIL FAIL',
        'shard': 0,
        'is_unexpected': True
    }
    disabled_test_value = {
        'expected': 'SKIP',
        'actual': 'SKIP',
        'shard': 0,
        'is_unexpected': False
    }
    unexpected_skip_test_value = {
        'expected': 'PASS',
        'actual': 'SKIP',
        'shard': 0,
        'is_unexpected': True
    }
    crashed_test_value = {
        'expected': 'PASS',
        'actual': 'CRASH',
        'shard': 0,
        'is_unexpected': True
    }
    flaky_test_value = {
        'expected': 'PASS',
        'actual': 'PASS FAIL',
        'shard': 0,
        'is_unexpected': False,
        'is_flaky': True
    }
    aborted_test_value = {
        'expected': 'PASS',
        'actual': 'TIMEOUT',
        'shard': 0,
        'is_unexpected': True
    }
    expected_tests = collections.OrderedDict()
    expected_tests['passed/test'] = passed_test_value
    expected_tests['failed/test'] = failed_test_value
    expected_tests['disabled/test'] = disabled_test_value
    expected_tests['unexpected/skipped_test'] = unexpected_skip_test_value
    expected_tests['crashed/test'] = crashed_test_value
    expected_tests['flaky/test'] = flaky_test_value
    expected_tests['aborted/test'] = aborted_test_value
    expected_num_failures_by_type = {
        'PASS': 2,
        'FAIL': 1,
        'CRASH': 1,
        'SKIP': 2,
        'TIMEOUT': 1
    }
    expected_json = {
        'version': 3,
        'path_delimiter': '/',
        'seconds_since_epoch': 10000,
        'interrupted': False,
        'num_failures_by_type': expected_num_failures_by_type,
        'tests': expected_tests
    }
    self.assertEqual(
        self.full_collection.standard_json_output(path_delimiter='/'),
        expected_json)

  def test_test_runner_logs(self):
    """Test test_runner_logs."""
    expected_logs = collections.OrderedDict()
    expected_logs['passed tests'] = ['passed/test']
    expected_logs['disabled tests'] = ['disabled/test']
    flaky_logs = ['Failure log of attempt 1:', 'line1', 'line2']
    failed_logs = [
        'Failure log of attempt 1:', 'line1', 'line2',
        'Failure log of attempt 2:', 'line3', 'line4'
    ]
    no_logs = ['Failure log of attempt 1:', '']
    expected_logs['flaked tests'] = {'flaky/test': flaky_logs}
    expected_logs['failed tests'] = {
        'failed/test': failed_logs,
        'crashed/test': no_logs,
        'unexpected/skipped_test': no_logs,
        'aborted/test': no_logs
    }
    expected_logs['failed/test'] = failed_logs
    expected_logs['unexpected/skipped_test'] = no_logs
    expected_logs['flaky/test'] = flaky_logs
    expected_logs['crashed/test'] = no_logs
    expected_logs['aborted/test'] = no_logs
    generated_logs = self.full_collection.test_runner_logs()
    keys = [
        'passed tests', 'disabled tests', 'flaked tests', 'failed tests',
        'failed/test', 'unexpected/skipped_test', 'flaky/test', 'crashed/test',
        'aborted/test'
    ]
    for key in keys:
      self.assertEqual(generated_logs[key], expected_logs[key])


if __name__ == '__main__':
  unittest.main()
