# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for finding and suppressing flaky Web tests.

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
from flake_suppressor_common import argument_parsing
from flake_suppressor_common import result_output
from flake_suppressor_common import tag_utils as common_tag_utils

from blinkpy.web_tests.flake_suppressor import web_tests_tag_utils as tag_utils
from blinkpy.web_tests.flake_suppressor import web_tests_queries
from blinkpy.web_tests.flake_suppressor import web_tests_results as results_module
from blinkpy.web_tests.flake_suppressor import web_tests_expectations


def main() -> int:
    args = argument_parsing.ParseArgs()
    common_tag_utils.SetTagUtilsImplementation(tag_utils.WebTestsTagUtils)
    expectations_processor = (
        web_tests_expectations.WebTestsExpectationProcessor())

    if not args.bypass_up_to_date_check:
        expectations_processor.AssertCheckoutIsUpToDate()

    results_processor = results_module.WebTestsResultProcessor(
        expectations_processor)
    querier_instance = web_tests_queries.WebTestsBigQueryQuerier(
        args.sample_period, args.project, results_processor)

    if args.non_hidden_failures_only:
        if len(args.builder_names) > 0:
            results = querier_instance.\
                GetFailingBuildCulpritFromCiBuilders(args.builder_names)
        else:
            results = querier_instance.GetFailingCiBuildCulpritTests()

        aggregated_results = results_processor.AggregateTestStatusResults(
            results)
    else:
        if len(args.builder_names) > 0:
            results = querier_instance.\
                GetFlakyOrFailingTestsFromCiBuilders(args.builder_names)
        else:
            results = querier_instance.GetFlakyOrFailingCiTests()
            results.extend(querier_instance.GetFlakyOrFailingTryTests())

        aggregated_results = results_processor.AggregateResults(results)

    if args.result_output_file:
        with open(args.result_output_file, 'w') as outfile:
            result_output.GenerateHtmlOutputFile(aggregated_results, outfile)
    else:
        result_output.GenerateHtmlOutputFile(aggregated_results)
    print(
        'If there are many instances of failed tests, that may be indicative '
        'of an issue that should be handled in some other way, e.g. reverting '
        'a bad CL.')

    if args.prompt_for_user_input:
        input('\nBeginning of user input section - press any key to continue')
        expectations_processor.IterateThroughResultsForUser(
            aggregated_results, args.group_by_tags, args.include_all_tags)
    else:
        if args.non_hidden_failures_only:
            expectations_processor.CreateExpectationsForAllResults(
                aggregated_results, args.group_by_tags, args.include_all_tags,
                args.build_fail_total_number_threshold,
                args.build_fail_consecutive_days_threshold,
                args.build_fail_recent_days_threshold)
        else:
            if len(args.builder_names) > 0:
                result_counts = querier_instance.\
                    GetResultCountFromCiBuilders(args.builder_names)
            else:
                result_counts = querier_instance.GetResultCounts()
            expectations_processor.IterateThroughResultsWithThresholds(
                aggregated_results, args.group_by_tags, result_counts,
                args.ignore_threshold, args.flaky_threshold,
                args.include_all_tags)

    print('\nGenerated expectations will need to have bugs manually added.')

    return 0
