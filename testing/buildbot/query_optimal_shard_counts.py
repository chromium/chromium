#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to query optimal shards for swarmed test suites.

This script queries bigquery for recent test suite runtimes. For suites running
over the desired max runtime, it'll suggest optimal shard counts to bring the
duration below the desired max runtime.
"""

import argparse
import datetime
import json
import os
import math
import subprocess
import sys

_CLOUD_PROJECT_ID = 'chrome-trooper-analytics'

# TODO(crbug.com/40281184): Replace with queried, per-suite overheads, once
# infra is set up to support automated overhead measurements.
# See go/nplus1shardsproposal
DEFAULT_OVERHEAD_SEC = 60
ANDROID_OVERHEAD_SEC = 60 * 2

# ===========================================================================
# Excluding a builder and/or test suite from autosharding means:
# - If it was already autosharded before and exists in the
# autoshard_exceptions.json file, don't change the shard value any further.
# - If it was never autosharded, never add it to autoshard_exceptions.json
# ===========================================================================

# All suites triggered by the builder will not be autosharded.
BUILDER_EXCLUDE_SET = set([
    'mac-rel',
    'mac14-arm64-rel',
    'ios-simulator',
    'ios-simulator-full-configs',
    'android-arm64-rel',
])

# Test suites will not be autosharded on all builders that run the test suite.
# Example: 'browser_tests' -> turns of browser_tests on linux-rel and win-rel
# 'chrome_all_tast_tests': crbug.com/1516971
TEST_SUITE_EXCLUDE_SET = set(['chrome_all_tast_tests'])

# Test suite and try builder dicts that should not be autosharded any further.
# Maps try builder to set of test suite
# Example: {'linux-rel': {'browser_tests'}}
BUILDER_TEST_SUITE_EXCLUDE_DICT = {}

_BQ_SETUP_INSTRUCTION = """
** NOTE: this script is only for Googlers to use. **

bq script isn't found on your machine. To run this script, you need to be able
to run bigquery in your terminal.
If this is the first time you run the script, do the following steps:

1) Follow the steps at https://cloud.google.com/sdk/docs/ to download and
   unpack google-cloud-sdk in your home directory.
2) Run `gcloud auth login`
3) Run `gcloud config set project chrome-trooper-analytics`
   3a) If 'chrome-trooper-analytics' does not show up, contact
       chrome-browser-infra@ to be added as a user of the table
4) Run this script!
"""


def _query_overheads(lookback_start_date, lookback_end_date):
  query_file = os.path.join(os.path.dirname(__file__), 'autosharder_sql',
                            'query_test_overheads.sql')
  with open(query_file, 'r') as f:
    query_str = f.read()
  query = query_str.format(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
  )
  return _run_query([
      'bq', 'query', '--project_id=' + _CLOUD_PROJECT_ID, '--format=json',
      '--max_rows=100000', '--nouse_legacy_sql', query
  ])


def _query_suite_durations(lookback_start_date, lookback_end_date, percentile):
  query_file = os.path.join(os.path.dirname(__file__), 'autosharder_sql',
                            'query_suite_durations.sql')
  with open(query_file, 'r') as f:
    query_str = f.read()
  query = query_str.format(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
      percentile=percentile,
  )
  return _run_query([
      'bq', 'query', '--project_id=' + _CLOUD_PROJECT_ID, '--format=json',
      '--max_rows=100000', '--nouse_legacy_sql', query
  ])


def _query_avg_num_builds_per_hour(lookback_start_date, lookback_end_date):
  query_file = os.path.join(os.path.dirname(__file__), 'autosharder_sql',
                            'query_average_number_builds_per_hour.sql')
  with open(query_file, 'r') as f:
    query_str = f.read()
  query = query_str.format(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
  )
  return _run_query([
      'bq', 'query', '--project_id=' + _CLOUD_PROJECT_ID, '--format=json',
      '--max_rows=100000', '--nouse_legacy_sql', query
  ])


def _run_query(args):
  try:
    subprocess.check_call(['which', 'bq'])
  except subprocess.CalledProcessError as e:
    raise RuntimeError(_BQ_SETUP_INSTRUCTION) from e

  try:
    output = subprocess.check_output(args)
  except subprocess.CalledProcessError as e:
    print(e.output)
    raise e
  return json.loads(output)


def _get_overhead_dict(lookback_start_date, lookback_end_date):
  overhead_results = _query_overheads(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
  )
  overhead_dict = {}
  for r in overhead_results:
    try_builder = r['try_builder']
    test_suite = r['test_suite']

    overhead_info = {
        int(r['normally_assigned_shard_count']):
        (float(r['p50_task_setup_duration_sec']) +
         float(r['p50_test_harness_overhead_sec'])) / 60
    }
    overhead_dict.setdefault(try_builder,
                             {}).setdefault(test_suite,
                                            {}).update(overhead_info)
  return overhead_dict


def _calculate_and_filter_optimal_shard_counts(overhead_dict, durations,
                                               desired_runtime):
  filtered_durations = []
  for r in durations:
    try_builder = r['try_builder']
    test_suite = r['test_suite']
    shard_count = int(r['shard_count'])

    overhead = overhead_dict.get(try_builder, {}).get(test_suite,
                                                      {}).get(shard_count)
    if not overhead:
      if 'android' in try_builder:
        overhead = ANDROID_OVERHEAD_SEC / 60
      else:
        overhead = DEFAULT_OVERHEAD_SEC / 60
    r['test_overhead_min'] = overhead

    optimal_shard_count = math.ceil(
        (float(r['percentile_duration_minutes']) * shard_count -
         overhead * shard_count) / (desired_runtime - overhead))
    if optimal_shard_count <= 0:
      continue
    r['optimal_shard_count'] = optimal_shard_count

    overhead_change = (optimal_shard_count - shard_count) * overhead
    simulated_max_shard_duration = round(
        ((float(r['percentile_duration_minutes']) * shard_count +
          overhead_change) / optimal_shard_count), 2)
    r['simulated_max_shard_duration'] = simulated_max_shard_duration

    filtered_durations.append(r)
  return filtered_durations


def _calculate_estimated_bot_hour_cost(durations, lookback_start_date,
                                       lookback_end_date):
  results = _query_avg_num_builds_per_hour(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
  )
  avg_num_builds_per_hour = {
      r['try_builder']: int(math.ceil(float(r['avg_count'])))
      for r in results
  }

  updated_durations = []
  # Add estimated_bot_hour_cost and avg_num_builds_per_peak_hour
  for r in durations:
    try_builder = r['try_builder']
    shard_count = int(r['shard_count'])

    r['estimated_bot_hour_cost'] = round(
        (r['optimal_shard_count'] - shard_count) *
        (r['test_overhead_min'] / 60) * avg_num_builds_per_hour[try_builder],2)
    r['avg_num_builds_per_peak_hour'] = avg_num_builds_per_hour[try_builder]
    updated_durations.append(r)
  return updated_durations

def _meets_optimal_shard_count_and_simulated_duration_requirements(
    row, data, desired_runtime):
  builder_group = row['waterfall_builder_group']
  builder_name = row['waterfall_builder_name']
  test_suite = row['test_suite']

  current_autoshard_val = data.get(builder_group,
                                   {}).get(builder_name,
                                           {}).get(test_suite,
                                                   {}).get('shards')

  # No autosharding needed.
  if int(row['optimal_shard_count']) == int(row['shard_count']):
    return False

  # Throw out any attempt to shard to 1. This will lock the test suite
  # and prevent go/nplus1shardsproposal from running new shardings
  if int(row['optimal_shard_count']) == 1:
    return False

  # Don't bother resharding if the simulated runtime is greater than the
  # desired runtime.
  if float(row['simulated_max_shard_duration']) > desired_runtime:
    return False

  # Shard values may have changed over the lookback period, so the query
  # results could have multiple rows for each builder+test_suite. Logic below
  # skips the rows that are for outdated shard counts.

  # First check if this suite has been autosharded before
  # If it has been autosharded before, we should only look at the row
  # containing a matching 'shard_count' with the current autoshard value.
  if current_autoshard_val:
    # If this row does not match, skip it. This row is for an old shard count
    # that is no longer being used.
    if int(current_autoshard_val) != int(row['shard_count']):
      return False
  else:
    # Query suggests we should decrease shard count for suite that has
    # never been autosharded
    if int(row['optimal_shard_count']) < int(row['shard_count']):
      # Only use lower shard count value if the suite was previously
      # autosharded.
      # This is because the suite could have been previously autosharded with
      # more shards due to a test regression. If the regression is fixed, that
      # suite should have those extra shards removed.
      # There's many existing suites that already run pretty fast from
      # previous manual shardings. Those technically can have fewer shards as
      # well, but let's leave those alone until we have a good reason to
      # change a bunch of suites at once.
      return False
  return True


def main(args):
  parser = argparse.ArgumentParser(
      description=('Calculate optimal shard counts from bigquery.\n\n'
                   'This script queries for recent test suite runtimes for '
                   'each CQ try builder. If the runtime is above the desired '
                   'runtime threshold, the script will output a suggested '
                   'shard count for that suite.\n'
                   'Example invocation: '
                   '`vpython3 testing/buildbot/query_optimal_shard_counts.py '
                   '--verbose`'),
      formatter_class=argparse.RawDescriptionHelpFormatter,
  )
  parser.add_argument('--output-file',
                      '-o',
                      action='store',
                      help='The filename to store bigquery results.')
  parser.add_argument('--overwrite-output-file',
                      action='store_true',
                      help='If there is already an output-file written, '
                      'overwrite the contents with new data. Otherwise, results'
                      ' will be merged with existing file.')
  parser.add_argument('--lookback-days',
                      default=14,
                      type=int,
                      help='The number of days to lookback.')
  parser.add_argument('--lookback-start-date',
                      type=str,
                      help='Start date of which days to query. Should be in '
                      'year-month-day format e.g. 2023-03-21')
  parser.add_argument('--lookback-end-date',
                      type=str,
                      help='End date of which days to query. Should be in '
                      'year-month-day format e.g. 2023-03-21')
  parser.add_argument('--desired-runtime',
                      default=15,
                      type=int,
                      help=('The desired max runtime minutes that all test '
                            'suites should run at, with a minimum of 5 '
                            'minutes (query is set to filter for suites that '
                            'take at least 5 minutes long. Note that this is '
                            'not the total shard duration, but the max shard '
                            'runtime among all the shards for one triggered '
                            'suite.'))
  parser.add_argument('--percentile',
                      '-p',
                      default=80,
                      type=int,
                      help=('The percentile of suite durations to use to '
                            'calculate the current suite runtime.'))
  parser.add_argument('--min-sample-size',
                      default=2000,
                      type=int,
                      help=('The minimum number of times a suite must run '
                            'longer than the desired runtime, in order to be'
                            ' resharded. 2000 is an appropriate default for '
                            'a 14 day window. For something smaller like a '
                            'couple of days, the sample size should be much '
                            'smaller.'))
  parser.add_argument('--verbose',
                      '-v',
                      action='store_true',
                      help=('Output more info like max shard duration, '
                            'overheads, estimated bot_cost, and more.'))
  opts = parser.parse_args(args)

  if opts.desired_runtime < 5:
    parser.error('Minimum --desired-runtime is 5 minutes.')

  if opts.lookback_start_date and opts.lookback_end_date:
    lookback_start_date = opts.lookback_start_date
    lookback_end_date = opts.lookback_end_date
  else:
    today = datetime.datetime.now()
    start = today - datetime.timedelta(days=opts.lookback_days)
    lookback_start_date = start.strftime('%Y-%m-%d')
    lookback_end_date = today.strftime('%Y-%m-%d')

  print('Querying between {} and {}'.format(lookback_start_date,
                                            lookback_end_date))
  durations = _query_suite_durations(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
      percentile=opts.percentile,
  )

  data = {}
  if not opts.overwrite_output_file and os.path.exists(opts.output_file):
    with open(opts.output_file, 'r') as existing_output_file:
      print('Output file already exists. Will merge query results with existing'
            ' output file.')
      data = json.load(existing_output_file)

  filtered_durations = []
  for d in durations:
    # Filter out durations that don't meet sample size
    if int(d['sample_size']) < opts.min_sample_size:
      continue

    builder_group = d['waterfall_builder_group']
    builder_name = d['waterfall_builder_name']
    test_suite = d['test_suite']

    excluded_tests = BUILDER_TEST_SUITE_EXCLUDE_DICT.get(d['try_builder'])
    if (test_suite in TEST_SUITE_EXCLUDE_SET
        or (excluded_tests and test_suite in excluded_tests)
        or d['try_builder'] in BUILDER_EXCLUDE_SET):
      continue

    # Don't bother resharding suites that are running < 1 minute faster
    # than desired.
    if abs(
        float(d['percentile_duration_minutes']) -
        float(opts.desired_runtime)) < 1:
      continue
    filtered_durations.append(d)

  overhead_dict = _get_overhead_dict(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
  )

  # Add optimal shard counts and filter out when the optimal shard count is 0
  durations_with_optimal_shards = _calculate_and_filter_optimal_shard_counts(
      overhead_dict, filtered_durations, opts.desired_runtime)

  # Add estimated_bot_hour_cost and avg_num_builds_per_peak_hour
  durations_with_optimal_shards_and_bot_hours = (
      _calculate_estimated_bot_hour_cost(
          durations=durations_with_optimal_shards,
          lookback_start_date=lookback_start_date,
          lookback_end_date=lookback_end_date,
      ))

  new_data = {}
  for r in durations_with_optimal_shards_and_bot_hours:
    builder_group = r['waterfall_builder_group']
    builder_name = r['waterfall_builder_name']
    test_suite = r['test_suite']
    if not _meets_optimal_shard_count_and_simulated_duration_requirements(
        r, data, opts.desired_runtime):
      continue
    shard_dict = {
        test_suite: {
            'shards': r['optimal_shard_count'],
        },
    }
    if opts.verbose:
      debug_dict = {
          'avg_num_builds_per_peak_hour':
          r['avg_num_builds_per_peak_hour'],
          'estimated_bot_hour_delta':
          r['estimated_bot_hour_cost'],
          'prev_avg_pending_time_sec':
          float(r['avg_pending_time_sec']),
          'prev_p50_pending_time_sec':
          float(r['p50_pending_time_sec']),
          'prev_p90_pending_time_sec':
          float(r['p90_pending_time_sec']),
          'prev_percentile_duration_minutes':
          float(r['percentile_duration_minutes']),
          'prev_shard_count':
          int(r['shard_count']),
          'simulated_max_shard_duration':
          r['simulated_max_shard_duration'],
          'try_builder':
          r['try_builder'],
          'test_overhead_min':
          r['test_overhead_min'],
      }
      shard_dict[r['test_suite']]['debug'] = debug_dict
    data.setdefault(builder_group, {}).setdefault(builder_name,
                                                  {}).update(shard_dict)
    new_data.setdefault(builder_group, {}).setdefault(builder_name,
                                                      {}).update(shard_dict)

  output_data = json.dumps(data,
                           indent=4,
                           separators=(',', ': '),
                           sort_keys=True)
  print('Results from query:')
  print(json.dumps(new_data, indent=4, separators=(',', ': ')))

  if opts.output_file:
    if opts.overwrite_output_file and os.path.exists(opts.output_file):
      print('Will overwrite existing output file.')
    with open(opts.output_file, 'w') as output_file:
      print('Writing to output file.')
      output_file.write(output_data)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
