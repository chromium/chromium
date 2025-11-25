#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs the unit tests from the directories touched in the current CL."""

import argparse
import os
import plistlib
import re
import subprocess
import sys
from typing import List, Set

import shared_test_utils
from shared_test_utils import Colors, print_header, print_command


def _get_test_files_in_touched_dirs() -> List[str]:
  """
  Gets all _unittest files under `ios/chrome/` from directories touched by the
  current CL.

  Returns:
    A list of absolute paths to all found unit test files.
  """
  touched_files = subprocess.check_output(
      ['git', 'diff', '--name-only',
       'origin/main']).decode('utf-8').splitlines()
  touched_dirs = set()
  for f in touched_files:
    touched_dirs.add(os.path.dirname(f))

  test_files = []
  for d in touched_dirs:
    for root, _, files in os.walk(d):
      for f in files:
        if f.endswith(('_unittest.mm', '_unittest.cc')):
          full_path = os.path.join(root, f)
          if full_path.startswith('ios/chrome/'):
            test_files.append(full_path)
  return test_files


def _get_test_suites(test_files: List[str]) -> Set[str]:
  """Parses a list of test files to extract the names of the test suites.

  Args:
    test_files: A list of paths to unit test files.

  Returns:
    A set of all found test suite names.
  """
  test_suites = set()
  for f in test_files:
    with open(f, 'r') as reader:
      for line in reader.readlines():
        if line.startswith('TEST_F('):
          test_suites.add(line.split('(')[1].split(',')[0])
  return test_suites


def main() -> int:
  """Main function for the script.

  Parses arguments, finds and filters tests, and invokes the test runner
  script.

  Returns:
    The exit code of the test runner script.
  """
  print_header("=== Run CL Tests ===")
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--out-dir',
      default='out/Debug-iphonesimulator',
      help='The output directory to use for the build (default: %(default)s).')
  parser.add_argument('--device', help='The device type to use for the test.')
  parser.add_argument('--os',
                      help='The OS version to use for the test (e.g., 17.5).')
  args = parser.parse_args()

  print_header("--- Finding Test Suites ---")
  test_files = _get_test_files_in_touched_dirs()
  if not test_files:
    print('No test files found in the touched directories.')
    return 0

  test_suites = _get_test_suites(test_files)
  if not test_suites:
    print('No test suites found in the touched test files.')
    return 0

  sorted_test_suites = sorted(list(test_suites))
  print(
      'Found the following unit test suites in directories with changed '
      'files:')
  for suite in sorted_test_suites:
    print(f'{Colors.BLUE}' + suite + f'{Colors.RESET}')

  gtest_filter = ':'.join([suite + '.*' for suite in sorted_test_suites])

  run_command = [
      'ios/tools/run_unittests.py',
      '--out-dir',
      args.out_dir,
      '--gtest_filter',
      gtest_filter,
  ]
  if args.device:
    run_command.extend(['--device', args.device])
    if args.os:
      run_command.extend(['--os', args.os])
  print_header("--- Invoking Test Runner ---")
  print_command(run_command)
  return subprocess.call(run_command)


if __name__ == '__main__':
  sys.exit(main())
