#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs selected unit tests."""

import argparse
import json
import os
import plistlib
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple

import shared_test_utils
from shared_test_utils import Colors, Simulator, print_header, print_command



def _build_tests(out_dir: str) -> bool:
  """Builds the unit test target.

  Args:
    out_dir: The output directory for the build.

  Returns:
    True if the build was successful, False otherwise.
  """
  build_command = ['autoninja', '-C', out_dir, 'ios_chrome_unittests']
  print_header("--- Building Tests ---")
  print_command(build_command)
  try:
    subprocess.check_call(build_command)
    return True
  except subprocess.CalledProcessError as e:
    print(f"Build failed with exit code {e.returncode}")
    return False


def _run_tests(out_dir: str, simulator_udid: str,
               gtest_filter: Optional[str]) -> int:
  """Installs and runs the tests on the specified simulator.

  Args:
    out_dir: The output directory for the build.
    simulator_udid: The UDID of the simulator to use.
    gtest_filter: The gtest filter to apply.

  Returns:
    The exit code of the test runner (0 for success, non-zero for failure).
  """
  app_path = os.path.join(out_dir, 'ios_chrome_unittests.app')
  info_plist_path = os.path.join(app_path, 'Info.plist')
  with open(info_plist_path, 'rb') as f:
    info_plist = plistlib.load(f)
  bundle_id = info_plist['CFBundleIdentifier']

  install_command = ['xcrun', 'simctl', 'install', simulator_udid, app_path]
  print_header("--- Installing App ---")
  print_command(install_command)
  subprocess.check_call(install_command)

  launch_command = [
      'xcrun', 'simctl', 'launch', '--console-pty', simulator_udid, bundle_id
  ]
  if gtest_filter:
    launch_command.append('--gtest_filter=' + gtest_filter)
  print_header("--- Running Tests ---")
  print_command(launch_command)

  process = subprocess.Popen(launch_command,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,
                             text=True)

  test_failed = False
  test_completed = False
  while True:
    line = process.stdout.readline()
    if not line:
      break
    sys.stdout.write(line)
    if '[  PASSED  ]' in line or '[  FAILED  ]' in line:
      test_completed = True
    if '[  FAILED  ]' in line:
      test_failed = True

  process.wait()

  # If the test completed and there were failures, return 1.
  if test_completed and test_failed:
    return 1

  # If the test did not complete (i.e. crashed), return 1.
  if not test_completed:
    print(f'\n{Colors.FAIL}{Colors.BOLD}'
          'ERROR: Test runner did not complete. Assuming crash.'
          f'{Colors.RESET}')
    return 1

  # If the test completed and there were no failures, return 0.
  return 0


def main() -> int:
  """Main function for the script.

  Parses arguments, finds a simulator, builds, and runs the tests.

  Returns:
    The exit code of the test run.
  """
  print_header("=== Run Unit Tests ===")
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--out-dir',
      default='out/Debug-iphonesimulator',
      help='The output directory to use for the build (default: %(default)s).')
  parser.add_argument('--gtest_filter',
                      help='The gtest_filter to use for running the tests.')
  parser.add_argument('--device', help='The device type to use for the test.')
  parser.add_argument('--os',
                      help='The OS version to use for the test (e.g., 17.5).')
  args = parser.parse_args()

  simulator = shared_test_utils.find_and_boot_simulator(
      args.device, args.os)
  if not simulator:
    return 1

  if not _build_tests(args.out_dir):
    return 1

  return _run_tests(args.out_dir, simulator.udid, args.gtest_filter)


if __name__ == '__main__':
  sys.exit(main())
