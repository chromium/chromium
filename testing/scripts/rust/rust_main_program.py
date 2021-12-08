# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a library for wrapping Rust test executables in a way that is
compatible with the requirements of the `main_program` module.
"""

import os
import re
import subprocess
import sys

import test_results
import main_program
import exe_util


def _scrape_test_list(output):
    """Scrapes stdout from running a Rust test executable with
    --list and --format=terse.

    Returns:
        A list of strings - a list of all test names.
    """
    TEST_SUFFIX = ': test'
    tests = [
        line[:-len(TEST_SUFFIX)] for line in output.splitlines()
        if line.endswith(TEST_SUFFIX)
    ]
    return tests


def _scrape_test_results(output, list_of_expected_test_names):
    """Scrapes stdout from running a Rust test executable with
    --test --format=pretty.

    Returns:
        A list of test_results.TestResult objects.
    """
    results = []
    regex = re.compile(r'^test (\w+) \.\.\. (\w+)')
    for line in output.splitlines():
        match = regex.match(line.strip())
        if not match:
            continue

        test_name = match.group(1)
        if test_name not in list_of_expected_test_names:
            continue

        actual_test_result = match.group(2)
        if actual_test_result == 'ok':
            actual_test_result = 'PASS'

        results.append(test_results.TestResult(test_name, actual_test_result))
    return results


class _TestExecutableWrapper:
    def __init__(self, path_to_test_executable):
        if not os.path.isfile(path_to_test_executable):
            raise ValueError('No such file: ' + path_to_test_executable)
        self._path_to_test_executable = path_to_test_executable

    def list_all_tests(self):
        """Returns:
            A list of strings - a list of all test names.
        """
        args = [self._path_to_test_executable, '--list', '--format=terse']
        output = subprocess.check_output(args, text=True)
        return _scrape_test_list(output)

    def run_tests(self, list_of_tests_to_run):
        """Runs tests listed in `list_of_tests_to_run`.

        Args:
            list_of_tests_to_run: A list of strings (a list of test names).

        Returns:
            A list of test_results.TestResult objects.
        """
        # TODO(lukasza): Avoid passing all test names on the cmdline (might
        # require adding support to Rust test executables for reading cmdline
        # args from a file).
        # TODO(lukasza): Avoid scraping human-readable output (try using
        # JSON output once it stabilizes;  hopefully preserving human-readable
        # output to the terminal).
        args = [
            self._path_to_test_executable, '--test', '--format=pretty',
            '--exact'
        ]
        args.extend(list_of_tests_to_run)
        output = exe_util.run_and_tee_output(args)
        return _scrape_test_results(output, list_of_tests_to_run)


def main(test_executable_path):
    """Runs Rust tests from the given executable file.  Uses cmdline arguments
    and environment variables to figure out 1) which subset of tests to run, 2)
    where to save the JSON file with test results.

    Args:
        test_executable_path: A string with the filepath to the test executable.
    """
    main_program.main(_TestExecutableWrapper(test_executable_path),
                      sys.argv[1:], os.environ)
