# Copyright 2021 The Chromium Authors
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
        cmdline_parser = argparse.ArgumentParser()
        main_program.add_cmdline_args(cmdline_parser)
        ... adding other cmdline parameter definitions ...
        parsed_cmdline_args = cmdline_parser.parse_args()

        test_executable_wrappers = []
        test_executable_wrappers.append(
            rust_main_program.TestExecutableWrapper(...))
        ...

        main_program.main(
            test_executable_wrappers, parsed_cmdline_args, os.environ)
"""

import time

import test_filtering
import test_results


def add_cmdline_args(argparse_parser):
    """Adds test-filtering-specific cmdline parameter definitions to
    `argparse_parser`.

    Args:
        argparse_parser: An object of argparse.ArgumentParser type.
    """
    test_filtering.add_cmdline_args(argparse_parser)
    test_results.add_cmdline_args(argparse_parser)

    argparse_parser.add_argument(
        '--isolated-script-test-launcher-retry-limit',
        dest='retry_limit',
        default=3,
        help='Sets the limit of test retries on failures to N.',
        metavar='N',
        type=int)
    argparse_parser.add_argument('--isolated-script-test-repeat',
                                 dest='repetitions',
                                 default=1,
                                 help='Repeats each test N times.',
                                 metavar='N',
                                 type=int)


def _calculate_tests_to_run(argparse_parsed_args, env,
                            test_executable_wrappers):
    tests = []
    for wrapper in test_executable_wrappers:
        extra_tests = wrapper.list_all_tests()
        for extra_test in extra_tests:
            assert extra_test not in tests
        tests.extend(extra_tests)
    return test_filtering.filter_tests(argparse_parsed_args, env, tests)


def _run_tests_and_save_results(argparse_parsed_args, list_of_tests_to_run,
                                test_executable_wrapper):
    start_time = time.time()
    results = []
    for wrapper in test_executable_wrapper:
        results.extend(wrapper.run_tests(list_of_tests_to_run))
    test_results.print_test_results(argparse_parsed_args, results, start_time)


def main(test_executable_wrappers, argparse_parsed_args, env):
    """Runs tests within `test_executable_wrappers` using cmdline arguments and
    environment variables to figure out 1) which subset of tests to run, 2)
    where to save the JSON file with test results.

    Args:
        test_executable_wrappers: A list of objects providing
          list_all_tests(...) and run_tests(...) methods (see
          rust_main_program._TestExecutableWrapper).
        argparse_parsed_arg: A result of an earlier call to
          argparse_parser.parse_args() call (where `argparse_parser` has been
          populated via an even earlier call to add_cmdline_args).
        env: a dictionary-like object (typically from `os.environ`).
    """
    list_of_test_names_to_run = _calculate_tests_to_run(
        argparse_parsed_args, env, test_executable_wrappers)
    _run_tests_and_save_results(argparse_parsed_args,
                                list_of_test_names_to_run,
                                test_executable_wrappers)
    # TODO(lukasza): Repeat tests `args.repetitions` times.
    # TODO(lukasza): Retry failing times up to `args.retry_limit` times.
