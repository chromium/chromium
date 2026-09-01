# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium tvOS implementation of the Port interface."""

import json
import logging
import socket
import time

from blinkpy.web_tests.port.tvos_simulator_server_process import TVOSSimulatorServerProcess
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import mac

_log = logging.getLogger(__name__)

BOOT_STATE = 'Booted'
DEFAULT_SDK_VERSION = '26.0'


class TVOSPort(base.Port):
    SUPPORTED_VERSIONS = ('tvos26-simulator', )

    port_name = 'tvos'

    runtime_version = ''

    FALLBACK_PATHS = {}

    FALLBACK_PATHS['tvos26-simulator'] = (
        ['tvos'] + mac.MacPort.latest_platform_fallback_path())

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/main/docs/ios/build_instructions.md#Blink-for-tvOS-builds-and-running'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('tvos'):
            parts = [port_name, 'tvos26-simulator']
            return '-'.join(parts)
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(TVOSPort, self).__init__(host, port_name, **kwargs)
        self.server_process_constructor = TVOSSimulatorServerProcess
        self._version = port_name[port_name.index('tvos-') + len('tvos-'):]
        self._stdio_redirect_port = self._get_available_port()

    def check_build(self, needs_http, printer):
        result = super(TVOSPort, self).check_build(needs_http, printer)
        if result:
            _log.error('For complete tvos build requirements, please see:')
            _log.error('')
            _log.error(self.BUILD_REQUIREMENTS_URL)

        return result

    def reinstall_cmd_line(self):
        return [
            self.path_to_simulator(), '-d',
            self.device_name(), '-x', 'tvos', '-s',
            self.sdk_version(), '-k', 'never', '-c', '--prepare-web-tests',
            self.path_to_driver()
        ]

    def path_to_driver(self, target=None):
        return self.build_path(self.driver_name() + '.app', target=target)

    def path_to_simulator(self, target=None):
        return self.build_path('iossim', target=target)

    def device_name(self, target=None):
        return 'Apple TV 4K'

    def sdk_version(self, target=None):
        if len(self.runtime_version) != 0:
            return self.runtime_version

        # Use the default sdk version for testing.
        if self._is_testing():
            return DEFAULT_SDK_VERSION

        self.runtime_version = self._get_target_runtime()['version']
        return self.runtime_version

    def _driver_class(self):
        return ChromiumTVOSDriver

    def _get_available_port(self):
        # TODO(gyuyoung): Can we get a port in the tvOS server process that it
        # really binds to a socket?
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('localhost', 0))
        port = int(s.getsockname()[1])
        return port

    def _get_device(self, device_name):
        devices = json.loads(self._run_simctl('list -j devices available'))
        if len(devices) == 0:
            raise RuntimeError('No available device in the tvOS simulator.')
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
            raise RuntimeError('No valid runtime in the tvOS simulator.')

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
        return 'tvos'

    def num_workers(self, requested_num_workers):
        # Only support a single worker because the tvOS simulator is not able to
        # run multiple instances of the same application at the same time. And,
        # we do not support running multiple simulators for testing yet.
        return min(1, requested_num_workers)

    def additional_driver_flags(self):
        flags = super(TVOSPort, self).additional_driver_flags()
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
        super(TVOSPort, self).setup_test_run()
        # Because the tests are being run on a simulator rather than directly on
        # this device, re-deploy the content shell app to the simulator to
        # ensure it is up to date.
        self.host.executive.run_command(self.reinstall_cmd_line())

    def get_platform_tags(self):
        # tvOS is an iOS subset, so also match iOS tags to inherit
        # IOSTestExpectations.
        return super(TVOSPort, self).get_platform_tags() | {'ios26-simulator'}

    def used_expectations_files(self):
        files = super(TVOSPort, self).used_expectations_files()
        ios_expectations_file = self._filesystem.join(self.web_tests_dir(),
                                                      'IOSTestExpectations')
        tvos_additional_expectations_files = self._filesystem.join(
            self.web_tests_dir(), 'TVOSTestExpectations')
        files.append(ios_expectations_file)
        files.append(tvos_additional_expectations_files)
        return files


class ChromiumTVOSDriver(driver.Driver):

    def __init__(self, port, worker_number, no_timeout=False):
        super(ChromiumTVOSDriver, self).__init__(port, worker_number,
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
            '-x',
            'tvos',
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
