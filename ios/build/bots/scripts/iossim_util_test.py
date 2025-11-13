#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for iossim_util.py."""

from copy import deepcopy
import plistlib
import mock
import unittest

import constants
import iossim_util
import mac_util
import os
import subprocess
import test_runner
import test_runner_test

SIMULATORS_LIST = {
    'devices': {
        'com.apple.CoreSimulator.SimRuntime.iOS-11-4': [{
            'isAvailable':
                True,
            'name':
                'iPhone 5s 11.4 test simulator',
            'deviceTypeIdentifier':
                'com.apple.CoreSimulator.SimDeviceType.iPhone-5s',
            'state':
                'Shutdown',
            'udid':
                'E4E66320-177A-450A-9BA1-488D85B7278E'
        }],
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2': [{
            'isAvailable':
                True,
            'name':
                'iPhone X 13.2.2 test simulator',
            'deviceTypeIdentifier':
                'com.apple.CoreSimulator.SimDeviceType.iPhone-X',
            'state':
                'Shutdown',
            'udid':
                'E4E66321-177A-450A-9BA1-488D85B7278E'
        }, {
            'isAvailable':
                True,
            'name':
                'iPhone 11 13.2.2 test simulator',
            'deviceTypeIdentifier':
                'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
            'state':
                'Shutdown',
            'udid':
                'A4E66321-177A-450A-9BA1-488D85B7278E'
        }],
        'com.apple.CoreSimulator.SimRuntime.tvOS-18-2': [{
            'isAvailable':
                True,
            'name':
                'Apple TV 4K (3rd generation)',
            'deviceTypeIdentifier':
                'com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K',
            'state':
                'Shutdown',
            'udid':
                'C2ECB6C1-801B-497F-BB17-19D656613E98',
        }],
        'com.apple.CoreSimulator.SimRuntime.watchOS-11-2': [{
            'isAvailable':
                True,
            'name':
                'Apple Watch Series 10 (46mm)',
            'deviceTypeIdentifier':
                'com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-10-46mm',
            'state':
                'Shutdown',
            'udid':
                '130E3116-DB18-4199-B3C7-6720DB10FB42',
        }],
    },
    'devicetypes': [
        {
            'name': 'iPhone 5s',
            'bundlePath': '/path/iPhone 4s/Content',
            'identifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-5s'
        },
        {
            'name': 'iPhone X',
            'bundlePath': '/path/iPhone X/Content',
            'identifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-X'
        },
        {
            'name': 'iPhone 11',
            'bundlePath': '/path/iPhone 11/Content',
            'identifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-11'
        },
        {
            'name':
                'Apple TV 4K (3rd generation)',
            'bundlePath':
                '/path/Apple TV 4K (3rd generation)/Content',
            'identifier':
                'com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K',
        },
        {
            'name':
                'Apple Watch Series 10 (46mm)',
            'bundlePath':
                '/path/Apple Watch Series 10 (46mm).simdevicetype/Content',
            'identifier':
                'com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-10-46mm',
        },
    ],
    'pairs': [],
    'runtimes': [{
        "buildversion": "15F79",
        "bundlePath": "/path/Runtimes/iOS 11.4.simruntime",
        "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-11-4",
        "isAvailable": True,
        "name": "iOS 11.4",
        "version": "11.4",
        "supportedDeviceTypes": [
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 11.simdevicetype",
                "name":
                    "iPhone 11",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
                "productFamily":
                    "iPhone"
            },
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone X.simdevicetype",
                "name":
                    "iPhone X",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-X",
                "productFamily":
                    "iPhone"
            },
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 5s.simdevicetype",
                "name":
                    "iPhone 5s",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-5s",
                "productFamily":
                    "iPhone"
            },
        ],
    }, {
        "buildversion": "17A844",
        "bundlePath": "/path/Runtimes/iOS 13.1.simruntime",
        "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-1",
        "isAvailable": True,
        "name": "iOS 13.1",
        "version": "13.1",
        "supportedDeviceTypes": [
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 11.simdevicetype",
                "name":
                    "iPhone 11",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
                "productFamily":
                    "iPhone"
            },
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone X.simdevicetype",
                "name":
                    "iPhone X",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-X",
                "productFamily":
                    "iPhone"
            },
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 5s.simdevicetype",
                "name":
                    "iPhone 5s",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-5s",
                "productFamily":
                    "iPhone"
            },
        ],
    }, {
        "buildversion": "17B102",
        "bundlePath": "/path/Runtimes/iOS.simruntime",
        "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-2",
        "isAvailable": True,
        "name": "iOS 13.2",
        "version": "13.2.2",
        "supportedDeviceTypes": [
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 11.simdevicetype",
                "name":
                    "iPhone 11",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
                "productFamily":
                    "iPhone"
            },
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone X.simdevicetype",
                "name":
                    "iPhone X",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-X",
                "productFamily":
                    "iPhone"
            },
            {
                "bundlePath":
                    "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 5s.simdevicetype",
                "name":
                    "iPhone 5s",
                "identifier":
                    "com.apple.CoreSimulator.SimDeviceType.iPhone-5s",
                "productFamily":
                    "iPhone"
            },
        ],
    }, {
        "buildversion": "22K153",
        "bundlePath": "/path/Runtimes/tvOS 13.2.simruntime",
        "identifier": "com.apple.CoreSimulator.SimRuntime.tvOS-13-2",
        "isAvailable": True,
        "name": "tvOS 13.2",
        "version": "13.2",
        "supportedDeviceTypes": [{
            "bundlePath":
                "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/Apple TV 4K (3rd generation).simdevicetype",
            "name":
                "Apple TV 4K (3rd generation)",
            "identifier":
                "com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K",
            "productFamily":
                "Apple TV"
        },],
    }, {
        "buildversion": "22K154",
        "bundlePath": "/path/Runtimes/tvOS 18.2.simruntime",
        "identifier": "com.apple.CoreSimulator.SimRuntime.tvOS-18-2",
        "isAvailable": True,
        "name": "tvOS 18.2",
        "version": "18.2",
        "supportedDeviceTypes": [{
            "bundlePath":
                "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/Apple TV 4K (3rd generation).simdevicetype",
            "name":
                "Apple TV 4K (3rd generation)",
            "identifier":
                "com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K",
            "productFamily":
                "Apple TV"
        },],
    }, {
        "buildversion": "22S99",
        "bundlePath": "/path/Runtimes/watchOS 11.2.simruntime",
        "identifier": "com.apple.CoreSimulator.SimRuntime.watchOS-11-2",
        "isAvailable": True,
        "name": "watchOS 11.2",
        "version": "11.2",
        "supportedDeviceTypes": [{
            "bundlePath":
                "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/Apple Watch Series 10 (46mm).simdevicetype",
            "name":
                "Apple Watch Series 10 (46mm)",
            "identifier":
                "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-10-46mm",
            "productFamily":
                "Apple Watch"
        },],
    }]
}

RUNTIMES_LIST = {
    "111111": {
        "build": "21A111111",
        "deletable": True,
        "identifier": "111111",
        "kind": "Disk Image",
        "lastUsedAt": "2023-09-28T21:57:06Z",
        "mountPath": "path/to/mount",
        "path": "path/to/runtime/111111.dmg",
        "platformIdentifier": "com.apple.platform.iphonesimulator",
        "runtimeBundlePath": "path/to/bundle/runtime/111111.simruntime",
        "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-1",
        "signatureState": "Verified",
        "sizeBytes": 3595537104,
        "state": "Ready",
        "version": "13.1"
    },
    "222222": {
        "build": "21A222222",
        "deletable": False,
        "identifier": "222222",
        "kind": "Bundled with Xcode",
        "path": "path/to/runtime/222222.simruntime",
        "platformIdentifier": "com.apple.platform.iphonesimulator",
        "runtimeBundlePath": "path/to/bundle/runtime/222222.simruntime",
        "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-2",
        "signatureState": "Unknown",
        "sizeBytes": 14115082240,
        "state": "Ready",
        "version": "13.2"
    },
    "333333": {
        "build": "21A333333",
        "deletable": True,
        "identifier": "333333",
        "kind": "Legacy Download",
        "path": "path/to/runtime/333333.simruntime",
        "platformIdentifier": "com.apple.platform.iphonesimulator",
        "runtimeBundlePath": "path/to/bundle/runtime/333333.simruntime",
        "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-11-4",
        "signatureState": "Unknown",
        "sizeBytes": 12596879360,
        "state": "Ready",
        "version": "11.4"
    },
    "444444": {
        "build": "21A444444",
        "deletable": True,
        "identifier": "444444",
        "kind": "Disk Image",
        "lastUsedAt": "2022-09-28T21:57:06Z",
        "path": "path/to/runtime/444444.simruntime",
        "platformIdentifier": "com.apple.platform.iphonesimulator",
        "runtimeBundlePath": "path/to/bundle/runtime/444444.simruntime",
        "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-14-1",
        "signatureState": "Unknown",
        "sizeBytes": 12596879361,
        "state": "Ready",
        "version": "14.1"
    },
    "555555": {
        "build": "555555",
        "deletable": True,
        "identifier": "555555",
        "kind": "Disk Image",
        "path": "path/to/runtime/555555.simruntime",
        "platformIdentifier": "com.apple.platform.iphonesimulator",
        "runtimeBundlePath": "path/to/bundle/runtime/555555.simruntime",
        "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.iOS-15-1",
        "signatureState": "Unknown",
        "sizeBytes": 12596879362,
        "state": "Ready",
        "version": "15.1"
    },
    "FFFFFF": {
        "build": "22K154",
        "deletable": True,
        "identifier": "FFFFFF",
        "kind": "Bundled with Xcode",
        "path": "path/to/runtime/FFFFFF.runtime",
        "platformIdentifier": "com.apple.platform.appletvsimulator",
        "runtimeBundlePath": "path/to/bundle/runtime/tvOS 18.2.simruntime",
        "runtimeIdentifier": "com.apple.CoreSimulator.SimRuntime.tvOS-18-2",
        "signatureState": "Unknown",
        "sizeBytes": 4345796963,
        "state": "Ready",
        "version": "18.2"
    },
    "GGGGGG": {
        "build": "GGGGGG",
        "kind": "Legacy Download",
        "deletable": True,
        "identifier": "GGGGGG",
        "state": "Unusable",
        "version": "18.2"
    },
}

RUNTIMES_MATCH_LIST = {
    "appletvos17.0": {
        "chosenRuntimeBuild": "21J11111",
        "defaultBuild": "21J11111",
        "platform": "com.apple.platform.appletvos",
        "preferredBuild": "21J11111",
        "sdkBuild": "21J11111",
        "sdkVersion": "13.1"
    },
    "iphoneos17.0": {
        "chosenRuntimeBuild": "21A111112",
        "defaultBuild": "21A111112",
        "platform": "com.apple.platform.iphoneos",
        "preferredBuild": "21A111111",
        "sdkBuild": "21A111112",
        "sdkVersion": "13.1"
    }
}

ADD_RUNTIME_OUTPUT = "111111 (iOS17.0)"


@mock.patch.object(
    iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST)
@mock.patch.object(
    iossim_util, 'get_simulator_runtime_list', return_value=RUNTIMES_LIST)
class GetiOSSimUtil(test_runner_test.TestCase):
  """Tests for iossim_util.py."""

  def setUp(self):
    super(GetiOSSimUtil, self).setUp()

  def test_get_simulator_runtime_by_platform_and_version(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
        iossim_util.get_simulator_runtime_by_platform_and_version(
            iossim_util.get_simulator_list(), 'iPhone X', '13.2.2'))

  def test_get_simulator_runtime_by_platform_and_version_multiple_devices(
      self, _, _2):
    """Ensures correctness of filter when runtimes for different product families have the same
    version number"""
    self.assertEqual(
        'com.apple.CoreSimulator.SimRuntime.tvOS-13-2',
        iossim_util.get_simulator_runtime_by_platform_and_version(
            iossim_util.get_simulator_list(), 'Apple TV 4K (3rd generation)',
            '13.2'))

  def test_get_simulator_runtime_by_platform_and_version_non_ios_platform(
      self, _, _2):
    """Ensures a non-iOS runtime version can be found as well."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimRuntime.tvOS-18-2',
        iossim_util.get_simulator_runtime_by_platform_and_version(
            iossim_util.get_simulator_list(), 'Apple TV 4K (3rd generation)',
            '18.2'))

  def test_get_simulator_runtime_by_platform_and_version_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError raises if no runtime."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_runtime_by_platform_and_version(
          iossim_util.get_simulator_list(), 'iPhone X', '13.3')
    expected_message = ('Simulator does not exist: Not found '
                        '"13.3" SDK in runtimes')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_device_type_by_platform(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
        iossim_util.get_simulator_device_type_by_platform(
            iossim_util.get_simulator_list(), 'iPhone 11'))

  def test_get_simulator_device_type_by_platform_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError raises if no platform."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_device_type_by_platform(
          iossim_util.get_simulator_list(), 'iPhone XI')
    expected_message = ('Simulator does not exist: Not found device '
                        '"iPhone XI" in devicetypes')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_runtime_by_device_udid(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
        iossim_util.get_simulator_runtime_by_device_udid(
            'E4E66321-177A-450A-9BA1-488D85B7278E'))

  def test_get_simulator_runtime_by_device_udid_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError raises if no device with UDID."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_runtime_by_device_udid('non_existing_UDID')
    expected_message = ('Simulator does not exist: Not found simulator with '
                        '"non_existing_UDID" UDID in devices')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_udids_by_platform_and_version(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(['A4E66321-177A-450A-9BA1-488D85B7278E'],
                     iossim_util.get_simulator_udids_by_platform_and_version(
                         'iPhone 11', '13.2.2'))

  def test_get_simulator_udids_by_platform_and_version_not_found(self, _, _2):
    """Ensures that filter returns empty list if no device with version."""
    self.assertEqual([],
                     iossim_util.get_simulator_udids_by_platform_and_version(
                         'iPhone 11', '13.1'))

  def test_get_platform_type_by_platform(self, _, _2):
    """Ensures correctess of filter."""
    self.assertEqual(constants.IOSPlatformType.IPHONEOS,
                     iossim_util.get_platform_type_by_platform('iPhone 11'))
    self.assertEqual(
        constants.IOSPlatformType.TVOS,
        iossim_util.get_platform_type_by_platform(
            'Apple TV 4K (3rd generation)'))

  def test_get_platform_type_by_platform_unsupported(self, _, _2):
    """Ensures that UnsupportedDeviceTypeError is raised for unsupported
    target platforms."""
    with self.assertRaises(test_runner.UnsupportedDeviceTypeError) as context:
      iossim_util.get_platform_type_by_platform('Apple Watch Series 10 (46mm)')
    expected_message = (
        'Unsupported device type: com.apple.CoreSimulator.SimDeviceType.'
        'Apple-Watch-Series-10-46mm')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_platform_type_by_platform_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError is raised if a platform cannot be
    found."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_platform_type_by_platform('Foo')
    expected_message = ('Simulator does not exist: Not found device "Foo" in '
                        'devicetypes')
    self.assertTrue(expected_message in str(context.exception))

  @mock.patch('subprocess.check_output', autospec=True)
  def test_create_device_by_platform_and_version(self, subprocess_mock, _, _2):
    """Ensures that command is correct."""
    subprocess_mock.return_value = b'NEW_UDID'
    self.assertEqual(
        'NEW_UDID',
        iossim_util.create_device_by_platform_and_version(
            'iPhone 11', '13.2.2'))
    self.assertEqual([
        'xcrun', 'simctl', 'create', 'iPhone 11 13.2.2 test simulator',
        'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2'
    ], subprocess_mock.call_args[0][0])

  @mock.patch('subprocess.check_output', autospec=True)
  def test_delete_simulator_by_udid(self, subprocess_mock, _, _2):
    """Ensures that command is correct."""
    iossim_util.delete_simulator_by_udid('UDID')
    self.assertEqual(['xcrun', 'simctl', 'delete', 'UDID'],
                     subprocess_mock.call_args[0][0])

  @mock.patch('subprocess.check_call', autospec=True)
  def test_wipe_simulator_by_platform_and_version(self, subprocess_mock, _, _2):
    """Ensures that command is correct."""
    iossim_util.wipe_simulator_by_udid('A4E66321-177A-450A-9BA1-488D85B7278E')
    self.assertEqual(
        ['xcrun', 'simctl', 'erase', 'A4E66321-177A-450A-9BA1-488D85B7278E'],
        subprocess_mock.call_args[0][0])

  @mock.patch('subprocess.check_output', autospec=True)
  def test_get_home_directory(self, subprocess_mock, _, _2):
    """Ensures that command is correct."""
    subprocess_mock.return_value = b'HOME_DIRECTORY'
    self.assertEqual('HOME_DIRECTORY',
                     iossim_util.get_home_directory('iPhone 11', '13.2.2'))
    self.assertEqual([
        'xcrun', 'simctl', 'getenv', 'A4E66321-177A-450A-9BA1-488D85B7278E',
        'HOME'
    ], subprocess_mock.call_args[0][0])

  @mock.patch.object(iossim_util, 'create_device_by_platform_and_version')
  def test_no_new_sim_created_when_one_exists(self, mock_create, _, _2):
    """Ensures no simulator is created when one in desired dimension exists."""
    self.assertEqual('A4E66321-177A-450A-9BA1-488D85B7278E',
                     iossim_util.get_simulator('iPhone 11', '13.2.2'))
    self.assertFalse(mock_create.called)

  def test_copy_cert(self, _, _2):
    """Ensures right commands are issued to copy cert"""
    self.mock(os.path, 'exists', lambda *args: True)

    check_call_mock = mock.Mock()
    self.mock(subprocess, 'check_call', check_call_mock)
    iossim_util.copy_trusted_certificate('test/cert', 'UDID')

    calls = [
        mock.call(['xcrun', 'simctl', 'boot', 'UDID']),
        mock.call([
            'xcrun', 'simctl', 'keychain', 'UDID', 'add-root-cert', 'test/cert'
        ]),
        mock.call(['xcrun', 'simctl', 'shutdown', 'UDID'])
    ]

    check_call_mock.assert_has_calls(calls)
    # ensure subprocess.check_call was only called 3 times
    self.assertEqual(check_call_mock.call_count, 3)

  def test_get_simulator_runtime_info_by_build(self, _, _2):
    runtime = iossim_util.get_simulator_runtime_info_by_build('21A111111')
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['version'], '13.1')
    self.assertEqual(runtime['identifier'], '111111')

  def test_get_simulator_runtime_info_by_id(self, _, _2):
    runtime = iossim_util.get_simulator_runtime_info_by_id('111111')
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['version'], '13.1')
    self.assertEqual(runtime['identifier'], '111111')

  def test_get_simulator_runtime_info(self, _, _2):
    runtime = iossim_util.get_simulator_runtime_info(
        constants.IOSPlatformType.IPHONEOS, '13.1')
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['version'], '13.1')

    runtime = iossim_util.get_simulator_runtime_info(
        constants.IOSPlatformType.TVOS, '13.1')
    self.assertIsNone(runtime)

    runtime = iossim_util.get_simulator_runtime_info(
        constants.IOSPlatformType.TVOS, '18.2')
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['version'], '18.2')

  @mock.patch.object(iossim_util, 'get_simulator_runtime_match_list')
  def test_override_default_iphonesim_runtime(self, override_runtime_mock, _,
                                              _2):
    check_call_mock = mock.Mock()
    self.mock(subprocess, 'check_call', check_call_mock)
    override_runtime_mock.return_value = RUNTIMES_MATCH_LIST
    iossim_util.override_default_iphonesim_runtime(ADD_RUNTIME_OUTPUT, '17.0')
    calls = [
        mock.call([
            'xcrun', 'simctl', 'runtime', 'match', 'set', 'iphoneos17.0',
            '21A111111', '--sdkBuild', '21A111112'
        ]),
    ]
    check_call_mock.assert_has_calls(calls)

  @mock.patch.object(iossim_util, 'get_simulator_runtime_match_list')
  def test_override_default_iphonesim_runtime_not_found(self,
                                                        override_runtime_mock,
                                                        _, _2):
    check_call_mock = mock.Mock()
    self.mock(subprocess, 'check_call', check_call_mock)
    override_runtime_mock.return_value = RUNTIMES_MATCH_LIST
    iossim_util.override_default_iphonesim_runtime('random string', '17.0')
    check_call_mock.assert_not_called()

  def test_add_simulator_runtime(self, _, _2):
    check_output_mock = mock.Mock()
    self.mock(subprocess, 'check_output', check_output_mock)
    dmg_path = 'path/to/dmg'
    iossim_util.add_simulator_runtime(dmg_path)

    calls = [
        mock.call(['xcrun', 'simctl', 'runtime', 'add', dmg_path, '--verbose'],
                  stderr=subprocess.STDOUT),
    ]

    check_output_mock.assert_has_calls(calls)
    self.assertEqual(check_output_mock.call_count, 1)

  def test_delete_simulator_runtime(self, _, _2):
    check_output_mock = mock.Mock()
    self.mock(subprocess, 'check_output', check_output_mock)
    runtime_id = '111111'
    iossim_util.delete_simulator_runtime(runtime_id)

    calls = [
        mock.call(['xcrun', 'simctl', 'runtime', 'delete', runtime_id],
                  stderr=subprocess.STDOUT),
    ]

    check_output_mock.assert_has_calls(calls)
    self.assertEqual(check_output_mock.call_count, 1)

  def test_delete_simulator_runtime_already_deleted(self, _, _2):
    error_output = (b'Cannot stage disk image or bundle for delete '
                    b'Underlying error (domain=NSCocoaErrorDomain, code=4):'
                    b'The file doesn\xe2\x80\x99t exist.')
    check_output_mock = mock.Mock(
        side_effect=subprocess.CalledProcessError(1, 'cmd', error_output))
    self.mock(subprocess, 'check_output', check_output_mock)
    runtime_id = '111111'
    # This should not raise an exception.
    iossim_util.delete_simulator_runtime(runtime_id)

  def test_delete_simulator_runtime_other_error(self, _, _2):
    error_output = b'Some other error'
    check_output_mock = mock.Mock(
        side_effect=subprocess.CalledProcessError(1, 'cmd', error_output))
    self.mock(subprocess, 'check_output', check_output_mock)
    runtime_id = '111111'
    with self.assertRaises(subprocess.CalledProcessError):
      iossim_util.delete_simulator_runtime(runtime_id)

  def test_delete_least_recently_used_simulator_runtimes(self, _, _2):
    with mock.patch('iossim_util.delete_simulator_runtime') \
       as mock_delete_simulator_runtime:
      iossim_util.delete_least_recently_used_simulator_runtimes(1)

      calls = [
          mock.call('444444', True),
          mock.call('555555', True),
      ]

      self.assertEqual(mock_delete_simulator_runtime.call_count, 2)
      mock_delete_simulator_runtime.assert_has_calls(calls, any_order=True)

  def test_delete_least_recently_used_simulator_runtimes_no_op(self, _, _2):
    with mock.patch('iossim_util.delete_simulator_runtime') \
       as mock_delete_simulator_runtime:
      iossim_util.delete_least_recently_used_simulator_runtimes()

      self.assertEqual(mock_delete_simulator_runtime.call_count, 0)

  def test_delete_stale_simulator_runtimes(self, _, _2):
    with mock.patch('iossim_util.delete_simulator_runtime') \
       as mock_delete_simulator_runtime:
      iossim_util.delete_stale_simulator_runtimes()

      calls = [
          mock.call('GGGGGG', True),
      ]

      self.assertEqual(mock_delete_simulator_runtime.call_count, 1)

  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch.object(plistlib, 'dump')
  @mock.patch.object(plistlib, 'load')
  def test_disable_hardware_keyboard(self, mock_load, mock_dump, mock_open, _,
                                     _2):
    """Ensures right commands are issued to disable hardware keyboard"""

    def reset():
      mock_load.reset_mock()
      mock_dump.reset_mock()
    self.mock(os.path, 'expanduser', lambda *args: 'PATH')
    udid = '0CAF5FCB-8C07-41B1-8624-D08BCCEC360C'
    expected = {'DevicePreferences': {udid: {'ConnectHardwareKeyboard': False}}}

    # file does not exist
    self.mock(os.path, 'exists', lambda *args: False)
    iossim_util.disable_hardware_keyboard(udid)
    mock_load.assert_not_called()
    mock_dump.assert_called_with(expected, mock_open(), fmt=plistlib.FMT_BINARY)
    reset()

    # file exists but is empty
    self.mock(os.path, 'exists', lambda *args: True)
    mock_load.return_value = {}
    iossim_util.disable_hardware_keyboard(udid)
    mock_load.assert_called()
    mock_dump.assert_called_with(expected, mock_open(), fmt=plistlib.FMT_BINARY)
    reset()

    # DevicePreferences dictionary and udid exist
    load_return_val = {
        'DevicePreferences': {
            udid: {
                'SimulatorExternalDisplay': 2114
            }
        }
    }
    expected = deepcopy(load_return_val)
    expected['DevicePreferences'][udid]['ConnectHardwareKeyboard'] = False
    mock_load.return_value = load_return_val
    iossim_util.disable_hardware_keyboard(udid)
    mock_load.assert_called()
    mock_dump.assert_called_with(expected, mock_open(), fmt=plistlib.FMT_BINARY)

  def test_disable_simulator_keyboard_tutorial(self, _, _2):
    with mock.patch('iossim_util.boot_simulator_if_not_booted') \
        as mock_boot_simulator:
      # boots successfully
      mock_boot_simulator.return_value = None

      check_call_mock = mock.Mock()
      self.mock(subprocess, 'check_call', check_call_mock)
      udid = "1111111"
      check_calls = [
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences',
              'DidShowContinuousPathIntroduction', '1'
          ]),
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences',
              'KeyboardDidShowProductivityTutorial', '1'
          ]),
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences',
              'DidShowGestureKeyboardIntroduction', '1'
          ]),
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences',
              'UIKeyboardDidShowInternationalInfoIntroduction', '1'
          ]),
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences', 'KeyboardAutocorrection', '0'
          ]),
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences', 'KeyboardPrediction', '0'
          ]),
          mock.call([
              'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
              'com.apple.keyboard.preferences', 'KeyboardShowPredictionBar', '0'
          ])
      ]

      iossim_util.disable_simulator_keyboard_tutorial(udid)
      check_call_mock.assert_has_calls(check_calls)


if __name__ == '__main__':
  unittest.main()
