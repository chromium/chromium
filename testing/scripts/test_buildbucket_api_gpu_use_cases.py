#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import common

# Add src/content/test/gpu into sys.path for importing common.
sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', 'content', 'test', 'gpu'))

import gather_power_measurement_results
import gather_swarming_json_results


class BuildBucketApiGpuUseCaseTests:

  @classmethod
  def GenerateTests(cls):
    return [
        'TestGatherPowerMeasurementResultsFromLatestGreenBuild',
        'TestGatherWebGL2TestTimesFromLatestGreenBuild',
    ]

  @staticmethod
  def TestGatherPowerMeasurementResultsFromLatestGreenBuild():
    # Verify we can get power measurement test data from latest successful
    # build, including the swarming bot that runs the test, and actual test
    # results.
    bot = 'Win10 FYI x64 Release (Intel HD 630)'
    step = 'power_measurement_test'
    build_id = gather_power_measurement_results.GetLatestGreenBuild(bot)
    build_json = gather_power_measurement_results.GetJsonForBuildSteps(
        bot, build_id)
    if 'steps' not in build_json:
      return '"steps" is missing from the build json'
    stdout_url = gather_power_measurement_results.FindStepLogURL(
        build_json['steps'], step, 'stdout')
    if not stdout_url:
      return 'Unable to find stdout from step %s' % step
    results = { 'number': build_id, 'tests': [] }
    gather_power_measurement_results.ProcessStepStdout(stdout_url, results)
    if 'bot' not in results or not results['bot'].startswith('BUILD'):
      return 'Failed to find bot name as BUILD*'
    if not results['tests']:
      return 'Failed to find power measurment test data'
    return None

  @staticmethod
  def TestGatherWebGL2TestTimesFromLatestGreenBuild():
    # Verify we can get more than 2000 WebGL2 tests running time from the
    # latest successful build.
    extracted_times, _ = gather_swarming_json_results.GatherResults(
        bot='Linux FYI Release (NVIDIA)',
        build=None, # Use the latest green build
        step='webgl2_conformance_tests')

    if 'times' not in extracted_times:
      return '"times" is missing from the extracted dict'
    num_of_tests = len(extracted_times['times'])
    # From local run, there are 2700+ tests. This is sanity check that we
    # get reasonable data.
    if num_of_tests < 2000:
      return 'expected 2000+ tests, got %d tests' % num_of_tests
    return None


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--isolated-script-test-output', type=str,
      required=True)
  parser.add_argument(
      '--isolated-script-test-chartjson-output', type=str,
      required=False)
  parser.add_argument(
      '--isolated-script-test-perf-output', type=str,
      required=False)
  parser.add_argument(
      '--isolated-script-test-filter', type=str,
      required=False)

  args = parser.parse_args(argv)

  # Run actual tests
  failures = []
  for test_name in BuildBucketApiGpuUseCaseTests.GenerateTests():
    test = getattr(BuildBucketApiGpuUseCaseTests, test_name)
    error_msg = test()
    if error_msg is not None:
      result = '%s: %s' % (test_name, error_msg)
      print 'FAIL: %s' % result
      failures.append(result)

  if not failures:
    print 'PASS: test_buildbucket_api_gpu_use_cases ran successfully.'

  with open(args.isolated_script_test_output, 'w') as json_file:
    json.dump({
        'valid': True,
        'failures': failures,
    }, json_file)

  return 0


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
  sys.exit(main(sys.argv[1:]))
