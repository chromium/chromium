#!/usr/bin/env vpython3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import gpu_project_config
import gpu_path_util
from gpu_path_util import setup_telemetry_paths  # pylint: disable=unused-import

from telemetry.testing import browser_test_runner
from telemetry.testing import serially_executed_browser_test_case
from py_utils import discover


def PostprocessJSON(file_name, run_test_args):
  # The file is not necessarily written depending on the arguments - only
  # postprocess it in case it is.
  if os.path.isfile(file_name):
    with open(file_name) as f:
      test_result = json.load(f)
    test_result['run_test_args'] = run_test_args
    with open(file_name, 'w') as f:
      json.dump(test_result, f, indent=2)


def FailIfScreenLockedOnMac():
  # Detect if the Mac lockscreen is present, which causes issues when running
  # tests.
  if not sys.platform.startswith('darwin'):
    return
  import Quartz  # pylint: disable=import-outside-toplevel,import-error
  current_session = Quartz.CGSessionCopyCurrentDictionary()
  if not current_session:
    # Using the logging module doesn't seem to be guaranteed to show up in
    # stdout, so use print instead.
    print('WARNING: Unable to obtain CGSessionCoppyCurrentDictionary via '
          'Quartz - unable to determine whether Mac lockscreen is present or '
          'not.')
    return
  if current_session.get('CGSSessionScreenIsLocked'):
    raise RuntimeError('Mac lockscreen detected, aborting.')


def FindTestCase(test_name):
  for start_dir in gpu_project_config.CONFIG.start_dirs:
    modules_to_classes = discover.DiscoverClasses(
        start_dir,
        gpu_project_config.CONFIG.top_level_dir,
        base_class=serially_executed_browser_test_case.
        SeriallyExecutedBrowserTestCase)
    for cl in modules_to_classes.values():
      if cl.Name() == test_name:
        return cl
  return None


def ProcessArgs(args, parser=None):
  parser = parser or argparse.ArgumentParser()
  parser.add_argument(
      '--write-run-test-arguments',
      action='store_true',
      help=('Write the test script arguments to the results file.'))
  option, rest_args_filtered = parser.parse_known_args(args)

  parser.add_argument('test', nargs='*', type=str, help=argparse.SUPPRESS)
  option, _ = parser.parse_known_args(rest_args_filtered)

  if option.test:
    test_class = FindTestCase(option.test[0])
  else:
    test_class = None

  if test_class:
    rest_args_filtered.extend([
        '--test-name-prefix=%s.%s.' % (test_class.__module__,
                                       test_class.__name__)
    ])

  if not any(arg.startswith('--retry-limit') for arg in rest_args_filtered):
    if '--retry-only-retry-on-failure-tests' not in rest_args_filtered:
      rest_args_filtered.append('--retry-only-retry-on-failure-tests')
    rest_args_filtered.append('--retry-limit=2')
  rest_args_filtered.extend(
      ['--repository-absolute-path', gpu_path_util.CHROMIUM_SRC_DIR])
  return rest_args_filtered


def main():
  rest_args = sys.argv[1:]
  FailIfScreenLockedOnMac()
  parser = argparse.ArgumentParser(
      description='Extra argument parser', add_help=False)

  rest_args_filtered = ProcessArgs(rest_args, parser)

  retval = browser_test_runner.Run(gpu_project_config.CONFIG,
                                   rest_args_filtered)

  # We're not relying on argparse to print the help in the normal way, because
  # we need the help output from both the argument parser here and the argument
  # parser in browser_test_runner.
  if '--help' in rest_args:
    print('\n\nCommand line arguments handed by run_gpu_integration_test:')
    parser.print_help()
    return retval

  # This duplicates an argument of browser_test_runner.
  parser.add_argument(
      '--write-full-results-to',
      metavar='FILENAME',
      action='store',
      help=('If specified, writes the full results to that path.'))

  option, _ = parser.parse_known_args(rest_args)

  # Postprocess the outputted JSON to add test arguments.
  if option.write_run_test_arguments and option.write_full_results_to:
    PostprocessJSON(option.write_full_results_to, rest_args)
  return retval


if __name__ == '__main__':
  sys.exit(main())
