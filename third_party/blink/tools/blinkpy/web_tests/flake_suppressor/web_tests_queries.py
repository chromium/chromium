# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import List
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
    (SELECT value FROM tr.tags
     WHERE key = "step_name") as step_name,

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
   (ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" OR
    ARRAY_TO_STRING(ft.typ_expectations, '') = "PassSlow") AND
   (STARTS_WITH(step_name, 'blink_wpt_tests') OR
    STARTS_WITH(step_name, 'blink_web_tests'))
"""

# Gets all failures from the past |sample_period| days from the input builders
# that did not already have an associated test suppression when the test ran.
CI_FAILED_TEST_FROM_BUILDERS_QUERY = """\
WITH
  failed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    status,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag") as typ_tags,
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation") as typ_expectations,
    (SELECT value FROM tr.tags
     WHERE key = "step_name") as step_name,
    (SELECT value FROM tr.variant
     WHERE key = "builder") as builder,

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
  (ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" OR
   ARRAY_TO_STRING(ft.typ_expectations, '') = "PassSlow") AND
  builder IN UNNEST({builder_names}) AND
  (STARTS_WITH(step_name, 'blink_wpt_tests') OR
   STARTS_WITH(step_name, 'blink_web_tests'))
"""

# Gets all failing build culprit results from the past |sample_period| days
# from CI bots that did not already have an associated test suppression when
# the test ran, test with one pass in retry will consider as pass.
# TODO(crbug.com/1382494): Support consistent failure test suppression in a new query.
CI_FAILED_BUILD_CULPRIT_TEST_QUERY = """\
WITH
  {sheriff_rotations_ci_builds_subquery}
  failed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    status,
    TO_JSON_STRING(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag")) as typ_tags_string,
    ANY_VALUE(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag")) as typ_tags,
    TO_JSON_STRING(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation")) as typ_expectations_string,
    ANY_VALUE(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation")) as typ_expectations,
    (SELECT value FROM tr.variant
     WHERE key = "test_suite") as test_suite,
    (SELECT value FROM tr.variant
     WHERE key = "builder") as builder,
     DATE(partition_time) AS date
  FROM
    `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status != "PASS" AND status != "SKIP" AND
    exported.realm = "chromium:ci" AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY)
  GROUP BY id, name, status, test_suite, builder, date, typ_expectations_string, typ_tags_string
  ),
  passed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    (SELECT value FROM tr.variant
     WHERE key = "test_suite") as test_suite,
    (SELECT value FROM tr.variant
     WHERE key = "builder") as builder
  FROM
    `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status = "PASS" AND
    exported.realm = "chromium:ci" AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY)
  GROUP BY id, name, builder, test_suite
  ),
  tests_cause_build_fail AS (
  SELECT
    ft.name,
    ft.id,
    ft.status,
    ft.builder,
    ft.test_suite,
    ft.typ_expectations,
    ft.typ_expectations_string,
    ft.typ_tags,
    ft.typ_tags_string,
    ft.date
  FROM failed_tests ft
  LEFT JOIN passed_tests pt ON (ft.name = pt.name AND ft.id = pt.id
    AND ft.test_suite = pt.test_suite)
  WHERE
    (ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" OR
    ARRAY_TO_STRING(ft.typ_expectations, '') = "PassSlow") AND
    pt.name IS NULL AND
    ft.builder IN (SELECT builder FROM sheriff_rotations_ci_builds) AND
    REGEXP_CONTAINS(ft.test_suite, 'blink_w(pt|eb)_tests')
    AND NOT REGEXP_CONTAINS(ft.test_suite, '^webgpu'))
SELECT
  bf.name,
  bf.id,
  bf.status,
  bf.builder,
  bf.test_suite,
  ANY_VALUE(bf.typ_expectations) AS typ_expectations,
  ANY_VALUE(bf.typ_tags) AS typ_tags,
  bf.date
FROM tests_cause_build_fail bf
INNER JOIN passed_tests pt
ON bf.name = pt.name AND bf.builder = pt.builder AND bf.test_suite = pt.test_suite
GROUP BY bf.name, bf.id, bf.status, bf.builder, bf.test_suite, bf.date, bf.typ_expectations_string, bf.typ_tags_string;
""".format(
    sheriff_rotations_ci_builds_subquery=SHERIFF_ROTATIONS_CI_BUILDS_SUBQUERY)

# Gets the failing input build names culprit results from the past
# |sample_period| days from CI bots that did not already have an associated test
# suppression when  the test ran, test with one pass in retry will consider as
# pass.
CI_FAILED_BUILD_CULPRIT_FROM_BUILDERS_QUERY = """\
WITH
  unpassed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    status,
    TO_JSON_STRING(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag")) as typ_tags_string,
    ANY_VALUE(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "typ_tag")) as typ_tags,
    TO_JSON_STRING(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation")) as typ_expectations_string,
    ANY_VALUE(ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "raw_typ_expectation")) as typ_expectations,
    (SELECT value FROM tr.variant
     WHERE key = "test_suite") as test_suite,
    (SELECT value FROM tr.variant
     WHERE key = "builder") as builder,
     DATE(partition_time) AS date
  FROM
    `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status != "PASS" AND status != "SKIP" AND
    (SELECT value FROM tr.variant
     WHERE key = "builder") IN UNNEST({builder_names}) AND
    exported.realm = "chromium:ci" AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY)
  GROUP BY id, name, status, test_suite, builder, date, typ_expectations_string, typ_tags_string
  ),
  passed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
    (SELECT value FROM tr.variant
     WHERE key = "builder") as builder,
    (SELECT value FROM tr.variant
     WHERE key = "test_suite") as test_suite,
  FROM
    `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status = "PASS" AND
    exported.realm = "chromium:ci" AND
    (SELECT value FROM tr.variant
     WHERE key = "builder") IN UNNEST({builder_names}) AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY)
  GROUP BY id, name, builder, test_suite
  ),
  tests_cause_build_fail AS (
  SELECT
    ut.name,
    ut.id,
    ut.status,
    ut.builder,
    ut.test_suite,
    ut.typ_expectations,
    ut.typ_expectations_string,
    ut.typ_tags,
    ut.typ_tags_string,
    ut.date
  FROM unpassed_tests ut
  LEFT JOIN passed_tests pt ON (ut.name = pt.name AND ut.id = pt.id AND
    ut.test_suite = pt.test_suite)
  WHERE
    (ARRAY_TO_STRING(ut.typ_expectations, '') = "Pass" OR
    ARRAY_TO_STRING(ut.typ_expectations, '') = "PassSlow") AND
    pt.name IS NULL AND
    REGEXP_CONTAINS(ut.test_suite, "blink_w(pt|eb)_tests")
    AND NOT REGEXP_CONTAINS(ut.test_suite, "^webgpu"))
SELECT
  bf.name,
  bf.id,
  bf.status,
  bf.builder,
  bf.test_suite,
  ANY_VALUE(bf.typ_expectations) as typ_expectations,
  ANY_VALUE(bf.typ_tags) as typ_tags,
  bf.date
FROM tests_cause_build_fail bf
INNER JOIN passed_tests pt
ON bf.name = pt.name AND bf.builder = pt.builder AND bf.test_suite = pt.test_suite
GROUP BY bf.name, bf.id, bf.status, bf.builder, bf.test_suite, bf.date, bf.typ_expectations_string, bf.typ_tags_string;
"""

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
    (SELECT value FROM tr.tags
     WHERE key = "step_name") as step_name,
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
  (ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass" OR
   ARRAY_TO_STRING(ft.typ_expectations, '') = "PassSlow") AND
  (STARTS_WITH(step_name, 'blink_wpt_tests') OR
   STARTS_WITH(step_name, 'blink_web_tests'))
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
      (SELECT value FROM tr.tags
       WHERE key = "step_name") as step_name,

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
WHERE (STARTS_WITH(step_name, 'blink_wpt_tests') OR
   STARTS_WITH(step_name, 'blink_web_tests'))
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
      (SELECT value FROM tr.tags
       WHERE key = "step_name") as step_name,

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
WHERE (STARTS_WITH(step_name, 'blink_wpt_tests') OR
   STARTS_WITH(step_name, 'blink_web_tests'))
GROUP BY gr.name, ARRAY_TO_STRING(gr.typ_tags, '')
""".format(submitted_builds_subquery=SUBMITTED_BUILDS_SUBQUERY)

# Gets the count of all results in the past |sample_period| days for distinct
# test/tag combinations from input CI builders.
CI_RESULT_COUNT_FROM_BUILDERS_QUERY = """\
WITH
  ci_results AS (
    SELECT
      exported.id as id,
      test_metadata.name as name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      (SELECT value FROM tr.tags
       WHERE key = "step_name") as step_name,
      (SELECT value FROM tr.variant
       WHERE key = "builder") as builder,

    FROM
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
    WHERE
      exported.realm = "chromium:ci"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
  )
SELECT
  COUNT(cr.id) as result_count,
  cr.name as test_name,
  ANY_VALUE(cr.typ_tags) as typ_tags
FROM ci_results cr
WHERE (STARTS_WITH(step_name, 'blink_wpt_tests') OR
   STARTS_WITH(step_name, 'blink_web_tests')) AND
   builder IN UNNEST({builder_names})
GROUP BY cr.name, ARRAY_TO_STRING(cr.typ_tags, '')
"""


class WebTestsBigQueryQuerier(queries_module.BigQueryQuerier):
    def GetFlakyOrFailingCiQuery(self) -> str:
        return CI_FAILED_TEST_QUERY

    def GetFailingBuildCulpritFromCiQuery(self) -> str:
        return CI_FAILED_BUILD_CULPRIT_TEST_QUERY

    def GetFlakyOrFailingFromCIBuildersQuery(self,
                                             builder_names: List[str]) -> str:
        return CI_FAILED_TEST_FROM_BUILDERS_QUERY.format(
            builder_names=builder_names)

    def GetFailingBuildCulpritFromCIBuildersQuery(
            self, builder_names: List[str]) -> str:
        return CI_FAILED_BUILD_CULPRIT_FROM_BUILDERS_QUERY.format(
            builder_names=builder_names)

    def GetFlakyOrFailingTryQuery(self) -> str:
        return TRY_FAILED_TEST_QUERY

    def GetResultCountCIQuery(self) -> str:
        return CI_RESULT_COUNT_QUERY

    def GetResultCountTryQuery(self) -> str:
        return TRY_RESULT_COUNT_QUERY

    def GetResultCountFromCIBuildersQuery(self,
                                          builder_names: List[str]) -> str:
        return CI_RESULT_COUNT_FROM_BUILDERS_QUERY.format(
            builder_names=builder_names)
