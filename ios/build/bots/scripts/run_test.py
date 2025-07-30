#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for run.py."""

import contextlib
import datetime
import io
import json
import mock
import os
import re
import sys
import unittest

from pyfakefs.fake_filesystem_unittest import TestCase

import constants
import iossim_util
import run
import result_sink_util
from test_runner import SimulatorNotFoundError, TestRunner
from xcodebuild_runner import SimulatorParallelTestRunner
import xcode_util
import test_runner_errors
import test_runner_test

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.extend([
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')),
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/'))
])
import exception_recorder


@mock.patch(
    'os.path.getmtime', return_value=datetime.datetime(2025, 1, 1).timestamp())
class RotateOutDirTest(TestCase):

  def setUp(self):
    super().setUp()
    self.setUpPyfakefs()

  def test_rotate_with_clobber(self, _):
    self.fs.create_dir('out/Release/test-results')
    self.fs.create_file(
        'out/Release/test-results_2024-01-01-000000/output.json')
    runner = run.Runner()
    runner.maybe_rotate_out_dir('out/Release/test-results', archive_limit=1)
    self.assertFalse(os.path.exists('out/Release/test-results'))
    self.assertTrue(os.path.isdir('out/Release/test-results_2025-01-01-000000'))
    self.assertFalse(
        os.path.isdir('out/Release/test-results_2024-01-01-000000'))

  def test_does_not_exist(self, mock_getmtime):
    mock_getmtime.side_effect = FileNotFoundError('not found')
    runner = run.Runner()
    runner.maybe_rotate_out_dir('out/Release/test-results')
    self.assertFalse(os.path.exists('out/Release/test-results'))

  def test_skip_build_dir(self, _):
    self.fs.create_file('out/Release/args.gn')
    runner = run.Runner()
    runner.maybe_rotate_out_dir('out/Release')
    self.assertTrue(os.path.isdir('out/Release'))


class ParseArgsUnitTest(unittest.TestCase):
  def test_parse_args_ok(self):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--host-app',
        './bar.app',
        '--runtime-cache-prefix',
        'some/dir',
        '--xcode-path',
        'some/Xcode.app',
        '--gtest_repeat',
        '2',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]

    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertTrue(runner.args.app == './foo-Runner.app')
    self.assertTrue(runner.args.runtime_cache_prefix == 'some/dir')
    self.assertTrue(runner.args.xcode_path == 'some/Xcode.app')
    self.assertTrue(runner.args.repeat == 2)
    self.assertTrue(runner.args.record_video == None)
    self.assertFalse(runner.args.output_disabled_tests)

  def test_isolated_repeat_ok(self):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--host-app',
        './bar.app',
        '--runtime-cache-prefix',
        'some/dir',
        '--xcode-path',
        'some/Xcode.app',
        '--isolated-script-test-repeat',
        '2',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertTrue(runner.args.repeat == 2)

  # Don't try to set any defaults.
  @mock.patch('xcode_util.is_local_run', return_value=False)
  def test_parse_args_iossim_platform_version(self, _):
    """iossim, platform and version should all be set together."""
    test_cases = [
        {
            'error':
                2,
            'cmd': [
                '--platform',
                'iPhone X',
                '--version',
                '13.2.2',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ],
        },
        {
            'error':
                2,
            'cmd': [
                '--iossim',
                'path/to/iossim',
                '--version',
                '13.2.2',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ],
        },
        {
            'error':
                2,
            'cmd': [
                '--iossim',
                'path/to/iossim',
                '--platform',
                'iPhone X',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ],
        },
    ]

    runner = run.Runner()
    for test_case in test_cases:
      stderr_buf = io.StringIO()
      with contextlib.redirect_stderr(stderr_buf):
        with self.assertRaises(SystemExit) as ctx:
          runner.parse_args(test_case['cmd'])
      self.assertEqual(ctx.exception.code, test_case['error'])
      self.assertRegex(stderr_buf.getvalue(), 'must specify all or none of .*')

  @mock.patch('xcode_util.is_local_run', return_value=True)
  @mock.patch('xcode_util.version', return_value=('20.0', '123abc'))
  def test_parse_args_default_xcode_build_version(self, *_mocks):
    cmd = [
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertEqual('123abc', runner.args.xcode_build_version)

  @mock.patch('xcode_util.is_local_run', return_value=True)
  @mock.patch('xcode_util.version', return_value=('20.0', '123abc'))
  def test_parse_args_default_xcode_build_version_override(self, *_mocks):
    cmd = [
        '--out-dir',
        'some/dir',
        '--xcode-build-version',
        'efefef',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertEqual('efefef', runner.args.xcode_build_version)

  @mock.patch('xcode_util.is_local_run', return_value=True)
  @mock.patch(
      'iossim_util.get_simulator_list',
      return_value={
          'runtimes': [{
              'version':
                  '20.0',
              'supportedDeviceTypes': [{
                  'productFamily': 'iPhone',
                  'name': 'iPhone 20',
              }, {
                  'productFamily': 'iPhone',
                  'name': 'iPhone 20 Pro',
              }, {
                  'productFamily': 'iPad',
                  'name': 'iPad 10',
              }],
          }],
      })
  def test_parse_args_default_simulator_params(self, *_mocks):
    cmd = [
        '--out-dir',
        'some/dir',
        '--xcode-build-version',
        '123abc',
        '--iossim',
        'path/to/iossim',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertEqual('20.0', runner.args.version)
    self.assertEqual('iPhone 20 Pro', runner.args.platform)

  @mock.patch('xcode_util.is_local_run', return_value=True)
  @mock.patch(
      'iossim_util.get_simulator_list',
      return_value={
          'runtimes': [{
              'version':
                  '19.0',
              'supportedDeviceTypes': [{
                  'productFamily': 'iPhone',
                  'name': 'iPhone 19',
              }],
          }],
      })
  def test_parse_args_default_simulator_params_override(self, *_mocks):
    cmd = [
        '--out-dir',
        'some/dir',
        '--xcode-build-version',
        '123abc',
        '--xcodebuild-sim-runner',
        '--version',
        '20.0',
        '--platform',
        'iPad 20',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertEqual('20.0', runner.args.version)
    self.assertEqual('iPad 20', runner.args.platform)

  def test_parse_args_xcode_parallelization_requirements(self):
    """
    xcode parallelization set requires both platform and version
    """
    test_cases = [
        {
            'error':
                2,
            'cmd': [
                '--xcode-parallelization',
                '--platform',
                'iPhone X',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ]
        },
        {
            'error':
                2,
            'cmd': [
                '--xcode-parallelization',
                '--version',
                '13.2.2',

                # Required
                '--xcode-build-version',
                '123abc',
                '--out-dir',
                'some/dir'
            ]
        }
    ]

    runner = run.Runner()
    for test_case in test_cases:
      with self.assertRaises(SystemExit) as ctx:
        runner.parse_args(test_case['cmd'])
        self.assertTrue(
            re.match('--xcode-parallelization also requires both *',
                     ctx.message))
        self.assertEqual(ctx.exception.code, test_case['error'])

  def test_parse_args_from_json(self):
    json_args = {
        'test_cases': ['test1'],
        'restart': 'true',
        'xcodebuild_sim_runner': True,
        'clones': 2
    }

    cmd = [
        '--clones',
        '1',
        '--platform',
        'iPhone X',
        '--version',
        '13.2.2',
        '--args-json',
        json.dumps(json_args),

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir'
    ]

    # clones should be 2, since json arg takes precedence over cmd line
    runner = run.Runner()
    runner.parse_args(cmd)
    # Empty array
    self.assertEquals(len(runner.args.env_var), 0)
    self.assertTrue(runner.args.xcodebuild_sim_runner)
    self.assertTrue(runner.args.restart)
    self.assertEquals(runner.args.clones, 2)

  def test_parse_args_record_video_without_xcode_parallelization(self):
    """
    enabling video plugin requires xcode parallelization (eg test on simulator)
    """
    cmd = [
        '--app',
        './foo-Runner.app',
        '--host-app',
        './bar.app',
        '--runtime-cache-prefix',
        'some/dir',
        '--xcode-path',
        'some/Xcode.app',
        '--gtest_repeat',
        '2',
        '--record-video',
        'failed_only',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]

    runner = run.Runner()
    with self.assertRaises(SystemExit) as ctx:
      runner.parse_args(cmd)
      self.assertTrue(re.match('is only supported on EG tests', ctx.message))
      self.assertEqual(ctx.exception.code, 2)

  def test_parse_args_output_disabled_tests(self):
    """
    report disabled tests to resultdb
    """
    cmd = [
        '--app',
        './foo-Runner.app',
        '--host-app',
        './bar.app',
        '--runtime-cache-prefix',
        'some/dir',
        '--xcode-path',
        'some/Xcode.app',
        '--gtest_repeat',
        '2',
        '--output-disabled-tests',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]

    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertTrue(runner.args.output_disabled_tests)

  @mock.patch('os.getenv')
  def test_sharding_in_env_var(self, mock_env):
    mock_env.side_effect = [2, 1]
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    sharding_env_vars = runner.sharding_env_vars()
    self.assertIn('GTEST_SHARD_INDEX=1', sharding_env_vars)
    self.assertIn('GTEST_TOTAL_SHARDS=2', sharding_env_vars)

  @mock.patch('os.getenv')
  def test_sharding_in_env_var_assertion_error(self, mock_env):
    mock_env.side_effect = [2, 1]
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',
        '--env-var',
        'GTEST_SHARD_INDEX=5',
        '--env-var',
        'GTEST_TOTAL_SHARDS=6',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertIn('GTEST_SHARD_INDEX=5', runner.args.env_var)
    self.assertIn('GTEST_TOTAL_SHARDS=6', runner.args.env_var)
    with self.assertRaises(AssertionError) as ctx:
      runner.sharding_env_vars()
      self.assertTrue(
          re.match('GTest shard env vars should not be passed '
                   'in --env-var', ctx.message))

  @mock.patch('os.getenv', return_value='2')
  def test_parser_error_sharding_environment(self, _):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',
        '--test-cases',
        'SomeClass.SomeTestCase',
        '--gtest_filter',
        'TestClass1.TestCase2:TestClass2.TestCase3',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    with self.assertRaises(SystemExit) as ctx:
      runner.parse_args(cmd)
      self.assertTrue(
          re.match(
              'Specifying test cases is not supported in multiple swarming '
              'shards environment.', ctx.message))
      self.assertEqual(ctx.exception.code, 2)

  @mock.patch('os.getenv', side_effect=[1, 0])
  def test_no_retries_when_repeat(self, _):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',
        '--test-cases',
        'SomeClass.SomeTestCase',
        '--isolated-script-test-repeat',
        '20',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertEqual(0, runner.args.retries)

  @mock.patch('os.getenv', side_effect=[1, 0])
  def test_override_retries_when_repeat(self, _):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',
        '--test-cases',
        'SomeClass.SomeTestCase',
        '--isolated-script-test-repeat',
        '20',
        '--retries',
        '3',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)
    self.assertEqual(0, runner.args.retries)

  def test_merge_test_cases(self):
    """Tests test cases are merges in --test-cases and --args-json."""
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',
        '--test-cases',
        'TestClass1.TestCase2',
        '--args-json',
        '{"test_cases": ["TestClass2.TestCase3"]}',
        '--gtest_filter',
        'TestClass3.TestCase4:TestClass4.TestCase5',
        '--isolated-script-test-filter',
        'TestClass6.TestCase6::TestClass7.TestCase7',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)

    expected_test_cases = [
        'TestClass1.TestCase2',
        'TestClass3.TestCase4',
        'TestClass4.TestCase5',
        'TestClass6.TestCase6',
        'TestClass7.TestCase7',
        'TestClass2.TestCase3',
    ]
    self.assertEqual(runner.args.test_cases, expected_test_cases)

  def test_gtest_filter_arg(self):
    cmd = [
        '--app',
        './foo-Runner.app',
        '--xcode-path',
        'some/Xcode.app',
        '--gtest_filter',
        'TestClass1.TestCase2:TestClass2.TestCase3',

        # Required
        '--xcode-build-version',
        '123abc',
        '--out-dir',
        'some/dir',
    ]
    runner = run.Runner()
    runner.parse_args(cmd)

    expected_test_cases = ['TestClass1.TestCase2', 'TestClass2.TestCase3']
    self.assertEqual(runner.args.test_cases, expected_test_cases)


class RunnerInstallXcodeTest(test_runner_test.TestCase):
  """Tests Xcode and runtime installing logic in Runner.run()"""

  def setUp(self):
    super(RunnerInstallXcodeTest, self).setUp()
    self.runner = run.Runner()

    self.mock(self.runner, 'parse_args', lambda _: None)
    self.mock(xcode_util, 'is_local_run', lambda: False)
    self.mock(
        iossim_util, 'get_platform_type_by_platform',
        lambda platform: constants.IOSPlatformType.TVOS if platform.startswith(
            'Apple TV') else constants.IOSPlatformType.IPHONEOS)
    self.runner.args = mock.MagicMock()
    # Make run() choose xcodebuild_runner.SimulatorParallelTestRunner as tr.
    self.runner.args.xcode_parallelization = True
    # Used in run.Runner.install_xcode().
    self.runner.args.mac_toolchain_cmd = 'mac_toolchain'
    self.runner.args.xcode_path = 'test/xcode/path'
    self.runner.args.xcode_build_version = 'testXcodeVersion'
    self.runner.args.runtime_cache_prefix = 'test/runtime-ios-'
    self.runner.args.platform = 'iPhone 11'
    self.runner.args.version = '14.4'
    self.runner.args.out_dir = 'out/dir'

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=True)
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_xcode(self, mock_macos_13_or_higher,
                        mock_check_xcode_exists_in_apps, mock_move_runtime,
                        mock_install, mock_construct_runtime_cache_folder,
                        mock_tr, _1, _2, _3, _4):
    mock_macos_13_or_higher.return_value = False
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        runtime_cache_folder='test/runtime-ios-14.4',
        ios_version='14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-ios-', '14.4')
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=True)
  @mock.patch('xcode_util.install_runtime_dmg')
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_xcode_tvos(self, mock_macos_13_or_higher,
                             mock_check_xcode_exists_in_apps, mock_move_runtime,
                             mock_install_runtime_dmg, mock_install,
                             mock_construct_runtime_cache_folder, mock_tr, _1,
                             _2, _3, _4):
    mock_macos_13_or_higher.return_value = False
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    self.runner.args.platform = 'Apple TV 4K'
    self.runner.args.runtime_cache_prefix = 'test/runtime-tvos-'
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        runtime_cache_folder='test/runtime-tvos-14.4',
        ios_version='14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-tvos-', '14.4')
    self.assertFalse(mock_install_runtime_dmg.called)
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=True)
  @mock.patch('xcode_util.install_runtime_dmg')
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch(
      'xcode_util.is_runtime_builtin', autospec=True, return_value=False)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_xcode_macos13_runtime_not_builtin(
      self, mock_macos_13_or_higher, mock_check_xcode_exists_in_apps,
      mock_is_runtime_builtin, mock_move_runtime, mock_install_runtime_dmg,
      mock_install, mock_construct_runtime_cache_folder, mock_tr, _1, _2, _3,
      _4):
    mock_macos_13_or_higher.return_value = True
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        runtime_cache_folder='test/runtime-ios-14.4',
        ios_version='14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-ios-', '14.4')
    mock_install_runtime_dmg.assert_called_with(
        'mac_toolchain', 'test/runtime-ios-14.4',
        constants.IOSPlatformType.IPHONEOS, '14.4', 'testXcodeVersion')
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=True)
  @mock.patch('xcode_util.install_runtime_dmg')
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch(
      'xcode_util.is_runtime_builtin', autospec=True, return_value=False)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_xcode_macos13_runtime_not_builtin_tvos(
      self, mock_macos_13_or_higher, mock_check_xcode_exists_in_apps,
      mock_is_runtime_builtin, mock_move_runtime, mock_install_runtime_dmg,
      mock_install, mock_construct_runtime_cache_folder, mock_tr, _1, _2, _3,
      _4):
    mock_macos_13_or_higher.return_value = True
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    self.runner.args.platform = 'Apple TV 4K'
    self.runner.args.runtime_cache_prefix = 'test/runtime-tvos-'
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        ios_version='14.4',
        runtime_cache_folder='test/runtime-tvos-14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-tvos-', '14.4')
    mock_install_runtime_dmg.assert_called_with('mac_toolchain',
                                                'test/runtime-tvos-14.4',
                                                constants.IOSPlatformType.TVOS,
                                                '14.4', 'testXcodeVersion')
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=True)
  @mock.patch('xcode_util.install_runtime_dmg')
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.is_runtime_builtin', autospec=True, return_value=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_legacy_xcode_macos13_runtime_builtin(
      self, mock_macos_13_or_higher, mock_check_xcode_exists_in_apps,
      mock_is_runtime_builtin, mock_move_runtime, mock_install_runtime_dmg,
      mock_install, mock_construct_runtime_cache_folder, mock_tr, _1, _2, _3,
      _4):
    mock_macos_13_or_higher.return_value = True
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        runtime_cache_folder='test/runtime-ios-14.4',
        ios_version='14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-ios-', '14.4')
    mock_install_runtime_dmg.assert_called_with(
        'mac_toolchain', 'test/runtime-ios-14.4',
        constants.IOSPlatformType.IPHONEOS, '14.4', 'testXcodeVersion')
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=False)
  @mock.patch('xcode_util.install_runtime_dmg')
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_not_legacy_xcode(self, mock_macos_13_or_higher,
                            mock_check_xcode_exists_in_apps, mock_move_runtime,
                            mock_install_runtime_dmg, mock_install,
                            mock_construct_runtime_cache_folder, mock_tr, _1,
                            _2, _3, _4):
    mock_macos_13_or_higher.return_value = False
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        runtime_cache_folder='test/runtime-ios-14.4',
        ios_version='14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-ios-', '14.4')
    self.assertFalse(mock_install_runtime_dmg.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=True)
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps')
  @mock.patch('mac_util.is_macos_13_or_higher', autospec=True)
  def test_xcode_exists_in_apps(self, mock_macos_13_or_higher,
                                mock_check_xcode_exists_in_apps,
                                mock_move_runtime, mock_install,
                                mock_construct_runtime_cache_folder, mock_tr,
                                _1, _2, _3, _4):
    mock_check_xcode_exists_in_apps = True
    mock_macos_13_or_higher.return_value = False
    mock_construct_runtime_cache_folder.side_effect = lambda a, b: a + b
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        '/Applications/xcode_testxcodeversion.app',
        runtime_cache_folder='test/runtime-ios-14.4',
        ios_version='14.4')
    mock_construct_runtime_cache_folder.assert_called_once_with(
        'test/runtime-ios-', '14.4')
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch('xcode_util.install', autospec=True, return_value=False)
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  def test_device_task(self, mock_check_xcode_exists_in_apps, mock_move_runtime,
                       mock_install, mock_construct_runtime_cache_folder,
                       mock_tr, _1, _2, _3, _4):
    """Check if Xcode is correctly installed for device tasks."""
    self.runner.args.platform = None
    self.runner.args.version = None
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    mock_install.assert_called_with(
        'mac_toolchain',
        'testXcodeVersion',
        'test/xcode/path',
        ios_version=None,
        runtime_cache_folder=None)

    self.assertFalse(mock_construct_runtime_cache_folder.called)
    self.assertFalse(mock_move_runtime.called)

  @mock.patch('test_runner.defaults_delete')
  @mock.patch('json.dump')
  @mock.patch('xcode_util.select', autospec=True)
  @mock.patch('os.path.exists', autospec=True, return_value=True)
  @mock.patch('xcodebuild_runner.SimulatorParallelTestRunner')
  @mock.patch('xcode_util.construct_runtime_cache_folder', autospec=True)
  @mock.patch(
      'xcode_util.install_xcode', autospec=True, return_value=False)
  @mock.patch('xcode_util.move_runtime', autospec=True)
  @mock.patch('xcode_util.check_xcode_exists_in_apps', return_value=False)
  @mock.patch('shutil.rmtree')
  def test_report_extended_properties(self, mock_rmtree,
                                      mock_check_xcode_exists_in_apps,
                                      mock_move_runtime, mock_install,
                                      mock_construct_runtime_cache_folder,
                                      mock_tr, _1, _2, _3, _4):
    self.runner.args.version = None
    test_runner = mock_tr.return_value
    test_runner.launch.return_value = True
    test_runner.logs = {}

    with mock.patch('run.open', mock.mock_open()):
      self.runner.run(None)

    expected_exception_str = 'test_runner_errors.XcodeInstallFailedError: ' + \
      str(test_runner_errors.XcodeInstallFailedError(
        self.runner.args.xcode_build_version))
    actual_exceptions = exception_recorder._records
    for exception in actual_exceptions:
      exception_str = str(exception.stacktrace[-1]).rstrip()
      if 'Xcode' in exception_str:
        self.assertEqual(expected_exception_str, exception_str)
    mock_rmtree.assert_called_with('test/xcode/path')


if __name__ == '__main__':
  unittest.main()
