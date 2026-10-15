#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs the app in the simulator."""

from __future__ import annotations

import argparse
import os
import plistlib
import subprocess
import sys

import shared_test_utils
from shared_test_utils import Colors, print_command, print_header


def _build_app(out_dir: str, target: str) -> bool:
    """Builds the app target.

    Args:
        out_dir: The output directory for the build.
        target: The target to build.

    Returns:
        True if the build was successful, False otherwise.
    """
    build_command = ['autoninja', '-C', out_dir, target]
    print_header("--- Building App ---")
    print_command(build_command)
    try:
        subprocess.check_call(build_command)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Build failed with exit code {e.returncode}")
        return False


def _run_app(
    out_dir: str,
    simulator_udid: str,
    target: str,
    app_name: str | None,
    enable_features: str | None = None,
    disable_features: str | None = None,
    app_args: list[str] | None = None,
    wipe: bool = False,
) -> int:
    """Installs and runs the app on the specified simulator.

    Args:
        out_dir: The output directory for the build.
        simulator_udid: The UDID of the simulator to use.
        target: The build target.
        app_name: The app bundle name.
        enable_features: Comma-separated list of features to enable.
        disable_features: Comma-separated list of features to disable.
        app_args: Additional arguments to pass to the app.
        wipe: Whether to wipe the app container before installation.

    Returns:
        The exit code of the app run (0 for success, non-zero for failure).
    """
    if not app_name:
        app_name = 'Chromium.app' if target == 'chrome' else f'{target}.app'

    app_path = os.path.join(out_dir, app_name)
    if not os.path.exists(app_path):
        print(
            f"\n{Colors.FAIL}{Colors.BOLD}ERROR: App bundle not "
            f"found at {app_path}{Colors.RESET}"
        )
        return 1

    info_plist_path = os.path.join(app_path, 'Info.plist')
    with open(info_plist_path, 'rb') as f:
        info_plist = plistlib.load(f)
    bundle_id = info_plist['CFBundleIdentifier']

    if wipe:
        shared_test_utils.uninstall_app(simulator_udid, bundle_id)

    shared_test_utils.install_app(simulator_udid, app_path)

    launch_args: list[str] = []
    if enable_features:
        launch_args.append(f'--enable-features={enable_features}')
    if disable_features:
        launch_args.append(f'--disable-features={disable_features}')
    if app_args:
        launch_args.extend(app_args)

    print_header("--- Running App ---")
    return shared_test_utils.launch_app_with_console(
        simulator_udid=simulator_udid,
        bundle_id=bundle_id,
        app_args=launch_args,
        app_name=app_name,
    )


def main() -> int:
    """Main function for the script.

    Parses arguments, finds a simulator, builds, and runs the app.

    Returns:
        The exit code of the run.
    """
    print_header("=== Run App ===")
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--out-dir',
        default='out/Debug-iphonesimulator',
        help=(
            'The output directory to use for the build (default: %(default)s).'
        ),
    )
    parser.add_argument(
        '--target',
        default='chrome',
        help='The build target to build (default: %(default)s).',
    )
    parser.add_argument(
        '--app',
        help="The name of the app bundle to run (default: Chromium.app if "
        "target is chrome, else <target>.app).",
    )
    parser.add_argument(
        '--build',
        action='store_true',
        help='Build the app target before running.',
    )
    parser.add_argument(
        '--device',
        help='The device type or UDID to use for running the app '
        '(can also be set via IOS_SIMULATOR_DEFAULT_DEVICE).',
    )
    parser.add_argument(
        '--os',
        help='The OS version to use for running the app (e.g., 17.5) '
        '(can also be set via IOS_SIMULATOR_DEFAULT_OS).',
    )
    parser.add_argument(
        '--enable-features',
        help='Comma-separated list of feature flags to enable.',
    )
    parser.add_argument(
        '--disable-features',
        help='Comma-separated list of feature flags to disable.',
    )
    parser.add_argument(
        '--args', nargs='+', help='Additional arguments to pass to the app.'
    )
    parser.add_argument(
        '--wipe',
        action='store_true',
        help='Uninstall the app before installing to wipe stale '
        'container data/preferences.',
    )
    args = parser.parse_args()

    simulator = shared_test_utils.find_and_boot_simulator(args.device, args.os)
    if not simulator:
        return 1

    if args.build:
        if not _build_app(args.out_dir, args.target):
            return 1

    return _run_app(
        args.out_dir,
        simulator.udid,
        args.target,
        args.app,
        args.enable_features,
        args.disable_features,
        args.args,
        args.wipe,
    )


if __name__ == '__main__':
    sys.exit(main())
