# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to fuzzy diff analyzer for flaky image comparison web
tests.

Example usage, which finds all failures for image comparison web tests in the
past 3 days. Any tests that both failed and passed more than twice on a
configuration is considered as a flaky test. The script will provide a
recommended fuzzy fixable range for the test:

third_party/blink/tools/run_fuzzy_diff_analyzer.py \
  --project chrome-unexpected-pass-data \
  --sample-period 3
"""

import argparse

from blinkpy.web_tests.fuzzy_diff_analyzer import analyzer
from blinkpy.web_tests.fuzzy_diff_analyzer import queries
from blinkpy.web_tests.fuzzy_diff_analyzer import results


def ParseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=(
        'Script to fuzzy diff analyzer for flaky image comparison web tests'))
    parser.add_argument(
        '--project',
        required=True,
        help=('The billing project to use for BigQuery queries. '
              'Must have access to the ResultDB BQ tables, e.g. '
              '"luci-resultdb.chromium.web_tests_ci_test_results".'))
    parser.add_argument('--sample-period',
                        type=int,
                        default=1,
                        help='The number of days to sample data from.')
    parser.add_argument(
        '--test-path',
        help='The test path that contains the tests to do fuzzy diff analyzer.'
    )
    args = parser.parse_args()
    return args


def main() -> int:
    args = ParseArgs()

    querier_instance = queries.FuzzyDiffAnalyzerQuerier(
        args.sample_period, args.project)
    query_results = querier_instance.get_failed_image_comparison_ci_tests()

    results_processor = results.ResultProcessor()
    aggregated_results = results_processor.aggregate_results(query_results)

    matching_analyzer = analyzer.FuzzyMatchingAnalyzer()
    for test_name, test_data in aggregated_results.items():
        test_result = matching_analyzer.run_analyzer(test_data)
        print('')
        print('test_name: %s' % test_name)
        print('test_result: %s' % test_result)
        print('')

    return 0
