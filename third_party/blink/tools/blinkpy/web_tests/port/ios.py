# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium iOS implementation of the Port interface."""

import logging

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

    BUILD_REQUIREMENTS_URL = 'https://chromium.googlesource.com/chromium/src/+/main/docs/ios_build_instructions.md'

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

    def check_build(self, needs_http, printer):
        result = super(IOSPort, self).check_build(needs_http, printer)
        if result:
            _log.error('For complete ios build requirements, please see:')
            _log.error('')
            _log.error(BUILD_REQUIREMENTS_URL)

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

    #
    # PROTECTED METHODS
    #

    def operating_system(self):
        return 'ios'

    def additional_driver_flags(self):
        flags = (
            '--run-web-tests --ignore-certificate-errors-spki-list=%s,%s,%s --webtransport-developer-mode --user-data-dir') % \
            (base.WPT_FINGERPRINT, base.SXG_FINGERPRINT, base.SXG_WPT_FINGERPRINT)
        return flags

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
        # Because the tests are being run on a simulator rather than directly on this
        # device, re-deploy the content shell app to the simulator to ensure it is up
        # to date.
        self.host.executive.run_command(self.reinstall_cmd_line())


class ChromiumIOSDriver(driver.Driver):
    def __init__(self, port, worker_number, no_timeout=False):
        super(ChromiumIOSDriver, self).__init__(port, worker_number,
                                                no_timeout)

    def cmd_line(self, per_test_args):
        cmd = self._port.cmd_line()
        cmd += self._base_cmd_line()
        return cmd
