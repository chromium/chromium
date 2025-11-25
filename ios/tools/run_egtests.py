#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs selected EG tests."""

import argparse
import glob
import json
import os
import plistlib
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple
import shlex

import shared_test_utils
from shared_test_utils import Colors, Simulator, print_header, print_command

_EG2TESTS_MODULE_SUFFIX = '_eg2tests_module'
_TEST_CASE_SUFFIX = 'TestCase'
_NON_PREFERRED_MODULE_SUFFIXES = (
    f'_flaky{_EG2TESTS_MODULE_SUFFIX}',
    f'_multitasking{_EG2TESTS_MODULE_SUFFIX}',
)


def _camel_to_snake(name: str) -> str:
    """Converts a CamelCase string to snake_case."""
    s1 = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s1).lower()


def _find_test_file(test_case: str) -> Optional[str]:
    """Finds the test file for a given test case.

    Args:
        test_case: The name of the test case (e.g., 'TestCase').

    Returns:
        The path to the test file, or None if not found or ambiguous.
    """
    # Convert TestCase to test_case
    if test_case.endswith(_TEST_CASE_SUFFIX):
        test_case = test_case[:-len(_TEST_CASE_SUFFIX)]
    snake_case_name = _camel_to_snake(test_case)

    # Find the test file.
    search_pattern = f'ios/**/{snake_case_name}_egtest.mm'
    test_files = glob.glob(search_pattern, recursive=True)
    if not test_files:
        search_pattern = f'ios/**/{snake_case_name}_eg.mm'
        test_files = glob.glob(search_pattern, recursive=True)

    if not test_files:
        print(f"Could not find a test file for test case '{test_case}'.")
        return None
    if len(test_files) > 1:
        print(f"Found multiple test files for '{test_case}': {test_files}")
        print("Please specify the scheme manually using --scheme.")
        return None
    test_file = test_files[0]
    print(f"{Colors.BLUE}Found test file: {test_file}{Colors.RESET}")
    return test_file


def _find_gn_modules_for_file(test_file: str,
                              out_dir: str) -> Optional[List[str]]:
    """Finds all eg2tests_module GN targets that depend on a test file.

    Args:
        test_file: The path to the test file.
        out_dir: The output directory for the build.

    Returns:
        A list of GN target labels, or None on error.
    """
    try:
        # Find all test-only targets that depend on the test file.
        gn_refs_cmd = [
            'gn', 'refs', out_dir, f'//{test_file}', '--all', '--testonly=true'
        ]
        refs_output = subprocess.check_output(gn_refs_cmd,
                                              encoding='utf-8').strip()

        # Find all targets that end with _eg2tests_module.
        eg2tests_modules = []
        for line in refs_output.splitlines():
            if line.strip().endswith(_EG2TESTS_MODULE_SUFFIX):
                eg2tests_modules.append(line.strip())

        if not eg2tests_modules:
            print(f"Could not find a suitable test scheme for '{test_file}'.")
            return None

        return eg2tests_modules

    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Error running gn command: {e}")
        return None


def _select_best_target(eg2tests_modules: List[str]) -> str:
    """Selects the best target from a list of eg2tests_module targets.

    Args:
        eg2tests_modules: A list of eg2tests_module GN targets.

    Returns:
        The label of the best target.
    """
    preferred_modules = []
    for target in eg2tests_modules:
        is_non_preferred = False
        for suffix in _NON_PREFERRED_MODULE_SUFFIXES:
            if target.endswith(suffix):
                is_non_preferred = True
                break
        if not is_non_preferred:
            preferred_modules.append(target)

    if preferred_modules:
        return preferred_modules[0]
    # If no preferred modules are found, return the first available, even if
    # it's non-preferred.
    return eg2tests_modules[0]


def _find_scheme_for_tests(test: str, out_dir: str) -> Optional[str]:
    """Finds the scheme that contains the given test.

    Args:
        test: The test specifier (e.g., 'TestCase/testMethod').
        out_dir: The output directory for the build.

    Returns:
        The name of the scheme, or None if it could not be determined.
    """
    test_case = test.split('/')[0]
    test_file = _find_test_file(test_case)
    if not test_file:
        return None

    eg2tests_modules = _find_gn_modules_for_file(test_file, out_dir)
    if not eg2tests_modules:
        return None

    final_target = _select_best_target(eg2tests_modules)
    scheme = final_target.split(':')[-1]
    return scheme


def _build_tests(out_dir: str, scheme: str) -> bool:
    """Builds the EG test target.

    Args:
        out_dir: The output directory for the build.
        scheme: The EG test scheme to build.

    Returns:
        True if the build was successful, False otherwise.
    """
    build_command = ['autoninja', '-C', out_dir, scheme]
    print_header("--- Building Tests ---")
    print_command(build_command)
    try:
        subprocess.check_call(build_command)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Build failed with exit code {e.returncode}")
        return False


def _run_tests(out_dir: str, simulator_name: str, scheme: str,
               test_filters: List[str]) -> int:
    """Runs the EG tests on the specified simulator.

    Args:
        out_dir: The output directory for the build.
        simulator_name: The name of the simulator to use.
        scheme: The EG test scheme to run.
        test_filters: A list of test filters to apply.

    Returns:
        The exit code of the test runner.
    """
    project_path = os.path.join(os.getcwd(), 'out', 'build', 'all.xcodeproj')
    launch_command = [
        'xcodebuild',
        'test-without-building',
        '-project',
        project_path,
        '-scheme',
        scheme,
        '-destination',
        f'platform=iOS Simulator,name={simulator_name}',
    ]
    if test_filters:
        for test_filter in test_filters:
            launch_command.append(f'-only-testing:{scheme}/{test_filter}')

    print_header("--- Running Tests ---")
    print_command(launch_command)

    try:
        # The command is expected to return a non-zero exit code on test
        # failure.
        subprocess.check_call(launch_command)
        return 0
    except subprocess.CalledProcessError as e:
        return e.returncode


def _build_and_run_eg_tests(args: argparse.Namespace) -> int:
    """Runs the test logic based on the parsed arguments."""
    if not args.tests:
        print(f"{Colors.FAIL}Error: You must provide --tests.{Colors.RESET}")
        return 1

    tests_by_scheme = {}
    test_filters = [f.strip() for f in args.tests.split(',')]

    if args.scheme:
        # If a scheme is provided, all tests run against it.
        tests_by_scheme[args.scheme] = test_filters
    else:
        # Otherwise, determine the scheme for each test.
        print_header("--- Selecting Scheme ---")
        print(f"{Colors.CYAN}Scheme not provided. Inferring from test filter..."
              f"{Colors.RESET}")
        for test in test_filters:
            scheme = _find_scheme_for_tests(test, args.out_dir)
            if not scheme:
                print(f"\n{Colors.FAIL}Could not determine the scheme for "
                      f"test filter '{test}'. Please specify it with --scheme."
                      f"{Colors.RESET}")
                return 1
            if scheme not in tests_by_scheme:
                tests_by_scheme[scheme] = []
            tests_by_scheme[scheme].append(test)

    for scheme, tests in tests_by_scheme.items():
        print(f"{Colors.GREEN}Found scheme '{scheme}' for tests: "
              f"{', '.join(tests)}{Colors.RESET}")

    simulator = shared_test_utils.find_and_boot_simulator(args.device, args.os)
    if not simulator:
        return 1

    final_exit_code = 0
    for scheme, tests in tests_by_scheme.items():
        if not _build_tests(args.out_dir, scheme):
            final_exit_code = 1
            continue  # Try next scheme even if this one fails to build.

        exit_code = _run_tests(args.out_dir, simulator.name, scheme, tests)
        if exit_code != 0:
            final_exit_code = exit_code

    return final_exit_code


def main() -> int:
    """Main function for the script.

    Parses arguments, finds a simulator, builds, and runs the tests.

    Returns:
        The exit code of the test run.
    """
    print_header("=== Run EG Tests ===")
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--out-dir',
        default='out/Debug-iphonesimulator',
        help='The output directory to use for the build (default: %(default)s).'
    )
    parser.add_argument('--tests',
                        help='Comma-separated list of tests to run (e.g., '
                        "'TestCase1/testMethodA,TestCase2/testMethodB'"
                        '). '
                        'Does not support wildcards.')
    parser.add_argument(
        '--scheme',
        help='The EG test scheme to build and run. If not provided, it will be '
        'inferred from --tests.')
    parser.add_argument('--device', help='The device type to use for the test.')
    parser.add_argument('--os',
                        help='The OS version to use for the test (e.g., 17.5).')
    args = parser.parse_args()

    return _build_and_run_eg_tests(args)


if __name__ == '__main__':
    sys.exit(main())
