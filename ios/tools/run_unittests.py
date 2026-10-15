#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs selected unit tests."""

from __future__ import annotations

import argparse
import os
import plistlib
import signal
import subprocess
import sys

import shared_test_utils
from shared_test_utils import Colors, print_command, print_header

_GTEST_PASSED_MARKER = '[  PASSED  ]'
_GTEST_FAILED_MARKER = '[  FAILED  ]'


def _build_tests(out_dir: str, test_target: str) -> bool:
    """Builds the unit test target.

    Args:
      out_dir: The output directory for the build.
      test_target: The target to build.

    Returns:
      True if the build was successful, False otherwise.
    """
    build_command = ['autoninja', '-C', out_dir, test_target]
    print_header("--- Building Tests ---")
    print_command(build_command)
    try:
        subprocess.check_call(build_command)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Build failed with exit code {e.returncode}")
        return False


def _run_tests(
    out_dir: str,
    simulator_udid: str,
    gtest_filter: str | None,
    test_target: str,
    gtest_repeat: str | None = None,
) -> int:
    """Installs and runs the tests on the specified simulator.

    Args:
      out_dir: The output directory for the build.
      simulator_udid: The UDID of the simulator to use.
      gtest_filter: The gtest filter to apply.
      test_target: The target to run.
      gtest_repeat: The gtest repeat count or pattern to apply.

    Returns:
      The exit code of the test runner (0 for success, non-zero for failure).
    """
    app_path = os.path.join(out_dir, f'{test_target}.app')
    info_plist_path = os.path.join(app_path, 'Info.plist')
    with open(info_plist_path, 'rb') as f:
        info_plist = plistlib.load(f)
    bundle_id = info_plist['CFBundleIdentifier']

    shared_test_utils.install_app(simulator_udid, app_path)

    launch_args: list[str] = []
    if gtest_filter:
        launch_args.append('--gtest_filter=' + gtest_filter)
    if gtest_repeat:
        launch_args.append('--gtest_repeat=' + gtest_repeat)
    print_header("--- Running Tests ---")

    test_failed = False
    test_completed = False

    def _on_line(line: str) -> None:
        nonlocal test_failed, test_completed
        if _GTEST_PASSED_MARKER in line or _GTEST_FAILED_MARKER in line:
            test_completed = True
        if _GTEST_FAILED_MARKER in line:
            test_failed = True

    exit_code = shared_test_utils.launch_app_with_console(
        simulator_udid=simulator_udid,
        bundle_id=bundle_id,
        app_args=launch_args,
        line_callback=_on_line,
    )

    if exit_code in (shared_test_utils.SIGINT_EXIT_CODE, -signal.SIGINT):
        return shared_test_utils.SIGINT_EXIT_CODE

    # If the test completed and there were failures, return 1.
    if test_completed and test_failed:
        return 1

    # If the test did not complete (i.e. crashed), return 1.
    if not test_completed:
        print(
            f'\n{Colors.FAIL}{Colors.BOLD}'
            'ERROR: Test runner did not complete. Assuming crash.'
            f'{Colors.RESET}'
        )
        return 1

    if exit_code != 0:
        return exit_code

    # If the test completed and there were no failures, return 0.
    return 0


def main() -> int:
    """Main function for the script.

    Parses arguments, finds a simulator, builds, and runs the tests.

    Returns:
      The exit code of the test run.
    """
    print_header("=== Run Unit Tests ===")
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--out-dir',
        default='out/Debug-iphonesimulator',
        help='The output directory to use for the build '
        '(default: %(default)s).',
    )
    parser.add_argument(
        '--gtest_filter', help='The gtest_filter to use for running the tests.'
    )
    parser.add_argument(
        '--gtest_repeat', help='The gtest_repeat to use for running the tests.'
    )
    parser.add_argument(
        '--device',
        help='The device type or UDID to use for the test '
        '(can also be set via IOS_SIMULATOR_DEFAULT_DEVICE).',
    )
    parser.add_argument(
        '--os',
        help='The OS version to use for the test (e.g., 17.5) '
        '(can also be set via IOS_SIMULATOR_DEFAULT_OS).',
    )
    parser.add_argument(
        '--test-target',
        default='ios_chrome_unittests',
        help='The test target to run (default: %(default)s).',
    )
    parser.add_argument(
        '--no-build',
        action='store_true',
        help='Skip compiling/building the tests and only run them.',
    )
    args = parser.parse_args()

    simulator = shared_test_utils.find_and_boot_simulator(args.device, args.os)
    if not simulator:
        return 1

    if not args.no_build:
        if not _build_tests(args.out_dir, args.test_target):
            return 1

    return _run_tests(
        args.out_dir,
        simulator.udid,
        args.gtest_filter,
        args.test_target,
        args.gtest_repeat,
    )


if __name__ == '__main__':
    sys.exit(main())
