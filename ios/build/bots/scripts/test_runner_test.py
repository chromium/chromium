#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for test_runner.py."""

import collections
import logging
import mock
import os
import tempfile
import unittest

import iossim_util
import result_sink_util
import test_apps
from test_result_util import ResultCollection, TestResult, TestStatus
import test_runner
import xcode_util


class TestCase(unittest.TestCase):
  """Test case which supports installing mocks. Uninstalls on tear down."""

  def __init__(self, *args, **kwargs):
    """Initializes a new instance of this class."""
    super(TestCase, self).__init__(*args, **kwargs)

    # Maps object to a dict which maps names of mocked members to their
    # original values.
    self._mocks = collections.OrderedDict()

  def mock(self, obj, member, mock):
    """Installs mock in place of the named member of the given obj.

    Args:
      obj: Any object.
      member: String naming the attribute of the object to mock.
      mock: The mock to install.
    """
    self._mocks.setdefault(obj, collections.OrderedDict()).setdefault(
        member, getattr(obj, member))
    setattr(obj, member, mock)

  def unmock(self, obj, member):
    """Uninstalls the mock from the named member of given obj.

    Args:
      obj: An obj who's member has been mocked
      member: String naming the attribute of the object to unmock
    """
    if self._mocks[obj][member]:
      setattr(obj, member, self._mocks[obj][member])

  def tearDown(self, *args, **kwargs):
    """Uninstalls mocks."""
    super(TestCase, self).tearDown(*args, **kwargs)

    for obj in self._mocks:
      for member, original_value in self._mocks[obj].items():
        setattr(obj, member, original_value)


class SimulatorTestRunnerTest(TestCase):
  """Tests for test_runner.SimulatorTestRunner."""

  def setUp(self):
    super(SimulatorTestRunnerTest, self).setUp()
    self.mock(iossim_util, 'get_simulator', lambda _1, _2: 'sim-UUID')
    self.mock(result_sink_util.ResultSinkClient,
              'post', lambda *args, **kwargs: None)

    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(test_apps, 'get_bundle_id', lambda _: 'fake-bundle-id')
    self.mock(xcode_util, 'xctest_path', lambda _: 'fake-path')
    self.mock(test_apps.plistlib, 'dump', lambda _1, _2: '')
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(test_runner.TestRunner, 'set_sigterm_handler',
      lambda self, handler: 0)
    self.mock(os, 'listdir', lambda _: [])
    self.mock(test_apps.GTestsApp, 'fill_xctest_run',
              lambda _, folder: '/abs/path/to/%s' % folder)

  def test_app_not_found(self):
    """Ensures AppNotFoundError is raised."""

    self.mock(os.path, 'exists', lambda p: not p.endswith('fake-app'))

    with self.assertRaises(test_runner.AppNotFoundError):
      test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'platform',
        'os',
        'out-dir',
      )

  def test_iossim_not_found(self):
    """Ensures SimulatorNotFoundError is raised."""
    self.mock(os.path, 'exists', lambda p: not p.endswith('fake-iossim'))

    with self.assertRaises(test_runner.SimulatorNotFoundError):
      test_runner.SimulatorTestRunner(
          'fake-app',
          'fake-iossim',
          'iPhone X',
          '11.4',
          'out-dir',
      )

  def test_init(self):
    """Ensures instance is created."""
    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'iPhone X',
        '11.4',
        'out-dir',
    )

    self.assertTrue(tr)

  @mock.patch('test_runner.SimulatorTestRunner.tear_down')
  @mock.patch('test_runner.SimulatorTestRunner.set_up')
  @mock.patch('test_runner.TestRunner._run')
  def test_startup_crash(self, mock_run, _1, _2):
    """Ensures test is relaunched once on startup crash."""
    result = ResultCollection()
    result.crashed = True
    mock_run.return_value = result

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'iPhone X',
        '11.4',
        'out-dir',
        xctest=True,
    )
    with self.assertRaises(test_runner.AppLaunchError):
      tr.launch()
    self.assertEqual(len(mock_run.mock_calls), 2)

  def test_relaunch(self):
    """Ensures test is relaunched on test crash until tests complete."""
    def set_up(self):
      return

    @staticmethod
    def _run(cmd, clones=None):
      if not any('retry_after_crash' in cmd_arg for cmd_arg in cmd):
        # First run, has no test filter supplied. Mock a crash.
        result = ResultCollection(
            test_results=[TestResult('crash', TestStatus.CRASH)])
        result.crashed = True
        result.add_test_result(TestResult('pass', TestStatus.PASS))
        result.add_test_result(
            TestResult('fail', TestStatus.FAIL, test_log='some logs'))
        return result
      else:
        return ResultCollection(
            test_results=[TestResult('crash', TestStatus.PASS)])

    def tear_down(self):
      return

    self.mock(test_runner.SimulatorTestRunner, 'set_up', set_up)
    self.mock(test_runner.TestRunner, '_run', _run)
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', tear_down)

    tr = test_runner.SimulatorTestRunner(
        'fake-app',
        'fake-iossim',
        'iPhone X',
        '11.4',
        'out-dir',
    )
    tr.launch()
    self.assertTrue(tr.logs)

  @mock.patch('test_runner.SimulatorTestRunner.tear_down')
  @mock.patch('test_runner.SimulatorTestRunner.set_up')
  @mock.patch('test_runner.TestRunner._run')
  def test_failed_test_retry(self, mock_run, _1, _2):
    test1_fail_result = TestResult('test1', TestStatus.FAIL)
    test2_fail_result = TestResult('test2', TestStatus.FAIL)
    test1_pass_result = TestResult('test1', TestStatus.PASS)
    test2_pass_result = TestResult('test2', TestStatus.PASS)
    result1 = ResultCollection(
        test_results=[test1_fail_result, test2_fail_result])
    retry_result1 = ResultCollection(test_results=[test1_pass_result])
    retry_result2 = ResultCollection(test_results=[test2_pass_result])
    mock_run.side_effect = [result1, retry_result1, retry_result2]
    tr = test_runner.SimulatorTestRunner(
        'fake-app', 'fake-iossim', 'iPhone X', '11.4', 'out-dir', retries=3)
    tr.launch()
    self.assertEqual(len(mock_run.mock_calls), 3)
    self.assertTrue(tr.logs)

  @mock.patch('test_runner.SimulatorTestRunner.tear_down')
  @mock.patch('test_runner.SimulatorTestRunner.set_up')
  @mock.patch('test_runner.TestRunner._run')
  def test_crashed_if_crash_in_final_crash_retry(self, mock_run, _1, _2):
    test1_crash_result = TestResult('test1', TestStatus.CRASH)
    test2_crash_result = TestResult('test2', TestStatus.CRASH)
    test3_pass_result = TestResult('test3', TestStatus.PASS)
    test1_pass_result = TestResult('test1', TestStatus.PASS)
    test2_pass_result = TestResult('test2', TestStatus.PASS)
    initial_result = ResultCollection(test_results=[test1_crash_result])
    initial_result.crashed = True
    crash_retry1_result = ResultCollection(test_results=[test2_crash_result])
    crash_retry1_result.crashed = True
    crash_retry2_result = ResultCollection(test_results=[test3_pass_result])
    crash_retry2_result.crashed = True
    test_retry1_result = ResultCollection(test_results=[test1_pass_result])
    test_retry2_result = ResultCollection(test_results=[test2_pass_result])
    mock_run.side_effect = [
        initial_result, crash_retry1_result, crash_retry2_result,
        test_retry1_result, test_retry2_result
    ]
    tr = test_runner.SimulatorTestRunner(
        'fake-app', 'fake-iossim', 'iPhone X', '11.4', 'out-dir', retries=3)
    tr.launch()
    self.assertEqual(len(mock_run.mock_calls), 5)
    self.assertTrue(tr.test_results['interrupted'])
    self.assertIn('test suite crash', tr.logs)
    self.assertTrue(tr.logs)

  @mock.patch('test_runner.SimulatorTestRunner.tear_down')
  @mock.patch('test_runner.SimulatorTestRunner.set_up')
  @mock.patch('test_runner.TestRunner._run')
  def test_not_crashed_if_no_crash_in_final_crash_retry(self, mock_run, _1, _2):
    test1_crash_result = TestResult('test1', TestStatus.CRASH)
    test2_crash_result = TestResult('test2', TestStatus.CRASH)
    test3_pass_result = TestResult('test3', TestStatus.PASS)
    test1_pass_result = TestResult('test1', TestStatus.PASS)
    test2_pass_result = TestResult('test2', TestStatus.PASS)
    initial_result = ResultCollection(test_results=[test1_crash_result])
    initial_result.crashed = True
    crash_retry1_result = ResultCollection(test_results=[test2_crash_result])
    crash_retry1_result.crashed = True
    crash_retry2_result = ResultCollection(test_results=[test3_pass_result])
    test_retry1_result = ResultCollection(test_results=[test1_pass_result])
    test_retry2_result = ResultCollection(test_results=[test2_pass_result])
    mock_run.side_effect = [
        initial_result, crash_retry1_result, crash_retry2_result,
        test_retry1_result, test_retry2_result
    ]
    tr = test_runner.SimulatorTestRunner(
        'fake-app', 'fake-iossim', 'iPhone X', '11.4', 'out-dir', retries=3)
    tr.launch()
    self.assertEqual(len(mock_run.mock_calls), 5)
    self.assertFalse(tr.test_results['interrupted'])
    self.assertTrue(tr.logs)

  @mock.patch('test_runner.SimulatorTestRunner.tear_down')
  @mock.patch('test_runner.SimulatorTestRunner.set_up')
  @mock.patch('test_runner.TestRunner._run')
  def test_not_crashed_if_crashed_in_failed_test_retry(self, mock_run, _1, _2):
    test1_fail_result = TestResult('test1', TestStatus.FAIL)
    initial_result = ResultCollection(test_results=[test1_fail_result])
    test1_retry1_result = ResultCollection(test_results=[test1_fail_result])
    test1_retry2_result = ResultCollection(test_results=[test1_fail_result])
    test1_retry3_result = ResultCollection()
    test1_retry3_result.crashed = True

    mock_run.side_effect = [
        initial_result, test1_retry1_result, test1_retry2_result,
        test1_retry3_result
    ]
    tr = test_runner.SimulatorTestRunner(
        'fake-app', 'fake-iossim', 'iPhone X', '11.4', 'out-dir', retries=3)
    tr.launch()
    self.assertEqual(len(mock_run.mock_calls), 4)
    self.assertFalse(tr.test_results['interrupted'])
    self.assertEqual(tr.test_results['tests']['test1']['actual'],
                     'FAIL FAIL FAIL SKIP')
    self.assertTrue(tr.logs)

  @mock.patch('test_runner.SimulatorTestRunner.tear_down')
  @mock.patch('test_runner.SimulatorTestRunner.set_up')
  @mock.patch('test_runner.TestRunner._run')
  def test_crashed_spawning_launcher_no_retry(self, mock_run, _1, _2):
    test1_crash_result = TestResult('test1', TestStatus.CRASH)
    initial_result = ResultCollection(test_results=[test1_crash_result])
    initial_result.crashed = True
    initial_result.spawning_test_launcher = True
    mock_run.side_effect = [initial_result]
    tr = test_runner.SimulatorTestRunner(
        'fake-app', 'fake-iossim', 'iPhone X', '11.4', 'out-dir', retries=3)
    tr.launch()
    self.assertEqual(len(mock_run.mock_calls), 1)
    self.assertTrue(tr.test_results['interrupted'])
    self.assertIn('test suite crash', tr.logs)
    self.assertTrue(tr.logs)


class DeviceTestRunnerTest(TestCase):
  def setUp(self):
    super(DeviceTestRunnerTest, self).setUp()

    def install_xcode(build, mac_toolchain_cmd, xcode_app_path):
      return True

    self.mock(result_sink_util.ResultSinkClient,
              'post', lambda *args, **kwargs: None)
    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(test_runner, 'install_xcode', install_xcode)
    self.mock(test_runner.subprocess,
              'check_output', lambda _: b'fake-bundle-id')
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(os, 'listdir', lambda _: [])
    self.mock(tempfile, 'mkstemp', lambda: '/tmp/tmp_file')
    self.tr = test_runner.DeviceTestRunner(
        'fake-app',
        'xcode-version',
        'xcode-build',
        'out-dir',
    )
    self.tr.xctestrun_data = {'TestTargetName': {}}


if __name__ == '__main__':
  logging.basicConfig(format='[%(asctime)s:%(levelname)s] %(message)s',
    level=logging.DEBUG, datefmt='%I:%M:%S')
  unittest.main()
