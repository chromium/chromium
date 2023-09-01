# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging

from blinkpy.web_tests.port.server_process import ServerProcess

_log = logging.getLogger(__name__)


# Define a custom version of ServerProcess for running tests on the iOS
# simulator. The default ServerProcess does not work as it uses
# stdin/stdout/stderr to communicate, which the iOS simulator does not
# allow for security.
#
# TODO(crbug.com/1421239): iOS port communicates with the content shell
# through a file handle temporarily. Socket connection should be used
# instead.
class IOSSimulatorServerProcess(ServerProcess):
    def __init__(self,
                 port_obj,
                 name,
                 cmd,
                 env=None,
                 treat_no_data_as_crash=False,
                 more_logging=False):
        super(IOSSimulatorServerProcess,
              self).__init__(port_obj, name, cmd, env, treat_no_data_as_crash,
                             more_logging)

        self._web_test_path_file = self._get_web_test_file_path()
        if not self._web_test_path_file:
            raise RuntimeError('_web_test_path_file does not exist.')

        # Create a file at the path.
        test_file_handle = open(self._web_test_path_file, 'wb')
        test_file_handle.close()

    def _get_web_test_file_path(self):
        simulator_root = self._host.filesystem.expanduser(
            "~/Library/Developer/CoreSimulator/Devices/")
        udid = self._get_simulator_udid()
        if not udid:
            raise RuntimeError('The udid value of the Simulator is none.')

        app_data_path = self._host.filesystem.join(
            simulator_root, udid, "data/Containers/Data/Application")

        content_shell_dir = self._get_content_shell_dir(app_data_path)
        if not content_shell_dir:
            _log.error('Cannot find the content shell directory.')
            return None

        content_shell_data_path = self._host.filesystem.join(
            app_data_path, content_shell_dir)
        content_shell_tmp_path = self._host.filesystem.join(
            content_shell_data_path, "tmp")
        if not self._host.filesystem.exists(content_shell_tmp_path):
            raise RuntimeError('%s path does not exist.' %
                               content_shell_tmp_path)

        return self._host.filesystem.join(content_shell_tmp_path,
                                          "webtest_test_name")

    def _get_content_shell_dir(self, app_data_path):
        for app_dir in self._host.filesystem.listdir(app_data_path):
            # Check if |app_dir| has the content shell directory.
            content_shell_dir = self._host.filesystem.join(
                app_data_path, app_dir,
                "Library/Application Support/Chromium Content Shell")
            if self._host.filesystem.exists(content_shell_dir):
                return app_dir
        return None

    def _get_simulator_udid(self):
        device = self._get_device(self._port.device_name())
        if not device:
            _log.error('There is no available device.')
            return None
        udid = device.get("udid")
        if not udid:
            _log.error('Cannot find the udid of the iOS simulator.')
            return None
        return udid

    def _get_device(self, device_name):
        devices = json.loads(self._simctl('devices available'))
        if len(devices) == 0:
            raise RuntimeError('No available device in the iOS simulator.')
        runtime = self._latest_runtime()
        return next(
            (d
             for d in devices['devices'][runtime] if d['name'] == device_name),
            None)

    def _latest_runtime(self):
        runtimes = json.loads(self._simctl('runtimes available'))
        valid_runtimes = [
            runtime for runtime in runtimes['runtimes']
            if 'identifier' in runtime and runtime['identifier'].startswith(
                'com.apple.CoreSimulator.SimRuntime')
        ]
        if len(valid_runtimes) == 0:
            raise RuntimeError('No valid runtime in the iOS simulator.')

        valid_runtimes.sort(key=lambda runtime: float(runtime['version']),
                            reverse=True)
        return valid_runtimes[0]['identifier']

    def _simctl(self, command):
        prefix_commands = ['/usr/bin/xcrun', 'simctl', 'list', '-j']
        command_array = prefix_commands + command.split()
        return self._host.executive.run_command(command_array)

    #
    # PROTECTED METHODS
    #

    def write(self, bytes):
        super().write(bytes)
        # iOS application can't communicate with the Mac host through stdin yet.
        # Instead a file stream is used for the communication temporarily.
        self._host.filesystem.write_binary_file(self._web_test_path_file,
                                                bytes)
