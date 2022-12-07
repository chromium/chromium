#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for comparing known typ tags to what's generated on the bots.

This is important for making unknown generated tags fatal, as we need an easy
way to check if any bots are producing tags we don't know about.

Depends on the `bq` tool, which is available as part of the Google Cloud SDK
https://cloud.google.com/sdk/docs/quickstarts.
"""

import argparse
import json
import os
import subprocess

from gpu_path_util import setup_typ_paths  # pylint: disable=unused-import

from gpu_tests import gpu_integration_test

from typ import expectations_parser

BQ_QUERY_TEMPLATE = """\
WITH
    tags AS (
        SELECT (
            ARRAY(
                SELECT value
                FROM tr.tags
                WHERE key = "typ_tag"
            )
        ) AS typ_tags
        FROM `{table}` tr
        WHERE DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
    )
SELECT DISTINCT typ_tags
FROM tags
CROSS JOIN UNNEST(tags.typ_tags) as typ_tags
"""
MAX_ROWS = (2**31) - 1


def ParseArgs():
  parser = argparse.ArgumentParser(
      'Script for finding cases where the typ tags generated on the bots and '
      'the typ tags we know about are out of sync.')
  parser.add_argument('--project',
                      required=True,
                      help='The billing project to use for BigQuery queries. '
                      'Must have access to the ResultDB BQ tables, e.g. '
                      '"chrome-luci-data.chromium.gpu_ci_test_results".')
  return parser.parse_args()


def _GetUsedTags():
  """Helper function to get all currently used tags."""
  # Get the list of tags in expectation files. Any expectation file will do
  # since tags are synced between all of them.
  expectation_file_path = os.path.join(os.path.dirname(__file__), 'gpu_tests',
                                       'test_expectations',
                                       'info_collection_expectations.txt')
  with open(expectation_file_path) as f:
    list_parser = expectations_parser.TaggedTestListParser(f.read())
  used_tags = set()
  for tag_set in list_parser.tag_sets:
    used_tags |= set(tag_set)
  return used_tags


def _GetGeneratedTags(args):
  """Helper function to get all currently generated tags from bots."""
  generated_tags = set()
  for table in [
      'chrome-luci-data.chromium.gpu_ci_test_results',
      'chrome-luci-data.chromium.gpu_try_test_results'
  ]:
    query = BQ_QUERY_TEMPLATE.format(table=table)
    cmd = [
        'bq',
        'query',
        '--max_rows=%d' % MAX_ROWS,
        '--format=json',
        '--project_id=%s' % args.project,
        '--use_legacy_sql=false',
        query,
    ]
    with open(os.devnull, 'w') as devnull:
      try:
        stdout = subprocess.check_output(cmd, stderr=devnull)
      except subprocess.CalledProcessError as e:
        print(e.output)
        raise
    results = json.loads(stdout)
    for pair in results:
      generated_tags |= set(pair.values())
  return generated_tags


def main():
  args = ParseArgs()

  used_tags = _GetUsedTags()
  # Get the list of ignored tags from the GPU tests.
  ignored_tags = set(gpu_integration_test.GpuIntegrationTest.IgnoredTags())
  generated_tags = _GetGeneratedTags(args)
  known_tags = used_tags | ignored_tags
  unused_tags = generated_tags - known_tags

  if unused_tags:
    print('Tags that were generated but unused:')
    for t in unused_tags:
      print(t)
    print('')

  stale_tags = known_tags - generated_tags
  if stale_tags:
    print('Tags that are known but not generated:')
    for t in stale_tags:
      print(t)

  if not (unused_tags or stale_tags):
    print('Known and generated tags are in sync.')


if __name__ == '__main__':
  main()
