#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for finding and suppressing flaky GPU tests.

This relies on ResultDB BigQuery data under the hood, so it requires the `bq`
tool which is available as part of the Google Cloud SDK
https://cloud.google.com/sdk/docs/quickstarts.

Example usage, which finds all failures in the past 5 days. Any tests that
failed more than twice on a configuration is marked as flaky, and any that
failed more than 5 times is marked as failing:

suppress_flakes.py \
  --project chrome-unexpected-pass-data \
  --sample-period 5
"""

import argparse

from flake_suppressor import expectations
from flake_suppressor import queries
from flake_suppressor import result_output
from flake_suppressor import results as results_module


def ParseArgs():
  # TODO(crbug.com/1192733): Add flaky and failure thresholds, likely in the
  # form of % of failures out of the total runs for a (test, tags) combination.
  # <1% can be ignored, > 50% can be treated as a failure instead of a flake.
  parser = argparse.ArgumentParser(
      description=('Script for automatically suppressing flaky/failing GPU '
                   'Telemetry-based tests.'))
  parser.add_argument('--project',
                      required=True,
                      help=('The billing project to use for BigQuery queries. '
                            'Must have access to the ResultDB BQ tables, e.g. '
                            '"luci-resultdb.chromium.gpu_ci_test_results".'))
  parser.add_argument('--sample-period',
                      type=int,
                      default=1,
                      help=('The number of days to sample data from.'))
  parser.add_argument('--no-group-by-tags',
                      action='store_false',
                      default=True,
                      dest='group_by_tags',
                      help=('Append added expectations to the end of the file '
                            'instead of attempting to automatically group with '
                            'similar expectations.'))
  args = parser.parse_args()

  return args


def main():
  args = ParseArgs()
  results = queries.GetFlakyOrFailingTests(args.sample_period, args.project)
  aggregated_results = results_module.AggregateResults(results)
  result_output.GenerateHtmlOutputFile(aggregated_results)
  print('If there are many instances of failed tests, that may be indicative '
        'of an issue that should be handled in some other way, e.g. reverting '
        'a bad CL.')
  input('\nBeginning of user input section - press any key to continue')
  expectations.IterateThroughResultsForUser(aggregated_results,
                                            args.group_by_tags)
  print('\nGenerated expectations likely contain conflicting tags that need to '
        'be removed.')


if __name__ == '__main__':
  main()
