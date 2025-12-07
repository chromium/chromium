#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utility functions for iOS test runners."""

import dataclasses
import json
import re
import subprocess
from typing import Any, Dict, List, Optional, Tuple
import shlex


@dataclasses.dataclass
class Simulator:
    """Dataclass for simulator information."""
    name: str
    udid: str
    os_version: str
    state: str
    is_available: bool

    @property
    def booted(self) -> bool:
        """Returns True if the simulator is in the 'Booted' state."""
        return self.state == 'Booted'

    @property
    def parsed_os_version(self) -> Tuple[int, ...]:
        """Returns the OS version string as a tuple of integers."""
        return tuple(map(int, self.os_version.split('.')))

    @property
    def display_string(self) -> str:
        """Returns a formatted string for displaying simulator details."""
        booted_status = (f"({Colors.GREEN}Booted{Colors.RESET})"
                         if self.booted else "")
        return (f"{self.name} (OS: {self.os_version}, UDID: {self.udid}) "
                f"{booted_status}")


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


def print_command(command_list: List[str]) -> None:
    """Prints a shell command, quoting arguments for safe copy-pasting."""
    quoted_command = ' '.join(shlex.quote(arg) for arg in command_list)
    print(f'{quoted_command}\n')


def print_header(header_text: str) -> None:
    """Prints a formatted header string with special colors."""
    print(f'\n{Colors.HEADER}{Colors.BOLD}{header_text}{Colors.RESET}')


class SimulatorManager:
    """Manages and queries the list of available simulators."""

    def __init__(self):
        """Initializes the manager, fetching and parsing the simulator list."""
        self.simulators: List[Simulator] = []
        try:
            output = subprocess.check_output(
                ['xcrun', 'simctl', 'list', 'devices', '--json'],
                encoding='utf-8')
            all_devices = json.loads(output).get('devices', {})
            for runtime, devices in all_devices.items():
                # Only process iOS runtimes.
                if 'iOS' not in runtime:
                    continue
                os_version = runtime.split('iOS-')[-1].replace('-', '.')
                for device in devices:
                    self.simulators.append(
                        Simulator(name=device.get('name', ''),
                                  udid=device.get('udid', ''),
                                  os_version=os_version,
                                  state=device.get('state', ''),
                                  is_available=device.get('isAvailable',
                                                          False)))
        except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
            print(f"Error fetching simulator list: {e}")

    def list_available_simulators(self):
        """Prints a formatted list of available simulators."""
        print("\nAvailable Simulators:")
        for sim in self.simulators:
            if sim.is_available:
                print(f"  - {sim.display_string}")

    def find_specific_device(self, identifier: str) -> Optional[Simulator]:
        """Finds a specific device by name or UDID."""
        for sim in self.simulators:
            if sim.name.lower() == identifier.lower() or sim.udid == identifier:
                return sim
        return None

    def find_best_available_device(self) -> Optional[Simulator]:
        """Finds the best available iPhone simulator to use as a default."""
        best_candidate = None
        best_sdk_version = (0,)
        best_iphone_version = 0

        for sim in self.simulators:
            if not sim.is_available:
                continue

            current_sdk_version = sim.parsed_os_version
            match = re.match(r'^iPhone (\d+)', sim.name)
            if match:
                iphone_version = int(match.group(1))
                if current_sdk_version > best_sdk_version:
                    best_sdk_version = current_sdk_version
                    best_iphone_version = iphone_version
                    best_candidate = sim
                elif current_sdk_version == best_sdk_version:
                    if iphone_version > best_iphone_version:
                        best_iphone_version = iphone_version
                        best_candidate = sim
        return best_candidate

    def find_device_by_type_and_version(
            self,
            device_type: str,
            os_version: Optional[str] = None) -> Optional[Simulator]:
        """Finds a device that matches a specific type and OS version."""
        best_candidate = None
        best_sdk_version = (0,)

        for sim in self.simulators:
            if not sim.is_available or sim.name.lower() != device_type.lower():
                continue

            if os_version:
                if sim.os_version == os_version:
                    return sim
            else:
                current_sdk_version = sim.parsed_os_version
                if current_sdk_version > best_sdk_version:
                    best_sdk_version = current_sdk_version
                    best_candidate = sim
        return best_candidate


def find_and_boot_simulator(device_type: Optional[str],
                            os_version: Optional[str]) -> Optional[Simulator]:
    """Finds the requested simulator, booting it if necessary.

    Args:
        device_type: The device type to look for (e.g., 'iPhone 15 Pro').
        os_version: The OS version to look for (e.g., '17.5').

    Returns:
        A Simulator object for the selected device, or None on failure.
    """
    print_header("--- Selecting Simulator ---")
    sim_manager = SimulatorManager()
    if not sim_manager.simulators:
        return None

    simulator_to_use = None

    # 1. If a specific device is requested, try to find it.
    if device_type:
        simulator_to_use = sim_manager.find_device_by_type_and_version(
            device_type, os_version)
        if not simulator_to_use:
            print(f"Could not find a simulator for device '{device_type}' "
                  f"and OS '{os_version}'.")
            sim_manager.list_available_simulators()
            return None

    # 2. If no device was specified, look for an already booted one.
    if not simulator_to_use:
        for sim in sim_manager.simulators:
            if sim.booted:
                simulator_to_use = sim
                break

    # 3. If still no device, find the best available default.
    if not simulator_to_use:
        print(f"{Colors.BLUE}No simulator booted. "
              f"Finding the newest available iPhone...{Colors.RESET}")
        simulator_to_use = sim_manager.find_best_available_device()
        if not simulator_to_use:
            print("Could not find a suitable default iPhone simulator.")
            sim_manager.list_available_simulators()
            return None

    print(
        f"{Colors.BLUE}Device: {simulator_to_use.display_string}{Colors.RESET}")

    # Boot the selected device if it's not already running.
    if not simulator_to_use.booted:
        print(
            f"\n{Colors.CYAN}Simulator '{simulator_to_use.display_string}' is "
            f"not booted. Booting...{Colors.RESET}")
        try:
            subprocess.check_call(
                ['xcrun', 'simctl', 'boot', simulator_to_use.udid])
        except subprocess.CalledProcessError as e:
            print(f"Error booting simulator: {e}")
            return None
    return simulator_to_use
