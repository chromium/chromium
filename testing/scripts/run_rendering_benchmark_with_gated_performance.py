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
# compared to the recorded value (multiplier of upper limit).
AVG_ERROR_MARGIN = 1.1
# CI stands for confidence intervals. "ci_095"s recorded in the data is the
# recorded range between upper and lower CIs. CI_ERROR_MARGIN is the maximum
# acceptable ratio of calculated ci_095 to the recorded ones.
# TODO(behdadb) crbug.com/1052054
CI_ERROR_MARGIN = 1.5

METRIC_NAME = 'frame_times'

class ResultRecorder(object):
  def __init__(self):
    self.fails = 0
    self.tests = 0
    self.start_time = time.time()
    self.output = {}
    self.return_code = 0
    self._failed_stories = set()
    self._noisy_control_stories = set()
    # Set of _noisy_control_stories keeps track of control tests which failed
    # because of high noise values.

  def set_tests(self, output):
    self.output = output
    self.fails = output['num_failures_by_type'].get('FAIL', 0)
    self.tests = self.fails + output['num_failures_by_type'].get('PASS', 0)

  def add_failure(self, name, benchmark, is_control=False):
    self.output['tests'][benchmark][name]['actual'] = 'FAIL'
    self.output['tests'][benchmark][name]['is_unexpected'] = True
    self._failed_stories.add(name)
    self.fails += 1
    if is_control:
      self._noisy_control_stories.add(name)

  def remove_failure(self, name, benchmark, is_control=False,
                      invalidation_reason=None):
    self.output['tests'][benchmark][name]['actual'] = 'PASS'
    self.output['tests'][benchmark][name]['is_unexpected'] = False
    self._failed_stories.remove(name)
    self.fails -= 1
    if is_control:
      self._noisy_control_stories.remove(name)
    if invalidation_reason:
      self.add_invalidation_reason(name, benchmark, invalidation_reason)

  def invalidate_failures(self, benchmark):
    # The method is for invalidating the failures in case of noisy control test
    for story in self._failed_stories.copy():
      print(story + ' [Invalidated Failure]: The story failed but was ' +
        'invalidated as a result of noisy control test.')
      self.remove_failure(story, benchmark, False, 'Noisy control test')

  def add_invalidation_reason(self, name, benchmark, reason):
    self.output['tests'][benchmark][name]['invalidation_reason'] = reason

  @property
  def failed_stories(self):
    return self._failed_stories

  @property
  def is_control_stories_noisy(self):
    return len(self._noisy_control_stories) > 0

  def get_output(self, return_code):
    self.output['seconds_since_epoch'] = time.time() - self.start_time
    self.output['num_failures_by_type']['PASS'] = self.tests - self.fails
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

class RenderingRepresentativePerfTest(object):
  def __init__(self, initialization_for_tests=False):
    self.return_code = 0
    # result_recorder for rerun, and non rerun
    self.result_recorder = {
      True: ResultRecorder(),
      False: ResultRecorder()
    }

    if initialization_for_tests is True:
      return

    self.options = parse_arguments()
    print (self.options)

    self.benchmark = self.options.benchmarks
    out_dir_path = os.path.dirname(self.options.isolated_script_test_output)
    re_run_output_dir = os.path.join(out_dir_path, 're_run_failures')

    self.output_path = {
      True: os.path.join(
        re_run_output_dir, self.benchmark, 'test_results.json'),
      False: os.path.join(out_dir_path, self.benchmark, 'test_results.json')
    }
    self.results_path = {
      True: os.path.join(
        re_run_output_dir, self.benchmark, 'perf_results.csv'),
      False: os.path.join(out_dir_path, self.benchmark, 'perf_results.csv')
    }

    re_run_test_output = os.path.join(re_run_output_dir,
      os.path.basename(self.options.isolated_script_test_output))

    self.set_platform_specific_attributes()

    # The values used as the upper limit are the 99th percentile of the
    # avg and ci_095 frame_times recorded by dashboard in the past 200
    # revisions. If the value measured here would be higher than this value at
    # least by 10 [AVG_ERROR_MARGIN] percent of upper limit, that would be
    # considered a failure. crbug.com/953895
    with open(
      os.path.join(os.path.dirname(__file__),
      'representative_perf_test_data',
      'representatives_frame_times_upper_limit.json')
    ) as bound_data:
      self.upper_limit_data = json.load(bound_data)[self.platform]

    self.args = list(sys.argv)
    # The first run uses all stories in the representative story tag, but for
    # rerun we use only the failed stories.
    self.args.extend(['--story-tag-filter', self.story_tag])

    self.re_run_args = replace_arg_values(list(sys.argv), [
      ('--isolated-script-test-output', re_run_test_output)])

  def parse_csv_results(self, csv_obj):
    """ Parses the raw CSV data
    Convers the csv_obj into an array of valid values for averages and
    confidence intervals based on the described upper_limits.

    Args:
      csv_obj: An array of rows (dict) describing the CSV results

    Raturns:
      A dictionary which has the stories as keys and an array of confidence
      intervals and valid averages as data.
    """
    values_per_story = {}
    for row in csv_obj:
      # For now only frame_times is used for testing representatives'
      # performance and cpu_wall_time_ratio is used for validation.
      if row['name'] != METRIC_NAME and row['name'] != 'cpu_wall_time_ratio':
        continue
      story_name = row['stories']
      if (story_name not in self.upper_limit_data):
        continue
      if story_name not in values_per_story:
        values_per_story[story_name] = {
          'averages': [],
          'ci_095': [],
          'cpu_wall_time_ratio': []
        }

      if row['name'] == METRIC_NAME and row['avg'] != '' and row['count'] != 0:
        values_per_story[story_name]['ci_095'].append(float(row['ci_095']))
        values_per_story[story_name]['averages'].append(float(row['avg']))
      elif row['name'] == 'cpu_wall_time_ratio' and row['avg'] != '':
        values_per_story[story_name]['cpu_wall_time_ratio'].append(
          float(row['avg']))

    return values_per_story

  def compare_values(self, values_per_story, rerun=False):
    """ Parses the raw CSV data
    Compares the values in values_per_story with the upper_limit_data and
    determines if the story passes or fails and updates the ResultRecorder.

    Args:
      values_per_story: An array of rows (dict) descriving the CSV results
      rerun: Is this a rerun or initial run
    """
    for story_name in values_per_story:
      # The experimental stories will not be considered for failing the tests
      if (self.is_experimental_story(story_name)):
        continue
      if len(values_per_story[story_name]['ci_095']) == 0:
        print(('[  FAILED  ] {}/{} has no valid values for {}. Check ' +
          'run_benchmark logs for more information.').format(
            self.benchmark, story_name, METRIC_NAME))
        self.result_recorder[rerun].add_failure(story_name, self.benchmark)
        continue

      upper_limits = self.upper_limit_data
      upper_limit_avg = upper_limits[story_name]['avg']
      upper_limit_ci = upper_limits[story_name]['ci_095']
      lower_limit_cpu_ratio = upper_limits[story_name]['cpu_wall_time_ratio']
      measured_avg = np.mean(np.array(values_per_story[story_name]['averages']))
      measured_ci = np.mean(np.array(values_per_story[story_name]['ci_095']))
      measured_cpu_ratio = np.mean(np.array(
        values_per_story[story_name]['cpu_wall_time_ratio']))

      if (measured_ci > upper_limit_ci * CI_ERROR_MARGIN and
        self.is_control_story(story_name)):
        print(('[  FAILED  ] {}/{} {} has higher noise ({:.3f}) ' +
          'compared to upper limit ({:.3f})').format(
            self.benchmark, story_name, METRIC_NAME, measured_ci,
          upper_limit_ci))
        self.result_recorder[rerun].add_failure(
          story_name, self.benchmark, True)
      elif (measured_avg > upper_limit_avg * AVG_ERROR_MARGIN):
        if (measured_cpu_ratio >= lower_limit_cpu_ratio):
          print(('[  FAILED  ] {}/{} higher average {}({:.3f}) compared' +
            ' to upper limit ({:.3f})').format(self.benchmark, story_name,
            METRIC_NAME, measured_avg, upper_limit_avg))
          self.result_recorder[rerun].add_failure(story_name, self.benchmark)
        else:
          print(('[       OK ] {}/{} higher average {}({:.3f}) compared ' +
            'to upper limit({:.3f}). Invalidated for low cpu_wall_time_ratio'
            ).format(self.benchmark, story_name, METRIC_NAME, measured_avg,
                      upper_limit_avg))
          self.result_recorder[rerun].add_invalidation_reason(
            story_name, self.benchmark, 'Low cpu_wall_time_ratio')
      else:
        print(('[       OK ] {}/{} lower average {}({:.3f}) compared ' +
          'to upper limit({:.3f}).').format(self.benchmark, story_name,
          METRIC_NAME, measured_avg, upper_limit_avg))

  def interpret_run_benchmark_results(self, rerun=False):
    with open(self.output_path[rerun], 'r+') as resultsFile:
      initialOut = json.load(resultsFile)
      self.result_recorder[rerun].set_tests(initialOut)

      with open(self.results_path[rerun]) as csv_file:
        csv_obj = csv.DictReader(csv_file)
        values_per_story = self.parse_csv_results(csv_obj)

      if not rerun:
        # Clearing the result of run_benchmark and write the gated perf results
        resultsFile.seek(0)
        resultsFile.truncate(0)

    self.compare_values(values_per_story, rerun)

  def run_perf_tests(self):
    self.return_code |= run_performance_tests.main(self.args)
    self.interpret_run_benchmark_results(False)

    if len(self.result_recorder[False].failed_stories) > 0:
      # For failed stories we run_tests again to make sure it's not a false
      # positive.
      print('============ Re_run the failed tests ============')
      all_failed_stories = '('+'|'.join(
        self.result_recorder[False].failed_stories)+')'
      # TODO(crbug.com/1055893): Remove the extra chrome categories after
      # investigation of flakes in representative perf tests.
      self.re_run_args.extend(
        ['--story-filter', all_failed_stories, '--pageset-repeat=3',
         '--extra-chrome-categories=blink,blink_gc,gpu,v8,viz'])
      self.return_code |= run_performance_tests.main(self.re_run_args)
      self.interpret_run_benchmark_results(True)

      for story_name in self.result_recorder[False].failed_stories.copy():
        if story_name not in self.result_recorder[True].failed_stories:
          self.result_recorder[False].remove_failure(story_name,
            self.benchmark, self.is_control_story(story_name))

    if self.result_recorder[False].is_control_stories_noisy:
      # In this case all failures are reported as expected, and the number of
      # Failed stories in output.json will be zero.
      self.result_recorder[False].invalidate_failures(self.benchmark)

    (
      finalOut,
      self.return_code
    ) = self.result_recorder[False].get_output(self.return_code)

    with open(self.output_path[False], 'r+') as resultsFile:
      json.dump(finalOut, resultsFile, indent=4)
    with open(self.options.isolated_script_test_output, 'w') as outputFile:
      json.dump(finalOut, outputFile, indent=4)

    if self.result_recorder[False].is_control_stories_noisy:
      assert self.return_code == 0
      print('Control story has high noise. These runs are not reliable!')

    return self.return_code

  def is_control_story(self, story_name):
    # The story tagged as control story in upper_limit_data, will be used to
    # identify possible flake and invalidates the results.
    return self.story_has_attribute_enabled(story_name, 'control')

  def is_experimental_story(self, story_name):
    # The story tagged as experimental story in upper_limit_data, will be used
    # to gather the performance results, but the test would not be failed as
    # a result of.
    return self.story_has_attribute_enabled(story_name, 'experimental')

  def story_has_attribute_enabled(self, story_name, attribute):
    return (attribute in self.upper_limit_data[story_name] and
      self.upper_limit_data[story_name][attribute] == True)


  def set_platform_specific_attributes(self):
    if self.benchmark == 'rendering.desktop':
      # Linux does not have it's own specific representatives
      # and uses the representatives chosen for windows.
      if sys.platform == 'win32' or sys.platform.startswith('linux'):
        self.platform = 'win'
        self.story_tag = 'representative_win_desktop'
      elif sys.platform == 'darwin':
        self.platform = 'mac'
        self.story_tag = 'representative_mac_desktop'
      else:
        self.return_code = 1
    elif self.benchmark == 'rendering.mobile':
      self.platform = 'android'
      self.story_tag = 'representative_mobile'
    else:
      self.return_code = 1

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
  test_runner = RenderingRepresentativePerfTest()
  if test_runner.return_code == 1:
    return 1

  return test_runner.run_perf_tests()

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
