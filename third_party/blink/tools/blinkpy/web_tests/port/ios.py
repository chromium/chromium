# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium iOS implementation of the Port interface."""

import logging
import socket

from blinkpy.web_tests.port.ios_simulator_server_process import IOSSimulatorServerProcess
from blinkpy.web_tests.port import base
from blinkpy.web_tests.port import driver

_log = logging.getLogger(__name__)


class IOSPort(base.Port):
    SUPPORTED_VERSIONS = ('ios16-simulator', )

    port_name = 'ios'

    FALLBACK_PATHS = {}

    FALLBACK_PATHS['ios16-simulator'] = ['ios']

    CONTENT_SHELL_NAME = 'content_shell'

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/main/docs/ios/build_instructions.md'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('ios'):
            parts = [port_name, 'ios16-simulator']
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

    def cmd_line(self):
        return [
            self._path_to_simulator(), '-d',
            self.device_name(), '-c',
            '%s -' % self.additional_driver_flags()
        ]

    def reinstall_cmd_line(self):
        return [
            self._path_to_simulator(), '-d',
            self.device_name(), '-c', '--prepare-web-tests',
            self.path_to_driver()
        ]

    def _path_to_simulator(self, target=None):
        return self.build_path('iossim', target=target)

    def path_to_driver(self, target=None):
        return self.build_path(self.driver_name() + '.app', target=target)

    def device_name(self, target=None):
        return 'iPhone 13'

    def _driver_class(self):
        return ChromiumIOSDriver

    def _get_available_port(self):
        # TODO(gyuyoung): Can we get a port in the iOS server process that it
        # really binds to a socket?
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(('localhost', 0))
        port = int(s.getsockname()[1])
        return port

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
        flags += ['--no-sandbox']
        stdio_redirect_flag = '--stdio-redirect=127.0.0.1:' + str(
            self._stdio_redirect_port)
        flags += [stdio_redirect_flag]
        return " ".join(flags)

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


class ChromiumIOSDriver(driver.Driver):
    def __init__(self, port, worker_number, no_timeout=False):
        super(ChromiumIOSDriver, self).__init__(port, worker_number,
                                                no_timeout)

    def cmd_line(self, per_test_args):
        cmd = self._port.cmd_line()
        cmd += self._base_cmd_line()
        return cmd
