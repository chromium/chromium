#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run a test.

Sample usage:
  ./run.py \
  -a src/xcodebuild/Release-iphoneos/base_unittests.app \
  -o /tmp/out \
  -p iPhone 5s \
  -v 9.3 \
  -b 9b46

  Installs base_unittests.app in an iPhone 5s simulator running iOS 9.3 under
  Xcode build version 9b46, runs it, and captures all test data in /tmp/out.
  """

import argparse
import json
import logging
import os
import shutil
import subprocess
import sys
import traceback

import constants
import iossim_util
import mac_util
import shard_util
import test_runner
import test_runner_errors
import variations_runner
import wpr_runner
import xcodebuild_runner
import xcode_util as xcode

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.extend([
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')),
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/'))
])
import measures
import exception_recorder

from result_sink_util import ResultSinkClient

# if the current directory is in scripts, then we need to add plugin
# path in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import VIDEO_RECORDER_PLUGIN_OPTIONS


def format_exception_step_text(e: Exception) -> str:
  return '%s%s' % (e.__class__.__name__, ': %s' % e.args[0] if e.args else '')


def use_xcodebuild_runner(args):
  return args.xcodebuild_sim_runner or args.xcodebuild_device_runner


class Runner():
  """
  Object to encapsulate iOS test runner execution coordination. Parses
  arguments and invokes underlying test runners accordingly.
  """

  def __init__(self, args=None):
    """
    args = argparse Namespace object.
    test_args = string list of args.
    """
    self.args = argparse.Namespace()
    self.test_args = []
    # Xcode might be corruped, so this the flag to decide
    # whether we should clear it from cache
    self.should_delete_xcode_cache = False

    if args:
      self.parse_args(args)

  def sharding_env_vars(self):
    """Returns env_var arg with GTest sharding env var."""
    gtest_total_shards = shard_util.gtest_total_shards()
    if gtest_total_shards > 1:
      assert not any((el.startswith('GTEST_SHARD_INDEX') or
                      el.startswith('GTEST_TOTAL_SHARDS'))
                     for el in self.args.env_var
                    ), 'GTest shard env vars should not be passed in --env-var'
      gtest_shard_index = shard_util.gtest_shard_index()
      return [
          'GTEST_SHARD_INDEX=%d' % gtest_shard_index,
          'GTEST_TOTAL_SHARDS=%d' % gtest_total_shards
      ]
    return []

  def run(self, args):
    """
    Main coordinating function.
    """
    summary = {}
    tr = None
    is_legacy_xcode = True
    self.parse_args(args)

    try:
      with measures.time_consumption(
          'mac_toolchain', 'Download and Install', 'Xcode and Runtime'):
        install_success, is_legacy_xcode = xcode.install_xcode(
            self.args.mac_toolchain_cmd, self.args.xcode_build_version,
            self.args.xcode_path, self.args.runtime_cache_prefix,
            self.args.version)
      if not install_success:
        raise test_runner_errors.XcodeInstallFailedError(
            self.args.xcode_build_version)

      # Sharding env var is required to shard GTest.
      env_vars = self.args.env_var + self.sharding_env_vars()

      if not os.path.exists(self.args.out_dir):
        os.makedirs(self.args.out_dir)

      if self.args.xcodebuild_sim_runner:
        tr = xcodebuild_runner.SimulatorParallelTestRunner(
            self.args.app,
            self.args.host_app,
            self.args.iossim,
            self.args.version,
            self.args.platform,
            out_dir=self.args.out_dir,
            readline_timeout=self.args.readline_timeout,
            release=self.args.release,
            repeat_count=self.args.repeat,
            retries=self.args.retries,
            clones=self.args.clones,
            test_cases=self.args.test_cases,
            test_args=self.test_args,
            use_clang_coverage=self.args.use_clang_coverage,
            env_vars=env_vars,
            record_video_option=self.args.record_video,
            output_disabled_tests=self.args.output_disabled_tests,
        )
      elif self.args.variations_seed_path != 'NO_PATH':
        tr = variations_runner.VariationsSimulatorParallelTestRunner(
            self.args.app,
            self.args.host_app,
            self.args.iossim,
            self.args.version,
            self.args.platform,
            self.args.out_dir,
            self.args.variations_seed_path,
            readline_timeout=self.args.readline_timeout,
            release=self.args.release,
            test_cases=self.args.test_cases,
            test_args=self.test_args,
            env_vars=env_vars)
      elif self.args.replay_path != 'NO_PATH':
        tr = wpr_runner.WprProxySimulatorTestRunner(
            self.args.app,
            self.args.host_app,
            self.args.iossim,
            self.args.replay_path,
            self.args.platform,
            self.args.version,
            self.args.wpr_tools_path,
            self.args.out_dir,
            env_vars=env_vars,
            readline_timeout=self.args.readline_timeout,
            retries=self.args.retries,
            clones=self.args.clones,
            test_args=self.test_args,
            test_cases=self.args.test_cases,
            xctest=self.args.xctest,
        )
      elif self.args.iossim and self.args.platform and self.args.version:
        tr = test_runner.SimulatorTestRunner(
            self.args.app,
            self.args.iossim,
            self.args.platform,
            self.args.version,
            self.args.out_dir,
            env_vars=env_vars,
            readline_timeout=self.args.readline_timeout,
            repeat_count=self.args.repeat,
            retries=self.args.retries,
            clones=self.args.clones,
            test_args=self.test_args,
            test_cases=self.args.test_cases,
            use_clang_coverage=self.args.use_clang_coverage,
            wpr_tools_path=self.args.wpr_tools_path,
            xctest=self.args.xctest,
            output_disabled_tests=self.args.output_disabled_tests,
        )
      elif self.args.xcodebuild_device_runner and self.args.xctest:
        tr = xcodebuild_runner.DeviceXcodeTestRunner(
            app_path=self.args.app,
            host_app_path=self.args.host_app,
            out_dir=self.args.out_dir,
            readline_timeout=self.args.readline_timeout,
            release=self.args.release,
            repeat_count=self.args.repeat,
            retries=self.args.retries,
            test_cases=self.args.test_cases,
            test_args=self.test_args,
            env_vars=env_vars,
            record_video_option=self.args.record_video,
            output_disabled_tests=self.args.output_disabled_tests,
        )
      else:
        tr = test_runner.DeviceTestRunner(
            self.args.app,
            self.args.out_dir,
            env_vars=env_vars,
            readline_timeout=self.args.readline_timeout,
            repeat_count=self.args.repeat,
            restart=self.args.restart,
            retries=self.args.retries,
            test_args=self.test_args,
            test_cases=self.args.test_cases,
            xctest=self.args.xctest,
            output_disabled_tests=self.args.output_disabled_tests,
        )

      logging.info("Using test runner %s" % type(tr).__name__)
      return 0 if tr.launch() else 1
    except test_runner.DeviceError as e:
      sys.stderr.write(traceback.format_exc())
      summary['step_text'] = format_exception_step_text(e)
      # Swarming infra marks device status unavailable for any device related
      # issue using this return code.
      exception_recorder.register(e)
      return 3
    except Exception as e:
      sys.stderr.write(traceback.format_exc())
      summary['step_text'] = format_exception_step_text(e)
      # test_runner.Launch returns 0 on success, 1 on failure, so return 2
      # on exception to distinguish between a test failure, and a failure
      # to launch the test at all.
      exception_recorder.register(e)
      return 2
    finally:
      if tr:
        summary['logs'] = tr.logs

      with open(os.path.join(self.args.out_dir, 'summary.json'), 'w') as f:
        json.dump(summary, f)

      is_eg_test = use_xcodebuild_runner(self.args)
      test_results = (
          tr.test_results
          if tr else test_runner.init_test_result_defaults(is_eg_test))

      with open(os.path.join(self.args.out_dir, 'full_results.json'), 'w') as f:
        json.dump(test_results, f)

      # The value of test-launcher-summary-output is set by the recipe
      # and passed here via swarming.py. This argument defaults to
      # ${ISOLATED_OUTDIR}/output.json. out-dir is set to ${ISOLATED_OUTDIR}

      # TODO(crbug.com/40110412) - the content of this output.json will
      # work with Chromium recipe because we use the noop_merge merge script,
      # but will require structural changes to support the default gtest
      # merge script (ref: //testing/merge_scripts/standard_gtest_merge.py)
      output_json_path = (
          self.args.test_launcher_summary_output or
          os.path.join(self.args.out_dir, 'output.json'))
      with open(output_json_path, 'w') as f:
        json.dump(test_results, f)

      if self.should_delete_xcode_cache:
        shutil.rmtree(self.args.xcode_path)

      test_runner.defaults_delete('com.apple.CoreSimulator',
                                  'FramebufferServerRendererPolicy')

      if exception_recorder.size() > 0 or measures.size() > 0:
        result_sink_client = ResultSinkClient()
        result_sink_client.post_extended_properties()

  def parse_args(self, args):
    """Parse the args into args and test_args.

    Note: test_cases related arguments are handled in |resolve_test_cases|
    instead of this function.
    """
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-a',
        '--app',
        help='Compiled .app to run for EG1, Compiled -Runner.app for EG2',
        metavar='app',
    )
    parser.add_argument(
        '-b',
        '--xcode-build-version',
        help='Xcode build version to install.',
        required=True,
        metavar='build_id',
    )
    parser.add_argument(
        '-c',
        '--clones',
        help='Number of iOS simulator clones to split test cases across',
        metavar='n',
        type=int,
        default=1,
    )
    parser.add_argument(
        '-e',
        '--env-var',
        action='append',
        help='Environment variable to pass to the test itself.',
        metavar='ENV=val',
    )
    parser.add_argument(
        '--gtest_filter',
        help='List of test names to run. Expected to be in GTest filter format,'
        'which should be a colon delimited list. Note: Specifying test cases '
        'is not supported in multiple swarming shards environment. Will be '
        'merged with tests specified in --test-cases, --args-json and '
        '--isolated-script-test-filter.',
        metavar='gtest_filter',
    )
    parser.add_argument(
        '--isolated-script-test-filter',
        help='A double-colon-separated ("::") list of test names to run. '
        'Note: Specifying test cases is not supported in multiple swarming '
        'shards environment. Will be merged with tests specified in '
        '--test-cases, --args-json and --gtest_filter.',
        metavar='isolated_test_filter',
    )
    parser.add_argument(
        '--gtest_repeat',
        '--isolated-script-test-repeat',
        help='Number of times to repeat each test case.',
        metavar='repeat',
        dest='repeat',
        type=int,
    )
    parser.add_argument(
        '--host-app',
        help='Compiled host .app to run.',
        default='NO_PATH',
        metavar='host_app',
    )
    parser.add_argument(
        '-i',
        '--iossim',
        help='Compiled iossim to run the app on.',
        metavar='iossim',
    )
    parser.add_argument(
        '-j',
        '--args-json',
        default='{}',
        help=
        'Specify "env_var": [...] and "test_args": [...] using a JSON dict.',
        metavar='{}',
    )
    parser.add_argument(
        '--mac-toolchain-cmd',
        help='Command to run mac_toolchain tool. Default: %(default)s.',
        default='mac_toolchain',
        metavar='mac_toolchain',
    )
    parser.add_argument(
        '-o',
        '--out-dir',
        help='Directory to store all test data in.',
        metavar='dir',
        required=True,
    )
    parser.add_argument(
        '-p',
        '--platform',
        help='Platform to simulate.',
        metavar='sim',
    )
    parser.add_argument(
        '--readline-timeout',
        help='Timeout to kill a test process when it doesn\'t'
        'have output (in seconds).',
        metavar='n',
        type=int,
        default=constants.READLINE_TIMEOUT,
    )
    #TODO(crbug.com/40120509): Implement this arg in infra.
    parser.add_argument(
        '--release',
        help='Indicates if this is a release build.',
        action='store_true',
    )
    parser.add_argument(
        '--replay-path',
        help=('Path to a directory containing WPR replay and recipe files, for '
              'use with WprProxySimulatorTestRunner to replay a test suite '
              'against multiple saved website interactions. '
              'Default: %(default)s'),
        default='NO_PATH',
        metavar='replay-path',
    )
    parser.add_argument(
        '--restart',
        action='store_true',
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        '-r',
        '--retries',
        help=('Number of times to retry failed test cases. Note: This will be '
              'overwritten as 0 if test repeat argument value > 1.'),
        metavar='n',
        type=int,
    )
    parser.add_argument(
        '--runtime-cache-prefix',
        metavar='PATH',
        help=(
            'Path prefix for runtime cache folder. The prefix will be appended '
            'with iOS version to construct the path. iOS simulator will be '
            'installed to the path and further copied into Xcode. Default: '
            '%(default)s. WARNING: this folder will be overwritten! This '
            'folder is intended to be a cached CIPD installation.'),
        default='Runtime-ios-',
    )
    parser.add_argument(
        '-t',
        '--test-cases',
        action='append',
        help=('Tests that should be included in the test run. All other tests '
              'will be excluded from this run. If unspecified, run all tests. '
              'Note: Specifying test cases is not supported in multiple '
              'swarming shards environment. Will be merged with tests '
              'specified in --gtest_filter and --args-json.'),
        metavar='testcase',
    )
    parser.add_argument(
        '--use-clang-coverage',
        help='Enable code coverage related steps in test runner scripts.',
        action='store_true',
    )
    parser.add_argument(
        '--use-trusted-cert',
        action='store_true',
        help=('Whether to install a cert to the simulator to allow for local '
              'HTTPS testing.'),
    )
    parser.add_argument(
        '-v',
        '--version',
        help='Version of iOS the simulator should run.',
        metavar='ver',
    )
    parser.add_argument(
        '--variations-seed-path',
        help=('Path to a JSON file with variations seed used in variations '
              'smoke testing. Default: %(default)s'),
        default='NO_PATH',
        metavar='variations-seed-path',
    )
    parser.add_argument(
        '--wpr-tools-path',
        help=(
            'Location of WPR test tools (should be preinstalled, e.g. as part '
            'of a swarming task requirement). Default: %(default)s.'),
        default='NO_PATH',
        metavar='wpr-tools-path',
    )
    parser.add_argument(
        '--xcode-path',
        metavar='PATH',
        help=('Path to <Xcode>.app folder where contents of the app will be '
              'installed. Default: %(default)s. WARNING: this folder will be '
              'overwritten! This folder is intended to be a cached CIPD '
              'installation.'),
        default='Xcode.app',
    )
    parser.add_argument(
        '--xcodebuild-device-runner',
        help='Run tests using xcodebuild\'s on real device.',
        action='store_true',
    )
    parser.add_argument(
        '--xcodebuild-sim-runner',
        help='Run tests using xcodebuild\'s on iOS simulators',
        action='store_true',
    )
    parser.add_argument(
        '--xctest',
        action='store_true',
        help='Whether or not the given app should be run as an XCTest.',
    )
    parser.add_argument(
        '--test-launcher-summary-output',
        default=None,
        help='Full path to output.json file. output.json is consumed by both '
        'collect_task.py and merge scripts.')
    parser.add_argument(
        '--record-video',
        choices=[o.name for o in VIDEO_RECORDER_PLUGIN_OPTIONS],
        help=(
            'Option to record video on EG tests. Currently this feature only '
            'works on tests running on simulators, and can only record failed '
            'test cases by specifying failed_only. More options coming soon...'
        ),
        metavar='record-video',
    )
    parser.add_argument(
        '--output-disabled-tests',
        action='store_true',
        help='Whether or not disabled test should be included in test output.',
    )

    def load_from_json(args):
      """Loads and sets arguments from args_json.

      Note: |test_cases| in --args-json is handled in merge_test_case instead
      of this function.
      """
      args_json = json.loads(args.args_json)
      args.env_var = args.env_var or []
      args.env_var.extend(args_json.get('env_var', []))
      args.restart = args_json.get('restart', args.restart)
      args.xctest = args_json.get('xctest', args.xctest)
      args.xcodebuild_sim_runner = args_json.get('xcodebuild_sim_runner',
                                                 args.xcodebuild_sim_runner)
      args.xcodebuild_device_runner = (
          args_json.get('xcodebuild_device_runner',
                        args.xcodebuild_device_runner))
      args.clones = args_json.get('clones', args.clones)
      test_args.extend(args_json.get('test_args', []))

    def validate(args):
      """
      Runs argument validation
      """
      if (not use_xcodebuild_runner(args) and
          (args.iossim or args.platform or args.version)):
        # If any of --iossim, --platform, or --version
        # are specified then they must all be specified.
        if not (args.iossim and args.platform and args.version):
          parser.error('must specify all or none of '
                       '-i/--iossim, -p/--platform, -v/--version')

      if args.xcodebuild_sim_runner and not (args.platform and args.version):
        parser.error('--xcodebuild-sim-runner also requires '
                     'both -p/--platform and -v/--version')

      if not use_xcodebuild_runner(args) and args.record_video:
        parser.error('--record-video is only supported on EG tests')

      # Do not retry when repeat
      if args.repeat and args.repeat > 1:
        args.retries = 0

      args_json = json.loads(args.args_json)
      if (args.gtest_filter or args.test_cases or
          args_json.get('test_cases')) and shard_util.gtest_total_shards() > 1:
        parser.error(
            'Specifying test cases is not supported in multiple swarming '
            'shards environment.')

    def merge_test_cases(args):
      """Forms |args.test_cases| considering cmd inputs.

      Note:
      - It's validated above that test filters won't work in
        sharding environment.
      """
      args.test_cases = args.test_cases or []
      if args.gtest_filter:
        args.test_cases.extend(args.gtest_filter.split(':'))
      if args.isolated_script_test_filter:
        args.test_cases.extend(args.isolated_script_test_filter.split('::'))
      args_json = json.loads(args.args_json)
      args.test_cases.extend(args_json.get('test_cases', []))

    args, test_args = parser.parse_known_args(args)
    load_from_json(args)
    validate(args)
    merge_test_cases(args)
    # TODO(crbug.com/40120476): |app| won't contain "Debug" or "Release" after
    # recipe migrations.
    args.release = args.release or (args.app and "Release" in args.app)
    self.args = args
    self.test_args = test_args


def main(args):
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')

  test_runner.defaults_delete('com.apple.CoreSimulator',
                              'FramebufferServerRendererPolicy')
  runner = Runner()
  logging.debug("Arg values passed for this run: %s" % args)
  return runner.run(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
