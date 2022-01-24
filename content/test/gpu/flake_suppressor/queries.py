# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for querying BigQuery."""

import json
import os
import subprocess

MAX_ROWS = (2**31) - 1

# Gets all failures from the past X days that did not already have an associated
# test suppression when the test ran.
# TODO(crbug.com/1192733): Look into updating this to also check try results
# once crbug.com/1217300 is complete.
QUERY = """\
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
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      status = "FAIL"
      AND exported.realm = "chromium:ci"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
  )
SELECT *
FROM failed_tests ft
WHERE
  ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass"
"""


def GetFlakyOrFailingTests(sample_period, billing_project):
  """Gets all flaky or failing GPU tests in the given |sample_period|.

  Args:
    sample_period: An int containing the number of days in the past from the
        current time to pull results from.
    billing_project: A string containing the billing project to use for
        BigQuery queries.

  Returns:
    A JSON representation of the BigQuery results containing all found flaky or
    failing test results.
  """
  cmd = [
      'bq',
      'query',
      '--max_rows=%d' % MAX_ROWS,
      '--format=json',
      '--project_id=%s' % billing_project,
      '--use_legacy_sql=false',
      '--parameter=sample_period:INT64:%d' % sample_period,
      QUERY,
  ]

  with open(os.devnull, 'w') as devnull:
    completed_process = subprocess.run(cmd,
                                       stdout=subprocess.PIPE,
                                       stderr=devnull,
                                       check=True,
                                       text=True)
  return json.loads(completed_process.stdout)
