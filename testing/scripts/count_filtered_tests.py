# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import json
import os
import subprocess
import sys

import common


def ParseTestList(test_list_contents):
  lines = test_list_contents.splitlines()

  all_tests = []
  fixture = None
  for line in lines:
    if '#' in line:
      line = line[:line.index('#')]
    line = line.rstrip()
    if line[0] == ' ':
      assert fixture
      all_tests.append(fixture + line.lstrip())
    else:
      fixture = line

  return all_tests


def LoadFilterList(filter_file):
  with open(filter_file, 'r') as f:
    lines = f.readlines()

  all_filters = []
  for line in lines:
    if '#' in line:
      line = line[:line.index('#')]
    line = line.strip()
    if not line:
      continue
    assert line.startswith('-')  # TODO(scottmg): Handle +s.
    all_filters.append(line[1:])

  return all_filters


def FilterMatchesTest(filter_string, test_string):
  """Does something close enough to base/strings/pattern.h's MatchPattern() for
  our purposes here."""
  return fnmatch.fnmatch(test_string, filter_string)


def main_run(args):
  binary_name = args.args[0]
  test_filter_file = args.args[1]
  base_path = args.build_dir
  list_tests_output = subprocess.check_output(
      [os.path.join(base_path, binary_name), '--gtest_list_tests'])
  tests = ParseTestList(list_tests_output)
  negative_filter_list = LoadFilterList(
      os.path.join(base_path, test_filter_file))

  result = {'valid': True, 'failures': []}
  result['total_tests'] = len(tests)
  result['disabled_tests'] = len([t for t in tests if '.DISABLED_' in t])

  # Filter out all the tests that aren't being run.
  for f in negative_filter_list:
    tests = [t for t in tests if not FilterMatchesTest(f, t)]
  result['filtered_tests'] = result['total_tests'] - len(tests)

  json.dump(result, args.output)
  return 0


def main_compile_targets(args):
  # TODO(scottmg): Get the binary name passed here instead of hardcoding.
  json.dump(['browser_tests', 'content_browsertests'], args.output)


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
