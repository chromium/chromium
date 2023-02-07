# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# pylint: disable=protected-access

import optparse
import os
import sys
import time
import unittest

from blinkpy.common.path_finder import get_chromium_src_dir
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.web_tests.port import android
from blinkpy.web_tests.port import driver_unittest
from blinkpy.web_tests.port import port_testcase
from blinkpy.web_tests.models.test_expectations import TestExpectations

_DEVIL_ROOT = os.path.join(get_chromium_src_dir(), 'third_party', 'catapult',
                           'devil')
sys.path.insert(0, _DEVIL_ROOT)
from devil.android import device_utils
from devil.android.sdk import adb_wrapper

import mock


def mock_devices():
    serials = [
        '123456789ABCDEF0', '123456789ABCDEF1', '123456789ABCDEF2',
        '123456789ABCDEF3', '123456789ABCDEF4', '123456789ABCDEF5'
    ]
    devices = []
    for serial in serials:
        mock_device = mock.Mock(spec=device_utils.DeviceUtils)
        mock_device.adb = mock.Mock(spec=adb_wrapper.AdbWrapper)
        mock_device.adb.GetAdbPath.return_value = 'adb'
        mock_device.adb.GetDeviceSerial.return_value = serial
        type(mock_device).serial = mock.PropertyMock(return_value=serial)
        devices.append(mock_device)
    return devices


class AndroidPortTest(port_testcase.PortTestCase):
    port_name = 'android'
    port_maker = android.AndroidPort

    def setUp(self):
        super(AndroidPortTest, self).setUp()
        self._mock_devices = mock.patch(
            'devil.android.device_utils.DeviceUtils.HealthyDevices',
            return_value=mock_devices())
        self._mock_devices.start()

        self._mock_battery = mock.patch(
            'devil.android.battery_utils.BatteryUtils.GetBatteryInfo',
            return_value={'level': '100'})
        self._mock_battery.start()

        self._mock_perf_control = mock.patch(
            'devil.android.perf.perf_control.PerfControl')
        self._mock_perf_control.start()

    def tearDown(self):
        super(AndroidPortTest, self).tearDown()
        self._mock_devices.stop()
        self._mock_battery.stop()
        self._mock_perf_control.stop()

    def test_check_build(self):
        host = MockSystemHost()
        port = self.make_port(
            host=host, options=optparse.Values({
                'child_processes': 1
            }))
        # Checking the devices is not tested in this unit test.
        port._check_devices = lambda _: None
        host.filesystem.exists = lambda p: True
        port.check_build(needs_http=True, printer=port_testcase.FakePrinter())

    def test_check_sys_deps(self):
        # FIXME: Do something useful here, but testing the full logic would be hard.
        pass

    # Test that the number of child processes to create depends on the devices.
    def test_default_child_processes(self):
        port_default = self.make_port(device_count=5)
        port_fixed_device = self.make_port(
            device_count=5,
            options=optparse.Values({
                'adb_devices': ['123456789ABCDEF9']
            }))

        self.assertEqual(6, port_default.default_child_processes())
        self.assertEqual(1, port_fixed_device.default_child_processes())

    def test_no_bot_expectations_searched(self):
        # We don't support bot expectations at the moment
        host = MockSystemHost()
        port = android.AndroidPort(host, apk='apks/WebLayerShell.apk')
        port.expectations_dict = lambda: {}
        test_expectations = TestExpectations(port)
        self.assertFalse(test_expectations._expectations)

    def test_weblayer_expectation_tags(self):
        host = MockSystemHost()
        port = android.AndroidPort(
            host, product='android_weblayer')
        self.assertEqual(port.get_platform_tags(),
                         set(['android', 'android-weblayer']))

    def test_default_no_wpt_product_tag(self):
        host = MockSystemHost()
        port = android.AndroidPort(host)
        self.assertEqual(port.get_platform_tags(),
                         set(['android']))

    # Test that an HTTP server indeed is required by Android (as we serve all tests over them)
    def test_requires_http_server(self):
        self.assertTrue(self.make_port(device_count=1).requires_http_server())

    # Tests the default timeouts for Android, which are different than the rest of Chromium.
    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port().timeout_ms(), 10000)

    def test_path_to_apache_config_file(self):
        port = self.make_port()
        port._local_port.path_to_apache_config_file = lambda: '/host/apache/conf'  # pylint: disable=protected-access
        self.assertEqual(port.path_to_apache_config_file(),
                         '/host/apache/conf')


class ChromiumAndroidDriverTest(unittest.TestCase):
    def setUp(self):
        self._mock_devices = mock.patch(
            'devil.android.device_utils.DeviceUtils.HealthyDevices',
            return_value=mock_devices())
        self._mock_devices.start()

        self._mock_battery = mock.patch(
            'devil.android.battery_utils.BatteryUtils.GetBatteryInfo',
            return_value={'level': '100'})
        self._mock_battery.start()

        self._mock_perf_control = mock.patch(
            'devil.android.perf.perf_control.PerfControl')
        self._mock_perf_control.start()

        self._port = android.AndroidPort(
            MockSystemHost(executive=MockExecutive()), 'android')
        self._driver = android.ChromiumAndroidDriver(
            self._port,
            worker_number=0,
            driver_details=android.ContentShellDriverDetails(),
            android_devices=self._port._devices)  # pylint: disable=protected-access

    def tearDown(self):
        # Make ChromiumAndroidDriver.__del__ run before we stop the mocks.
        del self._driver
        self._mock_battery.stop()
        self._mock_devices.stop()
        self._mock_perf_control.stop()

    # The cmd_line() method in the Android port is used for starting a shell, not the test runner.
    def test_cmd_line(self):
        self.assertEqual(['adb', '-s', '123456789ABCDEF0', 'shell'],
                         self._driver.cmd_line([]))

    # Test that the Chromium Android port can interpret Android's shell output.
    def test_read_prompt(self):
        self._driver._server_process = driver_unittest.MockServerProcess(
            lines=['root@android:/ # '])
        self.assertIsNone(self._driver._read_prompt(time.time() + 1))
        self._driver._server_process = driver_unittest.MockServerProcess(
            lines=['$ '])
        self.assertIsNone(self._driver._read_prompt(time.time() + 1))


class ChromiumAndroidDriverTwoDriversTest(unittest.TestCase):
    # Test two drivers getting the right serial numbers, and that we disregard per-test arguments.

    def setUp(self):
        self._mock_devices = mock.patch(
            'devil.android.device_utils.DeviceUtils.HealthyDevices',
            return_value=mock_devices())
        self._mock_devices.start()

        self._mock_battery = mock.patch(
            'devil.android.battery_utils.BatteryUtils.GetBatteryInfo',
            return_value={'level': '100'})
        self._mock_battery.start()

        self._mock_perf_control = mock.patch(
            'devil.android.perf.perf_control.PerfControl')
        self._mock_perf_control.start()

    def tearDown(self):
        self._mock_battery.stop()
        self._mock_devices.stop()
        self._mock_perf_control.stop()

    def test_two_drivers(self):
        port = android.AndroidPort(
            MockSystemHost(executive=MockExecutive()), 'android')
        driver0 = android.ChromiumAndroidDriver(
            port,
            worker_number=0,
            driver_details=android.ContentShellDriverDetails(),
            android_devices=port._devices)
        driver1 = android.ChromiumAndroidDriver(
            port,
            worker_number=1,
            driver_details=android.ContentShellDriverDetails(),
            android_devices=port._devices)

        self.assertEqual(['adb', '-s', '123456789ABCDEF0', 'shell'],
                         driver0.cmd_line([]))
        self.assertEqual(['adb', '-s', '123456789ABCDEF1', 'shell'],
                         driver1.cmd_line(['anything']))


class ChromiumAndroidTwoPortsTest(unittest.TestCase):
    # Test that the driver's command line indeed goes through to the driver.

    def setUp(self):
        self._mock_devices = mock.patch(
            'devil.android.device_utils.DeviceUtils.HealthyDevices',
            return_value=mock_devices())
        self._mock_devices.start()

        self._mock_battery = mock.patch(
            'devil.android.battery_utils.BatteryUtils.GetBatteryInfo',
            return_value={'level': '100'})
        self._mock_battery.start()

        self._mock_perf_control = mock.patch(
            'devil.android.perf.perf_control.PerfControl')
        self._mock_perf_control.start()

    def tearDown(self):
        self._mock_battery.stop()
        self._mock_devices.stop()
        self._mock_perf_control.stop()

    def test_options_with_two_ports(self):
        port0 = android.AndroidPort(
            MockSystemHost(executive=MockExecutive()),
            'android',
            options=optparse.Values({
                'additional_driver_flag': ['--foo=bar']
            }))
        port1 = android.AndroidPort(
            MockSystemHost(executive=MockExecutive()),
            'android',
            options=optparse.Values({
                'driver_name': 'content_shell'
            }))

        self.assertEqual(1, port0.driver_cmd_line().count('--foo=bar'))
        self.assertEqual(0,
                         port1.driver_cmd_line().count('--create-stdin-fifo'))


class ChromiumAndroidDriverTombstoneTest(unittest.TestCase):
    EXPECTED_STACKTRACE = '-rw------- 1000 1000 3604 2013-11-19 16:16 tombstone_10\ntombstone content'

    def setUp(self):
        self._mock_devices = mock.patch(
            'devil.android.device_utils.DeviceUtils.HealthyDevices',
            return_value=mock_devices())
        self._mock_devices.start()

        self._mock_battery = mock.patch(
            'devil.android.battery_utils.BatteryUtils.GetBatteryInfo',
            return_value={'level': '100'})
        self._mock_battery.start()

        self._port = android.AndroidPort(
            MockSystemHost(executive=MockExecutive()), 'android')
        self._driver = android.ChromiumAndroidDriver(
            self._port,
            worker_number=0,
            driver_details=android.ContentShellDriverDetails(),
            android_devices=self._port._devices)  # pylint: disable=protected-access

        self._errors = []
        self._driver._log_error = lambda msg: self._errors.append(msg)

        self._warnings = []
        self._driver._log_warning = lambda msg: self._warnings.append(msg)

    def tearDown(self):
        self._mock_battery.stop()
        self._mock_devices.stop()

    # Tests that we return an empty string and log an error when no tombstones could be found.
    def test_no_tombstones_found(self):
        self._driver._device.RunShellCommand = mock.Mock(return_value=[
            '/data/tombstones/tombstone_*: No such file or directory'
        ])
        stacktrace = self._driver._get_last_stacktrace()

        self.assertEqual(1, len(self._errors))
        self.assertEqual(
            'The driver crashed, but we could not find any valid tombstone!',
            self._errors[0])
        self.assertEqual('', stacktrace)

    # Tests that an empty string will be returned if we cannot read the tombstone files.
    def test_insufficient_tombstone_permission(self):
        self._driver._device.RunShellCommand = mock.Mock(
            return_value=['/data/tombstones/tombstone_*: Permission denied'])
        stacktrace = self._driver._get_last_stacktrace()

        self.assertEqual(1, len(self._errors))
        self.assertEqual(
            'The driver crashed, but we could not find any valid tombstone!',
            self._errors[0])
        self.assertEqual('', stacktrace)

    # Tests that invalid "ls" output will throw a warning when listing the tombstone files.
    def test_invalid_tombstone_list_entry_format(self):
        self._driver._device.RunShellCommand = mock.Mock(return_value=[
            '-rw------- 1000 1000 3604 2013-11-19 16:15 tombstone_00',
            '-- invalid entry --',
            '-rw------- 1000 1000 3604 2013-11-19 16:16 tombstone_10'
        ])
        self._driver._device.ReadFile = mock.Mock(
            return_value='tombstone content')
        stacktrace = self._driver._get_last_stacktrace()

        self.assertEqual(1, len(self._warnings))
        self.assertEqual(
            ChromiumAndroidDriverTombstoneTest.EXPECTED_STACKTRACE, stacktrace)

    # Tests the case in which we can't find any valid tombstone entries at all. The tombstone
    # output used for the mock misses the permission part.
    def test_invalid_tombstone_list(self):
        self._driver._device.RunShellCommand = mock.Mock(return_value=[
            '1000 1000 3604 2013-11-19 16:15 tombstone_00',
            '1000 1000 3604 2013-11-19 16:15 tombstone_01',
            '1000 1000 3604 2013-11-19 16:15 tombstone_02'
        ])
        self._driver._device.ReadFile = mock.Mock(
            return_value='tombstone content')
        stacktrace = self._driver._get_last_stacktrace()

        self.assertEqual(3, len(self._warnings))
        self.assertEqual(1, len(self._errors))
        self.assertEqual(
            'The driver crashed, but we could not find any valid tombstone!',
            self._errors[0])
        self.assertEqual('', stacktrace)

    # Tests that valid tombstone listings will return the contents of the most recent file.
    def test_read_valid_tombstone_file(self):
        self._driver._device.RunShellCommand = mock.Mock(return_value=[
            '-rw------- 1000 1000 3604 2013-11-19 16:15 tombstone_00',
            '-rw------- 1000 1000 3604 2013-11-19 16:16 tombstone_10',
            '-rw------- 1000 1000 3604 2013-11-19 16:15 tombstone_02'
        ])
        self._driver._device.ReadFile = mock.Mock(
            return_value='tombstone content')
        stacktrace = self._driver._get_last_stacktrace()

        self.assertEqual(0, len(self._warnings))
        self.assertEqual(0, len(self._errors))
        self.assertEqual(
            ChromiumAndroidDriverTombstoneTest.EXPECTED_STACKTRACE, stacktrace)
