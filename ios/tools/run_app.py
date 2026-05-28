#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs the app in the simulator."""

import argparse
import os
import plistlib
import subprocess
import sys
import time
from typing import List, Optional

import shared_test_utils
from shared_test_utils import Colors, Simulator, print_header, print_command


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


def _run_app(out_dir: str,
             simulator_udid: str,
             target: str,
             app_name: Optional[str],
             enable_features: Optional[str] = None,
             disable_features: Optional[str] = None,
             app_args: Optional[List[str]] = None,
             wipe: bool = False) -> int:
    """Installs and runs the app on the specified simulator.

    Args:
        out_dir: The output directory for the build.
        simulator_udid: The UDID of the simulator to use.
        target: The build target.
        app_name: The app bundle name.
        enable_features: Comma-separated list of features to enable.
        disable_features: Comma-separated list of features to disable.
        app_args: Additional arguments to pass to the app.

    Returns:
        The exit code of the app run (0 for success, non-zero for failure).
    """
    if not app_name:
        app_name = 'Chromium.app' if target == 'chrome' else f'{target}.app'

    app_path = os.path.join(out_dir, app_name)
    if not os.path.exists(app_path):
        print(f"\n{Colors.FAIL}{Colors.BOLD}ERROR: App bundle not "
              f"found at {app_path}{Colors.RESET}")
        return 1

    info_plist_path = os.path.join(app_path, 'Info.plist')
    with open(info_plist_path, 'rb') as f:
        info_plist = plistlib.load(f)
    bundle_id = info_plist['CFBundleIdentifier']

    if wipe:
        uninstall_command = [
            'xcrun', 'simctl', 'uninstall', simulator_udid, bundle_id
        ]
        print_header("--- Uninstalling App (Wiping Data) ---")
        print_command(uninstall_command)
        subprocess.run(uninstall_command)

    install_command = ['xcrun', 'simctl', 'install', simulator_udid, app_path]
    print_header("--- Installing App ---")
    print_command(install_command)
    subprocess.check_call(install_command)

    launch_command = [
        'xcrun', 'simctl', 'launch', '--console-pty', simulator_udid, bundle_id
    ]
    if enable_features:
        launch_command.append(f'--enable-features={enable_features}')
    if disable_features:
        launch_command.append(f'--disable-features={disable_features}')
    if app_args:
        launch_command.extend(app_args)

    print_header("--- Running App ---")
    print_command(launch_command)

    process = subprocess.Popen(launch_command,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT,
                               text=True)

    print(f"{Colors.BLUE}Runner Process PID: {process.pid}{Colors.RESET}")
    time.sleep(1)
    try:
        exec_name = os.path.splitext(app_name)[0]
        pgrep_out = subprocess.check_output(['pgrep', '-x', exec_name],
                                            text=True).strip()
        if pgrep_out:
            app_pid = pgrep_out.splitlines()[0]
            print(f"{Colors.BLUE}{Colors.BOLD}App Process PID (in simulator): "
                  f"{app_pid}{Colors.RESET}")
            print(f"{Colors.CYAN}To stop the app, run: xcrun simctl terminate "
                  f"{simulator_udid} {bundle_id}{Colors.RESET}\n")
    except subprocess.CalledProcessError:
        pass

    try:
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
        process.wait()
    except KeyboardInterrupt:
        # If the user terminates the script via Ctrl-C, terminate the launched
        # app in the simulator cleanly.
        print(f"\n{Colors.CYAN}Terminating app in simulator...{Colors.RESET}")
        subprocess.run(
            ['xcrun', 'simctl', 'terminate', simulator_udid, bundle_id],
            capture_output=True)
        process.terminate()
        process.wait()

    return process.returncode


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
        help='The output directory to use for the build (default: %(default)s).'
    )
    parser.add_argument(
        '--target',
        default='chrome',
        help='The build target to build (default: %(default)s).')
    parser.add_argument(
        '--app',
        help="The name of the app bundle to run (default: Chromium.app if "
        "target is chrome, else <target>.app).")
    parser.add_argument('--build',
                        action='store_true',
                        help='Build the app target before running.')
    parser.add_argument('--device',
                        help='The device type to use for running the app.')
    parser.add_argument(
        '--os', help='The OS version to use for running the app (e.g., 17.5).')
    parser.add_argument('--enable-features',
                        help='Comma-separated list of feature flags to enable.')
    parser.add_argument(
        '--disable-features',
        help='Comma-separated list of feature flags to disable.')
    parser.add_argument('--args',
                        nargs='+',
                        help='Additional arguments to pass to the app.')
    parser.add_argument(
        '--wipe',
        action='store_true',
        help='Uninstall the app before installing to wipe stale '
        'container data/preferences.')
    args = parser.parse_args()

    simulator = shared_test_utils.find_and_boot_simulator(args.device, args.os)
    if not simulator:
        return 1

    if args.build:
        if not _build_app(args.out_dir, args.target):
            return 1

    return _run_app(args.out_dir, simulator.udid, args.target, args.app,
                    args.enable_features, args.disable_features, args.args,
                    args.wipe)


if __name__ == '__main__':
    sys.exit(main())
