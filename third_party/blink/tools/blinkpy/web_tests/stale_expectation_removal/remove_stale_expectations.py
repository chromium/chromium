# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import sys

assert sys.version_info[0] == 3

from blinkpy.web_tests.stale_expectation_removal import builders
from blinkpy.web_tests.stale_expectation_removal import data_types
from blinkpy.web_tests.stale_expectation_removal import expectations
from blinkpy.web_tests.stale_expectation_removal import queries
from unexpected_passes_common import argument_parsing
from unexpected_passes_common import builders as common_builders
from unexpected_passes_common import data_types as common_data_types
from unexpected_passes_common import expectations as common_expectations
from unexpected_passes_common import result_output


def ParseArgs() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=(
        'Script for finding cases of stale expectations that can be '
        'removed/modified.'))
    argument_parsing.AddCommonArguments(parser)
    args = parser.parse_args()
    argument_parsing.PerformCommonPostParseSetup(args)
    return args


def main() -> int:
    args = ParseArgs()
    # Set any custom data types.
    common_data_types.SetExpectationImplementation(
        data_types.WebTestExpectation)
    common_data_types.SetResultImplementation(data_types.WebTestResult)
    common_data_types.SetBuildStatsImplementation(data_types.WebTestBuildStats)
    common_data_types.SetTestExpectationMapImplementation(
        data_types.WebTestTestExpectationMap)

    builders_instance = builders.WebTestBuilders(
        args.include_internal_builders)
    common_builders.RegisterInstance(builders_instance)
    expectations_instance = expectations.WebTestExpectations()
    common_expectations.RegisterInstance(expectations_instance)

    test_expectation_map = expectations_instance.CreateTestExpectationMap(
        expectations_instance.GetExpectationFilepaths(), None,
        datetime.timedelta(days=args.expectation_grace_period))
    ci_builders = builders_instance.GetCiBuilders()

    querier = queries.WebTestBigQueryQuerier(None, args.project,
                                             args.num_samples,
                                             args.keep_unmatched_results)
    # Unmatched results are mainly useful for script maintainers, as they don't
    # provide any additional information for the purposes of finding
    # unexpectedly passing tests or unused expectations.
    unmatched = querier.FillExpectationMapForBuilders(test_expectation_map,
                                                      ci_builders)
    try_builders = builders_instance.GetTryBuilders(ci_builders)
    unmatched.update(
        querier.FillExpectationMapForBuilders(test_expectation_map,
                                              try_builders))
    unused_expectations = test_expectation_map.FilterOutUnusedExpectations()
    stale, semi_stale, active = test_expectation_map.SplitByStaleness()
    if args.result_output_file:
        with open(args.result_output_file, 'w') as outfile:
            result_output.OutputResults(stale, semi_stale, active, unmatched,
                                        unused_expectations,
                                        args.output_format, outfile)
    else:
        result_output.OutputResults(stale, semi_stale, active, unmatched,
                                    unused_expectations, args.output_format)

    affected_urls = set()
    stale_message = ''
    if args.remove_stale_expectations:
        for expectation_file, expectation_map in stale.items():
            affected_urls |= expectations_instance.RemoveExpectationsFromFile(
                expectation_map.keys(), expectation_file,
                common_expectations.RemovalType.STALE)
            stale_message += (
                'Stale expectations removed from %s. Stale '
                'comments, etc. may still need to be removed.\n' %
                expectation_file)
        for expectation_file, unused_list in unused_expectations.items():
            affected_urls |= expectations_instance.RemoveExpectationsFromFile(
                unused_list, expectation_file,
                common_expectations.RemovalType.UNUSED)
            stale_message += (
                'Unused expectations removed from %s. Stale comments, etc. '
                'may still need to be removed.\n' % expectation_file)

    if args.narrow_semi_stale_expectation_scope:
        affected_urls |= expectations_instance.NarrowSemiStaleExpectationScope(
            semi_stale)
        stale_message += ('Semi-stale expectations narrowed in expectation '
                          'files. Stale comments, etc. may still need to be '
                          'removed.')

    if stale_message:
        print(stale_message)
    if affected_urls:
        orphaned_urls = expectations_instance.FindOrphanedBugs(affected_urls)
        if args.bug_output_file:
            with open(args.bug_output_file, 'w') as bug_outfile:
                result_output.OutputAffectedUrls(
                    affected_urls,
                    orphaned_urls,
                    bug_outfile,
                    auto_close_bugs=args.auto_close_bugs)
        else:
            result_output.OutputAffectedUrls(
                affected_urls,
                orphaned_urls,
                auto_close_bugs=args.auto_close_bugs)

    return 0


if __name__ == '__main__':
    sys.exit(main())
