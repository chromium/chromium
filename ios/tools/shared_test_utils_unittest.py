#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for ios/tools/shared_test_utils.py."""

from __future__ import annotations

import json
import subprocess
import unittest
from unittest import mock

import shared_test_utils
from shared_test_utils import (
    SIGINT_EXIT_CODE,
    Simulator,
    SimulatorBackend,
    SimulatorManager,
    boot_simulator,
    build_log_stream_command,
    find_and_boot_simulator,
    get_simulator_backend,
    get_workspace_id,
    install_app,
    launch_app_with_console,
    launch_egtests,
    list_simulators,
    set_simulator_backend,
    stream_process_console,
    terminate_app,
    uninstall_app,
)

_TEST_UDID = '12345678-1234-1234-1234-1234567890AB'
_TEST_BUNDLE_ID = 'com.google.chrome.ios'
_TEST_APP_PATH = '/path/to/Test.app'
_TEST_APP_NAME = 'Test.app'
_TEST_OUT_DIR = '/path/to/out/Debug'
_TEST_ARCH = 'arm64'
_TEST_SCHEME = 'ios_chrome_ui_eg2tests'
_TEST_PREDICATE = 'process == "Chrome"'
_SAMPLE_JSON_SIMCTL_OUTPUT = json.dumps(
    {
        'devices': {
            'com.apple.CoreSimulator.SimRuntime.iOS-17-5': [
                {
                    'name': 'iPhone 15 Pro',
                    'udid': _TEST_UDID,
                    'state': 'Booted',
                    'isAvailable': True,
                }
            ],
            'com.apple.CoreSimulator.SimRuntime.watchOS-10-0': [
                {
                    'name': 'Apple Watch',
                    'udid': 'WATCH-UDID',
                    'state': 'Shutdown',
                    'isAvailable': True,
                }
            ],
        }
    }
)


class TestSimulatorModel(unittest.TestCase):
    """Tests the `Simulator` dataclass properties."""

    def test_simulator_properties(self) -> None:
        """Tests property accessors on `Simulator`."""
        sim = Simulator(
            name='iPhone 15 Pro',
            udid=_TEST_UDID,
            os_version='17.5',
            state='Booted',
            is_available=True,
        )
        self.assertTrue(sim.booted)
        self.assertEqual(sim.parsed_os_version, (17, 5))
        self.assertIn('iPhone 15 Pro', sim.display_string)
        self.assertIn(_TEST_UDID, sim.display_string)


class TestWorkspaceHelpers(unittest.TestCase):
    """Tests repository workspace helper utilities."""

    def test_workspace_id(self) -> None:
        """Tests that `get_workspace_id` returns checkout identifier."""
        ws_id = get_workspace_id()
        self.assertIsInstance(ws_id, str)
        self.assertTrue(len(ws_id) > 0)

    def test_strip_workspace_prefix(self) -> None:
        """Tests stripping `[workspace]` prefix from simulator name."""
        self.assertEqual(
            shared_test_utils._strip_workspace_prefix('[bling] iPhone 17 Pro'),
            'iPhone 17 Pro',
        )
        self.assertEqual(
            shared_test_utils._strip_workspace_prefix('iPhone 17 Pro'),
            'iPhone 17 Pro',
        )

    def test_matches_current_workspace(self) -> None:
        """Tests matching name prefix with active workspace."""
        ws_id = get_workspace_id()
        self.assertTrue(
            shared_test_utils._matches_current_workspace(f'[{ws_id}] Device')
        )
        self.assertFalse(
            shared_test_utils._matches_current_workspace('[other] Device')
        )


class TestStreamProcessConsole(unittest.TestCase):
    """Tests the `stream_process_console` function."""

    def test_stream_process_console_success(self) -> None:
        """Tests streaming output with line_callback until process exit."""
        fake_proc = mock.MagicMock()
        fake_proc.stdout.readline.side_effect = ['line 1\n', 'line 2\n', '']
        fake_proc.poll.return_value = 0
        fake_proc.returncode = 0

        captured_lines: list[str] = []
        code = stream_process_console(
            process=fake_proc,
            simulator_udid=_TEST_UDID,
            bundle_id=_TEST_BUNDLE_ID,
            line_callback=captured_lines.append,
        )
        self.assertEqual(code, 0)
        self.assertEqual(captured_lines, ['line 1\n', 'line 2\n'])
        fake_proc.wait.assert_called_once()

    @mock.patch('shared_test_utils.terminate_app')
    def test_stream_process_console_sigint(
        self, mock_terminate: mock.MagicMock
    ) -> None:
        """Tests SIGINT terminates the app and returns SIGINT_EXIT_CODE."""
        fake_proc = mock.MagicMock()
        fake_proc.stdout.readline.side_effect = KeyboardInterrupt

        code = stream_process_console(
            process=fake_proc,
            simulator_udid=_TEST_UDID,
            bundle_id=_TEST_BUNDLE_ID,
        )
        self.assertEqual(code, SIGINT_EXIT_CODE)
        mock_terminate.assert_called_once_with(_TEST_UDID, _TEST_BUNDLE_ID)
        fake_proc.terminate.assert_called_once()
        fake_proc.wait.assert_called_once()


class TestSimulatorBackendBase(unittest.TestCase):
    """Tests `SimulatorBackend` default implementation."""

    def setUp(self) -> None:
        self.backend = SimulatorBackend()

    @mock.patch('subprocess.check_output')
    def test_list_simulators_success(
        self, mock_check_output: mock.MagicMock
    ) -> None:
        """Tests that `list_simulators` parses iOS devices from JSON."""
        mock_check_output.return_value = _SAMPLE_JSON_SIMCTL_OUTPUT
        simulators = self.backend.list_simulators()

        self.assertEqual(len(simulators), 1)
        sim = simulators[0]
        self.assertEqual(sim.name, 'iPhone 15 Pro')
        self.assertEqual(sim.udid, _TEST_UDID)
        self.assertEqual(sim.os_version, '17.5')
        self.assertEqual(sim.state, 'Booted')
        self.assertTrue(sim.is_available)

    @mock.patch('subprocess.check_output')
    def test_list_simulators_error(
        self, mock_check_output: mock.MagicMock
    ) -> None:
        """Tests error recovery when `simctl` command fails."""
        mock_check_output.side_effect = subprocess.CalledProcessError(1, 'cmd')
        self.assertEqual(self.backend.list_simulators(), [])

    def test_build_boot_command(self) -> None:
        """Tests `build_boot_command` output."""
        cmd = self.backend.build_boot_command(_TEST_UDID)
        self.assertEqual(cmd, ['xcrun', 'simctl', 'boot', _TEST_UDID])

    def test_build_uninstall_command(self) -> None:
        """Tests `build_uninstall_command` output."""
        cmd = self.backend.build_uninstall_command(_TEST_UDID, _TEST_BUNDLE_ID)
        self.assertEqual(
            cmd, ['xcrun', 'simctl', 'uninstall', _TEST_UDID, _TEST_BUNDLE_ID]
        )

    def test_build_install_command(self) -> None:
        """Tests `build_install_command` output."""
        cmd = self.backend.build_install_command(_TEST_UDID, _TEST_APP_PATH)
        self.assertEqual(
            cmd, ['xcrun', 'simctl', 'install', _TEST_UDID, _TEST_APP_PATH]
        )

    def test_build_terminate_command(self) -> None:
        """Tests `build_terminate_command` output."""
        cmd = self.backend.build_terminate_command(_TEST_UDID, _TEST_BUNDLE_ID)
        self.assertEqual(
            cmd, ['xcrun', 'simctl', 'terminate', _TEST_UDID, _TEST_BUNDLE_ID]
        )

    def test_build_launch_command(self) -> None:
        """Tests `build_launch_command` output with and without arguments."""
        cmd = self.backend.build_launch_command(_TEST_UDID, _TEST_BUNDLE_ID)
        self.assertEqual(
            cmd,
            [
                'xcrun',
                'simctl',
                'launch',
                '--console-pty',
                _TEST_UDID,
                _TEST_BUNDLE_ID,
            ],
        )

        cmd_with_args = self.backend.build_launch_command(
            _TEST_UDID, _TEST_BUNDLE_ID, ['--flag1', '--flag2']
        )
        self.assertEqual(
            cmd_with_args,
            [
                'xcrun',
                'simctl',
                'launch',
                '--console-pty',
                _TEST_UDID,
                _TEST_BUNDLE_ID,
                '--flag1',
                '--flag2',
            ],
        )

    def test_prepare_egtests(self) -> None:
        """Tests base `prepare_egtests` hook returns True."""
        self.assertTrue(
            self.backend.prepare_egtests(
                _TEST_OUT_DIR, _TEST_UDID, _TEST_SCHEME
            )
        )

    def test_build_egtests_command(self) -> None:
        """Tests `build_egtests_command` constructs `xcodebuild` invocation."""
        cmd = self.backend.build_egtests_command(
            out_dir=_TEST_OUT_DIR,
            simulator_udid=_TEST_UDID,
            arch=_TEST_ARCH,
            scheme=_TEST_SCHEME,
            test_filters=['TestCase/testOne'],
            extra_test_args=['-test-iterations', '2'],
        )
        self.assertIn('xcodebuild', cmd)
        self.assertIn('test-without-building', cmd)
        self.assertIn(f'-only-testing:{_TEST_SCHEME}/TestCase/testOne', cmd)
        self.assertIn('-test-iterations', cmd)
        self.assertIn('2', cmd)

    def test_build_log_stream_command(self) -> None:
        """Tests that `build_log_stream_command` produces stream command."""
        cmd = self.backend.build_log_stream_command(_TEST_UDID, _TEST_PREDICATE)
        self.assertEqual(
            cmd,
            [
                'xcrun',
                'simctl',
                'spawn',
                _TEST_UDID,
                'log',
                'stream',
                '--level',
                'debug',
                '--style',
                'syslog',
                '--predicate',
                _TEST_PREDICATE,
            ],
        )


class TestTopLevelExecution(unittest.TestCase):
    """Tests top-level wrapper functions delegating to active backend."""

    def setUp(self) -> None:
        set_simulator_backend(SimulatorBackend())

    def tearDown(self) -> None:
        set_simulator_backend(None)

    def test_list_simulators_delegates(self) -> None:
        """Tests `list_simulators` delegates to active backend."""
        mock_backend = mock.MagicMock(spec=SimulatorBackend)
        mock_backend.list_simulators.return_value = []
        set_simulator_backend(mock_backend)

        self.assertEqual(list_simulators(), [])
        mock_backend.list_simulators.assert_called_once()

    @mock.patch('subprocess.check_call')
    def test_boot_simulator(self, mock_check_call: mock.MagicMock) -> None:
        """Tests `boot_simulator` executes command from backend."""
        mock_check_call.return_value = 0
        self.assertTrue(boot_simulator(_TEST_UDID))
        mock_check_call.assert_called_once_with(
            ['xcrun', 'simctl', 'boot', _TEST_UDID]
        )

        mock_check_call.side_effect = subprocess.CalledProcessError(1, 'cmd')
        self.assertFalse(boot_simulator(_TEST_UDID))

    @mock.patch('subprocess.run')
    def test_uninstall_app(self, mock_run: mock.MagicMock) -> None:
        """Tests `uninstall_app` executes command from backend."""
        uninstall_app(_TEST_UDID, _TEST_BUNDLE_ID)
        mock_run.assert_called_once_with(
            ['xcrun', 'simctl', 'uninstall', _TEST_UDID, _TEST_BUNDLE_ID],
            check=False,
        )

    @mock.patch('subprocess.check_call')
    def test_install_app(self, mock_check_call: mock.MagicMock) -> None:
        """Tests `install_app` executes command from backend."""
        install_app(_TEST_UDID, _TEST_APP_PATH)
        mock_check_call.assert_called_once_with(
            ['xcrun', 'simctl', 'install', _TEST_UDID, _TEST_APP_PATH]
        )

    @mock.patch('subprocess.run')
    def test_terminate_app(self, mock_run: mock.MagicMock) -> None:
        """Tests `terminate_app` executes command from backend."""
        terminate_app(_TEST_UDID, _TEST_BUNDLE_ID)
        mock_run.assert_called_once_with(
            ['xcrun', 'simctl', 'terminate', _TEST_UDID, _TEST_BUNDLE_ID],
            capture_output=True,
            check=False,
        )

        mock_backend = mock.MagicMock(spec=SimulatorBackend)
        mock_backend.build_terminate_command.return_value = None
        set_simulator_backend(mock_backend)
        mock_run.reset_mock()
        terminate_app(_TEST_UDID, _TEST_BUNDLE_ID)
        mock_run.assert_not_called()

    @mock.patch('shared_test_utils.stream_process_console')
    @mock.patch('subprocess.Popen')
    def test_launch_app_with_console(
        self, mock_popen: mock.MagicMock, mock_stream: mock.MagicMock
    ) -> None:
        """Tests `launch_app_with_console` spawns process and streams output."""
        fake_proc = mock.MagicMock()
        mock_popen.return_value = fake_proc
        mock_stream.return_value = 0

        code = launch_app_with_console(
            simulator_udid=_TEST_UDID,
            bundle_id=_TEST_BUNDLE_ID,
            app_args=['--arg'],
        )
        self.assertEqual(code, 0)
        mock_popen.assert_called_once()
        mock_stream.assert_called_once()

    @mock.patch('time.sleep')
    @mock.patch('subprocess.check_output', return_value='9999\n')
    @mock.patch('shared_test_utils.stream_process_console')
    @mock.patch('subprocess.Popen')
    def test_launch_app_with_console_with_app_name(
        self,
        mock_popen: mock.MagicMock,
        mock_stream: mock.MagicMock,
        mock_check_output: mock.MagicMock,
        mock_sleep: mock.MagicMock,
    ) -> None:
        """Tests on_app_launched pgrep hook when app_name is provided."""
        fake_proc = mock.MagicMock()
        fake_proc.pid = 1234
        mock_popen.return_value = fake_proc
        mock_stream.return_value = 0

        code = launch_app_with_console(
            simulator_udid=_TEST_UDID,
            bundle_id=_TEST_BUNDLE_ID,
            app_name=_TEST_APP_NAME,
        )
        self.assertEqual(code, 0)
        mock_sleep.assert_called_once()
        mock_check_output.assert_called_once()
        pgrep_args = mock_check_output.call_args[0][0]
        self.assertEqual(pgrep_args[0], 'pgrep')

    @mock.patch('time.sleep')
    @mock.patch(
        'subprocess.check_output',
        side_effect=subprocess.CalledProcessError(1, 'pgrep'),
    )
    @mock.patch('shared_test_utils.stream_process_console')
    @mock.patch('subprocess.Popen')
    def test_launch_app_with_console_pgrep_error_handled(
        self,
        mock_popen: mock.MagicMock,
        mock_stream: mock.MagicMock,
        mock_check_output: mock.MagicMock,
        mock_sleep: mock.MagicMock,
    ) -> None:
        """Tests that on_app_launched handles CalledProcessError gracefully."""
        fake_proc = mock.MagicMock()
        fake_proc.pid = 1234
        mock_popen.return_value = fake_proc
        mock_stream.return_value = 0

        code = launch_app_with_console(
            simulator_udid=_TEST_UDID,
            bundle_id=_TEST_BUNDLE_ID,
            app_name=_TEST_APP_NAME,
        )
        self.assertEqual(code, 0)

    @mock.patch('subprocess.call')
    def test_launch_egtests(self, mock_call: mock.MagicMock) -> None:
        """Tests `launch_egtests` prepares state and executes test runner."""
        mock_call.return_value = 0
        code = launch_egtests(
            out_dir=_TEST_OUT_DIR,
            simulator_udid=_TEST_UDID,
            arch=_TEST_ARCH,
            scheme=_TEST_SCHEME,
            test_filters=['TestCase/test1'],
            test_iterations=3,
            retry_tests_on_failure=True,
            run_tests_until_failure=True,
            test_repetition_relaunch_enabled=True,
        )
        self.assertEqual(code, 0)
        mock_call.assert_called_once()
        cmd = mock_call.call_args[0][0]
        self.assertIn('-test-iterations', cmd)
        self.assertIn('3', cmd)
        self.assertIn('-retry-tests-on-failure', cmd)
        self.assertIn('-run-tests-until-failure', cmd)
        self.assertIn('-test-repetition-relaunch-enabled', cmd)
        self.assertIn('YES', cmd)

    @mock.patch.object(SimulatorBackend, 'prepare_egtests', return_value=False)
    def test_launch_egtests_prepare_failure(
        self, mock_prepare: mock.MagicMock
    ) -> None:
        """Tests `launch_egtests` returns 1 when `prepare_egtests` fails."""
        code = launch_egtests(
            out_dir=_TEST_OUT_DIR,
            simulator_udid=_TEST_UDID,
            arch=_TEST_ARCH,
            scheme=_TEST_SCHEME,
            test_filters=[],
        )
        self.assertEqual(code, 1)

    @mock.patch('subprocess.call', side_effect=KeyboardInterrupt)
    def test_launch_egtests_sigint(self, mock_call: mock.MagicMock) -> None:
        """Tests that `launch_egtests` handles SIGINT and returns exit code."""
        code = launch_egtests(
            out_dir=_TEST_OUT_DIR,
            simulator_udid=_TEST_UDID,
            arch=_TEST_ARCH,
            scheme=_TEST_SCHEME,
            test_filters=[],
        )
        self.assertEqual(code, SIGINT_EXIT_CODE)

    def test_build_log_stream_command_delegates(self) -> None:
        """Tests `build_log_stream_command` delegates to backend."""
        cmd = build_log_stream_command(_TEST_UDID, _TEST_PREDICATE)
        self.assertIn('stream', cmd)
        self.assertIn(_TEST_PREDICATE, cmd)


class TestPluginLoading(unittest.TestCase):
    """Tests dynamic backend plugin loading and fallbacks."""

    def setUp(self) -> None:
        set_simulator_backend(None)

    def tearDown(self) -> None:
        set_simulator_backend(None)

    @mock.patch('os.path.isfile', return_value=False)
    def test_get_simulator_backend_defaults_when_plugin_missing(
        self, mock_isfile: mock.MagicMock
    ) -> None:
        """Tests fallback to `SimulatorBackend` when plugin is missing."""
        backend = get_simulator_backend()
        self.assertIsInstance(backend, SimulatorBackend)

    @mock.patch('shared_test_utils._load_internal_simulator_backend')
    def test_get_simulator_backend_falls_back_when_plugin_returns_none(
        self, mock_load: mock.MagicMock
    ) -> None:
        """Tests fallback when internal plugin loader returns None."""
        mock_load.return_value = None
        backend = get_simulator_backend()
        self.assertIsInstance(backend, SimulatorBackend)

    def test_set_simulator_backend_override(self) -> None:
        """Tests manual override via `set_simulator_backend`."""
        custom_backend = SimulatorBackend()
        set_simulator_backend(custom_backend)
        self.assertIs(get_simulator_backend(), custom_backend)


class TestSimulatorManagerAndDeviceSelection(unittest.TestCase):
    """Tests simulator manager device lookup and selection."""

    def setUp(self) -> None:
        set_simulator_backend(SimulatorBackend())

    def tearDown(self) -> None:
        set_simulator_backend(None)

    @mock.patch.object(SimulatorBackend, 'list_simulators')
    def test_find_and_boot_simulator(self, mock_list: mock.MagicMock) -> None:
        """Tests finding and booting device via manager."""
        sim = Simulator(
            name='iPhone 17 Pro',
            udid=_TEST_UDID,
            os_version='18.0',
            state='Shutdown',
            is_available=True,
        )
        mock_list.return_value = [sim]

        with mock.patch('shared_test_utils.boot_simulator', return_value=True):
            chosen = find_and_boot_simulator('iPhone 17 Pro', '18.0')
            self.assertIsNotNone(chosen)
            self.assertEqual(chosen.udid, _TEST_UDID)

    @mock.patch.object(SimulatorBackend, 'list_simulators')
    def test_find_best_available_device_prefers_workspace(
        self, mock_list: mock.MagicMock
    ) -> None:
        """Tests that find_best_available_device prefers workspace prefix."""
        ws_id = get_workspace_id()
        sim_unprefixed = Simulator(
            name='iPhone 16',
            udid='UDID-UNPREFIXED',
            os_version='18.0',
            state='Shutdown',
            is_available=True,
        )
        sim_prefixed = Simulator(
            name=f'[{ws_id}] iPhone 16',
            udid='UDID-PREFIXED',
            os_version='18.0',
            state='Shutdown',
            is_available=True,
        )
        sim_other = Simulator(
            name='[other] iPhone 16',
            udid='UDID-OTHER',
            os_version='18.0',
            state='Shutdown',
            is_available=True,
        )
        mock_list.return_value = [sim_unprefixed, sim_prefixed, sim_other]

        manager = SimulatorManager()
        best = manager.find_best_available_device()
        self.assertIsNotNone(best)
        self.assertEqual(best.udid, 'UDID-PREFIXED')

    @mock.patch.object(SimulatorBackend, 'list_simulators')
    def test_find_device_by_type_and_version_prefers_workspace(
        self, mock_list: mock.MagicMock
    ) -> None:
        """Tests find_device_by_type_and_version prefers workspace prefix."""
        ws_id = get_workspace_id()
        sim_unprefixed = Simulator(
            name='iPhone 16 Pro',
            udid='UDID-1',
            os_version='18.0',
            state='Shutdown',
            is_available=True,
        )
        sim_prefixed = Simulator(
            name=f'[{ws_id}] iPhone 16 Pro',
            udid='UDID-2',
            os_version='18.0',
            state='Shutdown',
            is_available=True,
        )
        mock_list.return_value = [sim_unprefixed, sim_prefixed]

        manager = SimulatorManager()
        chosen = manager.find_device_by_type_and_version(
            'iPhone 16 Pro', '18.0'
        )
        self.assertIsNotNone(chosen)
        self.assertEqual(chosen.udid, 'UDID-2')


if __name__ == '__main__':
    unittest.main()
