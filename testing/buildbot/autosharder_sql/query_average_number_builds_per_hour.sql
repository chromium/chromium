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
),
# Get quantity of builds per hour and day
builds_per_hour_and_day_count AS (
  SELECT
    try_builder,
    TIMESTAMP_TRUNC(b.start_time, DAY, 'America/Los_Angeles') day,
    EXTRACT(HOUR FROM b.start_time AT TIME ZONE 'America/Los_Angeles') hour,
    COUNT(*) hour_count,
  FROM build_task_ids b
  WHERE
    EXTRACT(
      DAYOFWEEK FROM b.start_time AT TIME ZONE 'America/Los_Angeles') > 1
    AND EXTRACT(
      DAYOFWEEK FROM b.start_time AT TIME ZONE 'America/Los_Angeles') < 7
  GROUP BY try_builder, day, hour
),
# Get quantity of builds per day, using the peak hour
builds_per_day_peak_hour_count AS (
  SELECT
    try_builder,
    day,
    MAX(hour_count) count
  FROM builds_per_hour_and_day_count b
  GROUP BY try_builder, day
)
# Get average 1PM M-F builds per hour, grouped by try_builder
# This is the "peak hour" build count estimate for each try_builder and will
# be used at the end to estimate bot cost per hour.
SELECT
  try_builder,
  ROUND(avg(count)) avg_count
FROM builds_per_day_peak_hour_count
GROUP BY try_builder
