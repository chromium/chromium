# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to analysis the test slowness data.

Example usage, which finds slowness data of web tests in the
past 3 days. The script will provide a statistics of the slowness
and recommended an action for each slow test:

third_party/blink/tools/run_slow_test_analyzer.py \
  --sample-period 3

Example output:
Test name: test1
Test is slow in the below list of builders:
builder1 : timeout count: 7, slow count: 35, slow ratio: 1.00, avg duration: 5.30
builder2 : timeout count: 5, slow count: 32, slow ratio: 1.00, avg duration: 5.24
"""

import argparse

from blinkpy.web_tests.fuzzy_diff_analyzer import analyzer
from blinkpy.web_tests.fuzzy_diff_analyzer import queries
from blinkpy.web_tests.fuzzy_diff_analyzer import results


def ParseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=('Script to check and analysis slowness for web tests'))
    parser.add_argument(
        '--project',
        default='chrome-unexpected-pass-data',
        help=('The billing project to use for BigQuery queries. '
              'Must have access to the ResultDB BQ tables, e.g. '
              '"luci-resultdb.chromium.web_tests_ci_test_results".'))
    parser.add_argument(
        '--slow-result-ratio-threshold',
        default=0.9,
        type=float,
        help="A float denoting what fraction of results need to be slow"
        " for a test to be considered slow. Both thresholds must be"
        " hit for a test to be considered slow.")
    parser.add_argument(
        '--timeout-result-threshold',
        default=5,
        help="An int denoting the number of timeout results necessary"
        " for a test to be considered slow. Both thresholds must "
        "be hit for a test to be considered slow.")
    parser.add_argument('--sample-period',
                        type=int,
                        default=1,
                        choices=range(1, 30),
                        help='The number of days to sample data from, '
                        'must be within 30 days.')
    parser.add_argument(
        '--test-path',
        help='The test path that contains the tests to do slowness analysis.')
    args = parser.parse_args()
    return args


def main() -> int:
    args = ParseArgs()

    querier_instance = queries.Querier(args.sample_period, args.project)
    query_results = (querier_instance.get_overall_slowness_ci_tests(
        args.test_path))
    results_processor = results.ResultProcessor()
    aggregated_results = results_processor.aggregate_test_slowness_results(
        query_results)

    slowness_analyzer = analyzer.SlowTestAnalyzer(
        args.slow_result_ratio_threshold, args.timeout_result_threshold)
    for test_name, test_data in aggregated_results.items():
        test_analysis_result = slowness_analyzer.run_analyzer(test_data)
        if test_analysis_result.is_analyzed:
            result_string = ''
            result_string += '\nTest name: %s\n' % test_name
            result_string += test_analysis_result.analysis_result
            print(result_string)

    return 0
