# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# TODO(crbug/1493793): Split up this query into two: one for calculating
# shard counts and the other for test overheads
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
    # experimental_shard_count will be NULL for regular builds
    # See go/nplus1shardsproposal
    CAST(
      (
        SELECT SPLIT(tag, ':')[offset(1)]
        FROM UNNEST(request.tags) tag
        WHERE STARTS_WITH(tag, 'experimental_shard_count:')
      )
      AS INT64) AS experimental_shard_count,
    # normally_assigned_shard_count will be NULL for regular builds
    # See go/nplus1shardsproposal
    CAST(
      (
        SELECT SPLIT(tag, ':')[offset(1)]
        FROM UNNEST(request.tags) tag
        WHERE STARTS_WITH(tag, 'normally_assigned_shard_count:')
      )
      AS INT64) AS normally_assigned_shard_count,
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
# For each build, test_suite, waterfall_builder_group, waterfall_builder_name,
# and regular/experimental shard_count calculate required durations
durations_per_build AS (
  SELECT
    test_suite,
    try_builder,
    waterfall_builder_group,
    waterfall_builder_name,
    experimental_shard_count,
    normally_assigned_shard_count,
    MAX(running_duration_sec) max_shard_duration_sec,
    MAX(task_setup_overhead_sec) max_task_setup_overhead_sec,
    MAX(pending_time_sec) max_pending_time_sec,
    SUM(running_duration_sec) total_shard_duration,
    COUNT(*) shard_count,
  FROM tasks
  GROUP BY
    build_task_id,
    test_suite,
    try_builder,
    waterfall_builder_group,
    waterfall_builder_name,
    experimental_shard_count,
    normally_assigned_shard_count
),
# Aggregate all durations for each builder, test_suite,
# waterfall_builder_group, waterfall_builder_name, and regular/experimental
# shard_count
suite_and_builder_durations AS (
  SELECT
    COUNT(*) sample_size,
    d.test_suite,
    d.try_builder,
    d.waterfall_builder_group,
    d.waterfall_builder_name,
    shard_count,
    experimental_shard_count,
    normally_assigned_shard_count,
    ROUND(
      APPROX_QUANTILES(max_shard_duration_sec, 1000)[OFFSET({percentile}0)] / 60, 2)
      AS percentile_duration_minutes,
    AVG(max_task_setup_overhead_sec) avg_task_setup_overhead_sec,
    ROUND(AVG(max_pending_time_sec), 1) avg_pending_time_sec,
    ROUND(
    APPROX_QUANTILES(
      max_pending_time_sec, 1000)[OFFSET(500)], 1) AS p50_pending_time_sec,
    ROUND(
      APPROX_QUANTILES(
        max_pending_time_sec, 1000)[OFFSET(900)], 1) AS p90_pending_time_sec,
    ROUND(APPROX_QUANTILES(total_shard_duration, 100)[OFFSET(50)])
      AS p50_total_duration_sec,
  FROM durations_per_build d
  WHERE
    # Regular builds
    (
      experimental_shard_count IS NULL
      OR
        # Experimental builds where the shard_count (an integer) is equal to the experimental
        # shard count. This ensures that we filter out weird builds where the triggered shards
        # don’t match up with the experimental_shard_count quantity its supposed to use.
        shard_count = experimental_shard_count
     )
  GROUP BY
    test_suite,
    try_builder,
    waterfall_builder_group,
    waterfall_builder_name,
    experimental_shard_count,
    normally_assigned_shard_count,
    shard_count
),
# If a suite had its shards updated within the past lookback_days, there
# will be multiple rows for multiple shard counts. To be able to know which
# one to use, we will attach a "most_used_shard_count" to indicate what
# shard_count is currently being used (a best guess).
most_used_shard_counts AS (
  SELECT
    ARRAY_AGG(
      shard_count ORDER BY sample_size DESC)[
      OFFSET(0)]
      AS most_used_shard_count,
    test_suite,
    try_builder
  FROM suite_and_builder_durations
  WHERE
    experimental_shard_count IS NULL
    AND normally_assigned_shard_count IS NULL
  GROUP BY test_suite, try_builder
),
# Get aggregated durations for the experimental shard counts and filter out rows using old
# unused shard counts
experimental AS (
  SELECT l.*
  FROM suite_and_builder_durations l
  INNER JOIN most_used_shard_counts m
    ON
      l.try_builder = m.try_builder
      AND l.test_suite = m.test_suite
      AND l.normally_assigned_shard_count = m.most_used_shard_count
  WHERE
    experimental_shard_count IS NOT NULL
    AND normally_assigned_shard_count IS NOT NULL
),
# Get aggregated durations for non-experimental shard counts and filter out rows using old
# unused shard counts
regular AS (
  SELECT l.*
  FROM suite_and_builder_durations l
  INNER JOIN most_used_shard_counts m
    ON
      l.try_builder = m.try_builder
      AND l.test_suite = m.test_suite
      AND l.shard_count = m.most_used_shard_count
  WHERE
    experimental_shard_count IS NULL
    AND normally_assigned_shard_count IS NULL
),
# Calculate overheads by comparing durations of experimental vs regular
test_overheads AS (
  SELECT
    exp.test_suite,
    exp.try_builder,
    exp.experimental_shard_count,
    exp.normally_assigned_shard_count,
    reg.avg_task_setup_overhead_sec avg_task_setup_overhead_sec,
    exp.sample_size,
    # Subtract task setup overhead, so only test harness overhead is calculated
    # Use p50 instead of avg, so the small experimental sample size isn’t so affected by outliers
    (exp.p50_total_duration_sec - reg.p50_total_duration_sec)
      / (exp.experimental_shard_count - exp.normally_assigned_shard_count)
      - reg.avg_task_setup_overhead_sec AS p50_test_harness_overhead_sec,
  FROM experimental exp
  INNER JOIN regular reg
    ON (
      reg.shard_count = exp.normally_assigned_shard_count
      AND reg.try_builder = exp.try_builder
      AND reg.test_suite = exp.test_suite)
  ORDER BY p50_test_harness_overhead_sec DESC
),
long_poles_with_overheads AS (
  SELECT
    f.*,
    # test_overhead_min is the total test overhead encompassing task setup
    # + test harness overhead
    IF(
      (o.p50_test_harness_overhead_sec IS NOT NULL AND o.avg_task_setup_overhead_sec IS NOT NULL),
      ROUND((o.p50_test_harness_overhead_sec + o.avg_task_setup_overhead_sec) / 60, 2),
      # N plus 1 shards experiment is not enabled on some builders,
      # so use default overheads for those.
      IF(
        f.try_builder LIKE 'android%',
        {android_overhead_sec}/ 60,
        {default_overhead_sec}/ 60)) AS test_overhead_min,
  FROM
    (
      SELECT
        r.*,
        m.most_used_shard_count
      FROM
        suite_and_builder_durations r
      # Only look at the most recent durations
      INNER JOIN most_used_shard_counts m
        ON (
          r.try_builder = m.try_builder
          AND r.test_suite = m.test_suite
          AND r.shard_count = m.most_used_shard_count)
    ) AS f
  LEFT JOIN test_overheads o
    ON (
      f.try_builder = o.try_builder
      AND f.test_suite = o.test_suite
      AND f.shard_count = o.normally_assigned_shard_count)
),
# Using the percentile and estimated test overhead durations from the
# long_poles query above, calculate the optimal shard_count per suite and
# builder.
optimal_shard_counts AS (
  SELECT
    l.*,
    CAST(
      CEILING(
        (percentile_duration_minutes * shard_count - (test_overhead_min * shard_count))
        / ({desired_runtime_min} - test_overhead_min))
      AS INT64)
      optimal_shard_count,
    (
      SELECT
        avg_count
      FROM avg_builds_per_hour_count a
      WHERE a.try_builder = l.try_builder
    ) avg_num_builds_per_peak_hour
  FROM long_poles_with_overheads l
)
# Return optimal_shard_counts with a simulated shard duration and estimated
# bot hour cost.
SELECT
  o.*,
  ROUND(
    percentile_duration_minutes * shard_count / optimal_shard_count, 2)
    AS simulated_max_shard_duration,
  ROUND(
    (optimal_shard_count - shard_count)
    * (test_overhead_min / 60) * avg_num_builds_per_peak_hour,
    2) estimated_bot_hour_cost
FROM
  optimal_shard_counts o
WHERE
  optimal_shard_count > 0
  AND sample_size > {min_sample_size}
