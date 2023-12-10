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
import urllib.parse

from blinkpy.common.host import Host
from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.w3c.monorail import MonorailAPI
from blinkpy.web_tests.fuzzy_diff_analyzer import analyzer
from blinkpy.web_tests.fuzzy_diff_analyzer import queries
from blinkpy.web_tests.fuzzy_diff_analyzer import results


DASHBOARD_BASE_URL = 'https://dashboards.corp.google.com/image_comparison'\
                     '_web_test_status_dashboard_history_data_per_test'
RESULT_TITLE = 'Fuzzy Diff Analyzer result:'


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
    parser.add_argument(
        '--attach-analysis-result',
        action='store_true',
        default=False,
        help='Attach the fuzzy diff analysis result to the corresponding bug.'
        ' Only used with --check-bugs-only flag.')
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
            bug_id = ''
            if bug['bug_id'] and '/' in bug['bug_id']:
                bug_id = bug['bug_id'].split('/')[1]
            bugs[bug_id] = test_path_list
        if args.attach_analysis_result:
            token = LuciAuth(Host()).get_access_token()
            monorail_api = MonorailAPI(access_token=token)
    else:
        bugs = {'': [args.test_path]}

    for bug_id, test_list in bugs.items():
        bug_result_string = ''
        for test_path in test_list:
            query_results = (querier_instance.
                             get_failed_image_comparison_ci_tests(test_path))
            aggregated_results = results_processor.aggregate_results(
                query_results)

            for test_name, test_data in aggregated_results.items():
                test_analysis_result = matching_analyzer.run_analyzer(
                    test_data)
                if test_analysis_result.is_analyzed:
                    result_string = ''
                    if bug_id and not args.attach_analysis_result:
                        result_string += '\nBug number: %s' % bug_id
                    result_string += '\nTest name: %s' % test_name
                    result_string += '\nTest Result: %s' % \
                                     test_analysis_result.analysis_result
                    result_string += '\nDashboard link: %s\n' % \
                                     (DASHBOARD_BASE_URL +
                                      '?f=test_name_cgk78f:re:' +
                                      urllib.parse.quote(test_name, safe=''))
                    if not args.attach_analysis_result:
                        print(result_string)
                    else:
                        bug_result_string += result_string
        # Attach the analysis result for this bug.
        if bug_id and args.attach_analysis_result and bug_result_string:
            bug_result_string = RESULT_TITLE + bug_result_string
            if RESULT_TITLE not in str(
                    monorail_api.get_comment_list('chromium', bug_id)):
                monorail_api.insert_comment('chromium', bug_id,
                                            bug_result_string)

    return 0
