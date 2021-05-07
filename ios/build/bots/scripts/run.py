#!/usr/bin/env vpython
# Copyright 2016 The Chromium Authors. All rights reserved.
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
import subprocess
import sys
import traceback

import shard_util
import test_runner
import wpr_runner
import xcodebuild_runner
import xcode_util as xcode


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

    if args:
      self.parse_args(args)

  def install_xcode(self, xcode_build_version, mac_toolchain_cmd,
                    xcode_app_path):
    """Installs the requested Xcode build version.

    Args:
      xcode_build_version: (string) Xcode build version to install.
      mac_toolchain_cmd: (string) Path to mac_toolchain command to install Xcode
      See https://chromium.googlesource.com/infra/infra/+/master/go/src/infra/cmd/mac_toolchain/
      xcode_app_path: (string) Path to install the contents of Xcode.app.

    Returns:
      True if installation was successful. False otherwise.
    """
    try:
      if not mac_toolchain_cmd:
        raise test_runner.MacToolchainNotFoundError(mac_toolchain_cmd)
      # Guard against incorrect install paths. On swarming, this path
      # should be a requested named cache, and it must exist.
      if not os.path.exists(xcode_app_path):
        raise test_runner.XcodePathNotFoundError(xcode_app_path)

      # TODO(crbug.com/1191260): Pass in runtime args and handle moving runtime
      # back to cache after runtime cache is set up in swarming, if Xcode
      # installed is a new version (without runtime bundled)
      xcode.install(mac_toolchain_cmd, xcode_build_version, xcode_app_path)
      xcode.select(xcode_app_path)
    except subprocess.CalledProcessError as e:
      # Flush buffers to ensure correct output ordering.
      sys.stdout.flush()
      sys.stderr.write('Xcode build version %s failed to install: %s\n' %
                       (xcode_build_version, e))
      sys.stderr.flush()
      return False

    return True

  def run(self, args):
    """
    Main coordinating function.
    """
    self.parse_args(args)

    # This logic is run by default before the otool command is invoked such that
    # otool has the correct Xcode selected for command line dev tools.
    if not self.install_xcode(self.args.xcode_build_version,
                              self.args.mac_toolchain_cmd,
                              self.args.xcode_path):
      raise test_runner.XcodeVersionNotFoundError(self.args.xcode_build_version)

    # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS are additional test environment
    # variables, set by Swarming, that are only set for a swarming task
    # shard count is > 1.
    #
    # For a given test on a given run, otool should return the same total
    # counts and thus, should generate the same sublists. With the shard index,
    # each shard would then know the exact test case to run.
    gtest_shard_index = os.getenv('GTEST_SHARD_INDEX', 0)
    gtest_total_shards = os.getenv('GTEST_TOTAL_SHARDS', 0)
    if gtest_shard_index and gtest_total_shards:
      self.args.test_cases = shard_util.shard_test_cases(
          self.args, gtest_shard_index, gtest_total_shards)

    summary = {}
    tr = None

    if not os.path.exists(self.args.out_dir):
      os.makedirs(self.args.out_dir)

    try:
      if self.args.xcode_parallelization:
        tr = xcodebuild_runner.SimulatorParallelTestRunner(
            self.args.app,
            self.args.host_app,
            self.args.iossim,
            self.args.version,
            self.args.platform,
            out_dir=self.args.out_dir,
            release=self.args.release,
            retries=self.args.retries,
            shards=self.args.shards,
            test_cases=self.args.test_cases,
            test_args=self.test_args,
            use_clang_coverage=self.args.use_clang_coverage,
            env_vars=self.args.env_var)
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
            env_vars=self.args.env_var,
            retries=self.args.retries,
            shards=self.args.shards,
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
            env_vars=self.args.env_var,
            retries=self.args.retries,
            shards=self.args.shards,
            test_args=self.test_args,
            test_cases=self.args.test_cases,
            use_clang_coverage=self.args.use_clang_coverage,
            wpr_tools_path=self.args.wpr_tools_path,
            xctest=self.args.xctest,
        )
      elif self.args.xcodebuild_device_runner and self.args.xctest:
        tr = xcodebuild_runner.DeviceXcodeTestRunner(
            app_path=self.args.app,
            host_app_path=self.args.host_app,
            out_dir=self.args.out_dir,
            release=self.args.release,
            retries=self.args.retries,
            test_cases=self.args.test_cases,
            test_args=self.test_args,
            env_vars=self.args.env_var)
      else:
        tr = test_runner.DeviceTestRunner(
            self.args.app,
            self.args.out_dir,
            env_vars=self.args.env_var,
            restart=self.args.restart,
            retries=self.args.retries,
            test_args=self.test_args,
            test_cases=self.args.test_cases,
            xctest=self.args.xctest,
        )

      logging.info("Using test runner %s" % type(tr).__name__)
      return 0 if tr.launch() else 1
    except test_runner.DeviceError as e:
      sys.stderr.write(traceback.format_exc())
      summary['step_text'] = '%s%s' % (e.__class__.__name__,
                                       ': %s' % e.args[0] if e.args else '')

      # Swarming infra marks device status unavailable for any device related
      # issue using this return code.
      return 3
    except test_runner.TestRunnerError as e:
      sys.stderr.write(traceback.format_exc())
      summary['step_text'] = '%s%s' % (e.__class__.__name__,
                                       ': %s' % e.args[0] if e.args else '')

      # test_runner.Launch returns 0 on success, 1 on failure, so return 2
      # on exception to distinguish between a test failure, and a failure
      # to launch the test at all.
      return 2
    finally:
      if tr:
        summary['logs'] = tr.logs

      with open(os.path.join(self.args.out_dir, 'summary.json'), 'w') as f:
        json.dump(summary, f)
      if tr:
        with open(os.path.join(self.args.out_dir, 'full_results.json'),
                  'w') as f:
          json.dump(tr.test_results, f)

        # The value of test-launcher-summary-output is set by the recipe
        # and passed here via swarming.py. This argument defaults to
        # ${ISOLATED_OUTDIR}/output.json. out-dir is set to ${ISOLATED_OUTDIR}

        # TODO(crbug.com/1031338) - the content of this output.json will
        # work with Chromium recipe because we use the noop_merge merge script,
        # but will require structural changes to support the default gtest
        # merge script (ref: //testing/merge_scripts/standard_gtest_merge.py)
        output_json_path = (
            self.args.test_launcher_summary_output or
            os.path.join(self.args.out_dir, 'output.json'))
        with open(output_json_path, 'w') as f:
          json.dump(tr.test_results, f)

      test_runner.defaults_delete('com.apple.CoreSimulator',
                                  'FramebufferServerRendererPolicy')

  def parse_args(self, args):
    """
    Parse the args into args and test_args.
    """
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-x',
        '--xcode-parallelization',
        help='Run tests using xcodebuild\'s parallelization.',
        action='store_true',
    )
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
        '-e',
        '--env-var',
        action='append',
        help='Environment variable to pass to the test itself.',
        metavar='ENV=val',
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
    #TODO(crbug.com/1056887): Implement this arg in infra.
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
        help='Number of times to retry failed test cases.',
        metavar='n',
        type=int,
    )
    parser.add_argument(
        '-s',
        '--shards',
        help='Number of shards to split test cases.',
        metavar='n',
        type=int,
    )
    parser.add_argument(
        '-t',
        '--test-cases',
        action='append',
        help=('Tests that should be included in the test run. All other tests '
              'will be excluded from this run. If unspecified, run all tests.'),
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
        '--xctest',
        action='store_true',
        help='Whether or not the given app should be run as an XCTest.',
    )
    parser.add_argument(
        '--test-launcher-summary-output',
        default=None,
        help='Full path to output.json file. output.json is consumed by both '
        'collect_task.py and merge scripts.')

    def load_from_json(args):
      """
      Load and set arguments from args_json
      """
      args_json = json.loads(args.args_json)
      args.env_var = args.env_var or []
      args.env_var.extend(args_json.get('env_var', []))
      args.restart = args_json.get('restart', args.restart)
      args.test_cases = args.test_cases or []
      args.test_cases.extend(args_json.get('test_cases', []))
      args.xctest = args_json.get('xctest', args.xctest)
      args.xcode_parallelization = args_json.get('xcode_parallelization',
                                                 args.xcode_parallelization)
      args.xcodebuild_device_runner = (
          args_json.get('xcodebuild_device_runner',
                        args.xcodebuild_device_runner))
      args.shards = args_json.get('shards', args.shards)
      test_args.extend(args_json.get('test_args', []))

    def validate(args):
      """
      Runs argument validation
      """
      if (not (args.xcode_parallelization or args.xcodebuild_device_runner) and
          (args.iossim or args.platform or args.version)):
        # If any of --iossim, --platform, or --version
        # are specified then they must all be specified.
        if not (args.iossim and args.platform and args.version):
          parser.error('must specify all or none of '
                       '-i/--iossim, -p/--platform, -v/--version')

      if args.xcode_parallelization and not (args.platform and args.version):
        parser.error('--xcode-parallelization also requires '
                     'both -p/--platform and -v/--version')

    args, test_args = parser.parse_known_args(args)
    load_from_json(args)
    validate(args)
    # TODO(crbug.com/1056820): |app| won't contain "Debug" or "Release" after
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
