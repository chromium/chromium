#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""List the fuzzers within a fuzztest and confirm they match what we expect.

Invoked by GN from fuzzer_test.gni.
"""

import argparse
import os
import re
import subprocess
import sys

# //build imports.
sys.path.append(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        os.pardir,
        os.pardir,
        'build',
    ))
import action_helpers


def CreateArgumentParser():
  """Creates an argparse.ArgumentParser instance."""
  parser = argparse.ArgumentParser(description='Generate fuzztest fuzzers.')
  parser.add_argument('--executable',
                      help='Executable to interrogate for present fuzztests.')
  parser.add_argument(
      '--output',
      help='Path to the output file (which will be intentionally blank).',
  )
  parser.add_argument('--fuzztests',
                      nargs='+',
                      help='List of fuzztests we expect to find.')
  return parser


def main():
  parser = CreateArgumentParser()
  args = parser.parse_args()

  expected_tests = set(args.fuzztests)

  env = os.environ
  env['ASAN_OPTIONS'] = 'detect_odr_violation=0'
  process_result = subprocess.run([args.executable, '--list_fuzz_tests=1'],
                                  env=env,
                                  stdout=subprocess.PIPE,
                                  check=False)

  if process_result.returncode == 0:
    test_list = process_result.stdout.decode('utf-8')

    actual_tests = set(re.findall('Fuzz test: (.*)', test_list))

    if expected_tests != actual_tests:
      print('Unexpected fuzztests found in this binary.\nFuzztest binary: ' +
            args.executable + '\n' +
            'Expected fuzztests (as declared by the gn "fuzztests" variable on'
            ' this target): ' + str(expected_tests) +
            '\nActual tests (as found in the binary): ' + str(actual_tests) +
            '\nYou probably need to update the gn variable to match the tests'
            ' actually in the binary.')
      sys.exit(-1)

  # If we couldn't run the fuzztest binary itself, we'll
  # regard that as fine. This is a best-efforts check that the
  # gn 'fuzztests' variable is correct, and sometimes fuzzers don't
  # run on LUCI e.g. due to lack of dependencies.

  with action_helpers.atomic_output(args.output) as output:
    output.write(''.encode('utf-8'))


if __name__ == '__main__':
  main()
