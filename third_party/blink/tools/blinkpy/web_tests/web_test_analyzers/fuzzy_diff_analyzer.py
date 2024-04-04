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
  --sample-period 3

Use 'gcloud auth login' command first for local usage.
Use the --test-path flag to specify the tests you want to perform a fuzzy diff
analysis, instead of all tests.
"""

import argparse
import re
import logging
import urllib.parse

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.buganizer import BuganizerClient
from blinkpy.web_tests.web_test_analyzers import analyzer
from blinkpy.web_tests.web_test_analyzers import data_types
from blinkpy.web_tests.web_test_analyzers import queries
from blinkpy.web_tests.web_test_analyzers import results


DASHBOARD_BASE_URL = 'go/fuzzy_diff_dashboard'
RESULT_TITLE = 'Fuzzy Diff Analyzer result:'

_log = logging.getLogger(__name__)


def ParseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=(
        'Script to fuzzy diff analyzer for flaky image comparison web tests'))
    parser.add_argument(
        '--project',
        default='chrome-unexpected-pass-data',
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
    matching_analyzer = analyzer.FuzzyMatchingAnalyzer(
        args.image_diff_num_threshold, args.distinct_diff_num_threshold)
    bug_ids = []
    for bug_id, test_list in bugs.items():
        bug_result_string = ''
        for test_path in test_list:
            query_results = (querier_instance.
                             get_failed_image_comparison_ci_tests(test_path))
            aggregated_results = results_processor.aggregate_results(
                query_results)
            bug_result_string += analyze_aggregated_results(
                aggregated_results, matching_analyzer, bug_id,
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
            data_types.FUZZY_DIFF_ANALYZER, data_types.BUGANIZER, bug_ids)

    return 0


def analyze_aggregated_results(
        aggregated_results: data_types.AggregatedSlownessResultsType,
        matching_analyzer: analyzer.FuzzyMatchingAnalyzer, bug_id: str,
        attach_analysis_result: bool) -> str:
    """Analyze the input image test results.

    Args:
      aggregated_results: Image test results.
      matching_analyzer: The analyzer to check fuzzy diff range.
      bug_id: The bug id of test results.
      attach_analysis_result: Attach the result to bug or not.

    Returns:
      A string of the final analysis result.
    """
    res = ''
    for test_name, test_data in aggregated_results.items():
        test_analysis_result = matching_analyzer.run_analyzer(test_data)
        if test_analysis_result.is_analyzed:
            result_string = ''
            if bug_id and not attach_analysis_result:
                result_string = result_string + f'\nBug number: {bug_id}'
            dashboard_link = (DASHBOARD_BASE_URL + '?f=test_name_cgk78f:re:' +
                              urllib.parse.quote(test_name, safe=''))
            result_string = result_string + (
                f'\nTest name: {test_name}'
                f'\nTest Result: {test_analysis_result.analysis_result}'
                f'\nDashboard link: {dashboard_link}\n')
            if not attach_analysis_result:
                print(result_string)
            else:
                res = res + result_string
    return res
