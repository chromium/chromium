# Copyright 2023 The Chromium Authors
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
import re

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
    parser.add_argument(
        '--image-diff-num-threshold',
        default=3,
        action="store",
        help=
        "Threshold for the number of image diff data, must have this number "
        "to analyze the fuzzy diff range.")
    parser.add_argument(
        '--distinct-diff-num-threshold',
        default=3,
        action="store",
        help="Threshold for the number of distinct image diff data, must this"
        "number to furtuher provide prcentile data.")
    parser.add_argument('--sample-period',
                        type=int,
                        default=1,
                        help='The number of days to sample data from.')
    parser.add_argument(
        '--test-path',
        help='The test path that contains the tests to do fuzzy diff analyzer.'
    )
    parser.add_argument(
        '--check-bugs-only',
        action='store_true',
        default=False,
        help='Only checks the image diff tests result on existing bugs in the'
        ' LUCI analysis database.')
    args = parser.parse_args()
    return args


def main() -> int:
    args = ParseArgs()

    querier_instance = queries.FuzzyDiffAnalyzerQuerier(
        args.sample_period, args.project)
    results_processor = results.ResultProcessor()
    matching_analyzer = analyzer.FuzzyMatchingAnalyzer(
        args.image_diff_num_threshold, args.distinct_diff_num_threshold)

    # Find all bug ids or save empty id if it does not check all bugs.
    if args.check_bugs_only:
        bugs_info = querier_instance.get_web_test_flaky_bugs()
        bugs = {}
        for bug in bugs_info:
            test_path_list = [
                re.sub('ninja://:blink_w(eb|pt)_tests/', '', test_id)
                for test_id in bug['test_ids']
            ]
            bugs[bug['bug_id']] = test_path_list
    else:
        bugs = {'': [args.test_path]}

    for bug_id, test_list in bugs.items():
        for test_path in test_list:
            query_results = (querier_instance.
                             get_failed_image_comparison_ci_tests(test_path))
            aggregated_results = results_processor.aggregate_results(
                query_results)

            for test_name, test_data in aggregated_results.items():
                test_analysis_result = matching_analyzer.run_analyzer(
                    test_data)
                if test_analysis_result.is_analyzed:
                    print('')
                    if bug_id:
                        print('bug number: %s' % bug_id)
                    print('test_name: %s' % test_name)
                    print('test_result: %s' %
                          test_analysis_result.analysis_result)
                    print('')

    return 0
