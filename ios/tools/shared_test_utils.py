#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utility functions for iOS test runners."""

from __future__ import annotations

from collections.abc import Callable
import dataclasses
import glob
import importlib.util
import json
import os
import re
import shlex
import subprocess
import sys
import threading
import time
import uuid

SIGINT_EXIT_CODE = 130

_XCRUN_BIN = 'xcrun'
_SIMCTL_SUBCOMMAND = 'simctl'
_LIST_SUBCOMMAND = 'list'
_DEVICES_ARG = 'devices'
_JSON_FLAG = '--json'
_BOOT_SUBCOMMAND = 'boot'
_UNINSTALL_SUBCOMMAND = 'uninstall'
_INSTALL_SUBCOMMAND = 'install'
_TERMINATE_SUBCOMMAND = 'terminate'
_LAUNCH_SUBCOMMAND = 'launch'
_SPAWN_SUBCOMMAND = 'spawn'
_CONSOLE_PTY_FLAG = '--console-pty'
_LOG_COMMAND = 'log'
_STREAM_SUBCOMMAND = 'stream'
_FLAG_LEVEL = '--level'
_DEBUG_LEVEL = 'debug'
_FLAG_STYLE = '--style'
_LOG_STREAM_STYLE = 'syslog'
_FLAG_PREDICATE = '--predicate'
_XCODEBUILD_BIN = 'xcodebuild'
_TEST_WITHOUT_BUILDING_ACTION = 'test-without-building'
_FLAG_TEST_ITERATIONS = '-test-iterations'
_FLAG_RETRY_TESTS_ON_FAILURE = '-retry-tests-on-failure'
_FLAG_RUN_TESTS_UNTIL_FAILURE = '-run-tests-until-failure'
_FLAG_TEST_REPETITION_RELAUNCH_ENABLED = '-test-repetition-relaunch-enabled'
_YES_VALUE = 'YES'
_PGREP_STARTUP_DELAY_SECONDS = 1
_WORKSPACE_PREFIX_RE = re.compile(r'^\[[^\]]+\]\s*')
_IPHONE_MODEL_RE = re.compile(r'^iPhone (\d+)')
_INTERNAL_BACKEND_MODULE_NAME = 'ios_internal_simulator_backend'
_INTERNAL_BACKEND_FACTORY_NAME = 'create_simulator_backend'
_INTERNAL_BACKEND_REL_PATH = os.path.join(
    'ios_internal', 'tools', 'simulator_backend.py'
)


def _load_env() -> None:
    """Loads environment variables from a .env file if it exists."""
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(tools_dir, '..', '..'))

    paths_to_try = [
        os.path.join(os.getcwd(), '.env'),
        os.path.join(repo_root, '.env'),
    ]

    loaded = set()
    for path in paths_to_try:
        path = os.path.abspath(path)
        if path in loaded or not os.path.exists(path):
            continue
        loaded.add(path)
        try:
            with open(path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    if '=' not in line:
                        continue
                    key, val = line.split('=', 1)
                    key = key.strip()
                    val = val.strip()
                    if (val.startswith('"') and val.endswith('"')) or (
                        val.startswith("'") and val.endswith("'")
                    ):
                        val = val[1:-1]
                    if key not in os.environ:
                        os.environ[key] = val
        except Exception as e:
            print(f"Warning: Failed to read .env file at {path}: {e}")


# Automatically load environment variables from .env files on import.
_load_env()


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
    def parsed_os_version(self) -> tuple[int, ...]:
        """Returns the OS version string as a tuple of integers."""
        return tuple(map(int, self.os_version.split('.')))

    @property
    def display_string(self) -> str:
        """Returns a formatted string for displaying simulator details."""
        booted_status = (
            f"({Colors.GREEN}Booted{Colors.RESET})" if self.booted else ""
        )
        return (
            f"{self.name} (OS: {self.os_version}, UDID: {self.udid}) "
            f"{booted_status}"
        )


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


def print_command(command_list: list[str]) -> None:
    """Prints a shell command, quoting arguments for safe copy-pasting."""
    quoted_command = ' '.join(shlex.quote(arg) for arg in command_list)
    print(f'{quoted_command}\n')


def print_header(header_text: str) -> None:
    """Prints a formatted header string with special colors."""
    print(f'\n{Colors.HEADER}{Colors.BOLD}{header_text}{Colors.RESET}')


def get_workspace_id() -> str:
    """Derives the workspace identifier from the repository checkout path."""
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(tools_dir, '..', '..'))
    repo_name = os.path.basename(repo_root)
    parent_name = os.path.basename(os.path.dirname(repo_root))
    if repo_name == 'src' and parent_name:
        return parent_name
    return repo_name or 'chromium'


def _strip_workspace_prefix(name: str) -> str:
    """Strips a leading `[workspace] ` label from a simulator `name`."""
    return _WORKSPACE_PREFIX_RE.sub('', name)


def _matches_current_workspace(name: str) -> bool:
    """Returns `True` if `name` is prefixed with current workspace label."""
    return name.startswith(f'[{get_workspace_id()}]')


def stream_process_console(
    process: subprocess.Popen[str],
    simulator_udid: str,
    bundle_id: str,
    line_callback: Callable[[str], None] | None = None,
) -> int:
    """Streams `process.stdout` lines to `stdout` and `line_callback`."""
    try:
        assert process.stdout is not None
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
                if line_callback:
                    line_callback(line)
        process.wait()
    except KeyboardInterrupt:
        print(f"\n{Colors.CYAN}Terminating app in simulator...{Colors.RESET}")
        terminate_app(simulator_udid, bundle_id)
        process.terminate()
        process.wait()
        return SIGINT_EXIT_CODE

    return process.returncode


class SimulatorBackend:
    """Simulator backend utilizing `xcrun simctl` and `xcodebuild`."""

    def list_simulators(self) -> list[Simulator]:
        """Fetches and parses available simulators using `xcrun simctl`."""
        simulators: list[Simulator] = []
        try:
            output = subprocess.check_output(
                [
                    _XCRUN_BIN,
                    _SIMCTL_SUBCOMMAND,
                    _LIST_SUBCOMMAND,
                    _DEVICES_ARG,
                    _JSON_FLAG,
                ],
                encoding='utf-8',
            )
            all_devices = json.loads(output).get('devices', {})
            for runtime, devices in all_devices.items():
                if 'iOS' not in runtime:
                    continue
                os_version_full = runtime.split('iOS-')[-1].replace('-', '.')
                os_version = '.'.join(os_version_full.split('.')[:2])
                for device in devices:
                    simulators.append(
                        Simulator(
                            name=device.get('name', ''),
                            udid=device.get('udid', ''),
                            os_version=os_version,
                            state=device.get('state', ''),
                            is_available=device.get('isAvailable', False),
                        )
                    )
        except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
            print(f"Error fetching simulator list: {e}")
        return simulators

    def build_boot_command(self, simulator_udid: str) -> list[str]:
        """Builds command to boot `simulator_udid`."""
        return [
            _XCRUN_BIN,
            _SIMCTL_SUBCOMMAND,
            _BOOT_SUBCOMMAND,
            simulator_udid,
        ]

    def build_uninstall_command(
        self, simulator_udid: str, bundle_id: str
    ) -> list[str]:
        """Builds command to uninstall `bundle_id` from `simulator_udid`."""
        return [
            _XCRUN_BIN,
            _SIMCTL_SUBCOMMAND,
            _UNINSTALL_SUBCOMMAND,
            simulator_udid,
            bundle_id,
        ]

    def build_install_command(
        self, simulator_udid: str, app_path: str
    ) -> list[str]:
        """Builds command to install `app_path` onto `simulator_udid`."""
        return [
            _XCRUN_BIN,
            _SIMCTL_SUBCOMMAND,
            _INSTALL_SUBCOMMAND,
            simulator_udid,
            app_path,
        ]

    def build_terminate_command(
        self, simulator_udid: str, bundle_id: str
    ) -> list[str] | None:
        """Builds command to terminate `bundle_id` on `simulator_udid`."""
        return [
            _XCRUN_BIN,
            _SIMCTL_SUBCOMMAND,
            _TERMINATE_SUBCOMMAND,
            simulator_udid,
            bundle_id,
        ]

    def build_launch_command(
        self,
        simulator_udid: str,
        bundle_id: str,
        app_args: list[str] | None = None,
    ) -> list[str]:
        """Builds command to launch `bundle_id` on `simulator_udid`."""
        cmd = [
            _XCRUN_BIN,
            _SIMCTL_SUBCOMMAND,
            _LAUNCH_SUBCOMMAND,
            _CONSOLE_PTY_FLAG,
            simulator_udid,
            bundle_id,
        ]
        if app_args:
            cmd.extend(app_args)
        return cmd

    def on_app_launched(
        self,
        process: subprocess.Popen[str],
        simulator_udid: str,
        bundle_id: str,
        app_name: str,
    ) -> None:
        """Hook invoked after app process is spawned."""
        print(f"{Colors.BLUE}Runner Process PID: {process.pid}{Colors.RESET}")
        time.sleep(_PGREP_STARTUP_DELAY_SECONDS)
        try:
            exec_name = os.path.splitext(app_name)[0]
            pgrep_pattern = (
                rf'{simulator_udid}.*/{re.escape(app_name)}/'
                rf'{re.escape(exec_name)}(\s|$)'
            )
            pgrep_out = subprocess.check_output(
                ['pgrep', '-f', pgrep_pattern], text=True
            ).strip()
            if pgrep_out:
                app_pid = pgrep_out.splitlines()[0]
                print(
                    f"{Colors.BLUE}{Colors.BOLD}App Process PID "
                    f"(in simulator): {app_pid}{Colors.RESET}"
                )
                print(
                    f"{Colors.CYAN}To stop the app, run: xcrun simctl "
                    f"terminate {simulator_udid} {bundle_id}"
                    f"{Colors.RESET}\n"
                )
        except subprocess.CalledProcessError:
            pass

    def prepare_egtests(
        self, out_dir: str, simulator_udid: str, scheme: str
    ) -> bool:
        """Prepares test bundles and simulator state before EG2 tests."""
        return True

    def build_egtests_command(
        self,
        out_dir: str,
        simulator_udid: str,
        arch: str,
        scheme: str,
        test_filters: list[str],
        extra_test_args: list[str],
    ) -> list[str]:
        """Builds `xcodebuild` command to run EarlGrey tests."""
        project_path = os.path.join(
            os.getcwd(), 'out', 'build', 'all.xcodeproj'
        )
        launch_command = [
            _XCODEBUILD_BIN,
            _TEST_WITHOUT_BUILDING_ACTION,
            '-project',
            project_path,
            '-scheme',
            scheme,
            '-destination',
            f'platform=iOS Simulator,id={simulator_udid},arch={arch}',
            f'CONFIGURATION_BUILD_DIR={os.path.abspath(out_dir)}',
            '-collect-test-diagnostics',
            'never',
        ]
        for test_filter in test_filters:
            if test_filter:
                launch_command.append(f'-only-testing:{scheme}/{test_filter}')
        if extra_test_args:
            launch_command.extend(extra_test_args)
        return launch_command

    def build_log_stream_command(
        self, simulator_udid: str, predicate: str
    ) -> list[str]:
        """Builds command to stream simulator logs via `xcrun simctl spawn`."""
        return [
            _XCRUN_BIN,
            _SIMCTL_SUBCOMMAND,
            _SPAWN_SUBCOMMAND,
            simulator_udid,
            _LOG_COMMAND,
            _STREAM_SUBCOMMAND,
            _FLAG_LEVEL,
            _DEBUG_LEVEL,
            _FLAG_STYLE,
            _LOG_STREAM_STYLE,
            _FLAG_PREDICATE,
            predicate,
        ]


_simulator_backend: SimulatorBackend | None = None


def _load_internal_simulator_backend() -> SimulatorBackend | None:
    """Loads the internal simulator backend plugin if available."""
    if _INTERNAL_BACKEND_MODULE_NAME in sys.modules:
        module = sys.modules[_INTERNAL_BACKEND_MODULE_NAME]
        factory = getattr(module, _INTERNAL_BACKEND_FACTORY_NAME, None)
        if callable(factory):
            backend = factory()
            if isinstance(backend, SimulatorBackend):
                return backend
        return None

    tools_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(tools_dir, '..', '..'))
    plugin_path = os.path.join(repo_root, _INTERNAL_BACKEND_REL_PATH)
    if not os.path.isfile(plugin_path):
        return None

    if tools_dir not in sys.path:
        sys.path.insert(0, tools_dir)

    try:
        spec = importlib.util.spec_from_file_location(
            _INTERNAL_BACKEND_MODULE_NAME, plugin_path
        )
        if spec is None or spec.loader is None:
            return None
        module = importlib.util.module_from_spec(spec)
        sys.modules[_INTERNAL_BACKEND_MODULE_NAME] = module
        spec.loader.exec_module(module)
        factory = getattr(module, _INTERNAL_BACKEND_FACTORY_NAME, None)
        if callable(factory):
            backend = factory()
            if isinstance(backend, SimulatorBackend):
                return backend
    except Exception as e:
        print(f"Warning: Failed to load internal simulator backend: {e}")
    return None


def get_simulator_backend() -> SimulatorBackend:
    """Returns the active simulator backend, initializing it if needed."""
    global _simulator_backend
    if _simulator_backend is None:
        _simulator_backend = (
            _load_internal_simulator_backend() or SimulatorBackend()
        )
    return _simulator_backend


def set_simulator_backend(backend: SimulatorBackend | None) -> None:
    """Sets or overrides the active simulator backend."""
    global _simulator_backend
    _simulator_backend = backend


def list_simulators() -> list[Simulator]:
    """Returns the list of available iOS simulators from the active backend."""
    return get_simulator_backend().list_simulators()


def boot_simulator(simulator_udid: str) -> bool:
    """Boots `simulator_udid` using the active simulator backend."""
    try:
        cmd = get_simulator_backend().build_boot_command(simulator_udid)
        subprocess.check_call(cmd)
        return True
    except (subprocess.CalledProcessError, RuntimeError) as e:
        print(f"Error booting simulator: {e}")
        return False


def uninstall_app(simulator_udid: str, bundle_id: str) -> None:
    """Uninstalls `bundle_id` from `simulator_udid` to wipe container data."""
    cmd = get_simulator_backend().build_uninstall_command(
        simulator_udid, bundle_id
    )
    print_header("--- Uninstalling App (Wiping Data) ---")
    print_command(cmd)
    subprocess.run(cmd, check=False)


def install_app(simulator_udid: str, app_path: str) -> None:
    """Installs the `.app` bundle at `app_path` onto `simulator_udid`."""
    cmd = get_simulator_backend().build_install_command(
        simulator_udid, app_path
    )
    print_header("--- Installing App ---")
    print_command(cmd)
    subprocess.check_call(cmd)


def terminate_app(simulator_udid: str, bundle_id: str) -> None:
    """Terminates a running instance of `bundle_id` on `simulator_udid`."""
    cmd = get_simulator_backend().build_terminate_command(
        simulator_udid, bundle_id
    )
    if cmd is not None:
        subprocess.run(cmd, capture_output=True, check=False)


def launch_app_with_console(
    simulator_udid: str,
    bundle_id: str,
    app_args: list[str] | None = None,
    app_name: str | None = None,
    line_callback: Callable[[str], None] | None = None,
) -> int:
    """Launches `bundle_id` on `simulator_udid` and streams console output."""
    backend = get_simulator_backend()
    launch_command = backend.build_launch_command(
        simulator_udid, bundle_id, app_args
    )
    print_command(launch_command)
    process = subprocess.Popen(
        launch_command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors='replace',
    )
    if app_name:
        backend.on_app_launched(process, simulator_udid, bundle_id, app_name)
    return stream_process_console(
        process=process,
        simulator_udid=simulator_udid,
        bundle_id=bundle_id,
        line_callback=line_callback,
    )


def launch_egtests(
    out_dir: str,
    simulator_udid: str,
    arch: str,
    scheme: str,
    test_filters: list[str],
    test_iterations: int | None = None,
    retry_tests_on_failure: bool = False,
    run_tests_until_failure: bool = False,
    test_repetition_relaunch_enabled: str | bool | None = None,
) -> int:
    """Launches EarlGrey 2 tests on `simulator_udid` via active backend."""
    backend = get_simulator_backend()
    if not backend.prepare_egtests(out_dir, simulator_udid, scheme):
        return 1

    extra_test_args: list[str] = []
    if test_iterations is not None:
        extra_test_args.extend([_FLAG_TEST_ITERATIONS, str(test_iterations)])
    if retry_tests_on_failure:
        extra_test_args.append(_FLAG_RETRY_TESTS_ON_FAILURE)
    if run_tests_until_failure:
        extra_test_args.append(_FLAG_RUN_TESTS_UNTIL_FAILURE)
    if test_repetition_relaunch_enabled:
        relaunch_val = (
            _YES_VALUE
            if isinstance(test_repetition_relaunch_enabled, bool)
            else str(test_repetition_relaunch_enabled)
        )
        extra_test_args.extend(
            [_FLAG_TEST_REPETITION_RELAUNCH_ENABLED, relaunch_val]
        )

    launch_command = backend.build_egtests_command(
        out_dir=out_dir,
        simulator_udid=simulator_udid,
        arch=arch,
        scheme=scheme,
        test_filters=test_filters,
        extra_test_args=extra_test_args,
    )
    print_header("--- Running Tests ---")
    print_command(launch_command)
    try:
        return subprocess.call(launch_command)
    except KeyboardInterrupt:
        return SIGINT_EXIT_CODE


def build_log_stream_command(simulator_udid: str, predicate: str) -> list[str]:
    """Builds command to stream simulator system logs for `predicate`."""
    return get_simulator_backend().build_log_stream_command(
        simulator_udid, predicate
    )


class SimulatorManager:
    """Manages and queries the list of available simulators."""

    def __init__(self):
        """Initializes the manager, fetching and parsing simulator list."""
        self.simulators: list[Simulator] = list_simulators()

    def list_available_simulators(self):
        """Prints a formatted list of available simulators."""
        print("\nAvailable Simulators:")
        for sim in self.simulators:
            if sim.is_available:
                print(f"  - {sim.display_string}")

    def find_specific_device(self, identifier: str) -> Simulator | None:
        """Finds a specific device by name or UDID."""
        target = identifier.lower()
        for sim in self.simulators:
            stripped_name = _strip_workspace_prefix(sim.name).lower()
            if (
                sim.name.lower() == target
                or stripped_name == target
                or sim.udid.lower() == target
            ):
                return sim
        return None

    def find_best_available_device(self) -> Simulator | None:
        """Finds the best available iPhone simulator to use as a default."""
        best_candidate: Simulator | None = None
        best_key: tuple[tuple[int, ...], int, int] = ((0,), 0, -1)

        for sim in self.simulators:
            if not sim.is_available:
                continue

            stripped_name = _strip_workspace_prefix(sim.name)
            match = _IPHONE_MODEL_RE.match(stripped_name)
            if not match:
                continue

            candidate_key = (
                sim.parsed_os_version,
                int(match.group(1)),
                1 if _matches_current_workspace(sim.name) else 0,
            )
            if candidate_key > best_key:
                best_key = candidate_key
                best_candidate = sim
        return best_candidate

    def find_device_by_type_and_version(
        self, device_type: str, os_version: str | None = None
    ) -> Simulator | None:
        """Finds a device that matches a specific type and OS version."""
        best_candidate: Simulator | None = None
        best_key: tuple[tuple[int, ...], int, int] = ((0,), -1, -1)
        target_type = device_type.lower()
        os_version_truncated = (
            '.'.join(os_version.split('.')[:2]) if os_version else None
        )

        for sim in self.simulators:
            stripped_name = _strip_workspace_prefix(sim.name).lower()
            if not sim.is_available or (
                sim.name.lower() != target_type and stripped_name != target_type
            ):
                continue

            if os_version_truncated and sim.os_version != os_version_truncated:
                continue

            candidate_key = (
                sim.parsed_os_version,
                1 if _matches_current_workspace(sim.name) else 0,
                1 if sim.booted else 0,
            )
            if candidate_key > best_key:
                best_key = candidate_key
                best_candidate = sim
        return best_candidate


def find_and_boot_simulator(
    device_type: str | None, os_version: str | None
) -> Simulator | None:
    """Finds the requested simulator, booting it if necessary.

    Args:
        device_type: The device type to look for (e.g., 'iPhone 15 Pro').
        os_version: The OS version to look for (e.g., '17.5').

    Returns:
        A Simulator object for the selected device, or None on failure.
    """
    print_header("--- Selecting Simulator ---")

    env_device = os.environ.get('IOS_SIMULATOR_DEFAULT_DEVICE')
    env_os = os.environ.get('IOS_SIMULATOR_DEFAULT_OS')

    if not device_type and env_device:
        device_type = env_device
        print(f"Using default simulator device from environment: {device_type}")
    if not os_version and env_os:
        os_version = env_os
        print(f"Using default simulator OS from environment: {os_version}")
    sim_manager = SimulatorManager()
    if not sim_manager.simulators:
        return None

    simulator_to_use = None

    # 1. If a specific device is requested, try to find it.
    if device_type:
        is_udid = False
        try:
            uuid.UUID(device_type)
            is_udid = True
        except ValueError:
            pass

        if is_udid:
            simulator_to_use = sim_manager.find_specific_device(device_type)
            if not simulator_to_use:
                print(f"Could not find a simulator with UDID '{device_type}'.")
                return None
        else:
            simulator_to_use = sim_manager.find_device_by_type_and_version(
                device_type, os_version
            )
            if not simulator_to_use:
                print(
                    f"Could not find a simulator for device '{device_type}' "
                    f"and OS '{os_version}'."
                )
                sim_manager.list_available_simulators()
                return None

    # 2. If no device was specified, look for an already booted one.
    if not simulator_to_use:
        booted_sims = [sim for sim in sim_manager.simulators if sim.booted]
        for sim in booted_sims:
            if _matches_current_workspace(sim.name):
                simulator_to_use = sim
                break
        if not simulator_to_use and booted_sims:
            simulator_to_use = booted_sims[0]

    # 3. If still no device, find the best available default.
    if not simulator_to_use:
        print(
            f"{Colors.BLUE}No simulator booted. "
            f"Finding the newest available iPhone...{Colors.RESET}"
        )
        simulator_to_use = sim_manager.find_best_available_device()
        if not simulator_to_use:
            print("Could not find a suitable default iPhone simulator.")
            sim_manager.list_available_simulators()
            return None

    print(
        f"{Colors.BLUE}Device: {simulator_to_use.display_string}{Colors.RESET}"
    )

    # Boot the selected device if it's not already running.
    if not simulator_to_use.booted:
        print(
            f"\n{Colors.CYAN}Simulator '{simulator_to_use.display_string}' is "
            f"not booted. Booting...{Colors.RESET}"
        )
        if not boot_simulator(simulator_to_use.udid):
            return None
    return simulator_to_use


class SimulatorLogStreamer:
    """Streams logs from the simulator in the background."""

    def __init__(
        self,
        simulator_udid: str,
        out_dir: str,
        scheme: str,
        predicate: str,
        show_in_stdout: bool = False,
    ):
        self.simulator_udid = simulator_udid
        self.out_dir = out_dir
        self.scheme = scheme
        self.predicate = predicate
        self.show_in_stdout = show_in_stdout
        self.proc = None
        self.safe_scheme = re.sub(r'[^a-zA-Z0-9_]', '_', scheme)
        self.log_file_path = os.path.join(
            out_dir, f'app_logs_{self.safe_scheme}_{os.getpid()}.log'
        )
        self.thread = None
        self.log_file = None

    def start(self):
        os.makedirs(self.out_dir, exist_ok=True)

        # Keep only 5 most recent log files for this scheme to limit disk usage.
        try:
            pattern = os.path.join(
                self.out_dir, f'app_logs_{self.safe_scheme}_*.log'
            )
            existing_logs = glob.glob(pattern)
            if len(existing_logs) >= 5:
                existing_logs.sort(key=os.path.getmtime)
                for old_log in existing_logs[:-4]:
                    os.remove(old_log)
        except Exception:
            pass

        cmd = build_log_stream_command(self.simulator_udid, self.predicate)

        self.log_file = open(
            self.log_file_path, 'w', encoding='utf-8', errors='replace'
        )

        self.proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding='utf-8',
            errors='replace',
        )

        def read_loop():
            for line in self.proc.stdout:
                self.log_file.write(line)
                self.log_file.flush()
                if self.show_in_stdout:
                    print(f"{Colors.CYAN}[AppLog]{Colors.RESET} {line.strip()}")

        self.thread = threading.Thread(target=read_loop, daemon=True)
        self.thread.start()
        print(
            f"{Colors.BLUE}Streaming app logs to: "
            f"{self.log_file_path}{Colors.RESET}"
        )

    def stop(self):
        if self.proc:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

        if self.thread:
            self.thread.join()

        if self.log_file:
            self.log_file.close()
