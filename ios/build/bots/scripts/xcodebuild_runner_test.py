#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcodebuild_runner.py."""

import json
import logging
import mock
import os
import subprocess  # noqa: F401
import sys
import tempfile
import unittest

import constants
import mac_util
import iossim_util
import result_sink_util
import test_apps
from test_result_util import ResultCollection, TestResult, TestStatus
import test_runner
import test_runner_test
import xcode_log_parser  # noqa: F401
import xcode_util
import xcodebuild_runner

# if the current directory is in scripts, then we need to add plugin
# path in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin')
  )
import test_plugin_service  # noqa: F401

_ROOT_FOLDER_PATH = 'root/folder'
_XCODE_BUILD_VERSION = '10B61'
_DESTINATION = 'A4E66321-177A-450A-9BA1-488D85B7278E'
_OUT_DIR = 'out/dir'
_XTEST_RUN = '/tmp/temp_file.xctestrun'
_EGTESTS_APP_PATH = f'{_ROOT_FOLDER_PATH}/any_egtests.app'
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


class XcodebuildRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner."""

  def setUp(self):
    super(XcodebuildRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(os, 'listdir', lambda _: ['any_egtests.xctest'])
    self.mock(iossim_util, 'is_device_with_udid_simulator', lambda _: False)
    self.mock(
      iossim_util, 'ensure_simulator_fully_booted', lambda *args, **kw: True
    )
    self.mock(
      result_sink_util.ResultSinkClient, 'post', lambda *args, **kwargs: None
    )
    self.mock(
      test_apps.EgtestsApp,
      'get_all_tests',
      lambda _: ['Class1/passedTest1', 'Class1/passedTest2'],
    )
    self.mock(
      test_apps.EgtestsApp, 'fill_xctest_run', lambda _1, _2: 'xctestrun'
    )
    self.mock(iossim_util, 'get_simulator', lambda _1, _2: 'sim-UUID')
    self.mock(test_apps, 'get_bundle_id', lambda _: "fake-bundle-id")
    self.mock(test_apps, 'is_running_rosetta', lambda: False)
    self.mock(test_apps.plistlib, 'dump', lambda _1, _2: '')
    self.mock(test_runner.SimulatorTestRunner, 'tear_down', lambda _: None)
    self.mock(test_runner.DeviceTestRunner, 'tear_down', lambda _: None)
    self.mock(
      xcodebuild_runner.subprocess,
      'Popen',
      lambda cmd, env, stdout, stderr: 'fake-out',
    )
    self.mock(
      test_runner,
      'print_process_output',
      lambda _, timeout, exception_checker: [],
    )
    self.mock(xcode_util, 'xctest_path', lambda _: 'fake-path')
    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(
      xcodebuild_runner.SimulatorParallelTestRunner,
      '_create_xctest_run_enum_tests',
      lambda _, include_disabled: 'fake-path',
    )
    self.mock(xcode_util, 'using_xcode_16_or_higher', lambda: False)

  def tearDown(self):
    super(XcodebuildRunnerTest, self).tearDown()

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_restartCrashed1stAttempt(self, mock_collect_results):
    egtests = test_apps.EgtestsApp(
      _EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES, constants.IOSPlatformType.IPHONEOS
    )
    crashed_collection = ResultCollection()
    crashed_collection.crashed = True
    mock_collect_results.side_effect = [
      crashed_collection,
      ResultCollection(
        test_results=[
          TestResult('Class1/passedTest1', TestStatus.PASS),
          TestResult('Class1/passedTest2', TestStatus.PASS),
        ]
      ),
    ]
    mock_test_runner = mock.MagicMock(
      spec=xcodebuild_runner.DeviceXcodeTestRunner
    )
    launch_command = xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      exception_checker=None,
      test_runner=mock_test_runner,
    )
    overall_result = launch_command.launch()
    self.assertFalse(overall_result.crashed)
    self.assertEqual(len(overall_result.all_test_names()), 2)
    self.assertEqual(
      overall_result.expected_tests(),
      set(['Class1/passedTest1', 'Class1/passedTest2']),
    )
    mock_test_runner.uninstall_apps.assert_called_once_with()

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_notRestartPassedTest(self, mock_collect_results):
    egtests = test_apps.EgtestsApp(
      _EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES, constants.IOSPlatformType.IPHONEOS
    )
    collection = ResultCollection(
      test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS),
      ]
    )
    mock_collect_results.side_effect = [collection]
    mock_test_runner = mock.MagicMock(
      spec=xcodebuild_runner.DeviceXcodeTestRunner
    )
    launch_command = xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      exception_checker=None,
      test_runner=mock_test_runner,
    )
    launch_command.launch()
    xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      exception_checker=None,
      test_runner=mock.MagicMock(),
    )
    self.assertEqual(1, len(mock_collect_results.mock_calls))
    mock_test_runner.uninstall_apps.assert_not_called()

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def test_launch_command_restart_failed_attempt(self, mock_collect_results):
    egtests = test_apps.EgtestsApp(
      _EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES, constants.IOSPlatformType.IPHONEOS
    )
    mock_collect_results.side_effect = [
      ResultCollection(
        test_results=[
          TestResult('Class1/passedTest1', TestStatus.FAIL),
          TestResult('Class1/passedTest2', TestStatus.FAIL),
        ]
      ),
      ResultCollection(
        test_results=[
          TestResult('Class1/passedTest1', TestStatus.PASS),
          TestResult('Class1/passedTest2', TestStatus.PASS),
        ]
      ),
    ]
    launch_command = xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      exception_checker=None,
      test_runner=mock.MagicMock(),
    )
    overall_result = launch_command.launch()
    self.assertEqual(len(overall_result.all_test_names()), 2)
    self.assertEqual(
      overall_result.expected_tests(),
      set(['Class1/passedTest1', 'Class1/passedTest2']),
    )

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def test_launch_command_not_restart_crashed_attempt(
    self, mock_collect_results
  ):
    """Crashed first attempt of runtime select test suite won't be retried."""
    egtests = test_apps.EgtestsApp(
      _FLAKY_EGTEST_APP_PATH,
      _ALL_EG_TEST_NAMES,
      constants.IOSPlatformType.IPHONEOS,
    )
    crashed_collection = ResultCollection()
    crashed_collection.crashed = True
    mock_collect_results.return_value = crashed_collection
    launch_command = xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      exception_checker=None,
      test_runner=mock.MagicMock(),
    )
    overall_result = launch_command.launch()
    self.assertEqual(len(overall_result.all_test_names()), 0)
    self.assertEqual(overall_result.expected_tests(), set([]))
    self.assertTrue(overall_result.crashed)

  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def test_launch_command_reset_video_plugin_before_attempt(
    self, mock_collect_results
  ):
    egtests = test_apps.EgtestsApp(
      _EGTESTS_APP_PATH, _ALL_EG_TEST_NAMES, constants.IOSPlatformType.IPHONEOS
    )
    collection = ResultCollection(
      test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS),
      ]
    )
    mock_collect_results.side_effect = [collection]
    mock_plugin_service = mock.MagicMock()
    launch_command = xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      test_plugin_service=mock_plugin_service,
      exception_checker=None,
      test_runner=mock.MagicMock(),
    )
    launch_command.launch()
    xcodebuild_runner.LaunchCommand(
      egtests,
      _DESTINATION,
      clones=1,
      retries=3,
      readline_timeout=180,
      exception_checker=None,
      test_runner=mock.MagicMock(),
    )
    self.assertEqual(1, len(mock_collect_results.mock_calls))
    mock_plugin_service.reset.assert_called_once_with()


class DeviceXcodeTestRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner.DeviceXcodeTestRunner."""

  def setUp(self):
    super(DeviceXcodeTestRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(
      test_runner,
      'get_current_xcode_info',
      lambda: {
        'version': 'test version',
        'build': 'test build',
        'path': 'test/path',
      },
    )
    self.mock(os.path, 'abspath', lambda path: f'/abs/path/to/{path}')

    self.mock(
      result_sink_util.ResultSinkClient, 'post', lambda *args, **kwargs: None
    )
    self.mock(
      test_runner.subprocess,
      'check_output',
      lambda _, stderr=None: b'fake-output',
    )
    self.mock(test_runner.subprocess, 'check_call', lambda _: b'fake-out')
    self.mock(
      test_runner.subprocess,
      'Popen',
      lambda cmd, env, stdout, stderr: 'fake-out',
    )
    self.mock(
      test_runner.TestRunner, 'set_sigterm_handler', lambda self, handler: 0
    )
    self.mock(os, 'listdir', lambda _: [])
    self.mock(
      xcodebuild_runner.subprocess,
      'Popen',
      lambda cmd, env, stdout, stderr: 'fake-out',
    )
    self.mock(
      test_runner,
      'print_process_output',
      lambda _, timeout=None, exception_checker=None: [],
    )
    self.mock(test_runner.TestRunner, 'start_proc', lambda self, cmd: 0)
    self.mock(
      test_runner.DeviceTestRunner, 'get_installed_packages', lambda self: []
    )
    self.mock(test_runner.DeviceTestRunner, 'wipe_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'retrieve_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'process_xcresult_dir', lambda _: None)
    self.mock(
      test_apps.EgtestsApp, 'fill_xctest_run', lambda _1, _2: 'xctestrun'
    )
    self.mock(
      test_apps.EgtestsApp,
      'get_all_tests',
      lambda _: ['Class1/passedTest1', 'Class1/passedTest2'],
    )
    self.mock(iossim_util, 'is_device_with_udid_simulator', lambda _: False)
    self.mock(xcode_util, 'using_xcode_15_or_higher', lambda: True)
    self.mock(xcode_util, 'using_xcode_16_or_higher', lambda: False)
    self.mock(mac_util, 'kill_usbmuxd', lambda: None)
    self.mock(xcode_util, 'xctest_path', lambda _: 'fake-path')
    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(
      xcodebuild_runner.SimulatorParallelTestRunner,
      '_create_xctest_run_enum_tests',
      lambda _, include_disabled: 'fake-path',
    )

  @mock.patch(
    'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT)
  )
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_launch(self, _, mock_result):
    """Tests launch method in DeviceXcodeTestRunner"""
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
      "fake-app-path", "fake-host-app-path", "fake-out-dir"
    )
    mock_result.return_value = ResultCollection(
      test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS),
      ]
    )
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)

  @mock.patch(
    'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT)
  )
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_unexpected_skipped_crash_reported(self, _, mock_result):
    """Tests launch method in DeviceXcodeTestRunner"""
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
      "fake-app-path", "fake-host-app-path", "fake-out-dir"
    )
    crashed_collection = ResultCollection(
      test_results=[TestResult('Class1/passedTest1', TestStatus.PASS)]
    )
    crashed_collection.crashed = True
    mock_result.return_value = crashed_collection
    self.assertFalse(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)
    tests = tr.test_results['tests']
    self.assertEqual(tests['Class1/passedTest1']['actual'], 'PASS')
    self.assertEqual(tests['Class1/passedTest2']['actual'], 'SKIP')
    self.assertEqual(tests['Class1/passedTest2']['expected'], 'PASS')

  @mock.patch(
    'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT)
  )
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_unexpected_skipped_not_reported(self, _, mock_result):
    """Unexpected skip not reported for these selecting tests at runtime."""
    crashed_collection = ResultCollection(
      test_results=[TestResult('Class1/passedTest1', TestStatus.PASS)]
    )
    crashed_collection.crashed = True
    mock_result.return_value = crashed_collection
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
      _FLAKY_EGTEST_APP_PATH, "fake-host-app-path", "fake-out-dir"
    )
    self.assertFalse(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 1)
    tests = tr.test_results['tests']
    self.assertEqual(tests['Class1/passedTest1']['actual'], 'PASS')
    # Class1/passedTest2 doesn't appear in test results.

  @mock.patch(
    'builtins.open',
    new=mock.mock_open(read_data=_ENUMERATE_DISABLED_TESTS_OUTPUT),
  )
  @mock.patch('xcodebuild_runner.isinstance', return_value=True)
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('test_apps.EgtestsApp', autospec=True)
  @mock.patch('platform.system', return_value='Darwin')
  def test_disabled_reported(self, _, mock_test_app, mock_result, __):
    """Tests launch method in DeviceXcodeTestRunner"""
    test_app = mock_test_app.return_value
    test_app.test_app_path = _EGTESTS_APP_PATH
    test_app.get_all_tests.return_value = [
      'Class1/passedTest1',
      'Class1/passedTest2',
    ]
    mock_result.return_value = ResultCollection(
      test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS),
      ]
    )
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-out-dir",
      output_disabled_tests=True,
    )
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 3)
    tests = tr.test_results['tests']
    self.assertEqual(tests['Class1/passedTest1']['actual'], 'PASS')
    self.assertEqual(tests['Class1/passedTest2']['actual'], 'PASS')
    self.assertEqual(tests['Class2/disabled_test3']['actual'], 'SKIP')
    self.assertEqual(tests['Class2/disabled_test3']['expected'], 'SKIP')

  @mock.patch(
    'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT)
  )
  def test_tear_down(self):
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
      "fake-app-path", "fake-host-app-path", "fake-out-dir"
    )
    tr.tear_down()

  def test_skip_enumerate_tests(self):
    """Tests skip_enumerate_tests avoids fetch_test_names on device runner."""
    tr = xcodebuild_runner.DeviceXcodeTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-out-dir",
      skip_enumerate_tests=True,
    )
    self.assertEqual(tr.all_eg_test_names, [])


class SimulatorParallelTestRunnerTest(test_runner_test.SimulatorTestRunnerTest):
  """Test case to test xcodebuild_runner.SimulatorParallelTestRunner"""

  def setUp(self):
    super(SimulatorParallelTestRunnerTest, self).setUp()

    def set_up(self):
      return

    self.mock(xcodebuild_runner.SimulatorParallelTestRunner, 'set_up', set_up)

    self.mock(
      test_runner.subprocess,
      'check_output',
      lambda _, stderr=None: b'fake-output',
    )
    self.mock(test_runner.subprocess, 'check_call', lambda _: b'fake-out')
    self.mock(
      test_runner.subprocess,
      'Popen',
      lambda cmd, env, stdout, stderr: 'fake-out',
    )
    self.mock(
      xcodebuild_runner.subprocess,
      'Popen',
      lambda cmd, env, stdout, stderr: 'fake-out',
    )
    self.mock(
      test_runner,
      'print_process_output',
      lambda _, timeout, exception_checker: [],
    )
    self.mock(test_runner.TestRunner, 'start_proc', lambda self, cmd: 0)
    self.mock(test_runner.TestRunner, 'retrieve_derived_data', lambda _: None)
    self.mock(test_runner.TestRunner, 'process_xcresult_dir', lambda _: None)
    self.mock(
      test_apps.EgtestsApp, 'fill_xctest_run', lambda _1, _2: 'xctestrun'
    )
    self.mock(
      test_apps.EgtestsApp,
      'get_all_tests',
      lambda _: ['Class1/passedTest1', 'Class1/passedTest2'],
    )
    self.mock(iossim_util, 'is_device_with_udid_simulator', lambda _: False)
    self.mock(os.path, 'isfile', lambda _: True)
    self.mock(
      xcodebuild_runner.SimulatorParallelTestRunner,
      '_create_xctest_run_enum_tests',
      lambda _, include_disabled: 'fake-path',
    )
    self.mock(xcode_util, 'using_xcode_16_or_higher', lambda: False)

  @mock.patch(
    'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT)
  )
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('platform.system', return_value='Darwin')
  def test_launch_egtest(self, _, mock_result):
    """Tests launch method in SimulatorParallelTestRunner"""
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-version",
      "fake-platform",
      "fake-out-dir",
    )
    mock_result.return_value = ResultCollection(
      test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS),
      ]
    )
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)

  @mock.patch(
    'builtins.open', new=mock.mock_open(read_data=_ENUMERATE_TESTS_OUTPUT)
  )
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  @mock.patch('xcodebuild_runner.TestPluginServicerWrapper')
  @mock.patch('platform.system', return_value='Darwin')
  def test_launch_egtest_with_plugin_service(
    self, _, mock_plugin_service, mock_result
  ):
    """Tests launch method in SimulatorParallelTestRunner
    with plugin service running"""
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-version",
      "fake-platform",
      "fake-out-dir",
      video_plugin_option='failed_only',
    )
    self.assertTrue(tr.test_plugin_service != None)  # noqa: E711
    tr.test_plugin_service = mock_plugin_service
    mock_result.return_value = ResultCollection(
      test_results=[
        TestResult('Class1/passedTest1', TestStatus.PASS),
        TestResult('Class1/passedTest2', TestStatus.PASS),
      ]
    )
    self.assertTrue(tr.launch())
    self.assertEqual(len(tr.test_results['tests']), 2)
    mock_plugin_service.start_server.assert_called_once_with()
    mock_plugin_service.reset.assert_called_once_with()
    mock_plugin_service.tear_down.assert_called_once_with()

  def test_skip_enumerate_tests(self):
    """Tests skip_enumerate_tests avoids fetch_test_names."""
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-version",
      "fake-platform",
      "fake-out-dir",
      skip_enumerate_tests=True,
    )
    self.assertEqual(tr.all_eg_test_names, [])

  def test_auto_skip_enumerate_tests_with_test_cases(self):
    """Tests providing test_cases auto skips fetch_test_names on single
    shard."""
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-version",
      "fake-platform",
      "fake-out-dir",
      test_cases=['Class1/passedTest1'],
    )
    self.assertEqual(tr.all_eg_test_names, [])

  def test_clones_forced_to_one_for_duo(self):
    """Tests that clones is forced to 1 for iPhone Duo simulator."""
    tr = xcodebuild_runner.SimulatorParallelTestRunner(
      "fake-app-path",
      "fake-host-app-path",
      "fake-version",
      "iPhone Duo",
      "fake-out-dir",
      clones=2,
      skip_enumerate_tests=True,
    )
    self.assertEqual(tr.clones, 1)


class PerfArtifactsCollectionTest(unittest.TestCase):
  """Tests container perf artifact pulling and HistogramSet conversion."""

  @mock.patch.object(xcodebuild_runner.SimulatorParallelTestRunner, 'set_up')
  @mock.patch(
    'test_runner.get_current_xcode_info',
    return_value={'version': '27.0', 'build': '27A266A', 'path': '/Xcode.app'},
  )
  @mock.patch(
    'iossim_util.get_platform_type_by_platform',
    return_value=constants.IOSPlatformType.IPHONEOS,
  )
  @mock.patch('test_apps.get_bundle_id', return_value='org.chromium.gtest')
  @mock.patch('iossim_util.is_device_with_udid_simulator', return_value=True)
  @mock.patch('iossim_util.get_simulator', return_value='sim-UUID')
  def test_collect_perf_artifacts_simulator(
    self,
    mock_udid,
    mock_is_sim,
    mock_bundle_id,
    mock_platform_type,
    mock_xcode_info,
    mock_set_up,
  ):
    with tempfile.TemporaryDirectory() as tmpdir:
      host_app = os.path.join(tmpdir, 'Runner.app')
      os.makedirs(host_app)
      container_dir = os.path.join(tmpdir, 'sim_container')
      docs_dir = os.path.join(container_dir, 'Documents')
      os.makedirs(docs_dir)
      out_dir = os.path.join(tmpdir, 'out')

      samples_payload = [
        {
          'benchmark': 'bling.startup',
          'name': 'Startup.ColdStartFromMain',
          'unit': 'ms_smallerIsBetter',
          'samples': [210.5, 198.2],
        }
      ]
      with open(os.path.join(docs_dir, 'perf_samples.json'), 'w') as f:
        json.dump(samples_payload, f)

      tr = xcodebuild_runner.SimulatorParallelTestRunner(
        host_app,
        host_app,
        'fake-version',
        'iPhone 15 Pro',
        out_dir,
        skip_enumerate_tests=True,
      )

      def fake_extract(bundle_id=None):
        self.assertEqual(bundle_id, 'org.chromium.gtest')
        dest_docs = os.path.join(out_dir, 'Documents')
        os.makedirs(dest_docs, exist_ok=True)
        with open(os.path.join(dest_docs, 'perf_samples.json'), 'w') as f:
          json.dump(samples_payload, f)

      with mock.patch.object(tr, 'extract_test_data', side_effect=fake_extract):
        tr._collect_perf_artifacts()

      perf_results_path = os.path.join(
        out_dir, 'bling.startup', 'perf_results.json'
      )
      self.assertTrue(os.path.exists(perf_results_path))
      with open(perf_results_path, 'r') as f:
        histograms = json.load(f)
      self.assertEqual(len(histograms), 1)
      self.assertEqual(histograms[0]['name'], 'Startup.ColdStartFromMain')
      self.assertEqual(histograms[0]['unit'], 'ms_smallerIsBetter')
      self.assertEqual(histograms[0]['sampleValues'], [210.5, 198.2])
      self.assertIn('running', histograms[0])
      diagnostics = histograms[0]['diagnostics']
      self.assertEqual(diagnostics['benchmarks']['values'], ['bling.startup'])
      self.assertEqual(
        diagnostics['stories']['values'], ['Startup.ColdStartFromMain']
      )

  def test_convert_samples_without_benchmark_still_has_story(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      samples_path = os.path.join(tmpdir, 'perf_samples.json')
      with open(samples_path, 'w') as f:
        json.dump(
          [
            {
              'name': 'Memory.Footprint',
              'unit': 'sizeInBytes_smallerIsBetter',
              'samples': [1024],
              'story': 'about_blank',
            }
          ],
          f,
        )

      runner_class = xcodebuild_runner.SimulatorParallelTestRunner
      runner_class._convert_samples_to_histogram_sets(samples_path, tmpdir)

      with open(os.path.join(tmpdir, 'perf_results.json'), 'r') as f:
        histograms = json.load(f)
      self.assertEqual(len(histograms), 1)
      diagnostics = histograms[0]['diagnostics']
      self.assertNotIn('benchmarks', diagnostics)
      self.assertEqual(diagnostics['stories']['values'], ['about_blank'])

  def test_merge_perf_samples_appends_matching_metrics(self):
    saved = [
      {'benchmark': 'bling.startup', 'name': 'A', 'unit': 'ms', 'samples': [1]},
      {'name': 'B', 'unit': 'ms', 'samples': [2]},
    ]
    new = [
      {'benchmark': 'bling.startup', 'name': 'A', 'unit': 'ms', 'samples': [3]},
      {'benchmark': 'bling.memory', 'name': 'A', 'unit': 'ms', 'samples': [4]},
      {'benchmark': '', 'name': 'B', 'unit': 'ms', 'samples': [5]},
    ]
    merged = xcodebuild_runner.SimulatorParallelTestRunner._merge_perf_samples(
      saved, new
    )
    self.assertEqual(
      [(e.get('benchmark'), e['name'], e['samples']) for e in merged],
      [
        ('bling.startup', 'A', [1, 3]),
        (None, 'B', [2, 5]),
        ('bling.memory', 'A', [4]),
      ],
    )
    # Inputs are not modified.
    self.assertEqual(saved[0]['samples'], [1])

  def test_launch_command_saves_perf_samples_only_for_benchmarks(self):
    runner = mock.Mock()
    launch_command = object.__new__(xcodebuild_runner.LaunchCommand)
    launch_command.test_runner = runner

    runner.benchmark_name = None
    launch_command._save_perf_samples_before_reset()
    runner.save_perf_samples.assert_not_called()

    runner.benchmark_name = 'bling'
    launch_command._save_perf_samples_before_reset()
    runner.save_perf_samples.assert_called_once_with()

  @mock.patch.object(xcodebuild_runner.DeviceXcodeTestRunner, 'set_up')
  @mock.patch(
    'test_runner.get_current_xcode_info',
    return_value={'version': '27.0', 'build': '27A266A', 'path': '/Xcode.app'},
  )
  @mock.patch('test_apps.get_bundle_id', return_value='org.chromium.gtest')
  @mock.patch.object(
    test_runner.subprocess, 'check_output', return_value=b'device-UUID'
  )
  def test_collect_perf_artifacts_keeps_samples_from_wiped_attempts(
    self,
    mock_udid,
    mock_bundle_id,
    mock_xcode_info,
    mock_set_up,
  ):
    """Samples saved before a retry's reinstall survive to the final output."""
    with tempfile.TemporaryDirectory() as tmpdir:
      host_app = os.path.join(tmpdir, 'ios_chrome_eg2tests.app')
      os.makedirs(os.path.join(host_app, 'PlugIns', 'module.xctest'))
      out_dir = os.path.join(tmpdir, 'out')
      # What the app container holds at each pull: attempt 1 before the
      # reinstall, then the retry's fresh install, then nothing on failure.
      container_states = [
        [
          {
            'benchmark': 'bling.startup',
            'name': 'Startup',
            'unit': 'ms_smallerIsBetter',
            'samples': [200.0],
          }
        ],
        [
          {
            'benchmark': 'bling.memory',
            'name': 'Memory',
            'unit': 'sizeInBytes_smallerIsBetter',
            'samples': [1024.0],
          }
        ],
        None,
      ]

      def fake_device_extract(runner_self, bundle_id=None):
        state = container_states.pop(0)
        if state is None:
          return
        docs_dir = os.path.join(runner_self.out_dir, 'Documents')
        os.makedirs(docs_dir, exist_ok=True)
        with open(os.path.join(docs_dir, 'perf_samples.json'), 'w') as f:
          json.dump(state, f)

      tr = xcodebuild_runner.DeviceXcodeTestRunner(
        host_app,
        host_app,
        out_dir,
        skip_enumerate_tests=True,
      )
      with mock.patch.object(
        xcodebuild_runner.test_runner.DeviceTestRunner,
        'extract_test_data',
        new=fake_device_extract,
      ):
        tr.save_perf_samples()  # Before the retry's reinstall.
        tr._collect_perf_artifacts()  # After the last attempt.
        # A failed pull must not resurrect the previous pull's file.
        tr.save_perf_samples()

      for benchmark, name in (
        ('bling.startup', 'Startup'),
        ('bling.memory', 'Memory'),
      ):
        with open(os.path.join(out_dir, benchmark, 'perf_results.json')) as f:
          histograms = json.load(f)
        self.assertEqual([h['name'] for h in histograms], [name])
      self.assertEqual(len(tr._saved_perf_samples), 2)

  @mock.patch.object(xcodebuild_runner.DeviceXcodeTestRunner, 'set_up')
  @mock.patch(
    'test_runner.get_current_xcode_info',
    return_value={'version': '27.0', 'build': '27A266A', 'path': '/Xcode.app'},
  )
  @mock.patch('test_apps.get_bundle_id', return_value='org.chromium.gtest')
  @mock.patch.object(
    test_runner.subprocess, 'check_output', return_value=b'device-UUID'
  )
  def test_collect_perf_artifacts_device(
    self,
    mock_udid,
    mock_bundle_id,
    mock_xcode_info,
    mock_set_up,
  ):
    """Tests device artifact collection and HistogramSet conversion."""
    with tempfile.TemporaryDirectory() as tmpdir:
      host_app = os.path.join(tmpdir, 'ios_chrome_eg2tests.app')
      os.makedirs(os.path.join(host_app, 'PlugIns', 'module.xctest'))
      out_dir = os.path.join(tmpdir, 'out')

      samples_payload = [
        {
          'name': 'Memory.PhysFootprint',
          'unit': 'sizeInBytes_smallerIsBetter',
          'samples': [104857600.0, 106954752.0],
        }
      ]

      def fake_device_extract(runner_self, bundle_id=None):
        self.assertEqual(bundle_id, 'org.chromium.gtest')
        docs_dir = os.path.join(runner_self.out_dir, 'Documents')
        os.makedirs(docs_dir, exist_ok=True)
        with open(os.path.join(docs_dir, 'perf_samples.json'), 'w') as f:
          json.dump(samples_payload, f)

      tr = xcodebuild_runner.DeviceXcodeTestRunner(
        host_app,
        host_app,
        out_dir,
        skip_enumerate_tests=True,
      )
      with mock.patch.object(
        xcodebuild_runner.test_runner.DeviceTestRunner,
        'extract_test_data',
        new=fake_device_extract,
      ):
        tr._collect_perf_artifacts()

      perf_results_path = os.path.join(out_dir, 'perf_results.json')
      self.assertTrue(os.path.exists(perf_results_path))
      with open(perf_results_path, 'r') as f:
        histograms = json.load(f)
      self.assertEqual(len(histograms), 1)
      self.assertEqual(histograms[0]['name'], 'Memory.PhysFootprint')
      self.assertEqual(histograms[0]['unit'], 'sizeInBytes_smallerIsBetter')

  @mock.patch.object(xcodebuild_runner.SimulatorParallelTestRunner, 'set_up')
  @mock.patch(
    'test_runner.get_current_xcode_info',
    return_value={'version': '27.0', 'build': '27A266A', 'path': '/Xcode.app'},
  )
  @mock.patch(
    'iossim_util.get_platform_type_by_platform',
    return_value=constants.IOSPlatformType.IPHONEOS,
  )
  @mock.patch('test_apps.get_bundle_id', return_value='org.chromium.gtest')
  @mock.patch('iossim_util.get_simulator', return_value='sim-UUID')
  def test_collect_perf_artifacts_ignores_stale_documents(
    self,
    mock_udid,
    mock_bundle_id,
    mock_platform_type,
    mock_xcode_info,
    mock_set_up,
  ):
    """A leftover Documents/perf_samples.json isn't reused if the pull fails.

    Reusing it would double-count samples already saved by an earlier pull.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
      host_app = os.path.join(tmpdir, 'ios_chrome_eg2tests.app')
      os.makedirs(host_app)
      out_dir = os.path.join(tmpdir, 'out')
      docs_dir = os.path.join(out_dir, 'Documents')
      os.makedirs(docs_dir)
      with open(os.path.join(docs_dir, 'perf_samples.json'), 'w') as f:
        json.dump(
          [
            {
              'name': 'Startup.ColdStartFromMain',
              'unit': 'ms_smallerIsBetter',
              'samples': [180.0],
            }
          ],
          f,
        )

      tr = xcodebuild_runner.SimulatorParallelTestRunner(
        host_app,
        host_app,
        'fake-version',
        'iPhone 15 Pro',
        out_dir,
        skip_enumerate_tests=True,
        benchmark_name='bling.startup',
      )
      tr._collect_perf_artifacts()

      self.assertFalse(
        os.path.exists(os.path.join(out_dir, 'perf_results.json'))
      )
      self.assertFalse(
        os.path.exists(os.path.join(docs_dir, 'perf_samples.json'))
      )


if __name__ == '__main__':
  logging.basicConfig(
    format='[%(asctime)s:%(levelname)s] %(message)s',
    level=logging.DEBUG,
    datefmt='%I:%M:%S',
  )
  unittest.main()
