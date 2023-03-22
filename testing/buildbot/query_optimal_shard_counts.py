"""Script to query optimal shards for swarmed test suites.

This script queries bigquery for recent test suite runtimes. For suites running
over the desired max runtime, it'll suggest optimal shard counts to bring the
duration below the desired max runtime.
"""
#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import json
import subprocess
import sys

_CLOUD_PROJECT_ID = 'chrome-trooper-analytics'

# TODO(crbug.com/1418199): Replace with queried, per-suite overheads, once
# infra is set up to support automated overhead measurements.
# See go/nplus1shardsproposal
DEFAULT_OVERHEAD_SEC = 60 * 0.5
ANDROID_OVERHEAD_SEC = 60 * 2

# Builders that should currently not be autosharded because they don't use GCE
# swarming bots and have capacity issues.
BUILDER_EXCLUDE_LIST = (
    'mac-rel',
    'ios-simulator',
    'ios-simulator-full-configs',
    'android-arm64-rel',
)

QUERY = """
  WITH
    # Get swarming task IDs of builds that occurred between lookback dates.
    build_task_ids AS (
      SELECT
        b.infra.swarming.task_id build_task_id,
        b.builder.builder try_builder,
        b.start_time,
      FROM
        `cr-buildbucket.chromium.builds` b
      INNER JOIN `chrome-trooper-analytics.metrics.cq_builders` cq_builders
        ON b.builder.builder = cq_builders.builder
      WHERE
        b.start_time > '{lookback_start_date}'
        AND b.start_time <= '{lookback_end_date}'
        AND b.builder.bucket = 'try'
        AND JSON_QUERY(b.output.properties, '$.rts_was_used') IS NULL
        AND b.status = 'SUCCESS'
    ),
    # Get quantity of builds that started at 1PM M-F MTV timezone,
    # grouped by try_builder and day
    builds_per_1pm_day_count AS (
      SELECT
        try_builder,
        TIMESTAMP_TRUNC(b.start_time, DAY, 'America/Los_Angeles') day,
        COUNT(*) count,
      FROM build_task_ids b
      WHERE
        EXTRACT(HOUR FROM b.start_time AT TIME ZONE 'America/Los_Angeles') = 13
        AND EXTRACT(
          DAYOFWEEK FROM b.start_time AT TIME ZONE 'America/Los_Angeles') > 1
        AND EXTRACT(
          DAYOFWEEK FROM b.start_time AT TIME ZONE 'America/Los_Angeles') < 7
      GROUP BY try_builder, day
    ),
    # Get average 1PM M-F builds per hour, grouped by try_builder
    # This is the "peak hour" build count estimate for each try_builder and will
    # be used at the end to estimate bot cost per hour.
    avg_builds_per_hour_count AS (
      SELECT
        try_builder,
        ROUND(avg(count)) avg_count
      FROM builds_per_1pm_day_count
      GROUP BY try_builder
    ),
    # Now get the test swarming tasks triggered by each build from
    # build_task_ids.
    tasks AS (
      SELECT
        p.*,
        (
          SELECT
            SPLIT(tag, ':')[offset(1)]
          FROM UNNEST(c.request.tags) tag
          WHERE REGEXP_CONTAINS(tag, 'waterfall_builder_group:')
        ) AS waterfall_builder_group,
        (
          SELECT
            SPLIT(tag, ':')[offset(1)]
          FROM UNNEST(c.request.tags) tag
          WHERE REGEXP_CONTAINS(tag, 'waterfall_buildername:')
        ) AS waterfall_builder_name,
        (
          SELECT
            SPLIT(tag, ':')[offset(1)]
          FROM UNNEST(c.request.tags) tag
          WHERE REGEXP_CONTAINS(tag, 'test_suite:')
        ) AS test_suite,
        # Excludes task setup overhead
        c.duration running_duration_sec,
        # This is the time it takes for the swarming bot to get ready to run
        # the swarming task. This is different from the test harness overhead,
        # which occurs after the bot has started running the task.
        TIMESTAMP_DIFF(
          c.end_time, c.start_time, SECOND)-c.duration task_setup_overhead_sec,
        TIMESTAMP_DIFF(
          c.start_time, c.create_time, SECOND) pending_time_sec,
      FROM build_task_ids p
      JOIN `chromium-swarm.swarming.task_results_summary_flat` c
        ON p.build_task_id = c.request.parent_task_id
      # Ignore all retry and flakiness step level runs
      WHERE
        c.request.name NOT LIKE '%retry shards%'
        AND c.request.name NOT LIKE '%without patch%'
        AND c.request.name NOT LIKE '%check flakiness%'
        AND c.try_number != 0
    ),
    # Get a variety of durations and shard counts for each build's triggered
    # suites.
    suite_durations AS (
      SELECT
        build_task_id,
        test_suite,
        waterfall_builder_group,
        waterfall_builder_name,
        try_builder,
        MAX(running_duration_sec) max_shard_duration_sec,
        MAX(task_setup_overhead_sec) max_task_setup_overhead_sec,
        MAX(pending_time_sec) max_pending_time_sec,
        COUNT(*) shard_count
      FROM tasks
      GROUP BY
        build_task_id,
        test_suite,
        waterfall_builder_group,
        waterfall_builder_name,
        try_builder
    ),
    # Group suite_durations by test suite, builder, and shard count
    # Calculate percentiles for a variety of durations.
    # Filter out rows that don't exceed the duration threshold and don't satisfy
    # the sample size requirement.
    long_poles AS (
      SELECT
        test_suite,
        waterfall_builder_group,
        waterfall_builder_name,
        try_builder,
        shard_count,
        COUNT(*) sample_size,
        ROUND(
          APPROX_QUANTILES(
            max_shard_duration_sec, 100)[OFFSET({percentile})] / 60, 2)
          AS percentile_duration_minutes,
        AVG(max_task_setup_overhead_sec) avg_task_setup_overhead_sec,
        ROUND(AVG(max_pending_time_sec), 1) avg_pending_time_sec,
        ROUND(
          APPROX_QUANTILES(
            max_pending_time_sec, 100)[OFFSET(50)], 1) AS p50_pending_time_sec,
        ROUND(
          APPROX_QUANTILES(
            max_pending_time_sec, 100)[OFFSET(90)], 1) AS p90_pending_time_sec,
        IF(
          try_builder LIKE 'android%',
          {android_overhead_sec}/60,
          {default_overhead_sec}/60
        ) test_overhead_min
      FROM suite_durations
      WHERE
        waterfall_builder_group IS NOT NULL
        AND try_builder NOT IN {builder_exclude_list}
      GROUP BY
        test_suite,
        waterfall_builder_group,
        waterfall_builder_name,
        try_builder,
        shard_count
      HAVING
        percentile_duration_minutes > {desired_runtime_min}
        AND sample_size > {min_sample_size}
      ORDER BY sample_size DESC, percentile_duration_minutes DESC
    ),
    # Using the percentile and estimated test overhead durations from the
    # long_poles query above, calculate the optimal shard_count per suite and
    # builder.
    optimal_shard_counts AS (
      SELECT
        l.*,
        CAST(CEILING(
          (percentile_duration_minutes * shard_count -
          (test_overhead_min * shard_count))
          / ({desired_runtime_min} - test_overhead_min))
        AS INT64) optimal_shard_count,
        (
          SELECT
            avg_count
          FROM avg_builds_per_hour_count a
          WHERE a.try_builder = l.try_builder
        ) avg_num_builds_per_peak_hour
      FROM long_poles l
    )
    # Return optimal_shard_counts with a simulated shard duration and estimated
    # bot hour cost.
    SELECT
      *,
      ROUND(
        percentile_duration_minutes * shard_count / optimal_shard_count, 2)
        AS simulated_max_shard_duration,
      ROUND(
        (optimal_shard_count - shard_count)
        * (avg_task_setup_overhead_sec / 60 + test_overhead_min) /
        60 * avg_num_builds_per_peak_hour,
        2) estimated_bot_hour_cost
    FROM
      optimal_shard_counts
"""
_BQ_ERROR_MESSAGE = """
** NOTE: this script is only for Googlers to use. **

bq script isn't found on your machine. To run this script, you need to be able
to run bigquery in your terminal.
"""


def _run_query(lookback_start_date, lookback_end_date, desired_runtime,
               percentile, min_sample_size):
  try:
    subprocess.check_call(['which', 'bq'])
  except subprocess.CalledProcessError as e:
    raise RuntimeError(_BQ_ERROR_MESSAGE) from e
  query = QUERY.format(
      desired_runtime_min=desired_runtime,
      default_overhead_sec=DEFAULT_OVERHEAD_SEC,
      android_overhead_sec=ANDROID_OVERHEAD_SEC,
      lookback_start_date=lookback_start_date,
      lookback_end_date=lookback_end_date,
      percentile=percentile,
      builder_exclude_list=BUILDER_EXCLUDE_LIST,
      min_sample_size=min_sample_size,
  )
  args = [
      "bq", "query", "--project_id=" + _CLOUD_PROJECT_ID, "--format=json",
      "--max_rows=100000", "--nouse_legacy_sql", query
  ]

  output = subprocess.check_output(args)
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
                            'suites should run at. Note that this is not the '
                            'total shard duration, but the max shard runtime '
                            'among all the shards for one triggered suite.'))
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
  for r in results:
    builder_group = r['waterfall_builder_group']
    builder_name = r['waterfall_builder_name']
    shard_dict = {
        r['test_suite']: {
            'shards': r['optimal_shard_count'],
        },
    }
    if opts.verbose:
      suite_dict = shard_dict[r['test_suite']]
      suite_dict['current_shard_count'] = r['shard_count']
      suite_dict['current_percentile_duration_minutes'] = r[
          'percentile_duration_minutes']
      suite_dict['simulated_max_shard_duration'] = r[
          'simulated_max_shard_duration']
      suite_dict['estimated_bot_hour_cost'] = r['estimated_bot_hour_cost']
      suite_dict['try_builder'] = r['try_builder']
      suite_dict['avg_num_builds_per_peak_hour'] = r[
          'avg_num_builds_per_peak_hour']
      suite_dict['avg_pending_time_sec'] = r['avg_pending_time_sec']
      suite_dict['p50_pending_time_sec'] = r['p50_pending_time_sec']
      suite_dict['p90_pending_time_sec'] = r['p90_pending_time_sec']
    data.setdefault(builder_group, {}).setdefault(builder_name,
                                                  {}).update(shard_dict)

  output_data = json.dumps(data, indent=4, separators=(',', ': '))
  if opts.output_file:
    with open(opts.output_file, 'w') as output_file:
      output_file.write(output_data)
  print(output_data)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
