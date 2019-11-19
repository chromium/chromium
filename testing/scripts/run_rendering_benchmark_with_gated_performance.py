#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs telemetry benchmarks on representative story tag.

This script is a wrapper around run_performance_tests.py to capture the
values of performance metrics and compare them with the acceptable limits
in order to prevent regressions.

Arguments used for this script are the same as run_performance_tests.py.

The name and some functionalities of this script should be adjusted for
use with other benchmarks.
"""

from __future__ import print_function

import argparse
import csv
import json
import numpy as np
import os
import sys
import time

import common
import run_performance_tests

# AVG_ERROR_MARGIN determines how much more the value of frame times can be
# compared to the recorded value
AVG_ERROR_MARGIN = 2.0
# CI stands for confidence intervals. "ci_095"s recorded in the data is the
# recorded range between upper and lower CIs. CI_ERROR_MARGIN is the maximum
# acceptable ratio of calculated ci_095 to the recorded ones.
CI_ERROR_MARGIN = 2.0

class ResultRecorder(object):
  def __init__(self):
    self.fails = 0
    self.tests = 0
    self.start_time = time.time()
    self.output = {}
    self.return_code = 0
    self._failed_stories = set()

  def set_tests(self, output):
    self.output = output
    self.fails = 0
    if 'FAIL' in output['num_failures_by_type']:
      self.fails = output['num_failures_by_type']['FAIL']
    self.tests = self.fails + output['num_failures_by_type']['PASS']

  def add_failure(self, name, benchmark):
    self.output['tests'][benchmark][name]['actual'] = 'FAIL'
    self.output['tests'][benchmark][name]['is_unexpected'] = True
    self._failed_stories.add(name)
    self.fails += 1

  def remove_failure(self, name, benchmark):
    self.output['tests'][benchmark][name]['actual'] = 'PASS'
    self.output['tests'][benchmark][name]['is_unexpected'] = False
    self._failed_stories.remove(name)
    self.fails -= 1

  @property
  def failed_stories(self):
    return self._failed_stories

  def get_output(self, return_code):
    self.output['seconds_since_epoch'] = time.time() - self.start_time
    self.output['num_failures_by_type']['PASS'] = self.tests - self.fails
    if self.fails > 0:
      self.output['num_failures_by_type']['FAIL'] = self.fails
    if return_code == 1:
      self.output['interrupted'] = True

    plural = lambda n, s, p: '%d %s' % (n, p if n != 1 else s)
    tests = lambda n: plural(n, 'test', 'tests')

    print('[  PASSED  ] ' + tests(self.tests - self.fails) + '.')
    if self.fails > 0:
      print('[  FAILED  ] ' + tests(self.fails) + '.')
      self.return_code = 1

    return (self.output, self.return_code)

def interpret_run_benchmark_results(upper_limit_data,
    isolated_script_test_output, benchmark):
  out_dir_path = os.path.dirname(isolated_script_test_output)
  output_path = os.path.join(out_dir_path, benchmark, 'test_results.json')
  result_recorder = ResultRecorder()

  with open(output_path, 'r+') as resultsFile:
    initialOut = json.load(resultsFile)
    result_recorder.set_tests(initialOut)

    results_path = os.path.join(out_dir_path, benchmark, 'perf_results.csv')
    values_per_story = {}

    with open(results_path) as csv_file:
      reader = csv.DictReader(csv_file)
      for row in reader:
        # For now only frame_times is used for testing representatives'
        # performance.
        if row['name'] != 'frame_times':
          continue
        story_name = row['stories']
        if (story_name not in upper_limit_data):
          continue
        if story_name not in values_per_story:
          values_per_story[story_name] = {
            'averages': [],
            'ci_095': []
          }

        if (row['avg'] == '' or row['count'] == 0):
          continue
        values_per_story[story_name]['ci_095'].append(float(row['ci_095']))

        upper_limit_ci = upper_limit_data[story_name]['ci_095']
        # Only average values which are not noisy will be used
        if (float(row['ci_095']) <= upper_limit_ci * CI_ERROR_MARGIN):
          values_per_story[story_name]['averages'].append(float(row['ci_095']))

    # Clearing the result of run_benchmark and write the gated perf results
    resultsFile.seek(0)
    resultsFile.truncate(0)

  for story_name in values_per_story:
    if len(values_per_story[story_name]['averages']) == 0:
      print(('[  FAILED  ] {}/{} has no valid values for frame_times. Check ' +
        'run_benchmark logs for more information.').format(
          benchmark, story_name))
      result_recorder.add_failure(story_name, benchmark)
      continue

    upper_limit_avg = upper_limit_data[story_name]['avg']
    upper_limit_ci = upper_limit_data[story_name]['ci_095']
    measured_avg = np.mean(np.array(values_per_story[story_name]['averages']))
    measured_ci = np.mean(np.array(values_per_story[story_name]['ci_095']))

    if (measured_ci > upper_limit_ci * CI_ERROR_MARGIN):
      print(('[  FAILED  ] {}/{} frame_times has higher noise ({:.3f}) ' +
        'compared to upper limit ({:.3f})').format(
          benchmark, story_name, measured_ci,upper_limit_ci))
      result_recorder.add_failure(story_name, benchmark)
    elif (measured_avg > upper_limit_avg + AVG_ERROR_MARGIN):
      print(('[  FAILED  ] {}/{} higher average frame_times({:.3f}) compared' +
        ' to upper limit ({:.3f})').format(
          benchmark, story_name, measured_avg, upper_limit_avg))
      result_recorder.add_failure(story_name, benchmark)
    else:
      print(('[       OK ] {}/{} lower average frame_times({:.3f}) compared ' +
        'to upper limit({:.3f}).').format(
          benchmark, story_name, measured_avg, upper_limit_avg))

  return result_recorder

def replace_arg_values(args, key_value_pairs):
  for index in range(0, len(args)):
    for (key, value) in key_value_pairs:
      if args[index].startswith(key):
        if '=' in args[index]:
          args[index] = key + '=' + value
        else:
          args[index+1] = value
  return args

def main():
  overall_return_code = 0
  options = parse_arguments()

  print (options)

  if options.benchmarks == 'rendering.desktop':
    # Linux does not have it's own specific representatives
    # and uses the representatives chosen for windows.
    if sys.platform == 'win32' or sys.platform.startswith('linux'):
      platform = 'win'
      story_tag = 'representative_win_desktop'
    elif sys.platform == 'darwin':
      platform = 'mac'
      story_tag = 'representative_mac_desktop'
    else:
      return 1
  elif options.benchmarks == 'rendering.mobile':
    platform = 'android'
    story_tag = 'representative_mobile'
  else:
    return 1

  benchmark = options.benchmarks
  args = sys.argv
  re_run_args = sys.argv
  args.extend(['--story-tag-filter', story_tag])

  overall_return_code = run_performance_tests.main(args)

  # The values used as the upper limit are the 99th percentile of the
  # avg and ci_095 frame_times recorded by dashboard in the past 200 revisions.
  # If the value measured here would be higher than this value at least by
  # 2ms [AVG_ERROR_MARGIN], that would be considered a failure.
  # crbug.com/953895
  with open(
    os.path.join(os.path.dirname(__file__),
    'representative_perf_test_data',
    'representatives_frame_times_upper_limit.json')
  ) as bound_data:
    upper_limit_data = json.load(bound_data)

  out_dir_path = os.path.dirname(options.isolated_script_test_output)
  output_path = os.path.join(out_dir_path, benchmark, 'test_results.json')
  result_recorder = interpret_run_benchmark_results(upper_limit_data[platform],
      options.isolated_script_test_output, benchmark)

  with open(output_path, 'r+') as resultsFile:
    if len(result_recorder.failed_stories) > 0:
      # For failed stories we run_tests again to make sure it's not a false
      # positive.
      print('============ Re_run the failed tests ============')
      all_failed_stories = '('+'|'.join(result_recorder.failed_stories)+')'
      re_run_args.extend(
        ['--story-filter', all_failed_stories, '--pageset-repeat=3'])

      re_run_isolated_script_test_dir = os.path.join(out_dir_path,
        're_run_failures')
      re_run_isolated_script_test_output = os.path.join(
        re_run_isolated_script_test_dir,
        os.path.basename(options.isolated_script_test_output))
      re_run_isolated_script_test_perf_output = os.path.join(
        re_run_isolated_script_test_dir,
        os.path.basename(options.isolated_script_test_perf_output))

      re_run_args = replace_arg_values(re_run_args, [
        ('--isolated-script-test-output', re_run_isolated_script_test_output),
        ('--isolated-script-test-perf-output',
          re_run_isolated_script_test_perf_output)
      ])

      overall_return_code |= run_performance_tests.main(re_run_args)
      re_run_result_recorder = interpret_run_benchmark_results(
          upper_limit_data[platform], re_run_isolated_script_test_output,
          benchmark)

      for story_name in result_recorder.failed_stories.copy():
        if story_name not in re_run_result_recorder.failed_stories:
          result_recorder.remove_failure(story_name, benchmark)

    (
      finalOut,
      overall_return_code
    ) = result_recorder.get_output(overall_return_code)

    json.dump(finalOut, resultsFile, indent=4)

    with open(options.isolated_script_test_output, 'w') as outputFile:
      json.dump(finalOut, outputFile, indent=4)

  return overall_return_code

def parse_arguments():
  parser = argparse.ArgumentParser()
  parser.add_argument('executable', help='The name of the executable to run.')
  parser.add_argument(
      '--benchmarks', required=True)
  parser.add_argument(
      '--isolated-script-test-output', required=True)
  parser.add_argument(
      '--isolated-script-test-perf-output', required=False)
  return parser.parse_known_args()[0]

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