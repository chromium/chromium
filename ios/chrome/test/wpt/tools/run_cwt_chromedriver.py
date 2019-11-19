#!/usr/bin/python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import subprocess
import time

def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir,
                                      os.pardir, os.pardir, os.pardir,
                                      os.pardir))

def GetIosDir():
  return os.path.join(GetChromiumSrcDir(), 'ios')

sys.path.append(os.path.join(GetIosDir(), 'build', 'bots', 'scripts'))

import xcodebuild_runner

def GetDefaultBuildDir():
  return os.path.join(GetChromiumSrcDir(), 'out', 'Debug-iphonesimulator')

parser=argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('--port', default='9999',
    help='The port to listen on for WebDriver commands')
parser.add_argument('--build-dir', default=GetDefaultBuildDir(),
    help='Chrome build directory')
parser.add_argument('--out-dir', default='/tmp/cwt_chromedriver',
    help='Output directory for CWTChromeDriver\'s dummy test case')
parser.add_argument('--os', default='12.2', help='iOS version')
parser.add_argument('--device', default='iPhone 8', help='Device type')
args=parser.parse_args()

test_app = os.path.join(
    args.build_dir, 'ios_cwt_chromedriver_tests_module-Runner.app')
host_app = os.path.join(args.build_dir, 'ios_cwt_chromedriver_tests.app')
destination = 'platform=iOS Simulator,OS=%s,name=%s' % (args.os, args.device)

# Shutdown running simulators. This is needed since a running simulator may
# be in a hung state.
# TODO(crbug.com/673423): Change this logic to only shut down the simulator that
# will be used for this run, when adding support for running multiple instances
# of CWTChromeDriver.
subprocess.check_call(['xcrun', 'simctl', 'shutdown', 'booted'])

if not os.path.exists(args.out_dir):
  os.mkdir(args.out_dir)

# Make sure each run produces a unique output directory, since reusing an
# existing directory will cause CWTChromeDriver's dummy test case to get
# skipped, meaning that CWTChromeDriver's http server won't get launched.
output_directory = os.path.join(args.out_dir, 'run%d' %  int(time.time()))

egtests_app = xcodebuild_runner.EgtestsApp(
    egtests_app=test_app, test_args=['--port %s' % args.port],
    host_app_path=host_app)

launch_command = xcodebuild_runner.LaunchCommand(egtests_app, destination,
    shards=1, retries=1, out_dir=output_directory)

launch_command.launch()
