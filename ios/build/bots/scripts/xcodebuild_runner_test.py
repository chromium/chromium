#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcodebuild_runner.py."""

import logging
import mock
import os
import unittest
import sys

import mac_util
import iossim_util
import result_sink_util
import test_apps
from test_result_util import ResultCollection, TestResult, TestStatus
import test_runner
import test_runner_test
import xcode_log_parser
import xcode_util
import xcodebuild_runner

# if the current directory is in scripts, then we need to add plugin
# path in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
import test_plugin_service

_ROOT_FOLDER_PATH = 'root/folder'
_XCODE_BUILD_VERSION = '10B61'
_DESTINATION = 'A4E66321-177A-450A-9BA1-488D85B7278E'
_OUT_DIR = 'out/dir'
_XTEST_RUN = '/tmp/temp_file.xctestrun'
_EGTESTS_APP_PATH = '%s/any_egtests.app' % _ROOT_FOLDER_PATH
_ALL_EG_TEST_NAMES = [('Class1', 'passedTest1'), ('Class1', 'passedTest2')]
_FLAKY_EGTEST_APP_PATH = 'path/to/ios_chrome_flaky_eg2test_module.app'

_ENUMERATE_TESTS_OUTPUT = """
{
  "errors" : [

  ],
  "values" : [
    {
      "children" : [
        {
          "children" : [
            {
              "children" : [

              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "BaseEarlGreyTestCase"
            },
            {
              "children" : [

              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "ChromeTestCase"
            },
            {
              "children" : [
                {
                  "children" : [

                  ],
                  "disabled" : false,
                  "kind" : "test",
                  "name" : "passedTest1"
                },
                {
                  "children" : [

                  ],
                  "disabled" : false,
                  "kind" : "test",
                  "name" : "passedTest2"
                }
              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "Class1"
            },
            {
              "children" : [

              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "WebHttpServerChromeTestCase"
            }
          ],
          "disabled" : false,
          "kind" : "target",
          "name" : "ios_chrome_ui_eg2tests_module-Runner_module"
        }
      ],
      "disabled" : false,
      "kind" : "plan",
      "name" : ""
    }
  ]
}
"""

_ENUMERATE_DISABLED_TESTS_OUTPUT = """
{
  "errors" : [

  ],
  "values" : [
    {
      "children" : [
        {
          "children" : [
            {
              "children" : [

              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "BaseEarlGreyTestCase"
            },
            {
              "children" : [

              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "ChromeTestCase"
            },
            {
              "children" : [
                {
                  "children" : [

                  ],
                  "disabled" : false,
                  "kind" : "test",
                  "name" : "disabled_test3"
                }
              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "Class2"
            },
            {
              "children" : [

              ],
              "disabled" : false,
              "kind" : "class",
              "name" : "WebHttpServerChromeTestCase"
            }
          ],
          "disabled" : false,
          "kind" : "target",
          "name" : "ios_chrome_ui_eg2tests_module-Runner_module"
        }
      ],
      "disabled" : false,
      "kind" : "plan",
      "name" : ""
    }
  ]
}
"""

class XCodebuildRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner."""

  def setUp(self):
    super(XCodebuildRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(os, 'listdir', lambda _: ['any_egtests.xctest'])
    self.mock(iossim_util, 'is_device_with_udid_simulator', lambda _: False)
    self.mock(result_sink_util.ResultSinkClient,
              'post', lambda *args, **kwargs: None)
    self.mock(test_apps.EgtestsApp, 'get_all_tests',
              lambda _: ['Class1/passedTest1', 'Class1/passedTest2'])
    self.mock(test_apps.EgtestsApp, 'fill_xctest_run',
              lambda _1, _2: 'xctestrun')
    self.mock(iossim_util, 'get_simulator', lambda _1, _2: 'sim-UUID')
    self.mock(test_apps, 'get_bundle_id', lambda _: "fake-bundle-id")
    self.mock(test_apps, 'is_running_rosetta', lambda: False)
    self.mock(test_apps.plistlib, 'dump', lambda _1, _2: '')
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', lambda _: None)
    self.mock(test_runner.DeviceTestRunner, 'tear_down', lambda _: None)
    self.mock(xcodebuild_runner.subprocess,
              'Popen', lambda cmd, env, stdout, stderr: 'fake-out')
    self.mock(test_runner, 'print_process_output', lambda _, timeout: [])
    self.mock(xcode_util, 'xctest_path', lambda _: 'fake-path')
    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(xcodebuild_runner.SimulatorParallelTestRunner,
              '_create_xctest_run_enum_tests',
              lambda _, include_disabled: 'fake-path')

  def tearDown(self):
    super(XCodebuildRunnerTest, self).tearDown()

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_restartCrashed1stAttempt(self, mock_collect_results):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES)
    crashed_collection = ResultCollection()
    crashed_collection.crashed = True
    mock_collect_results.side_effect = [
        crashed_collection,
        ResultCollection(test_results=[
            TestResult('Class1/passedTest1', TestStatus.PASS),
            TestResult('Class1/passedTest2', TestStatus.PASS)
        ])
    ]
    launch_command = xcodebuild_runner.LaunchCommand(
        egtests, _DESTINATION, clones=1, retries=3, readline_timeout=180)
    overall_result = launch_command.launch()
    self.assertFalse(overall_result.crashed)
    self.assertEqual(len(overall_result.all_test_names()), 2)
    self.assertEqual(overall_result.expected_tests(),
                     set(['Class1/passedTest1', 'Class1/passedTest2']))

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_notRestartPassedTest(self, mock_collect_results):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES)
    collection = ResultCollection(test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS)
    ])
    mock_collect_results.side_effect = [collection]
    launch_command = xcodebuild_runner.LaunchCommand(
        egtests, _DESTINATION, clones=1, retries=3, readline_timeout=180)
    launch_command.launch()
    xcodebuild_runner.LaunchCommand(
        egtests, _DESTINATION, clones=1, retries=3, readline_timeout=180)
    self.assertEqual(1, len(mock_collect_results.mock_calls))

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def test_launch_command_restart_failed_attempt(self, mock_collect_results):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES)
    mock_collect_results.side_effect = [
        ResultCollection(test_results=[
            TestResult('Class1/passedTest1', TestStatus.FAIL),
            TestResult('Class1/passedTest2', TestStatus.FAIL)
        ]),
        ResultCollection(test_results=[
            TestResult('Class1/passedTest1', TestStatus.PASS),
            TestResult('Class1/passedTest2', TestStatus.PASS)
        ])
    ]
    launch_command = xcodebuild_runner.LaunchCommand(
        egtests, _DESTINATION, clones=1, retries=3, readline_timeout=180)
    overall_result = launch_command.launch()
    self.assertEqual(len(overall_result.all_test_names()), 2)
    self.assertEqual(overall_result.expected_tests(),
                     set(['Class1/passedTest1', 'Class1/passedTest2']))

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def test_launch_command_not_restart_crashed_attempt(self,
                                                      mock_collect_results):
    """Crashed first attempt of runtime select test suite won't be retried."""
    egtests = test_apps.EgtestsApp(_FLAKY_EGTEST_APP_PATH, _ALL_EG_TEST_NAMES)
    crashed_collection = ResultCollection()
    crashed_collection.crashed = True
    mock_collect_results.return_value = crashed_collection
    launch_command = xcodebuild_runner.LaunchCommand(
        egtests, _DESTINATION, clones=1, retries=3, readline_timeout=180)
    overall_result = launch_command.launch()
    self.assertEqual(len(overall_result.all_test_names()), 0)
    self.assertEqual(overall_result.expected_tests(), set([]))
    self.assertTrue(overall_result.crashed)

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def test_launch_command_reset_video_plugin_before_attempt(
      self, mock_collect_results):
    egtests = test_apps.EgtestsApp(_EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES)
    collection = ResultCollection(test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS)
    ])
    mock_collect_results.side_effect = [collection]
    mock_plugin_service = mock.MagicMock()
    launch_command = xcodebuild_runner.LaunchCommand(
        egtests,
        _DESTINATION,
        clones=1,
        retries=3,
        readline_timeout=180,
        test_plugin_service=mock_plugin_service)
    launch_command.launch()
    xcodebuild_runner.LaunchCommand(
        egtests, _DESTINATION, clones=1, retries=3, readline_timeout=180)
    self.assertEqual(1, len(mock_collect_results.mock_calls))
    mock_plugin_service.reset.assert_called_once_with()


class DeviceXcodeTestRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner.DeviceXcodeTestRunner."""

  def setUp(self):
    super(DeviceXcodeTestRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(test_runner, 'get_current_xcode_info', lambda: {
        'version': 'test version', 'build': 'test build', 'path': 'test/path'})
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)

    self.mock(result_sink_util.ResultSinkClient,
              'post', lambda *args, **kwargs: None)
    self.mock(
        test_runner.subprocess,
        'check_output',
        lambda _, stderr=None: b'fake-output')
    self.mock(test_runner.subprocess, 'check_call', lambda _: b'fake-out')
    self.mock(test_runner.subprocess,
              'Popen', lambda cmd, env, stdout, stderr: 'fake-out')
    self.mock(test_runner.TestRunner, 'set_sigterm_handler',
              lambda self, handler: 0)
    self.mock(os, 'listdir', lambda _: [])
    self.mock(xcodebuild_runner.subprocess,
              'Popen', lambda cmd, env, stdout, stderr: 'fake-out')
    self.mock(test_runner, 'print_process_output', lambda _, timeout: [])
    self.mock(test_runner.TestRunner, 'start_proc', lambda self, cmd: 0)
    self.mock(test_runner.DeviceTestRunner, 'get_installed_packages',
              lambda self: [])
    self.mock(test_runner.DeviceTestRunner, 'wipe_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'retrieve_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'process_xcresult_dir', lambda _: None)
    self.mock(test_apps.EgtestsApp,
              'fill_xctest_run', lambda _1, _2: 'xctestrun')
    self.mock(test_apps.EgtestsApp, 'get_all_tests',
              lambda _: ['Class1/passedTest1', 'Class1/passedTest2'])
    self.mock(iossim_util, 'is_device_with_udid_simulator', lambda _: False)
    self.mock(xcode_util, 'using_xcode_15_or_higher', lambda: True)
    self.mock(mac_util, 'kill_usbmuxd', lambda: None)
    self.mock(xcode_util, 'xctest_path', lambda _: 'fake-path')
    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(xcodebuild_runner.SimulatorParallelTestRunner,
              '_create_xctest_run_enum_tests',
              lambda _, include_disabled: 'fake-path')

  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT))
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_launch(self, _, mock_result):
    """Tests launch method in DeviceXcodeTestRunner"""
    tr = xcodebuild_runner.DeviceXcodeTestRunner("fake-app-path",
                                                 "fake-host-app-path",
                                                 "fake-out-dir")
    mock_result.return_value = ResultCollection(test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS)
    ])
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)

  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT))
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_unexpected_skipped_crash_reported(self, _, mock_result):
    """Tests launch method in DeviceXcodeTestRunner"""
    tr = xcodebuild_runner.DeviceXcodeTestRunner("fake-app-path",
                                                 "fake-host-app-path",
                                                 "fake-out-dir")
    crashed_collection = ResultCollection(
        test_results=[TestResult('Class1/passedTest1', TestStatus.PASS)])
    crashed_collection.crashed = True
    mock_result.return_value = crashed_collection
    self.assertFalse(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)
    tests = tr.test_results['tests']
    self.assertEqual(tests['Class1/passedTest1']['actual'], 'PASS')
    self.assertEqual(tests['Class1/passedTest2']['actual'], 'SKIP')
    self.assertEqual(tests['Class1/passedTest2']['expected'], 'PASS')

  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT))
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_unexpected_skipped_not_reported(self, _, mock_result):
    """Unexpected skip not reported for these selecting tests at runtime."""
    crashed_collection = ResultCollection(
        test_results=[TestResult('Class1/passedTest1', TestStatus.PASS)])
    crashed_collection.crashed = True
    mock_result.return_value = crashed_collection
    tr = xcodebuild_runner.DeviceXcodeTestRunner(_FLAKY_EGTEST_APP_PATH,
                                                 "fake-host-app-path",
                                                 "fake-out-dir")
    self.assertFalse(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 1)
    tests = tr.test_results['tests']
    self.assertEqual(tests['Class1/passedTest1']['actual'], 'PASS')
    # Class1/passedTest2 doesn't appear in test results.

  @mock.patch(
      'builtins.open',
      new=mock.mock_open(read_data=_ENUMERATE_DISABLED_TESTS_OUTPUT))
  @mock.patch('xcodebuild_runner.isinstance', return_value=True)
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('test_apps.EgtestsApp', autospec=True)
  @mock.patch('platform.system', return_value='Darwin')
  def test_disabled_reported(self, _, mock_test_app, mock_result, __):
    """Tests launch method in DeviceXcodeTestRunner"""
    test_app = mock_test_app.return_value
    test_app.test_app_path = _EGTESTS_APP_PATH
    test_app.get_all_tests.return_value = [
        'Class1/passedTest1', 'Class1/passedTest2'
    ]
    mock_result.return_value = ResultCollection(test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS)
    ])
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
        "fake-app-path",
        "fake-host-app-path",
        "fake-out-dir",
        output_disabled_tests=True)
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 3)
    tests = tr.test_results['tests']
    self.assertEqual(tests['Class1/passedTest1']['actual'], 'PASS')
    self.assertEqual(tests['Class1/passedTest2']['actual'], 'PASS')
    self.assertEqual(tests['Class2/disabled_test3']['actual'], 'SKIP')
    self.assertEqual(tests['Class2/disabled_test3']['expected'], 'SKIP')

  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT))
  def test_tear_down(self):
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
        "fake-app-path", "fake-host-app-path", "fake-out-dir")
    tr.tear_down()


class SimulatorParallelTestRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner.SimulatorParallelTestRunner"""

  def setUp(self):
    super(SimulatorParallelTestRunnerTest, self).setUp()
    self.mock(iossim_util, 'get_simulator', lambda _1, _2: 'sim-UUID')

    def set_up(self):
      return

    self.mock(xcodebuild_runner.SimulatorParallelTestRunner, 'set_up', set_up)
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(
        test_runner, 'get_current_xcode_info', lambda: {
            'version': 'test version',
            'build': 'test build',
            'path': 'test/path'
        })
    self.mock(os.path, 'abspath', lambda path: '/abs/path/to/%s' % path)

    self.mock(result_sink_util.ResultSinkClient,
              'post', lambda *args, **kwargs: None)
    self.mock(
        test_runner.subprocess,
        'check_output',
        lambda _, stderr=None: b'fake-output')
    self.mock(test_runner.subprocess, 'check_call', lambda _: b'fake-out')
    self.mock(test_runner.subprocess,
              'Popen', lambda cmd, env, stdout, stderr: 'fake-out')
    self.mock(test_runner.TestRunner,
              'set_sigterm_handler', lambda self, handler: 0)
    self.mock(os, 'listdir', lambda _: [])
    self.mock(xcodebuild_runner.subprocess,
              'Popen', lambda cmd, env, stdout, stderr: 'fake-out')
    self.mock(test_runner, 'print_process_output', lambda _, timeout: [])
    self.mock(test_runner.TestRunner, 'start_proc', lambda self, cmd: 0)
    self.mock(test_runner.TestRunner, 'retrieve_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'process_xcresult_dir', lambda _: None)
    self.mock(test_apps.EgtestsApp, 'fill_xctest_run',
              lambda _1, _2: 'xctestrun')
    self.mock(test_apps.EgtestsApp, 'get_all_tests',
              lambda _: ['Class1/passedTest1', 'Class1/passedTest2'])
    self.mock(iossim_util, 'is_device_with_udid_simulator', lambda _: False)
    self.mock(xcode_util, 'xctest_path', lambda _: 'fake-path')
    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(xcodebuild_runner.SimulatorParallelTestRunner,
              '_create_xctest_run_enum_tests',
              lambda _, include_disabled: 'fake-path')

  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT))
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_launch_egtest(self, _, mock_result):
    """Tests launch method in SimulatorParallelTestRunner"""
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
        "fake-app-path", "fake-host-app-path", "fake-iossim_path",
        "fake-version", "fake-platform", "fake-out-dir")
    mock_result.return_value = ResultCollection(test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS)
    ])
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)

  @mock.patch(
      'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT))
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('xcodebuild_runner.TestPluginServicerWrapper')
  @mock.patch('platform.system', return_value='Darwin')
  def test_launch_egtest_with_plugin_service(self, _, mock_plugin_service,
                                             mock_result):
    """ Tests launch method in SimulatorParallelTestRunner
        with plugin service running """
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
        "fake-app-path",
        "fake-host-app-path",
        "fake-iossim_path",
        "fake-version",
        "fake-platform",
        "fake-out-dir",
        video_plugin_option='failed_only')
    self.assertTrue(tr.test_plugin_service != None)
    tr.test_plugin_service = mock_plugin_service
    mock_result.return_value = ResultCollection(test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS)
    ])
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)
    mock_plugin_service.start_server.assert_called_once_with()
    mock_plugin_service.reset.assert_called_once_with()
    mock_plugin_service.tear_down.assert_called_once_with()


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')
  unittest.main()
