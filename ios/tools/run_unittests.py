#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs selected unit tests."""

import argparse
import json
import os
import plistlib
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple


class Colors:
  """ANSI color codes for terminal output."""
  HEADER = '\033[35m'  # Magenta
  BLUE = '\033[34m'
  CYAN = '\033[36m'
  GREEN = '\033[32m'
  WARNING = '\033[33m'  # Yellow
  FAIL = '\033[31m'  # Red
  BOLD = '\033[1m'
  RESET = '\033[0m'


def _parse_version_string(v_str: str) -> Tuple[int, ...]:
  """Parses a version string into a tuple of integers."""
  return tuple(map(int, v_str.split('.')))


def _find_specific_device(all_devices: Dict[str, List[Dict[str, Any]]],
                          identifier: str) -> Optional[Dict[str, Any]]:
  """Finds a specific device by name or UDID.

  Args:
    all_devices: A dictionary of devices from `simctl list`.
    identifier: The name or UDID of the device to find.

  Returns:
    The device dictionary if found, otherwise None.
  """
  for runtime, devices in all_devices.items():
    for device in devices:
      if (device['name'].lower() == identifier.lower() or
          device['udid'] == identifier):
        return device
  return None


def _find_best_available_device(
    all_devices: Dict[str, List[Dict[str, Any]]]) -> Optional[Dict[str, Any]]:
  """Finds the best available iPhone simulator to use as a default.

  Args:
    all_devices: A dictionary of devices from `simctl list`.

  Returns:
    The device dictionary for the best candidate, or None if none is found.
  """
  best_candidate = None
  best_sdk_version = (0,)
  best_iphone_version = 0

  for runtime, devices in all_devices.items():
    if 'iOS' not in runtime:
      continue

    sdk_version_str = runtime.split('iOS-')[-1].replace('-', '.')
    current_sdk_version = _parse_version_string(sdk_version_str)

    for device in devices:
      if not device['isAvailable']:
        continue
      match = re.match(r'^iPhone (\d+)', device['name'])
      if match:
        iphone_version = int(match.group(1))
        if current_sdk_version > best_sdk_version:
          best_sdk_version = current_sdk_version
          best_iphone_version = iphone_version
          best_candidate = device
        elif current_sdk_version == best_sdk_version:
          if iphone_version > best_iphone_version:
            best_iphone_version = iphone_version
            best_candidate = device
  return best_candidate


def _find_device_by_type_and_version(
    all_devices: Dict[str, List[Dict[str, Any]]],
    device_type: str,
    os_version: Optional[str] = None) -> Optional[Dict[str, Any]]:
  """Finds a device that matches a specific type and OS version.

  Args:
    all_devices: A dictionary of devices from `simctl list`.
    device_type: The device type to look for (e.g., 'iPhone 15 Pro').
    os_version: The OS version to look for (e.g., '17.5'). If None, the latest
      available OS for the device will be used.

  Returns:
    The device dictionary if found, otherwise None.
  """
  best_candidate = None
  best_sdk_version = (0,)

  for runtime, devices in all_devices.items():
    if os_version:
      if f"iOS-{os_version.replace('.', '-')}" not in runtime:
        continue
    elif 'iOS' not in runtime:
      continue

    sdk_version_str = runtime.split('iOS-')[-1].replace('-', '.')
    current_sdk_version = _parse_version_string(sdk_version_str)

    for device in devices:
      if (device['name'].lower() == device_type.lower() and
          device['isAvailable']):
        if os_version:
          return device
        if current_sdk_version > best_sdk_version:
          best_sdk_version = current_sdk_version
          best_candidate = device
  return best_candidate


def _find_and_boot_simulator(device_type: Optional[str],
                             os_version: Optional[str]) -> Optional[str]:
  """Finds the requested simulator, booting it if necessary.

  Args:
    device_type: The device type to look for (e.g., 'iPhone 15 Pro').
    os_version: The OS version to look for (e.g., '17.5').

  Returns:
    The UDID of the simulator to use. Returns None on failure.
  """
  try:
    output = subprocess.check_output(
        ['xcrun', 'simctl', 'list', 'devices', '--json'], encoding='utf-8')
    all_devices = json.loads(output)['devices']

    device_to_use = None
    if device_type:
      device_to_use = _find_device_by_type_and_version(all_devices, device_type,
                                                       os_version)
      if not device_to_use:
        print(f"Could not find a simulator for device '{device_type}' and OS "
              f"'{os_version}'.")
        subprocess.call(['xcrun', 'simctl', 'list', 'devices', 'available'])
        return None
    else:
      # No device specified, look for a booted one first.
      for runtime, devices in all_devices.items():
        for device in devices:
          if device['state'] == 'Booted':
            device_to_use = device
            break
        if device_to_use:
          break

      # No device is booted, find the best one.
      if not device_to_use:
        print(f"{Colors.BLUE}No simulator booted. "
              f"Finding the newest available iPhone...{Colors.RESET}")
        device_to_use = _find_best_available_device(all_devices)
        if not device_to_use:
          print("Could not find a suitable default iPhone simulator.")
          subprocess.call(['xcrun', 'simctl', 'list', 'devices', 'available'])
          return None

    # Find the OS version for the selected device.
    device_os_version = 'Unknown'
    for runtime, devices in all_devices.items():
      if any(d['udid'] == device_to_use['udid'] for d in devices):
        device_os_version = runtime.split('iOS-')[-1].replace('-', '.')
        break

    print(f'\n{Colors.HEADER}{Colors.BOLD}--- Selecting Simulator ---'
          f'{Colors.RESET}')
    print(f"{Colors.BLUE}Device: {device_to_use['name']}{Colors.RESET}")
    print(f"{Colors.BLUE}OS: {device_os_version}{Colors.RESET}")
    print(f"{Colors.BLUE}UDID: {device_to_use['udid']}{Colors.RESET}")

    # Boot the selected device if it's not already running.
    if device_to_use['state'] != 'Booted':
      print(f"\n{Colors.CYAN}Simulator '{device_to_use['name']}' is not "
            f"booted. Booting...{Colors.RESET}")
      subprocess.check_call(['xcrun', 'simctl', 'boot', device_to_use['udid']])
    return device_to_use['udid']

  except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
    print(f"Error managing simulators: {e}")
    return None


def _build_tests(out_dir: str) -> bool:
  """Builds the unit test target.

  Args:
    out_dir: The output directory for the build.

  Returns:
    True if the build was successful, False otherwise.
  """
  build_command = ['autoninja', '-C', out_dir, 'ios_chrome_unittests']
  print(f'\n{Colors.HEADER}{Colors.BOLD}--- Building Tests ---{Colors.RESET}')
  print(' '.join(build_command) + '\n')
  try:
    subprocess.check_call(build_command)
    return True
  except subprocess.CalledProcessError as e:
    print(f"Build failed with exit code {e.returncode}")
    return False


def _run_tests(out_dir: str, simulator_udid: str,
               gtest_filter: Optional[str]) -> int:
  """Installs and runs the tests on the specified simulator.

  Args:
    out_dir: The output directory for the build.
    simulator_udid: The UDID of the simulator to use.
    gtest_filter: The gtest filter to apply.

  Returns:
    The exit code of the test runner.
  """
  app_path = os.path.join(out_dir, 'ios_chrome_unittests.app')
  info_plist_path = os.path.join(app_path, 'Info.plist')
  with open(info_plist_path, 'rb') as f:
    info_plist = plistlib.load(f)
  bundle_id = info_plist['CFBundleIdentifier']

  install_command = ['xcrun', 'simctl', 'install', simulator_udid, app_path]
  print(f'\n{Colors.HEADER}{Colors.BOLD}--- Installing App ---{Colors.RESET}')
  print(' '.join(install_command))
  subprocess.check_call(install_command)

  launch_command = [
      'xcrun', 'simctl', 'launch', '--console-pty', simulator_udid, bundle_id
  ]
  if gtest_filter:
    launch_command.append('--gtest_filter=' + gtest_filter)
  print(f'\n{Colors.HEADER}{Colors.BOLD}--- Running Tests ---{Colors.RESET}')
  print(' '.join(launch_command) + '\n')

  process = subprocess.Popen(launch_command,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,
                             text=True)

  test_failed = False
  test_completed = False
  while True:
    line = process.stdout.readline()
    if not line:
      break
    sys.stdout.write(line)
    if '[  PASSED  ]' in line or '[  FAILED  ]' in line:
      test_completed = True
    if '[  FAILED  ]' in line:
      test_failed = True

  process.wait()

  # If the test completed and there were failures, return 1.
  if test_completed and test_failed:
    return 1

  # If the test did not complete (i.e. crashed), return 1.
  if not test_completed:
    print(f'\n{Colors.FAIL}{Colors.BOLD}'
          'ERROR: Test runner did not complete. Assuming crash.'
          f'{Colors.RESET}')
    return 1

  # If the test completed and there were no failures, return 0.
  return 0


def main() -> int:
  """Main function for the script.

  Parses arguments, finds a simulator, builds, and runs the tests.

  Returns:
    The exit code of the test run.
  """
  print(f'{Colors.HEADER}{Colors.BOLD}=== Run Unit Tests ==={Colors.RESET}')
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--out-dir',
      default='out/Debug-iphonesimulator',
      help='The output directory to use for the build (default: %(default)s).')
  parser.add_argument('--gtest_filter',
                      help='The gtest_filter to use for running the tests.')
  parser.add_argument('--device', help='The device type to use for the test.')
  parser.add_argument('--os',
                      help='The OS version to use for the test (e.g., 17.5).')
  args = parser.parse_args()

  simulator_udid = _find_and_boot_simulator(args.device, args.os)
  if not simulator_udid:
    return 1

  if not _build_tests(args.out_dir):
    return 1

  return _run_tests(args.out_dir, simulator_udid, args.gtest_filter)


if __name__ == '__main__':
  sys.exit(main())
