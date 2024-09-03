# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium iOS implementation of the Port interface."""

import json
import logging
import socket
import time

from blinkpy.web_tests.port.ios_simulator_server_process import IOSSimulatorServerProcess
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import mac

_log = logging.getLogger(__name__)

BOOT_STATE = 'Booted'
DEFAULT_SDK_VERSION = '17.4'


class IOSPort(base.Port):
    SUPPORTED_VERSIONS = ('ios17-simulator', )

    port_name = 'ios'

    runtime_version = ''

    FALLBACK_PATHS = {}

    FALLBACK_PATHS['ios17-simulator'] = (
        ['ios'] + mac.MacPort.latest_platform_fallback_path())

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/main/docs/ios/build_instructions.md'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('ios'):
            parts = [port_name, 'ios17-simulator']
            return '-'.join(parts)
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(IOSPort, self).__init__(host, port_name, **kwargs)
        self.server_process_constructor = IOSSimulatorServerProcess
        self._version = port_name[port_name.index('ios-') + len('ios-'):]
        self._stdio_redirect_port = self._get_available_port()

    def check_build(self, needs_http, printer):
        result = super(IOSPort, self).check_build(needs_http, printer)
        if result:
            _log.error('For complete ios build requirements, please see:')
            _log.error('')
            _log.error(self.BUILD_REQUIREMENTS_URL)

        return result

    def reinstall_cmd_line(self):
        return [
            self.path_to_simulator(), '-d',
            self.device_name(), '-s',
            self.sdk_version(), '-k', 'never', '-c', '--prepare-web-tests',
            self.path_to_driver()
        ]

    def path_to_driver(self, target=None):
        return self.build_path(self.driver_name() + '.app', target=target)

    def check_simulator_is_booted(self):
        device = self._get_device(self.device_name())
        state = device.get('state')
        if state != BOOT_STATE:
            _log.info('No simulator is booted. Booting a simulator...')
            udid = device.get('udid')
            self._run_simctl('boot ' + udid)

            while True:
                time.sleep(2)  # Wait for 2 seconds before checking the state.
                device = self._get_device(self.device_name())
                state = device.get('state')
                if state == BOOT_STATE:
                    break

    def path_to_simulator(self, target=None):
        return self.build_path('iossim', target=target)

    def device_name(self, target=None):
        return 'iPhone 13'

    def sdk_version(self, target=None):
        if len(self.runtime_version) != 0:
            return self.runtime_version

        # Use the default sdk version for testing.
        if self._is_testing():
            return DEFAULT_SDK_VERSION

        self.runtime_version = self._get_target_runtime()['version']
        return self.runtime_version

    def _driver_class(self):
        return ChromiumIOSDriver

    def _get_available_port(self):
        # TODO(gyuyoung): Can we get a port in the iOS server process that it
        # really binds to a socket?
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('localhost', 0))
        port = int(s.getsockname()[1])
        return port

    def _get_device(self, device_name):
        devices = json.loads(self._run_simctl('list -j devices available'))
        if len(devices) == 0:
            raise RuntimeError('No available device in the iOS simulator.')
        runtime_identifier = self._get_target_runtime()['identifier']
        return next((d for d in devices['devices'][runtime_identifier]
                     if d['name'] == device_name), None)

    def _get_target_runtime(self):
        valid_runtimes = self._get_valid_runtimes()
        # Check if the default SDK is installed on the testing environment.
        for runtime in valid_runtimes:
            if (runtime['version'] == DEFAULT_SDK_VERSION):
                return runtime

        # Sort valid runtimes to return the latest runtime.
        valid_runtimes.sort(key=lambda runtime: runtime['version'],
                            reverse=True)
        return valid_runtimes[0]

    def _get_valid_runtimes(self):
        runtimes = json.loads(self._run_simctl('list -j runtimes available'))
        valid_runtimes = [
            runtime for runtime in runtimes['runtimes']
            if 'identifier' in runtime and runtime['identifier'].startswith(
                'com.apple.CoreSimulator.SimRuntime')
        ]

        if len(valid_runtimes) == 0:
            raise RuntimeError('No valid runtime in the iOS simulator.')

        return valid_runtimes

    def _run_simctl(self, command):
        prefix_commands = ['/usr/bin/xcrun', 'simctl']
        command_array = prefix_commands + command.split()
        return self.host.executive.run_command(command_array)

    def _is_testing(self):
        runtimes = self._run_simctl('list -j runtimes available')
        return runtimes.startswith('MOCK output')

    #
    # PROTECTED METHODS
    #

    def operating_system(self):
        return 'ios'

    def num_workers(self, requested_num_workers):
        # Only support a single worker because the iOS simulator is not able to
        # run multiple instances of the same application at the same time. And,
        # we do not support running multiple simulators for testing yet.
        return min(1, requested_num_workers)

    def additional_driver_flags(self):
        flags = super(IOSPort, self).additional_driver_flags()
        flags += [
            '--no-sandbox',
            '--stdio-redirect=127.0.0.1:%s' % self._stdio_redirect_port
        ]
        return flags

    def stdio_redirect_port(self):
        return self._stdio_redirect_port

    def path_to_apache(self):
        import platform
        if platform.machine() == 'arm64':
            return self._path_from_chromium_base('third_party',
                                                 'apache-mac-arm64', 'bin',
                                                 'httpd')
        return self._path_from_chromium_base('third_party', 'apache-mac',
                                             'bin', 'httpd')

    def path_to_apache_config_file(self):
        config_file_basename = 'apache2-httpd-%s-php7.conf' % (
            self._apache_version(), )
        return self._filesystem.join(self.apache_config_directory(),
                                     config_file_basename)

    def setup_test_run(self):
        super(IOSPort, self).setup_test_run()
        # Because the tests are being run on a simulator rather than directly on
        # this device, re-deploy the content shell app to the simulator to
        # ensure it is up to date.
        self.host.executive.run_command(self.reinstall_cmd_line())

    def used_expectations_files(self):
        files = super(IOSPort, self).used_expectations_files()
        ios_additional_expectations_files = self._filesystem.join(
            self.web_tests_dir(), 'IOSTestExpectations')
        files.append(ios_additional_expectations_files)
        return files


class ChromiumIOSDriver(driver.Driver):
    def __init__(self, port, worker_number, no_timeout=False):
        super(ChromiumIOSDriver, self).__init__(port, worker_number,
                                                no_timeout)

    def _web_tests_driver_flags(self):
        flags = self._port.additional_driver_flags()
        flags += ['--run-web-tests']
        flags += ['--user-data-dir']
        return " ".join(flags)

    def _base_cmd_line(self):
        return [
            self._port.path_to_simulator(),
            '-d',
            self._port.device_name(),
            '-s',
            self._port.sdk_version(),
            '-k',
            'never',
            '-c',
            '%s -' % self._web_tests_driver_flags(),
            self._port.path_to_driver(),
        ]

    def cmd_line(self, per_test_args):
        cmd = self._base_cmd_line()
        return cmd
