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
import sys
import traceback

import test_runner
import wpr_runner
import xcodebuild_runner


def main():
  logging.basicConfig(format='[%(asctime)s:%(levelname)s] %(message)s',
    level=logging.DEBUG, datefmt='%I:%M:%S')

  test_runner.defaults_delete('com.apple.CoreSimulator',
                              'FramebufferServerRendererPolicy')
  args, test_args = parse_args()

  summary = {}
  tr = None

  if not os.path.exists(args.out_dir):
    os.makedirs(args.out_dir)

  try:
    if args.xcode_parallelization:
      tr = xcodebuild_runner.SimulatorParallelTestRunner(
          args.app,
          args.host_app,
          args.iossim,
          args.xcode_build_version,
          args.version,
          args.platform,
          out_dir=args.out_dir,
          mac_toolchain=args.mac_toolchain_cmd,
          retries=args.retries,
          shards=args.shards,
          xcode_path=args.xcode_path,
          test_cases=args.test_cases,
          test_args=test_args,
          env_vars=args.env_var
      )
    elif args.replay_path != 'NO_PATH':
      tr = wpr_runner.WprProxySimulatorTestRunner(
          args.app,
          args.host_app,
          args.iossim,
          args.replay_path,
          args.platform,
          args.version,
          args.wpr_tools_path,
          args.xcode_build_version,
          args.out_dir,
          env_vars=args.env_var,
          mac_toolchain=args.mac_toolchain_cmd,
          retries=args.retries,
          shards=args.shards,
          test_args=test_args,
          test_cases=args.test_cases,
          xcode_path=args.xcode_path,
          xctest=args.xctest,
      )
    elif args.iossim and args.platform and args.version:
      tr = test_runner.SimulatorTestRunner(
          args.app,
          args.iossim,
          args.platform,
          args.version,
          args.xcode_build_version,
          args.out_dir,
          env_vars=args.env_var,
          mac_toolchain=args.mac_toolchain_cmd,
          retries=args.retries,
          shards=args.shards,
          test_args=test_args,
          test_cases=args.test_cases,
          wpr_tools_path=args.wpr_tools_path,
          xcode_path=args.xcode_path,
          xctest=args.xctest,
      )
    elif args.xcodebuild_device_runner and args.xctest:
      tr = xcodebuild_runner.DeviceXcodeTestRunner(
          app_path=args.app,
          host_app_path=args.host_app,
          xcode_build_version=args.xcode_build_version,
          out_dir=args.out_dir,
          mac_toolchain=args.mac_toolchain_cmd,
          retries=args.retries,
          xcode_path=args.xcode_path,
          test_cases=args.test_cases,
          test_args=test_args,
          env_vars=args.env_var)
    else:
      tr = test_runner.DeviceTestRunner(
          args.app,
          args.xcode_build_version,
          args.out_dir,
          env_vars=args.env_var,
          mac_toolchain=args.mac_toolchain_cmd,
          restart=args.restart,
          retries=args.retries,
          test_args=test_args,
          test_cases=args.test_cases,
          xcode_path=args.xcode_path,
          xctest=args.xctest,
      )

    return 0 if tr.launch() else 1
  except test_runner.TestRunnerError as e:
    sys.stderr.write(traceback.format_exc())
    summary['step_text'] = '%s%s' % (
      e.__class__.__name__, ': %s' % e.args[0] if e.args else '')

    # test_runner.Launch returns 0 on success, 1 on failure, so return 2
    # on exception to distinguish between a test failure, and a failure
    # to launch the test at all.
    return 2
  finally:
    if tr:
      summary['logs'] = tr.logs

    with open(os.path.join(args.out_dir, 'summary.json'), 'w') as f:
      json.dump(summary, f)
    if tr:
      with open(os.path.join(args.out_dir, 'full_results.json'), 'w') as f:
        json.dump(tr.test_results, f)
    test_runner.defaults_delete('com.apple.CoreSimulator',
                                'FramebufferServerRendererPolicy')


def parse_args():
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
    help='Specify "env_var": [...] and "test_args": [...] using a JSON dict.',
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
    '--replay-path',
    help=('Path to a directory containing WPR replay and recipe files, for '
          'use with WprProxySimulatorTestRunner to replay a test suite'
          ' against multiple saved website interactions. Default: %(default)s'),
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
    '--use-trusted-cert',
    action='store_true',
    help=('Whether to install a cert to the simulator to allow for local HTTPS'
         'testing.'),
  )
  parser.add_argument(
    '-v',
    '--version',
    help='Version of iOS the simulator should run.',
    metavar='ver',
  )
  parser.add_argument(
    '--wpr-tools-path',
    help=('Location of WPR test tools (should be preinstalled, e.g. as part of '
         'a swarming task requirement). Default: %(default)s.'),
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
    default=False,
  )
  parser.add_argument(
    '--xctest',
    action='store_true',
    help='Whether or not the given app should be run as an XCTest.',
  )

  args, test_args = parser.parse_known_args()
  if not (args.xcode_parallelization or args.xcodebuild_device_runner) and (
      args.iossim or args.platform or args.version):
    # If any of --iossim, --platform, or --version
    # are specified then they must all be specified.
    if not (args.iossim and args.platform and args.version):
      parser.error(
        'must specify all or none of -i/--iossim, -p/--platform, -v/--version')

  if args.xcode_parallelization and not (args.platform and args.version):
    parser.error(''.join(['--xcode-parallezation also requires',
                          'both -p/--platform and -v/--version']))

  if args.xcodebuild_device_runner and not (args.platform and args.version):
    parser.error(''.join(['--xcodebuild-device-runner also requires',
                          'both -p/--platform and -v/--version']))

  args_json = json.loads(args.args_json)
  args.env_var = args.env_var or []
  args.env_var.extend(args_json.get('env_var', []))
  args.restart = args_json.get('restart', args.restart)
  args.test_cases = args.test_cases or []
  args.test_cases.extend(args_json.get('test_cases', []))
  args.xctest = args_json.get('xctest', args.xctest)
  args.xcode_parallelization = args_json.get('xcode_parallelization',
                                             args.xcode_parallelization)
  args.xcodebuild_device_runner = args_json.get('xcodebuild_device_runner',
                                                args.xcodebuild_device_runner)
  args.shards = args_json.get('shards', args.shards)
  test_args.extend(args_json.get('test_args', []))

  return args, test_args


if __name__ == '__main__':
  sys.exit(main())
