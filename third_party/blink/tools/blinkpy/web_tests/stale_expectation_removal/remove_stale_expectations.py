# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys

assert sys.version_info[0] == 3

from blinkpy.web_tests.stale_expectation_removal import builders
from blinkpy.web_tests.stale_expectation_removal import data_types
from blinkpy.web_tests.stale_expectation_removal import expectations
from blinkpy.web_tests.stale_expectation_removal import queries
from unexpected_passes_common import builders as common_builders
from unexpected_passes_common import data_types as common_data_types
from unexpected_passes_common import result_output


def ParseArgs():
    parser = argparse.ArgumentParser(description=(
        'Script for finding cases of stale expectations that can be '
        'removed/modified.'))
    parser.add_argument('--large-query-mode',
                        action='store_true',
                        default=False,
                        help=('Run the script in large query mode. This '
                              'incurs a significant performance hit, but '
                              'allows the use of larger sample sizes by '
                              'working around a hard memory limit in '
                              'BigQuery.'))
    parser.add_argument('--project',
                        default='chrome-unexpected-pass-data',
                        help=('The billing project to use for BigQuery '
                              'queries. Must have access to the ResultDB BQ '
                              'tables, e.g. "chrome-luci-data.chromium.'
                              'blink_web_testes_ci_results".'))
    parser.add_argument('--num-samples',
                        type=int,
                        default=100,
                        help='The number of recent builds to query.')
    parser.add_argument('--output-format',
                        choices=['html', 'print'],
                        default='html',
                        help='How to output script results.')
    parser.add_argument('--remove-stale-expectations',
                        action='store_true',
                        default=False,
                        help=('Automatically remove any expectations that are '
                              'determined to be stale from the expectation '
                              'files.'))
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help=('Increase logging verbosity, can be passed '
                              'multiple times.'))
    parser.add_argument('-q',
                        '--quiet',
                        action='store_true',
                        default=False,
                        help='Disable logging for non-errors.')

    args = parser.parse_args()
    if args.quiet:
        args.verbose = -1
    SetLoggingVerbosity(args.verbose)

    return args


def SetLoggingVerbosity(verbosity_level):
    if verbosity_level == -1:
        level = logging.ERROR
    elif verbosity_level == 0:
        level = logging.WARNING
    elif verbosity_level == 1:
        level = logging.INFO
    else:
        level = logging.DEBUG
    logging.getLogger().setLevel(level)


def main():
    args = ParseArgs()
    raise RuntimeError(
        'Script is still under active development and not currently functional'
    )
    # Set any custom data types.
    common_data_types.SetExpectationImplementation(
        data_types.WebTestExpectation)

    builders_instance = builders.WebTestBuilders()
    common_builders.RegisterInstance(builders_instance)
    expectations_instance = expectations.WebTestExpectations()

    test_expectation_map = expectations_instance.CreateTestExpectationMap(
        expectations_instance.GetExpectationFilepaths(), None)
    ci_builders = builders_instance.GetCiBuilders(None)

    querier = queries.WebTestBigQueryQuerier(None, args.project,
                                             args.num_samples,
                                             args.large_query_mode)
    # Unmatched results are mainly useful for script maintainers, as they don't
    # provide any additional information for the purposes of finding
    # unexpectedly passing tests or unused expectations.
    unmatched = querier.FillExpectationMapForCiBuilders(
        test_expectation_map, ci_builders)
    try_builders = builders_instance.GetTryBuilders(ci_builders)
    unmatched.update(
        querier.FillExpectationMapForTryBuilders(test_expectation_map,
                                                 try_builders))
    unused_expectations = test_expectation_map.FilterOutUnusedExpectations()
    stale, semi_stale, active = test_expectation_map.SplitByStaleness()
    result_output.OutputResults(stale, semi_stale, active, unmatched,
                                unused_expectations, args.output_format)
    return 0


if __name__ == '__main__':
    sys.exit(main())
