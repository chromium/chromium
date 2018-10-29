#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs an isolate bundled Telemetry benchmark.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format detailed here:
https://www.chromium.org/developers/the-json-test-results-format

Optional argument:

  --isolated-script-test-filter=[TEST_NAMES]

is a double-colon-separated ("::") list of test names, to run just that subset
of tests. This list is parsed by this harness and sent down via the
--story-filter argument.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.

"""

import argparse
import json
import os
import shutil
import sys
import tempfile
import traceback

import common

# Add src/testing/ into sys.path for importing xvfb.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb
import test_env

# Unfortunately we need to copy these variables from ../test_env.py.
# Importing it and using its get_sandbox_env breaks test runs on Linux
# (it seems to unset DISPLAY).
CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--isolated-script-test-output', type=argparse.FileType('w'),
      required=True)
  parser.add_argument(
      '--isolated-script-test-chartjson-output', required=False)
  parser.add_argument(
      '--isolated-script-test-perf-output', required=False)
  parser.add_argument(
      '--isolated-script-test-filter', type=str, required=False)
  parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')
  parser.add_argument('--output-format', action='append')
  args, rest_args = parser.parse_known_args()
  for output_format in args.output_format:
    rest_args.append('--output-format=' + output_format)

  rc, perf_results, json_test_results, _ = run_benchmark(args, rest_args,
      'histograms' in args.output_format)

  if perf_results:
    if args.isolated_script_test_perf_output:
      filename = args.isolated_script_test_perf_output
    elif args.isolated_script_test_chartjson_output:
      filename = args.isolated_script_test_chartjson_output
    else:
      filename = None

    if filename is not None:
      with open(filename, 'w') as perf_results_output_file:
        json.dump(perf_results, perf_results_output_file)

  json.dump(json_test_results, args.isolated_script_test_output)

  return rc

def run_benchmark(args, rest_args, histogram_results):
  """  Run benchmark with args.

  Args:
    args: the option object resulted from parsing commandline args required for
      IsolatedScriptTest contract (see
      https://cs.chromium.org/chromium/build/scripts/slave/recipe_modules/chromium_tests/steps.py?rcl=d31f256fb860701e6dc02544f2beffe4e17c9b92&l=1639).
    rest_args: the args (list of strings) for running Telemetry benchmark.
    histogram_results: a boolean describes whether to output histograms format
      for the benchmark.

  Returns: a tuple of (rc, perf_results, json_test_results, benchmark_log)
    rc: the return code of benchmark
    perf_results: json object contains the perf test results
    json_test_results: json object contains the Pass/Fail data of the benchmark.
    benchmark_log: string contains the stdout/stderr of the benchmark run.
  """
  env = os.environ.copy()
  env['CHROME_HEADLESS'] = '1'

  # Assume we want to set up the sandbox environment variables all the
  # time; doing so is harmless on non-Linux platforms and is needed
  # all the time on Linux.
  env[CHROME_SANDBOX_ENV] = CHROME_SANDBOX_PATH
  tempfile_dir = tempfile.mkdtemp('telemetry')
  benchmark_log = ''
  stdoutfile = os.path.join(tempfile_dir, 'benchmark_log.txt')
  valid = True
  num_failures = 0
  perf_results = None
  json_test_results = None

  results = None
  cmd_args = rest_args
  if args.isolated_script_test_filter:
    filter_list = common.extract_filter_list(args.isolated_script_test_filter)
    # Need to convert this to a valid regex.
    filter_regex = '(' + '|'.join(filter_list) + ')'
    cmd_args = cmd_args + [
      '--story-filter=' + filter_regex
    ]
  rc = 1  # Set default returncode in case there is an exception.
  try:
    cmd = [sys.executable] + cmd_args + [
      '--output-dir', tempfile_dir,
      '--output-format=json-test-results',
    ]
    if args.xvfb:
      rc = xvfb.run_executable(cmd, env=env, stdoutfile=stdoutfile)
    else:
      rc = test_env.run_command_with_output(cmd, env=env, stdoutfile=stdoutfile)

    with open(stdoutfile) as f:
      benchmark_log = f.read()

    # If we have also output chartjson read it in and return it.
    # results-chart.json is the file name output by telemetry when the
    # chartjson output format is included
    tempfile_name = None
    if histogram_results:
      tempfile_name = os.path.join(tempfile_dir, 'histograms.json')
    else:
      tempfile_name = os.path.join(tempfile_dir, 'results-chart.json')

    if tempfile_name is not None:
      with open(tempfile_name) as f:
        perf_results = json.load(f)

    # test-results.json is the file name output by telemetry when the
    # json-test-results format is included
    tempfile_name = os.path.join(tempfile_dir, 'test-results.json')
    with open(tempfile_name) as f:
      json_test_results = json.load(f)
    num_failures = json_test_results['num_failures_by_type'].get('FAIL', 0)
    valid = bool(rc == 0 or num_failures != 0)

  except Exception:
    traceback.print_exc()
    if results:
      print 'results, which possibly caused exception: %s' % json.dumps(
          results, indent=2)
    valid = False
  finally:
    # Add ignore_errors=True because otherwise rmtree may fail due to leaky
    # processes of tests are still holding opened handles to files under
    # |tempfile_dir|. For example, see crbug.com/865896
    shutil.rmtree(tempfile_dir, ignore_errors=True)

  if not valid and num_failures == 0:
    if rc == 0:
      rc = 1  # Signal an abnormal exit.

  return rc, perf_results, json_test_results, benchmark_log


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
