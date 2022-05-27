#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple helper script to run pytype on GPU Python code."""

import argparse
import json
import os
import subprocess
import sys
import time
import typing

# We can't depend on gpu_path_util, otherwise pytype's dependency graph ends up
# finding a cycle.
GPU_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(GPU_DIR, '..', '..', '..'))

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'build', 'util'))

# pylint: disable=wrong-import-position
from lib.results import result_sink
from lib.results import result_types
# pylint: disable=wrong-import-position

# This list should be kept in sync with EXTRA_PATH_COMPONENTS in PRESUBMIT.py
EXTRA_PATHS_COMPONENTS = [
    ('build', ),
    ('build', 'fuchsia'),
    ('build', 'util'),
    ('testing', ),
    ('third_party', 'catapult', 'common', 'py_utils'),
    ('third_party', 'catapult', 'devil'),
    ('third_party', 'catapult', 'telemetry'),
    ('third_party', 'catapult', 'third_party', 'typ'),
    ('tools', 'perf'),
]
EXTRA_PATHS = [
    os.path.join(CHROMIUM_SRC_DIR, *p) for p in EXTRA_PATHS_COMPONENTS
]
EXTRA_PATHS.append(GPU_DIR)

FILES_AND_DIRECTORIES_TO_CHECK = [
    'flake_suppressor',
    'gold_inexact_matching',
    'gpu_tests',
]

TEST_NAME = 'gpu_pytype'
TEST_LOCATION = '//content/test/gpu/run_pytype.py'


# pylint: disable=too-many-arguments
def report_results(status: str,
                   duration: float,
                   log: str,
                   output_file: typing.Optional[str],
                   sink_client: typing.Optional[result_sink.ResultSinkClient],
                   failure_reason: typing.Optional[str] = None) -> None:
  """Report results on bots.

  Args:
    status: A string containing the test status.
    duration: An float containing the test duration in seconds.
    log: A string containing the log output of the test.
    output_dir: An optional string containing a path to a file to output JSON
        to.
    sink_client: An optional client for reporting results to ResultDB.
    failure_reason: An optional string containing a reason why the test failed.
  """
  if output_file:
    report_json_results(status, duration, output_file)
  if sink_client:
    sink_client.Post(test_id=TEST_NAME,
                     status=status,
                     duration=(duration * 1000),
                     test_log=log,
                     test_file=TEST_LOCATION,
                     failure_reason=failure_reason)


# pylint: enable=too-many-arguments


def report_json_results(status: str, duration: float, output_file: str):
  num_passes = 1 if status == result_types.PASS else 0
  num_fails = 1 if status == result_types.FAIL else 0
  num_skips = 1 if status == result_types.SKIP else 0
  expected_result = (result_types.SKIP
                     if status == result_types.SKIP else result_types.PASS)

  output_json = {
      'version': 3,
      'interrupted': False,
      'path_delimiter': '/',
      'seconds_since_epoch': int(time.time()),
      'num_failures_by_type': {
          'FAIL': num_fails,
          'TIMEOUT': 0,
          'CRASH': 0,
          'PASS': num_passes,
          'SKIP': num_skips,
      },
      'num_regressions': 0 if status == expected_result else 1,
      'tests': {
          TEST_NAME: {
              'expected': expected_result,
              'actual': status,
              'times': [
                  duration,
              ]
          }
      }
  }

  with open(output_file, 'w') as outfile:
    json.dump(output_json, outfile)


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output',
                      dest='output_file',
                      help=('Path to JSON output file.'))

  args, _ = parser.parse_known_args()
  return args


def main():
  sink_client = result_sink.TryInitClient()
  args = parse_args()

  if sys.platform != 'linux':
    print('pytype is currently only supported on Linux, see '
          'https://github.com/google/pytype/issues/1154')
    report_results(result_types.SKIP, 0, 'Skipped due to unsupported platform.',
                   args.output_file, sink_client)
    sys.exit(0)

  # Strangely, pytype won't complain if you tell it to analyze a directory that
  # doesn't exist, which could potentially lead to code not being analyzed if
  # it's added here but not added to the isolate. So, ensure that everything we
  # expect to analyze actually exists.
  for f in FILES_AND_DIRECTORIES_TO_CHECK:
    if not os.path.exists(os.path.join(GPU_DIR, f)):
      raise RuntimeError('Requested file or directory %s does not exist.' % f)

  # pytype looks for a 'python' or 'python3' executable in PATH, so make sure
  # that the Python 3 executable from vpython is in the path.
  executable_dir = os.path.dirname(sys.executable)
  os.environ['PATH'] = executable_dir + os.pathsep + os.environ['PATH']

  # pytype specifies that the provided PYTHONPATH is :-separated.
  pythonpath = ':'.join(EXTRA_PATHS)
  pytype_cmd = [
      sys.executable,
      '-m',
      'pytype',
      '--pythonpath',
      pythonpath,
      '--keep-going',
      '--jobs',
      'auto',
  ]
  pytype_cmd.extend(FILES_AND_DIRECTORIES_TO_CHECK)

  if sink_client:
    stdout_handle = subprocess.PIPE
    stderr_handle = subprocess.STDOUT
  else:
    stdout_handle = None
    stderr_handle = None

  start_time = time.time()
  try:
    proc = subprocess.run(pytype_cmd,
                          check=True,
                          cwd=GPU_DIR,
                          stdout=stdout_handle,
                          stderr=stderr_handle,
                          text=True)
    stdout = proc.stdout
    status = result_types.PASS
    failure_reason = None
  except subprocess.CalledProcessError as e:
    stdout = e.stdout
    status = result_types.FAIL
    failure_reason = 'Checking Python 3 type hinting on GPU code failed.'
  duration = (time.time() - start_time)

  if stdout:
    print(stdout)
  report_results(status, duration, stdout or '', args.output_file, sink_client,
                 failure_reason)

  if status == result_types.FAIL:
    sys.exit(1)


if __name__ == '__main__':
  main()
