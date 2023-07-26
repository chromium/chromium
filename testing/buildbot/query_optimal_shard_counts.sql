# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
WITH
# Get swarming task IDs of builds that occurred between lookback dates.
build_task_ids AS (
  SELECT
    b.infra.swarming.task_id build_task_id,
    b.builder.builder try_builder,
    b.start_time,
  FROM
    `cr-buildbucket.chromium.builds` b
  WHERE
    b.start_time > '{lookback_start_date}'
    AND b.start_time <= '{lookback_end_date}'
    AND b.builder.bucket = 'try'
    AND JSON_VALUE(b.input.properties, '$.cq') = 'required'
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
deduped_tasks AS (
  SELECT DISTINCT
    request.parent_task_id AS parent_task_id,
   (
      SELECT
        SPLIT(tag, ':')[offset(1)]
      FROM UNNEST(request.tags) tag
      WHERE REGEXP_CONTAINS(tag, 'waterfall_builder_group:')
    ) AS waterfall_builder_group,
    (
      SELECT
        SPLIT(tag, ':')[offset(1)]
      FROM UNNEST(request.tags) tag
      WHERE REGEXP_CONTAINS(tag, 'waterfall_buildername:')
    ) AS waterfall_builder_name,
    (
      SELECT
        SPLIT(tag, ':')[offset(1)]
      FROM UNNEST(request.tags) tag
      WHERE REGEXP_CONTAINS(tag, 'test_suite:')
    ) AS test_suite,
    # Excludes task setup overhead
    duration AS running_duration_sec,
    # This is the time it takes for the swarming bot to get ready to run
    # the swarming task. This is different from the test harness overhead,
    # which occurs after the bot has started running the task.
    TIMESTAMP_DIFF(
      end_time, start_time, SECOND) - duration task_setup_overhead_sec,
    TIMESTAMP_DIFF(
      start_time, create_time, SECOND) pending_time_sec,
  FROM `chromium-swarm.swarming.task_results_summary`
  WHERE request.parent_task_id IS NOT NULL
    # Ignore all retry and flakiness step level runs
    # TODO(sshrimp): this is fragile and should be handled another way
    AND request.name NOT LIKE '%retry shards%'
    AND request.name NOT LIKE '%without patch%'
    AND request.name NOT LIKE '%check flakiness%'
    # Ignore tasks deduped by swarming
    AND start_time > create_time
    AND DATE(end_time) BETWEEN
      DATE_SUB(DATE('{lookback_start_date}'), INTERVAL 1 DAY) AND
      DATE_ADD(DATE('{lookback_end_date}'), INTERVAL 1 DAY)
),
# Now get the test swarming tasks triggered by each build from
# build_task_ids.
tasks AS (
  SELECT
    p.*,
    c.*,
  FROM build_task_ids p
  JOIN deduped_tasks c
    ON p.build_task_id = c.parent_task_id
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
        max_shard_duration_sec, 1000)[OFFSET({percentile}0)] / 60, 2)
      AS percentile_duration_minutes,
    AVG(max_task_setup_overhead_sec) avg_task_setup_overhead_sec,
    ROUND(AVG(max_pending_time_sec), 1) avg_pending_time_sec,
    ROUND(
      APPROX_QUANTILES(
        max_pending_time_sec, 1000)[OFFSET(500)], 1) AS p50_pending_time_sec,
    ROUND(
      APPROX_QUANTILES(
        max_pending_time_sec, 1000)[OFFSET(900)], 1) AS p90_pending_time_sec,
    IF(
      try_builder LIKE 'android%',
      {android_overhead_sec}/60,
      {default_overhead_sec}/60
    ) test_overhead_min
  FROM suite_durations
  WHERE
    waterfall_builder_group IS NOT NULL
  GROUP BY
    test_suite,
    waterfall_builder_group,
    waterfall_builder_name,
    try_builder,
    shard_count
  HAVING
    # Filters out suites that obviously don't need to be sharded more
    # and prevents optimal_shard_count from being 0, causing a division
    # by 0 error.
    percentile_duration_minutes > 5
    AND sample_size > {min_sample_size}
  ORDER BY sample_size DESC, percentile_duration_minutes DESC
),
# If a suite had its shards updated within the past lookback_days, there
# will be multiple rows for multiple shard counts. To be able to know which
# one to use, we'll attach a "most_used_shard_count" to indicate what
# shard_count is currently being used (a best guess).
most_used_shard_counts AS (
    SELECT
    ARRAY_AGG(
      shard_count ORDER BY sample_size DESC)[OFFSET(0)]
      AS most_used_shard_count,
    test_suite,
    try_builder
  FROM long_poles
  GROUP BY test_suite, try_builder
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
  o.*,
  m.most_used_shard_count,
  ROUND(
    percentile_duration_minutes * shard_count / optimal_shard_count, 2)
    AS simulated_max_shard_duration,
  ROUND(
    (optimal_shard_count - shard_count)
    * (avg_task_setup_overhead_sec / 60 + test_overhead_min) /
    60 * avg_num_builds_per_peak_hour,
    2) estimated_bot_hour_cost
FROM
  optimal_shard_counts o
  INNER JOIN most_used_shard_counts m
  ON o.try_builder = m.try_builder AND
  o.test_suite = m.test_suite
