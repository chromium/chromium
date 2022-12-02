# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from flake_suppressor_common import queries as queries_module

SUBMITTED_BUILDS_SUBQUERY = """\
  submitted_builds AS ({chromium_builds}
  ),""".format(chromium_builds=queries_module.SUBMITTED_BUILDS_TEMPLATE)

SHERIFF_ROTATIONS_CI_BUILDS_SUBQUERY = """\
  sheriff_rotations_ci_builds AS ({chromium_builds}
  ),""".format(
    chromium_builds=queries_module.SHERIFF_ROTATIONS_CI_BUILDS_TEMPLATE)

# Gets all failures from the past |sample_period| days from CI bots that did not
# already have an associated test suppression when the test ran.
CI_FAILED_TEST_QUERY = """\
WITH
  failed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag") as typ_tags,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation") as typ_expectations,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "step_name") as step_name

  FROM `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status = "FAIL" AND
    exported.realm = "chromium:ci" AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                          INTERVAL @sample_period DAY)
)
SELECT *
FROM failed_tests ft
WHERE
  ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" AND
  (STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_wpt_tests') OR
   STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_web_tests'))
"""

# Gets all failing build culprit results from the past |sample_period| days
# from CI bots that did not already have an associated test suppression when
# the test ran, test with one pass in retry will consider as pass.
# TODO(crbug.com/1382494): Update the query with multiple expectation types
# such as CRASH.
CI_FAILED_BUILD_CULPRIT_TEST_QUERY = """\
WITH
  {sheriff_rotations_ci_builds_subquery}
  failed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag") as typ_tags,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation") as typ_expectations,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "step_name") as step_name,
    ARRAY(
          SELECT value
          FROM tr.variant
          WHERE key = "builder") as builder
  FROM
    `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr,
    sheriff_rotations_ci_builds srcb
  WHERE
    status = "FAIL" AND
    exported.realm = "chromium:ci" AND
    builder = srcb.builder AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                          INTERVAL @sample_period DAY)
  ),
  passed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag") as typ_tags,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation") as typ_expectations,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "step_name") as step_name,
    ARRAY(
          SELECT value
          FROM tr.variant
          WHERE key = "builder") as builder
  FROM
    `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr,
    sheriff_rotations_ci_builds srcb
  WHERE
    status = "PASS" AND
    exported.realm = "chromium:ci" AND
    builder = srcb.builder AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                          INTERVAL @sample_period DAY)
  )
SELECT
  ft.name,
  ft.id,
  ft.builder,
  ft.step_name,
  ft.typ_expectations,
  ft.typ_tags
FROM failed_tests ft
LEFT JOIN passed_tests pt ON (ft.name = pt.name AND ft.id = pt.id)
WHERE
  ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" AND
  pt.name IS NULL AND
  (STARTS_WITH(ARRAY_TO_STRING(ft.step_name, ''), 'blink_wpt_tests') OR
   STARTS_WITH(ARRAY_TO_STRING(ft.step_name, ''), 'blink_web_tests'))
""".format(
    sheriff_rotations_ci_builds_subquery=SHERIFF_ROTATIONS_CI_BUILDS_SUBQUERY)

# Gets all failures from the past |sample_period| days from trybots that did not
# already have an associated test suppresssion when the test ran, only including
# data from builds that were used for CL submission.
TRY_FAILED_TEST_QUERY = """\
WITH
  {submitted_builds_subquery}
  failed_tests AS (
    SELECT
      exported.id,
      test_metadata.name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "step_name") as step_name
    FROM
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr,
      submitted_builds sb
    WHERE
      status = "FAIL"
      AND exported.realm = "chromium:try"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
      AND exported.id = sb.id
  )
SELECT *
FROM failed_tests ft
WHERE
  ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" AND
  (STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_wpt_tests') OR
   STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_web_tests'))
""".format(submitted_builds_subquery=SUBMITTED_BUILDS_SUBQUERY)

# Gets the count of all results in the past |sample_period| days for distinct
# test/tag combinations from CI bots.
CI_RESULT_COUNT_QUERY = """\
WITH
  grouped_results AS (
    SELECT
      exported.id as id,
      test_metadata.name as name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name

    FROM
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
    WHERE
      exported.realm = "chromium:ci"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
  )
SELECT
  COUNT(gr.id) as result_count,
  ANY_VALUE(gr.name) as test_name,
  ANY_VALUE(gr.typ_tags) as typ_tags
FROM grouped_results gr
WHERE (STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_wpt_tests') OR
   STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_web_tests'))
GROUP BY gr.name, ARRAY_TO_STRING(gr.typ_tags, '')
"""

# Gets the count of all results in the past |sample_period| days for distinct
# test/tag combinations from trybots, only including data from builds that were
# used for CL submission.
TRY_RESULT_COUNT_QUERY = """\
WITH
  {submitted_builds_subquery}
  grouped_results AS (
    SELECT
      exported.id as id,
      test_metadata.name as name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name

    FROM
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr
    WHERE
      exported.realm = "chromium:try"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
  )
SELECT
  COUNT(gr.id) as result_count,
  ANY_VALUE(gr.name) as test_name,
  ANY_VALUE(gr.typ_tags) as typ_tags
FROM grouped_results gr
WHERE (STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_wpt_tests') OR
   STARTS_WITH(ARRAY_TO_STRING(step_name, ''), 'blink_web_tests'))
GROUP BY gr.name, ARRAY_TO_STRING(gr.typ_tags, '')
""".format(submitted_builds_subquery=SUBMITTED_BUILDS_SUBQUERY)


class WebTestsBigQueryQuerier(queries_module.BigQueryQuerier):
    def GetFlakyOrFailingCiQuery(self) -> str:
        return CI_FAILED_TEST_QUERY

    def GetFailingBuildCulpritFromCiQuery(self) -> str:
        return CI_FAILED_BUILD_CULPRIT_TEST_QUERY

    def GetFlakyOrFailingTryQuery(self) -> str:
        return TRY_FAILED_TEST_QUERY

    def GetResultCountCIQuery(self) -> str:
        return CI_RESULT_COUNT_QUERY

    def GetResultCountTryQuery(self) -> str:
        return TRY_RESULT_COUNT_QUERY
