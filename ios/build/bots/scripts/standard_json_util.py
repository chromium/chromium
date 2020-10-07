# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from collections import OrderedDict

import result_sink_util

LOGGER = logging.getLogger(__name__)


class StdJson():

  def __init__(self, **kwargs):
    """Module for storing the results in standard JSON format.

    https://chromium.googlesource.com/chromium/src/+/master/docs/testing/json_test_results_format.md
    """

    self.tests = OrderedDict()
    self.result_sink = result_sink_util.ResultSinkClient()

    if 'passed' in kwargs:
      self.mark_all_passed(kwargs['passed'])
    if 'failed' in kwargs:
      self.mark_all_failed(kwargs['failed'])
    if 'flaked' in kwargs:
      self.mark_all_passed(kwargs['flaked'], flaky=True)

  def mark_passed(self, test, flaky=False):
    """Sets test as passed

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"

    If flaky=True, or if 'FAIL' already set in 'actual',
    apply is_flaky=True for all test(s).
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    result_sink_test_result = result_sink_util.compose_test_result(
        test, 'PASS', True)
    self.result_sink.post(result_sink_test_result)

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " PASS"
    else:
      self.tests[test] = {'expected': 'PASS', 'actual': 'PASS'}

    if flaky or 'FAIL' in self.tests[test]['actual']:
      self.tests[test]['is_flaky'] = True

    self.tests[test].pop('is_unexpected', None)

  def mark_all_passed(self, tests, flaky=False):
    """Marks all tests as PASS"""
    for test in tests:
      self.mark_passed(test, flaky)

  def mark_failed(self, test, test_log=None):
    """Sets test(s) as failed.

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
      test_log (str): log of the specific test
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    result_sink_test_result = result_sink_util.compose_test_result(
        test, 'FAIL', False, test_log=test_log)
    self.result_sink.post(result_sink_test_result)

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " FAIL"
      self.tests[test]['is_unexpected'] = True
    else:
      self.tests[test] = {
          'expected': 'PASS',
          'actual': 'FAIL',
          'is_unexpected': True
      }

  def mark_all_failed(self, tests):
    """Marks all tests as FAIL"""
    for test in tests:
      self.mark_failed(test)

  def mark_skipped(self, test):
    """Sets test(s) as expected SKIP.

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    result_sink_test_result = result_sink_util.compose_test_result(
        test, 'SKIP', True, tags=[('disabled_test', 'true')])
    self.result_sink.post(result_sink_test_result)

    self.tests[test] = {'expected': 'SKIP', 'actual': 'SKIP'}

  def mark_all_skipped(self, tests):
    for test in tests:
      self.mark_skipped(test)

  def mark_timeout(self, test):
    """Sets test as TIMEOUT, which is used to indicate a test abort/timeout

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    # Timeout tests in iOS test runner are tests that's unexpectedly not run.
    test_log = ('The test is compiled in test target but was unexpectedly not'
                ' run or not finished.')
    result_sink_test_result = result_sink_util.compose_test_result(
        test,
        'SKIP',
        False,
        test_log=test_log,
        tags=[('disabled_test', 'false')])
    self.result_sink.post(result_sink_test_result)

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " TIMEOUT"
      self.tests[test]['is_unexpected'] = True
    else:
      self.tests[test] = {
          'expected': 'PASS',
          'actual': 'TIMEOUT',
          'is_unexpected': True
      }
