# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for xcodebuild_runner.py."""

import mock
import os

import plistlib
import shutil
import tempfile
import test_runner
import test_runner_test
import xcode_log_parser
import xcodebuild_runner


_ROOT_FOLDER_PATH = 'root/folder'
_XCODE_BUILD_VERSION = '10B61'
_DESTINATION = 'platform=iOS Simulator,OS=12.0,name=iPhone X'
_OUT_DIR = 'out/dir'
_XTEST_RUN = '/tmp/temp_file.xctestrun'
_EGTESTS_APP_PATH = '%s/any_egtests.app' % _ROOT_FOLDER_PATH


class XCodebuildRunnerTest(test_runner_test.TestCase):
  """Test case to test xcodebuild_runner."""

  def setUp(self):
    super(XCodebuildRunnerTest, self).setUp()
    self.mock(os.path, 'exists', lambda _: True)
    self.mock(xcode_log_parser.XcodeLogParser, 'copy_screenshots',
              lambda _1, _2: None)
    self.mock(os, 'listdir', lambda _: ['any_egtests.xctest'])
    self.mock(xcodebuild_runner, 'get_all_tests',
              lambda _1, _2: ['Class1/passedTest1', 'Class1/passedTest2'])
    self.tmpdir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.tmpdir, ignore_errors=True)
    super(XCodebuildRunnerTest, self).tearDown()

  def fake_launch_attempt(self, obj, statuses):
    attempt = [0]
    info_plist_statuses = {
        'not_started': {
            'Actions': [{'ActionResult': {}}],
            'TestsCount': 0, 'TestsFailedCount': 0
        },
        'fail': {
            'Actions': [
                {'ActionResult': {
                    'TestSummaryPath': '1_Test/TestSummaries.plist'}
                }
            ],
            'TestsCount': 99,
            'TestsFailedCount': 1},
        'pass': {
            'Actions': [
                {'ActionResult': {
                    'TestSummaryPath': '1_Test/TestSummaries.plist'}
                }
            ],
            'TestsCount': 100,
            'TestsFailedCount': 0}
    }

    test_summary = {
        'TestableSummaries': [
            {'TargetName': 'egtests',
             'Tests': [
                 {'Subtests': [
                     {'Subtests': [
                         {'Subtests': [
                             {'TestIdentifier': 'passed_test',
                              'TestStatus': 'Success'
                             }
                         ]
                         }
                     ]
                     }
                 ]
                 }
             ]
            }
        ]
        }

    def the_fake(cmd, attempt_outdir):
      index = attempt[0]
      attempt[0] += 1
      self.assertEqual(os.path.join(self.tmpdir, 'attempt_%d' % index),
                       attempt_outdir)
      self.assertEqual(1, cmd.count(attempt_outdir))
      os.mkdir(attempt_outdir)
      with open(os.path.join(attempt_outdir, 'Info.plist'), 'w') as f:
        plistlib.writePlist(info_plist_statuses[statuses[index]], f)
      summary_folder = os.path.join(attempt_outdir, '1_Test')
      os.mkdir(summary_folder)
      with open(os.path.join(summary_folder, 'TestSummaries.plist'), 'w') as f:
        plistlib.writePlist(test_summary, f)
      return (-6, 'Output for attempt_%d' % index)

    obj.launch_attempt = the_fake

  def testEgtests_not_found_egtests_app(self):
    self.mock(os.path, 'exists', lambda _: False)
    with self.assertRaises(test_runner.AppNotFoundError):
      xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)

  def testEgtests_not_found_plugins(self):
    egtests = xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)
    self.mock(os.path, 'exists', lambda _: False)
    with self.assertRaises(test_runner.PlugInsNotFoundError):
      egtests._xctest_path()

  def testEgtests_found_xctest(self):
    self.assertEqual('/PlugIns/any_egtests.xctest',
                     xcodebuild_runner.EgtestsApp(
                         _EGTESTS_APP_PATH)._xctest_path())

  @mock.patch('os.listdir', autospec=True)
  def testEgtests_not_found_xctest(self, mock_listdir):
    mock_listdir.return_value = ['random_file']
    egtest = xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)
    with self.assertRaises(test_runner.XCTestPlugInNotFoundError):
      egtest._xctest_path()

  def testEgtests_xctestRunNode_without_filter(self):
    egtest_node = xcodebuild_runner.EgtestsApp(
        _EGTESTS_APP_PATH).xctestrun_node()['any_egtests_module']
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  def testEgtests_xctestRunNode_with_filter_only_identifiers(self):
    filtered_tests = ['TestCase1/testMethod1', 'TestCase1/testMethod2',
                      'TestCase2/testMethod1', 'TestCase1/testMethod2']
    egtest_node = xcodebuild_runner.EgtestsApp(
        _EGTESTS_APP_PATH, included_tests=filtered_tests).xctestrun_node()[
            'any_egtests_module']
    self.assertEqual(filtered_tests, egtest_node['OnlyTestIdentifiers'])
    self.assertNotIn('SkipTestIdentifiers', egtest_node)

  def testEgtests_xctestRunNode_with_filter_skip_identifiers(self):
    skipped_tests = ['TestCase1/testMethod1', 'TestCase1/testMethod2',
                     'TestCase2/testMethod1', 'TestCase1/testMethod2']
    egtest_node = xcodebuild_runner.EgtestsApp(
        _EGTESTS_APP_PATH, excluded_tests=skipped_tests
        ).xctestrun_node()['any_egtests_module']
    self.assertEqual(skipped_tests, egtest_node['SkipTestIdentifiers'])
    self.assertNotIn('OnlyTestIdentifiers', egtest_node)

  @mock.patch('xcodebuild_runner.LaunchCommand.fill_xctest_run', autospec=True)
  def testLaunchCommand_command(self, mock_fill_xctestrun):
    mock_fill_xctestrun.return_value = _XTEST_RUN
    mock_egtest = mock.MagicMock(spec=xcodebuild_runner.EgtestsApp)
    type(mock_egtest).egtests_path = mock.PropertyMock(
        return_value=_EGTESTS_APP_PATH)
    cmd = xcodebuild_runner.LaunchCommand(
        mock_egtest, _DESTINATION, shards=3, retries=1, out_dir=_OUT_DIR)
    self.assertEqual(['xcodebuild', 'test-without-building',
                      '-xctestrun', '/tmp/temp_file.xctestrun',
                      '-destination',
                      'platform=iOS Simulator,OS=12.0,name=iPhone X',
                      '-resultBundlePath', 'out/dir',
                      '-parallel-testing-enabled', 'YES',
                      '-parallel-testing-worker-count', '3'],
                     cmd.command(egtests_app=mock_egtest,
                                 out_dir=_OUT_DIR,
                                 destination=_DESTINATION,
                                 shards=3))

  @mock.patch('plistlib.writePlist', autospec=True)
  @mock.patch('os.path.join', autospec=True)
  @mock.patch('test_runner.get_current_xcode_info', autospec=True)
  def testFill_xctest_run(self, xcode_version, mock_path_join, _):
    mock_path_join.return_value = _XTEST_RUN
    mock_egtest = mock.MagicMock(spec=xcodebuild_runner.EgtestsApp)
    xcode_version.return_value = {'version': '10.2.1'}
    launch_command = xcodebuild_runner.LaunchCommand(
        mock_egtest, _DESTINATION, shards=1, retries=1, out_dir=_OUT_DIR)
    self.assertEqual(_XTEST_RUN, launch_command.fill_xctest_run(mock_egtest))
    self.assertEqual([mock.call.xctestrun_node()], mock_egtest.method_calls)

  def testFill_xctest_run_exception(self):
    with self.assertRaises(test_runner.AppNotFoundError):
      xcodebuild_runner.LaunchCommand([], 'destination', shards=1, retries=1,
                                      out_dir=_OUT_DIR).fill_xctest_run([])

  @mock.patch('test_runner.get_current_xcode_info', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_restartFailed1stAttempt(self, mock_collect_results,
                                                xcode_version):
    egtests = xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)
    xcode_version.return_value = {'version': '10.2.1'}
    mock_collect_results.side_effect = [
        {'failed': {'TESTS_DID_NOT_START': ['not started']}, 'passed': []},
        {'failed': {}, 'passed': ['Class1/passedTest1', 'Class1/passedTest2']}
    ]
    launch_command = xcodebuild_runner.LaunchCommand(egtests,
                                                     _DESTINATION,
                                                     shards=1,
                                                     retries=3,
                                                     out_dir=self.tmpdir)
    self.fake_launch_attempt(launch_command, ['not_started', 'pass'])
    launch_command.launch()
    self.assertEqual(2, len(launch_command.test_results))

  @mock.patch('test_runner.get_current_xcode_info', autospec=True)
  @mock.patch('xcode_log_parser.XcodeLogParser.collect_test_results')
  def testLaunchCommand_notRestartPassedTest(self, mock_collect_results,
                                             xcode_version):
    egtests = xcodebuild_runner.EgtestsApp(_EGTESTS_APP_PATH)
    xcode_version.return_value = {'version': '10.2.1'}
    mock_collect_results.side_effect = [
        {'failed': {'BUILD_INTERRUPTED': 'BUILD_INTERRUPTED: attempt # 0'},
         'passed': ['Class1/passedTest1', 'Class1/passedTest2']}
    ]
    launch_command = xcodebuild_runner.LaunchCommand(egtests,
                                                     _DESTINATION,
                                                     shards=1,
                                                     retries=3,
                                                     out_dir=self.tmpdir)
    self.fake_launch_attempt(launch_command, ['pass'])
    launch_command.launch()
    self.assertEqual(2, len(launch_command.test_results))
