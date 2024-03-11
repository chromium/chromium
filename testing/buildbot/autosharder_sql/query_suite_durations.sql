# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
WITH
# Get swarming task IDs of builds that occurred between lookback dates.
build_task_ids AS (
  SELECT
    b.infra.backend.task.id.id build_task_id,
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
    AND b.builder.project = 'chromium'
),
deduped_tasks AS (
  SELECT DISTINCT
    request.parent_task_id AS parent_task_id,
    (
      SELECT SPLIT(tag, ':')[offset(1)]
      FROM UNNEST(request.tags) tag
      WHERE REGEXP_CONTAINS(tag, 'waterfall_builder_group:')
    ) AS waterfall_builder_group,
    (
      SELECT SPLIT(tag, ':')[offset(1)]
      FROM UNNEST(request.tags) tag
      WHERE REGEXP_CONTAINS(tag, 'waterfall_buildername:')
    ) AS waterfall_builder_name,
    (
      SELECT SPLIT(tag, ':')[offset(1)]
      FROM UNNEST(request.tags) tag
      WHERE REGEXP_CONTAINS(tag, 'test_suite:')
    ) AS test_suite,
    # Excludes task setup overhead
    duration AS running_duration_sec,
    TIMESTAMP_DIFF(
      start_time, create_time, SECOND) pending_time_sec,
  FROM `chromium-swarm.swarming.task_results_summary`
  WHERE
    request.parent_task_id IS NOT NULL
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
# For each build and test_suite calculate required durations
durations_per_build_and_suite AS (
  SELECT
    test_suite,
    ANY_VALUE(try_builder) try_builder,
    ANY_VALUE(waterfall_builder_group) waterfall_builder_group,
    ANY_VALUE(waterfall_builder_name) waterfall_builder_name,
    MAX(running_duration_sec) max_shard_duration_sec,
    MAX(pending_time_sec) max_pending_time_sec,
    SUM(running_duration_sec) total_shard_duration,
    COUNT(*) shard_count,
  FROM tasks
  GROUP BY
    build_task_id,
    test_suite
),
# Aggregate all durations for each builder, test_suite,
# waterfall_builder_group, waterfall_builder_name, and shard_count
suite_and_builder_durations AS (
  SELECT
    COUNT(*) sample_size,
    d.test_suite,
    d.try_builder,
    d.waterfall_builder_group,
    d.waterfall_builder_name,
    shard_count,
    ROUND(
      APPROX_QUANTILES(max_shard_duration_sec, 1000)[OFFSET({percentile}0)] / 60, 2)
      AS percentile_duration_minutes,
    ROUND(AVG(max_pending_time_sec), 1) avg_pending_time_sec,
    ROUND(
    APPROX_QUANTILES(
      max_pending_time_sec, 1000)[OFFSET(500)], 1) AS p50_pending_time_sec,
    ROUND(
      APPROX_QUANTILES(
        max_pending_time_sec, 1000)[OFFSET(900)], 1) AS p90_pending_time_sec,
    ROUND(APPROX_QUANTILES(total_shard_duration, 100)[OFFSET(50)])
      AS p50_total_duration_sec,
  FROM durations_per_build_and_suite d
  GROUP BY
    test_suite,
    try_builder,
    waterfall_builder_group,
    waterfall_builder_name,
    shard_count
),
# If a suite had its shards updated within the past lookback_days, there
# will be multiple rows for multiple shard counts. To be able to know which
# one to use, we will attach a "most_used_shard_count" to indicate what
# shard_count is currently being used (a best guess).
most_used_shard_counts AS (
  SELECT
    ARRAY_AGG(
      (shard_count, sample_size) ORDER BY sample_size DESC)[
      OFFSET(0)][OFFSET(0)]
      AS most_used_shard_count,
    test_suite,
    try_builder
  FROM suite_and_builder_durations
  GROUP BY test_suite, try_builder
)
SELECT
  r.*,
FROM
  suite_and_builder_durations r
  # Only look at the most recent durations
  JOIN most_used_shard_counts m
    ON (
      r.try_builder = m.try_builder
      AND r.test_suite = m.test_suite
      AND r.shard_count = m.most_used_shard_count)

