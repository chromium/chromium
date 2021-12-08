# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a library for working with test executables in a way that is
Chromium-bot-friendly as specified by //docs/testing/test_executable_api.md

Example usage:
    import os
    import sys

    import main_program
    import rust_main_program

    if __name__ == '__main__':
        test_executable_wrapper = rust_main_program.TestExecutableWrapper(...)
        main_program.main(
            test_executable_wrapper, sys.argv[1:], os.environ)
"""

import argparse
import time

import test_filtering
import test_results


def _parse_cmdline_args(list_of_cmdline_args):
    description = 'Wrapper for running unit tests with support for ' \
                  'Chromium test filters, sharding, and test output.'
    parser = argparse.ArgumentParser(description=description)

    test_filtering.add_cmdline_args(parser)
    test_results.add_cmdline_args(parser)

    parser.add_argument(
        '--isolated-script-test-launcher-retry-limit',
        dest='retry_limit',
        default=3,
        help='Sets the limit of test retries on failures to N.',
        metavar='N',
        type=int)
    parser.add_argument('--isolated-script-test-repeat',
                        dest='repetitions',
                        default=1,
                        help='Repeats each test N times.',
                        metavar='N',
                        type=int)

    return parser.parse_args(args=list_of_cmdline_args)


def _calculate_tests_to_run(argparse_parsed_args, env,
                            test_executable_wrapper):
    tests = test_executable_wrapper.list_all_tests()
    tests = test_filtering.filter_tests(argparse_parsed_args, env, tests)
    return tests


def _run_tests_and_save_results(argparse_parsed_args, list_of_tests_to_run,
                                test_executable_wrapper):
    start_time = time.time()
    results = test_executable_wrapper.run_tests(list_of_tests_to_run)
    test_results.print_test_results(argparse_parsed_args, results, start_time)


def main(test_executable_wrapper, list_of_cmdline_args, env):
    """Runs tests within `test_executable_wrapper` using cmdline arguments and
    environment variables to figure out 1) which subset of tests to run, 2)
    where to save the JSON file with test results.

    Args:
        test_executable_wrapper: An object providing list_all_tests(...) and
          run_tests(...) methods (see rust_main_program._TestExecutableWrapper).
        list_of_cmdline_args: a list of strings (typically from `sys.argv`).
        env: a dictionary-like object (typically from `os.environ`).
    """
    args = _parse_cmdline_args(list_of_cmdline_args)
    list_of_test_names_to_run = _calculate_tests_to_run(
        args, env, test_executable_wrapper)
    _run_tests_and_save_results(args, list_of_test_names_to_run,
                                test_executable_wrapper)
    # TODO(lukasza): Repeat tests `args.repetitions` times.
    # TODO(lukasza): Retry failing times up to `args.retry_limit` times.
