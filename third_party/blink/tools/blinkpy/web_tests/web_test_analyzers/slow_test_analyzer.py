# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to analysis the test slowness data.

Example usage, which finds slowness data of web tests in the
past 3 days. The script will provide a statistics of the slowness
and recommended an action for each slow test:

third_party/blink/tools/run_slow_test_analyzer.py \
  --sample-period 3

Use 'gcloud auth login' command first for local usage.
Use the --test-path flag to specify the tests you want to perform a slow test
analysis on, instead of all tests.

Example output:
Test name: test1
Test is slow in the below list of builders:
builder1 : timeout count: 7, slow count: 35, slow ratio: 1.00, avg duration: 5.30
builder2 : timeout count: 5, slow count: 32, slow ratio: 1.00, avg duration: 5.24
"""

import argparse
import logging
import re
import urllib.parse

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.buganizer import BuganizerClient
from blinkpy.web_tests.web_test_analyzers import analyzer
from blinkpy.web_tests.web_test_analyzers import data_types
from blinkpy.web_tests.web_test_analyzers import queries
from blinkpy.web_tests.web_test_analyzers import results


DASHBOARD_BASE_URL = 'go/slow_test_dashboard'
RESULT_TITLE = 'Slow Test Analyzer result:'

_log = logging.getLogger(__name__)


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
        type=int,
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
    parser.add_argument(
        '--check-bugs-only',
        action='store_true',
        help='Only checks the slow tests result on existing bugs in the'
        ' LUCI analysis database.')
    parser.add_argument(
        '--attach-analysis-result',
        action='store_true',
        help='Attach the slow test analysis result to the corresponding bug.'
        ' Only used with --check-bugs-only flag.')
    args = parser.parse_args()
    return args


def main() -> int:
    configure_logging(logging_level=logging.INFO, include_time=True)
    args = ParseArgs()

    querier_instance = queries.Querier(args.sample_period, args.project)
    # Find all bug ids or save empty id if it does not check all bugs.
    if args.check_bugs_only:
        bugs_info = querier_instance.get_web_test_flaky_bugs()
        bugs = {}
        for bug in bugs_info:
            test_path_list = [
                re.sub('ninja://:blink_w(eb|pt)_tests/', '', test_id)
                for test_id in bug['test_ids']
            ]
            if bug['bug_id']:
                bugs[bug['bug_id']] = test_path_list
                _log.info('Adding bug to check: %s', bug['bug_id'])
        if args.attach_analysis_result:
            buganizer_api = BuganizerClient()
        _log.info('total bugs: %d', len(bugs))
    else:
        bugs = {'': [args.test_path]}

    results_processor = results.ResultProcessor()
    slowness_analyzer = analyzer.SlowTestAnalyzer(
        args.slow_result_ratio_threshold, args.timeout_result_threshold)
    bug_ids = []
    for bug_id, test_list in bugs.items():
        bug_result_string = ''
        for test_path in test_list:
            query_results = (
                querier_instance.get_overall_slowness_ci_tests(test_path))
            aggregated_results = (
                results_processor.aggregate_test_slowness_results(
                    query_results))
            bug_result_string += analyze_aggregated_results(
                aggregated_results, slowness_analyzer, bug_id,
                args.attach_analysis_result)
        # Attach the analysis result for this bug.
        if bug_id and args.attach_analysis_result and bug_result_string:
            bug_result_string = RESULT_TITLE + bug_result_string
            if RESULT_TITLE not in str(
                    buganizer_api.GetIssueComments(int(bug_id))):
                buganizer_api.NewComment(int(bug_id), bug_result_string)
                bug_ids.append(bug_id)
                _log.info('Successfully attach result to bug: %s', bug_id)

    # Insert bug attachment results to database.
    if bug_ids:
        querier_instance.insert_web_test_analyzer_result(
            data_types.SLOW_TEST_ANALYZER, data_types.BUGANIZER, bug_ids)

    return 0


def analyze_aggregated_results(
        aggregated_results: data_types.AggregatedSlownessResultsType,
        slowness_analyzer: analyzer.SlowTestAnalyzer, bug_id: str,
        attach_analysis_result: bool) -> str:
    """Analyze the input slow test results.

        Args:
          aggregated_results: Slow test results.
          slowness_analyzer: The analyzer to check slowness of results.
          bug_id: The bug id of test results.
          attach_analysis_result: Attach the result to bug or not.

        Returns:
          A string of the final analysis result.
        """
    res = ''
    for test_name, test_data in aggregated_results.items():
        test_analysis_result = slowness_analyzer.run_analyzer(test_data)
        if test_analysis_result.is_analyzed:
            result_string = ''
            if bug_id and not attach_analysis_result:
                result_string = result_string + f'\nBug number: {bug_id}'
            dashboard_link = (DASHBOARD_BASE_URL + '?f=test_name_h4s6v6:re:' +
                              urllib.parse.quote(test_name, safe=''))
            result_string = result_string + (
                f'\nTest name: {test_name}'
                f'\nTest Result: {test_analysis_result.analysis_result}'
                f'\nTest is slow, suggested to make the test smaller or add'
                f' this test to SlowTestExpectation.'
                f'\nDashboard link: {dashboard_link}\n')
            if not attach_analysis_result:
                print(result_string)
            else:
                res = res + result_string
    return res
