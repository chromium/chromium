# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from collections import OrderedDict
import os

import result_sink_util

LOGGER = logging.getLogger(__name__)


class StdJson():

  def __init__(self, **kwargs):
    """Module for storing the results in standard JSON format.

    https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md
    """

    self.tests = OrderedDict()
    self.result_sink = result_sink_util.ResultSinkClient()
    self._shard_index = os.getenv('GTEST_SHARD_INDEX', 0)

    if 'passed' in kwargs:
      self.mark_all_passed(kwargs['passed'])
    if 'failed' in kwargs:
      self.mark_all_failed(kwargs['failed'])
    if 'flaked' in kwargs:
      self.mark_all_passed(kwargs['flaked'], flaky=True)

  def _init_test(self, expected, actual, is_unexpected=False):
    """Returns a dict of test result info used as values in self.tests dict."""
    test = {
        'expected': expected,
        'actual': actual,
        'shard': self._shard_index,
    }
    if is_unexpected:
      test['is_unexpected'] = True

    return test

  def finalize(self):
    """Teardown and finalizing tasks needed after all results are reported."""
    LOGGER.info('Finalizing in standard json util.')
    self.result_sink.close()

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

    self.result_sink.post(test, 'PASS', True)

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " PASS"
    else:
      self.tests[test] = self._init_test('PASS', 'PASS')

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

    self.result_sink.post(test, 'FAIL', False, test_log=test_log)

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " FAIL"
      self.tests[test]['is_unexpected'] = True
    else:
      self.tests[test] = self._init_test('PASS', 'FAIL', True)

  def mark_all_failed(self, tests):
    """Marks all tests as FAIL"""
    for test in tests:
      self.mark_failed(test)

  def mark_disabled(self, test):
    """Sets test(s) as expected SKIP with disabled test label.

    Params:
      test (str): a test in format "{TestCase}/{testMethod}"
    """
    if not test:
      LOGGER.warn('Empty or None test name passed to standard_json_util')
      return

    self.result_sink.post(test, 'SKIP', True, tags=[('disabled_test', 'true')])

    self.tests[test] = self._init_test('SKIP', 'SKIP')

  def mark_all_disabled(self, tests):
    for test in tests:
      self.mark_disabled(test)

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
    self.result_sink.post(
        test,
        'SKIP',
        False,
        test_log=test_log,
        tags=[('disabled_test', 'false')])

    if test in self.tests:
      self.tests[test]['actual'] = self.tests[test]['actual'] + " TIMEOUT"
      self.tests[test]['is_unexpected'] = True
    else:
      self.tests[test] = self._init_test('PASS', 'TIMEOUT', True)
