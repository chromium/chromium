# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import sys

from telemetry.testing import fakes
from telemetry.testing import browser_test_context

from gpu_tests import gpu_integration_test


# pylint: disable=abstract-method
class _BaseSampleIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  _test_state = {}

  @classmethod
  def SetUpProcess(cls):
    finder_options = fakes.CreateBrowserFinderOptions()
    finder_options.browser_options.platform = fakes.FakeLinuxPlatform()
    finder_options.output_formats = ['none']
    finder_options.suppress_gtest_report = True
    finder_options.output_dir = None
    finder_options.upload_bucket = 'public'
    finder_options.upload_results = False
    cls._finder_options = finder_options
    cls.platform = None
    cls.browser = None
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(_BaseSampleIntegrationTest, cls).AddCommandlineArgs(parser)
    parser.add_argument(
        '--test-state-json-path',
        help=('Where to dump the test state json (this is used by '
              'gpu_integration_test_unittest)'))

  @classmethod
  def TearDownProcess(cls):
    actual_finder_options = browser_test_context.GetCopy().finder_options
    test_state_json_path = actual_finder_options.test_state_json_path
    with open(test_state_json_path, 'w') as f:
      json.dump(cls._test_state, f)
    super(_BaseSampleIntegrationTest, cls).TearDownProcess()
# pylint: enable=abstract-method


class SimpleTest(_BaseSampleIntegrationTest):
  _test_state = {'num_flaky_runs_to_fail': 2, 'num_browser_starts': 0}

  @classmethod
  def Name(cls):
    return 'simple_integration_unittest'

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('expected_failure', 'failure.html', ())
    yield ('expected_flaky', 'flaky.html', ())
    yield ('expected_skip', 'failure.html', ())
    yield ('unexpected_failure', 'failure.html', ())
    yield ('unexpected_error', 'error.html', ())

  @classmethod
  def StartBrowser(cls):
    super(SimpleTest, cls).StartBrowser()
    cls._test_state['num_browser_starts'] += 1

  def RunActualGpuTest(self, test_path, *args):
    logging.warning('Running %s', test_path)
    if test_path == 'failure.html':
      self.fail('Expected failure')
    elif test_path == 'flaky.html':
      if self._test_state['num_flaky_runs_to_fail'] > 0:
        self._test_state['num_flaky_runs_to_fail'] -= 1
        self.fail('Expected flaky failure')
    elif test_path == 'error.html':
      raise Exception('Expected exception')

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            ('test_expectations/'
             'simple_integration_unittest_expectations.txt'))
    ]


class BrowserStartFailureTest(_BaseSampleIntegrationTest):
  _test_state = {'num_browser_crashes': 0, 'num_browser_starts': 0}

  @classmethod
  def SetUpProcess(cls):
    cls._fake_browser_options = \
        fakes.CreateBrowserFinderOptions(execute_on_startup=cls.CrashOnStart)
    cls._fake_browser_options.browser_options.platform = \
        fakes.FakeLinuxPlatform()
    cls._fake_browser_options.output_formats = ['none']
    cls._fake_browser_options.suppress_gtest_report = True
    cls._fake_browser_options.output_dir = None
    cls._fake_browser_options.upload_bucket = 'public'
    cls._fake_browser_options.upload_results = False
    cls._finder_options = cls._fake_browser_options
    cls.platform = None
    cls.browser = None
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  @classmethod
  def CrashOnStart(cls):
    cls._test_state['num_browser_starts'] += 1
    if cls._test_state['num_browser_crashes'] < 2:
      cls._test_state['num_browser_crashes'] += 1
      raise Exception('Fake browser crash')

  @classmethod
  def Name(cls):
    return 'browser_start_failure_integration_unittest'

  @classmethod
  def GenerateGpuTests(cls, options):
    # This test causes the browser to try and restart the browser 3 times.
    yield ('restart', 'restart.html', ())

  def RunActualGpuTest(self, test_path, *args):
    # The logic of this test is run when the browser starts, it fails twice
    # and then succeeds on the third time so we are just testing that this
    # is successful based on the parameters.
    pass


class BrowserCrashAfterStartTest(_BaseSampleIntegrationTest):
  _test_state = {
      'num_browser_crashes': 0,
      'num_browser_starts': 0,
  }

  @classmethod
  def SetUpProcess(cls):
    cls._fake_browser_options = fakes.CreateBrowserFinderOptions(
        execute_after_browser_creation=cls.CrashAfterStart)
    cls._fake_browser_options.browser_options.platform = \
        fakes.FakeLinuxPlatform()
    cls._fake_browser_options.output_formats = ['none']
    cls._fake_browser_options.suppress_gtest_report = True
    cls._fake_browser_options.output_dir = None
    cls._fake_browser_options.upload_bucket = 'public'
    cls._fake_browser_options.upload_results = False
    cls._finder_options = cls._fake_browser_options
    cls.platform = None
    cls.browser = None
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()

  @classmethod
  def CrashAfterStart(cls, browser):
    cls._test_state['num_browser_starts'] += 1
    if cls._test_state['num_browser_crashes'] < 2:
      cls._test_state['num_browser_crashes'] += 1
      # This simulates the first tab's renderer process crashing upon
      # startup. The try/catch forces the GpuIntegrationTest's first
      # fetch of this tab to fail. crbug.com/682819
      try:
        browser.tabs[0].Navigate('chrome://crash')
      except Exception:  # pylint: disable=broad-except
        pass

  @classmethod
  def Name(cls):
    return 'browser_crash_after_start_integration_unittest'

  @classmethod
  def GenerateGpuTests(cls, options):
    # This test causes the browser to try and restart the browser 3 times.
    yield ('restart', 'restart.html', ())

  def RunActualGpuTest(self, test_path, *args):
    # The logic of this test is run when the browser starts, it fails twice
    # and then succeeds on the third time so we are just testing that this
    # is successful based on the parameters.
    pass


class RunTestsWithExpectationsFiles(_BaseSampleIntegrationTest):
  def __init__(self, methodName):
    super().__init__(methodName)
    self._flaky_test_run = 0

  @classmethod
  def GetPlatformTags(cls, browser):
    assert isinstance(browser, fakes.FakeBrowser)
    return ['foo']

  @classmethod
  def Name(cls):
    return 'run_tests_with_expectations_files'

  @classmethod
  def GenerateGpuTests(cls, options):
    tests = [('a/b/unexpected-fail.html', 'failure.html', ()),
             ('a/b/expected-fail.html', 'failure.html', ()),
             ('a/b/expected-flaky.html', 'flaky.html', ()),
             ('should_skip', 'skip.html', ())]
    for test in tests:
      yield test

  def RunActualGpuTest(self, test_path, *args):
    if test_path == 'failure.html' or self._flaky_test_run < 3:
      self._flaky_test_run += test_path == 'flaky.html'
      self.fail()

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            ('test_expectations/'
             'run_tests_with_expectations_files_expectations.txt'))
    ]


class TestRetryLimit(_BaseSampleIntegrationTest):
  _test_state = {
      'num_test_runs': 0,
  }

  @classmethod
  def Name(cls):
    return 'test_retry_limit'

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('unexpected_failure', 'failure.html', ())

  def RunActualGpuTest(self, test_path, *args):
    self._test_state['num_test_runs'] += 1
    if test_path == 'failure.html':
      self.fail('Expected failure')
    else:
      raise Exception('Unexpected test name ' + test_path)


class TestRepeat(_BaseSampleIntegrationTest):
  _test_state = {
      'num_test_runs': 0,
  }

  @classmethod
  def Name(cls):
    return 'test_repeat'

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('success', 'success.html', ())

  def RunActualGpuTest(self, test_path, *args):
    self._test_state['num_test_runs'] += 1
    if test_path != 'success.html':
      raise Exception('Unexpected test name ' + test_path)


class TestAlsoRunDisabledTests(_BaseSampleIntegrationTest):
  _test_state = {'num_flaky_test_runs': 0, 'num_test_runs': 0}

  @classmethod
  def Name(cls):
    return 'test_also_run_disabled_tests'

  @classmethod
  def GenerateGpuTests(cls, options):
    tests = [('skip', 'skip.html', ()), ('expected_failure', 'fail.html', ()),
             ('flaky', 'flaky.html', ())]
    for test in tests:
      yield test

  def RunActualGpuTest(self, test_path, *args):
    self._test_state['num_test_runs'] += 1
    self._test_state['num_flaky_test_runs'] += test_path == 'flaky.html'
    raise Exception('Everything fails')

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            ('test_expectations/'
             'tests_also_run_disabled_tests_expectations.txt'))
    ]


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
