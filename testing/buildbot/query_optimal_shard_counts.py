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
import subprocess
import sys

_CLOUD_PROJECT_ID = 'chrome-trooper-analytics'

# TODO(crbug.com/1418199): Replace with queried, per-suite overheads, once
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
    'ios-simulator',
    'ios-simulator-full-configs',
    'android-arm64-rel',
])

# Test suites will not be autosharded on all builders that run the test suite.
# Example: 'browser_tests' -> turns of browser_tests on linux-rel and win-rel
TEST_SUITE_EXCLUDE_SET = set([])

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


def _run_query(lookback_start_date, lookback_end_date, desired_runtime,
               percentile, min_sample_size):
  try:
    subprocess.check_call(['which', 'bq'])
  except subprocess.CalledProcessError as e:
    raise RuntimeError(_BQ_SETUP_INSTRUCTION) from e
  query_file = os.path.join(os.path.dirname(__file__),
                            'query_optimal_shard_counts.sql')
  with open(query_file, 'r') as f:
    query_str = f.read()
  query = query_str.format(
      desired_runtime_min=desired_runtime,
      default_overhead_sec=DEFAULT_OVERHEAD_SEC,
      android_overhead_sec=ANDROID_OVERHEAD_SEC,
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
      percentile=percentile,
      min_sample_size=min_sample_size,
  )
  args = [
      "bq", "query", "--project_id=" + _CLOUD_PROJECT_ID, "--format=json",
      "--max_rows=100000", "--nouse_legacy_sql", query
  ]

  try:
    output = subprocess.check_output(args)
  except subprocess.CalledProcessError as e:
    print(e.output)
    raise (e)
  return json.loads(output)


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
  results = _run_query(
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
      desired_runtime=opts.desired_runtime,
      percentile=opts.percentile,
      min_sample_size=opts.min_sample_size,
  )

  data = {}
  new_data = {}
  if not opts.overwrite_output_file and os.path.exists(opts.output_file):
    with open(opts.output_file, 'r') as existing_output_file:
      print('Output file already exists. Will merge query results with existing'
            ' output file.')
      data = json.load(existing_output_file)

  for r in results:
    builder_group = r['waterfall_builder_group']
    builder_name = r['waterfall_builder_name']
    test_suite = r['test_suite']

    excluded_tests = BUILDER_TEST_SUITE_EXCLUDE_DICT.get(r['try_builder'])
    if (test_suite in TEST_SUITE_EXCLUDE_SET
        or (excluded_tests and test_suite in excluded_tests)
        or r['try_builder'] in BUILDER_EXCLUDE_SET):
      continue

    current_autoshard_val = data.get(builder_group,
                                     {}).get(builder_name,
                                             {}).get(test_suite,
                                                     {}).get('shards')

    # No autosharding needed.
    if int(r['optimal_shard_count']) == int(r['shard_count']):
      continue

    # Throw out any attempt to shard to 1. This will lock the test suite
    # and prevent go/nplus1shardsproposal from running new shardings
    if int(r['optimal_shard_count']) == 1:
      continue

    # Shard values may have changed over the lookback period, so the query
    # results could have multiple rows for each builder+test_suite. Logic below
    # skips the rows that are for outdated shard counts.

    # First check if this suite has been autosharded before
    # If it has been autosharded before, we should only look at the row
    # containing a matching 'shard_count' with the current autoshard value.
    if current_autoshard_val:
      # If this row does not match, skip it. This row is for an old shard count
      # that is no longer being used.
      if int(current_autoshard_val) != int(r['shard_count']):
        continue
    else:
      # If a suite is not already being auosharded, we don't know what shard
      # it's actually using at this time if the shard count has been updated
      # within the past lookback_days. So our best guess for which shard count
      # is being used is 'most_usd_shard_count'.
      # So, if it doesn't match, skip this row, which is for an old shard count
      # that is no longer being used.
      if int(r['shard_count']) != int(r['most_used_shard_count']):
        continue

      # Query suggests we should decrease shard count
      if int(r['optimal_shard_count']) < int(r['shard_count']):
        # Only use lower shard count value if the suite was previously
        # autosharded.
        # This is because the suite could have been previously autosharded with
        # more shards due to a test regression. If the regression is fixed, that
        # suite should have those extra shards removed.
        # There's many existing suites that already run pretty fast from
        # previous manual shardings. Those technically can have fewer shards as
        # well, but let's leave those alone until we have a good reason to
        # change a bunch of suites at once.
        if not data.get(builder_group, {}).get(builder_name, {}).get(
            test_suite, {}):
          continue

    shard_dict = {
        test_suite: {
            'shards': r['optimal_shard_count'],
        },
    }
    if opts.verbose:
      debug_dict = {
          'avg_num_builds_per_peak_hour': r['avg_num_builds_per_peak_hour'],
          'estimated_bot_hour_delta': r['estimated_bot_hour_cost'],
          'prev_avg_pending_time_sec': r['avg_pending_time_sec'],
          'prev_p50_pending_time_sec': r['p50_pending_time_sec'],
          'prev_p90_pending_time_sec': r['p90_pending_time_sec'],
          'prev_percentile_duration_minutes': r['percentile_duration_minutes'],
          'prev_shard_count': r['shard_count'],
          'simulated_max_shard_duration': r['simulated_max_shard_duration'],
          'try_builder': r['try_builder'],
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
