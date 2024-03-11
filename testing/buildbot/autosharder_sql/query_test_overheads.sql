# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
WITH
    build_task_ids AS (
      SELECT
        b.infra.backend.task.id.id build_task_id,
        b.builder.builder try_builder,
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
    # Get all the triggered swarming test tasks for each build
    deduped_tasks AS (
      SELECT DISTINCT
        request.parent_task_id AS parent_task_id,
        (
          SELECT
            SPLIT(tag, ':')[offset(1)]
          FROM UNNEST(c.request.tags) tag
          WHERE STARTS_WITH(tag, 'test_suite:')
        ) AS test_suite,
        # experimental_shard_count will be NULL for regular builds
        CAST(
          (
            SELECT
              SPLIT(tag, ':')[offset(1)]
            FROM UNNEST(c.request.tags) tag
            WHERE STARTS_WITH(tag, 'experimental_shard_count:')
          )
          AS INT64) AS experimental_shard_count,
        # normally_assigned_shard_count will be NULL for regular builds
        CAST(
          (
            SELECT
              SPLIT(tag, ':')[offset(1)]
            FROM UNNEST(c.request.tags) tag
            WHERE STARTS_WITH(tag, 'normally_assigned_shard_count:')
          )
          AS INT64) AS normally_assigned_shard_count,
        TIMESTAMP_DIFF(c.end_time, c.start_time, SECOND)
          running_duration_sec,  # Includes task setup time
        TIMESTAMP_DIFF(c.end_time, c.start_time, SECOND) - c.duration task_setup_duration_sec,
        c.start_time
      FROM `chromium-swarm.swarming.task_results_summary_flat` c
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
    # For each build, test_suite, and shard_count, calculate required durations
    durations_per_build AS (
      SELECT
        test_suite,
        try_builder,
        experimental_shard_count,
        normally_assigned_shard_count,
        MAX(running_duration_sec) max_shard_duration,
        SUM(running_duration_sec) total_shard_duration,
        AVG(task_setup_duration_sec) avg_task_setup_duration,
        COUNT(*) shard_count,
        MAX(tasks.start_time) start_time,
      FROM tasks
      GROUP BY
        build_task_id, test_suite, try_builder, experimental_shard_count,
        normally_assigned_shard_count
    ),
    # Aggregate all durations for each builder, test_suite, and shard_count
    suite_and_builder_durations AS (
      SELECT
        COUNT(*) sample_size,
        d.test_suite,
        d.try_builder,
        shard_count,
        experimental_shard_count,
        normally_assigned_shard_count,
        ROUND(AVG(max_shard_duration) / 60, 2) avg_max_shard_duration,
        ROUND(AVG(total_shard_duration / 60), 2) avg_total_duration_minutes,
        ROUND(APPROX_QUANTILES(total_shard_duration, 1000)[OFFSET(900)] / 60, 2)
          AS p90_total_duration_minutes,
        ROUND(APPROX_QUANTILES(total_shard_duration, 1000)[OFFSET(500)])
          AS p50_total_duration_sec,
        ROUND(APPROX_QUANTILES(avg_task_setup_duration, 1000)[OFFSET(500)])
          AS p50_task_setup_duration_sec,
      FROM durations_per_build d
      WHERE
        # Regular builds
        (
          experimental_shard_count IS NULL
          OR
            # Experimental builds where the shard_count (an integer) is equal to the experimental
            # shard count. This ensures that we filter out weird builds where the triggered shards
            # don’t match up with the experimental_shard_count quantity its supposed to use.
            shard_count = experimental_shard_count)
      GROUP BY
        test_suite, try_builder, experimental_shard_count, normally_assigned_shard_count, shard_count
    ),
    # Get aggregated durations for the experimental shard counts and filter out rows using old
    # unused shard counts
    experimental AS (
      SELECT *
      FROM suite_and_builder_durations
      WHERE
        experimental_shard_count IS NOT NULL
        AND normally_assigned_shard_count IS NOT NULL
    ),
    # Get aggregated durations for non-experimental shard counts and filter out rows using old
    # unused shard counts
    regular AS (
      SELECT *
      FROM suite_and_builder_durations
      WHERE
        experimental_shard_count IS NULL
        AND normally_assigned_shard_count IS NULL
    )
  # Calculate overheads by comparing durations of experimental vs regular
  SELECT
    exp.test_suite,
    exp.try_builder,
    exp.experimental_shard_count,
    exp.normally_assigned_shard_count,
    CAST(reg.p50_task_setup_duration_sec AS INT64) p50_task_setup_duration_sec,
    # Subtract task setup overhead, so only test harness overhead is calculated
    # Use p50 instead of avg, so the small experimental sample size isn’t so affected by outliers
    CAST(IF(
      reg.p50_task_setup_duration_sec
        > (exp.p50_total_duration_sec - reg.p50_total_duration_sec),
      0,
      ROUND(
        (exp.p50_total_duration_sec - reg.p50_total_duration_sec)
            / (exp.experimental_shard_count - exp.normally_assigned_shard_count)
          - reg.p50_task_setup_duration_sec
        )) AS INT64)
      p50_test_harness_overhead_sec,
  FROM experimental exp
  INNER JOIN regular reg
    ON (
      reg.shard_count = exp.normally_assigned_shard_count
      AND reg.try_builder = exp.try_builder
      AND reg.test_suite = exp.test_suite)
  ORDER BY p50_test_harness_overhead_sec DESC
