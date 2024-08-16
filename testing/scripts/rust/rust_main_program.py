# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a library for wrapping Rust test executables in a way that is
compatible with the requirements of the `main_program` module.
"""

import argparse
import os
import re
import subprocess
import sys

import exe_util
import main_program
import test_results


def _format_test_name(test_executable_name, test_case_name):
    assert '//' not in test_executable_name
    assert '/' not in test_case_name
    test_case_name = '/'.join(test_case_name.split('::'))
    return '{}//{}'.format(test_executable_name, test_case_name)


def _parse_test_name(test_name):
    assert '//' in test_name
    assert '::' not in test_name
    test_executable_name, test_case_name = test_name.split('//', 1)
    test_case_name = '::'.join(test_case_name.split('/'))
    return test_executable_name, test_case_name


def _scrape_test_list(output, test_executable_name):
    """Scrapes stdout from running a Rust test executable with
    --list and --format=terse.

    Args:
        output: A string with the full stdout of a Rust test executable.
        test_executable_name: A string.  Used as a prefix in "full" test names
          in the returned results.

    Returns:
        A list of strings - a list of all test names.
    """
    TEST_SUFFIX = ': test'
    BENCHMARK_SUFFIX = ': benchmark'
    test_case_names = []
    for line in output.splitlines():
        if line.endswith(TEST_SUFFIX):
            test_case_names.append(line[:-len(TEST_SUFFIX)])
        elif line.endswith(BENCHMARK_SUFFIX):
            continue
        else:
            raise ValueError(
                'Unexpected format of a list of tests: {}'.format(output))
    test_names = [
        _format_test_name(test_executable_name, test_case_name)
        for test_case_name in test_case_names
    ]
    return test_names


def _scrape_test_results(output, test_executable_name,
                         list_of_expected_test_case_names):
    """Scrapes stdout from running a Rust test executable with
    --test --format=pretty.

    Args:
        output: A string with the full stdout of a Rust test executable.
        test_executable_name: A string.  Used as a prefix in "full" test names
          in the returned TestResult objects.
        list_of_expected_test_case_names: A list of strings - expected test case
          names (from the perspective of a single executable / with no prefix).
    Returns:
        A list of test_results.TestResult objects.
    """
    results = []
    regex = re.compile(r'^test ([:\w]+) \.\.\. (\w+)')
    for line in output.splitlines():
        match = regex.match(line.strip())
        if not match:
            continue

        test_case_name = match.group(1)
        if test_case_name not in list_of_expected_test_case_names:
            continue

        actual_test_result = match.group(2)
        if actual_test_result == 'ok':
            actual_test_result = 'PASS'
        elif actual_test_result == 'FAILED':
            actual_test_result = 'FAIL'
        elif actual_test_result == 'ignored':
            actual_test_result = 'SKIP'

        test_name = _format_test_name(test_executable_name, test_case_name)
        results.append(test_results.TestResult(test_name, actual_test_result))
    return results


def _get_exe_specific_tests(expected_test_executable_name, list_of_test_names):
    results = []
    for test_name in list_of_test_names:
        actual_test_executable_name, test_case_name = _parse_test_name(
            test_name)
        if actual_test_executable_name != expected_test_executable_name:
            continue
        results.append(test_case_name)
    return results


class _TestExecutableWrapper:
    def __init__(self, path_to_test_executable):
        if not os.path.isfile(path_to_test_executable):
            raise ValueError('No such file: ' + path_to_test_executable)
        self._path_to_test_executable = path_to_test_executable
        self._name_of_test_executable, _ = os.path.splitext(
            os.path.basename(path_to_test_executable))

    def list_all_tests(self):
        """Returns:
            A list of strings - a list of all test names.
        """
        args = [self._path_to_test_executable, '--list', '--format=terse']
        output = subprocess.check_output(args, text=True)
        return _scrape_test_list(output, self._name_of_test_executable)

    def run_tests(self, list_of_tests_to_run):
        """Runs tests listed in `list_of_tests_to_run`.  Ignores tests for other
        test executables.

        Args:
            list_of_tests_to_run: A list of strings (a list of test names).

        Returns:
            A list of test_results.TestResult objects.
        """
        list_of_tests_to_run = _get_exe_specific_tests(
            self._name_of_test_executable, list_of_tests_to_run)
        if not list_of_tests_to_run:
            return []

        # TODO(lukasza): Avoid passing all test names on the cmdline (might
        # require adding support to Rust test executables for reading cmdline
        # args from a file).
        # TODO(lukasza): Avoid scraping human-readable output (try using
        # JSON output once it stabilizes;  hopefully preserving human-readable
        # output to the terminal).
        args = [
            self._path_to_test_executable, '--test', '--format=pretty',
            '--color=always', '--exact'
        ]
        args.extend(list_of_tests_to_run)

        print('Running tests from {}...'.format(self._name_of_test_executable))
        output = exe_util.run_and_tee_output(args)
        print('Running tests from {}... DONE.'.format(
            self._name_of_test_executable))
        print()

        return _scrape_test_results(output, self._name_of_test_executable,
                                    list_of_tests_to_run)


def _parse_args(args):
    description = 'Wrapper for running Rust unit tests with support for ' \
                  'Chromium test filters, sharding, and test output.'
    parser = argparse.ArgumentParser(description=description)
    main_program.add_cmdline_args(parser)

    parser.add_argument('--rust-test-executable',
                        action='append',
                        dest='rust_test_executables',
                        default=[],
                        help=argparse.SUPPRESS,
                        metavar='FILEPATH',
                        required=True)

    return parser.parse_args(args=args)


if __name__ == '__main__':
    parsed_args = _parse_args(sys.argv[1:])
    rust_tests_wrappers = [
        _TestExecutableWrapper(path)
        for path in parsed_args.rust_test_executables
    ]
    main_program.main(rust_tests_wrappers, parsed_args, os.environ)
