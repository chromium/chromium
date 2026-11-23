#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for iossim_util.py."""

from copy import deepcopy
import json
import os
import plistlib
import subprocess
import tempfile
import threading
import time
import types
import unittest
import uuid
import mock

import constants
import iossim_util
import mac_util  # noqa: F401
import test_runner
import test_runner_test

SIMULATORS_LIST = {
  'devices': {
    'com.apple.CoreSimulator.SimRuntime.iOS-11-4': [
      {
        'isAvailable': True,
        'name': 'iPhone 5s 11.4 test simulator',
        'deviceTypeIdentifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-5s',
        'state': 'Shutdown',
        'udid': 'E4E66320-177A-450A-9BA1-488D85B7278E',
      }
    ],
    'com.apple.CoreSimulator.SimRuntime.iOS-13-2': [
      {
        'isAvailable': True,
        'name': 'iPhone X 13.2.2 test simulator',
        'deviceTypeIdentifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-X',
        'state': 'Shutdown',
        'udid': 'E4E66321-177A-450A-9BA1-488D85B7278E',
      },
      {
        'isAvailable': True,
        'name': 'iPhone 11 13.2.2 test simulator',
        'deviceTypeIdentifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
        'state': 'Shutdown',
        'udid': 'A4E66321-177A-450A-9BA1-488D85B7278E',
      },
    ],
    'com.apple.CoreSimulator.SimRuntime.tvOS-18-2': [
      {
        'isAvailable': True,
        'name': 'Apple TV 4K (3rd generation)',
        'deviceTypeIdentifier': 'com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K',
        'state': 'Shutdown',
        'udid': 'C2ECB6C1-801B-497F-BB17-19D656613E98',
      }
    ],
    'com.apple.CoreSimulator.SimRuntime.watchOS-11-2': [
      {
        'isAvailable': True,
        'name': 'Apple Watch Series 10 (46mm)',
        'deviceTypeIdentifier': 'com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-10-46mm',
        'state': 'Shutdown',
        'udid': '130E3116-DB18-4199-B3C7-6720DB10FB42',
      }
    ],
  },
  'devicetypes': [
    {
      'name': 'iPhone 5s',
      'bundlePath': '/path/iPhone 4s/Content',
      'identifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-5s',
    },
    {
      'name': 'iPhone X',
      'bundlePath': '/path/iPhone X/Content',
      'identifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-X',
    },
    {
      'name': 'iPhone 11',
      'bundlePath': '/path/iPhone 11/Content',
      'identifier': 'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
    },
    {
      'name': 'Apple TV 4K (3rd generation)',
      'bundlePath': '/path/Apple TV 4K (3rd generation)/Content',
      'identifier': 'com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K',
    },
    {
      'name': 'Apple Watch Series 10 (46mm)',
      'bundlePath': '/path/Apple Watch Series 10 (46mm).simdevicetype/Content',
      'identifier': 'com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-10-46mm',
    },
  ],
  'pairs': [],
  'runtimes': [
    {
      "buildversion": "15F79",
      "bundlePath": "/path/Runtimes/iOS 11.4.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-11-4",
      "isAvailable": True,
      "name": "iOS 11.4",
      "version": "11.4",
      "supportedDeviceTypes": [
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 11.simdevicetype",
          "name": "iPhone 11",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
          "productFamily": "iPhone",
        },
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone X.simdevicetype",
          "name": "iPhone X",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-X",
          "productFamily": "iPhone",
        },
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 5s.simdevicetype",
          "name": "iPhone 5s",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-5s",
          "productFamily": "iPhone",
        },
      ],
    },
    {
      "buildversion": "17A844",
      "bundlePath": "/path/Runtimes/iOS 13.1.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-1",
      "isAvailable": True,
      "name": "iOS 13.1",
      "version": "13.1",
      "supportedDeviceTypes": [
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 11.simdevicetype",
          "name": "iPhone 11",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
          "productFamily": "iPhone",
        },
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone X.simdevicetype",
          "name": "iPhone X",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-X",
          "productFamily": "iPhone",
        },
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 5s.simdevicetype",
          "name": "iPhone 5s",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-5s",
          "productFamily": "iPhone",
        },
      ],
    },
    {
      "buildversion": "17B102",
      "bundlePath": "/path/Runtimes/iOS.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-2",
      "isAvailable": True,
      "name": "iOS 13.2",
      "version": "13.2.2",
      "supportedDeviceTypes": [
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 11.simdevicetype",
          "name": "iPhone 11",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
          "productFamily": "iPhone",
        },
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone X.simdevicetype",
          "name": "iPhone X",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-X",
          "productFamily": "iPhone",
        },
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/iPhone 5s.simdevicetype",
          "name": "iPhone 5s",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-5s",
          "productFamily": "iPhone",
        },
      ],
    },
    {
      "buildversion": "22K153",
      "bundlePath": "/path/Runtimes/tvOS 13.2.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.tvOS-13-2",
      "isAvailable": True,
      "name": "tvOS 13.2",
      "version": "13.2",
      "supportedDeviceTypes": [
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/Apple TV 4K (3rd generation).simdevicetype",
          "name": "Apple TV 4K (3rd generation)",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K",
          "productFamily": "Apple TV",
        },
      ],
    },
    {
      "buildversion": "22K154",
      "bundlePath": "/path/Runtimes/tvOS 18.2.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.tvOS-18-2",
      "isAvailable": True,
      "name": "tvOS 18.2",
      "version": "18.2",
      "supportedDeviceTypes": [
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/Apple TV 4K (3rd generation).simdevicetype",
          "name": "Apple TV 4K (3rd generation)",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.Apple-TV-4K-3rd-generation-4K",
          "productFamily": "Apple TV",
        },
      ],
    },
    {
      "buildversion": "22S99",
      "bundlePath": "/path/Runtimes/watchOS 11.2.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.watchOS-11-2",
      "isAvailable": True,
      "name": "watchOS 11.2",
      "version": "11.2",
      "supportedDeviceTypes": [
        {
          "bundlePath": "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/Apple Watch Series 10 (46mm).simdevicetype",
          "name": "Apple Watch Series 10 (46mm)",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.Apple-Watch-Series-10-46mm",
          "productFamily": "Apple Watch",
        },
      ],
    },
    {
      "buildversion": "24B114",
      "bundlePath": "/path/Runtimes/iOS 27.2.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-27-2",
      "isAvailable": True,
      "name": "iOS 27.2",
      "version": "27.2",
      "supportedDeviceTypes": [
        {
          "bundlePath": (
            "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/"
            "iPhone 11.simdevicetype"
          ),
          "name": "iPhone 11",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-11",
          "productFamily": "iPhone",
        },
      ],
    },
    {
      "buildversion": "24B91016s",
      "bundlePath": "/path/Runtimes/iOS 27.2.simruntime",
      "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4",
      "isAvailable": True,
      "name": "iOS 27.2",
      "version": "27.2",
      "supportedDeviceTypes": [
        {
          "bundlePath": (
            "\/Library\/Developer\/CoreSimulator\/Profiles\/DeviceTypes\/"
            "iPhone Duo.simdevicetype"
          ),
          "name": "iPhone Duo",
          "identifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-Duo",
          "productFamily": "iPhone",
        },
      ],
    },
  ],
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
    "version": "13.1",
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
    "version": "13.2",
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
    "version": "11.4",
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
    "version": "14.1",
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
    "version": "15.1",
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
    "version": "18.2",
  },
  "GGGGGG": {
    "build": "GGGGGG",
    "kind": "Legacy Download",
    "deletable": True,
    "identifier": "GGGGGG",
    "state": "Unusable",
    "version": "18.2",
  },
}

RUNTIMES_MATCH_LIST = {
  "appletvos17.0": {
    "chosenRuntimeBuild": "21J11111",
    "defaultBuild": "21J11111",
    "platform": "com.apple.platform.appletvos",
    "preferredBuild": "21J11111",
    "sdkBuild": "21J11111",
    "sdkVersion": "13.1",
  },
  "iphoneos17.0": {
    "chosenRuntimeBuild": "21A111112",
    "defaultBuild": "21A111112",
    "platform": "com.apple.platform.iphoneos",
    "preferredBuild": "21A111111",
    "sdkBuild": "21A111112",
    "sdkVersion": "13.1",
  },
}

ADD_RUNTIME_OUTPUT = "111111 (iOS17.0)"


@mock.patch.object(
  iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST
)
@mock.patch.object(
  iossim_util, 'get_simulator_runtime_list', return_value=RUNTIMES_LIST
)
class GetiOSSimUtil(test_runner_test.TestCase):
  """Tests for iossim_util.py."""

  def setUp(self):
    super(GetiOSSimUtil, self).setUp()
    # Keep existing unit tests from writing to `/private/var/tmp` on macOS.
    self.mock(
      iossim_util, 'disable_simulator_daemons', lambda *args, **kwargs: False
    )

  def test_get_simulator_runtime_by_platform_and_version(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'iPhone X', '13.2.2'
      ),
    )

  def test_get_simulator_runtime_by_platform_and_version_multiple_devices(
    self, _, _2
  ):
    """Ensures correctness of filter when runtimes for different product families have the same
    version number"""
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.tvOS-13-2',
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'Apple TV 4K (3rd generation)', '13.2'
      ),
    )

  def test_get_simulator_runtime_by_platform_and_version_non_ios_platform(
    self, _, _2
  ):
    """Ensures a non-iOS runtime version can be found as well."""
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.tvOS-18-2',
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'Apple TV 4K (3rd generation)', '18.2'
      ),
    )

  def test_get_simulator_runtime_by_platform_and_version_variant_suffix(
    self, _, _2
  ):
    """Ensures a device-specific runtime variant is found with or without suffix."""
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4',
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'iPhone Duo', '27.2-iphone19-4'
      ),
    )
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4',
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'iPhone Duo', '27.2'
      ),
    )
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.iOS-27-2',
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'iPhone 11', '27.2'
      ),
    )

  def test_get_simulator_runtime_by_platform_and_version_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError raises if no runtime."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_runtime_by_platform_and_version(
        iossim_util.get_simulator_list(), 'iPhone X', '13.3'
      )
    expected_message = (
      'Simulator does not exist: Not found "13.3" SDK in runtimes'
    )
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_device_type_by_platform(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
      'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
      iossim_util.get_simulator_device_type_by_platform(
        iossim_util.get_simulator_list(), 'iPhone 11'
      ),
    )

  def test_get_simulator_device_type_by_platform_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError raises if no platform."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_device_type_by_platform(
        iossim_util.get_simulator_list(), 'iPhone XI'
      )
    expected_message = (
      'Simulator does not exist: Not found device "iPhone XI" in devicetypes'
    )
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_runtime_by_device_udid(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
      'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
      iossim_util.get_simulator_runtime_by_device_udid(
        'E4E66321-177A-450A-9BA1-488D85B7278E'
      ),
    )

  def test_get_simulator_runtime_by_device_udid_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError raises if no device with UDID."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_runtime_by_device_udid('non_existing_UDID')
    expected_message = (
      'Simulator does not exist: Not found simulator with '
      '"non_existing_UDID" UDID in devices'
    )
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_udids_by_platform_and_version(self, _, _2):
    """Ensures correctness of filter."""
    self.assertEqual(
      ['A4E66321-177A-450A-9BA1-488D85B7278E'],
      iossim_util.get_simulator_udids_by_platform_and_version(
        'iPhone 11', '13.2.2'
      ),
    )

  def test_get_simulator_udids_by_platform_and_version_not_found(self, _, _2):
    """Ensures that filter returns empty list if no device with version."""
    self.assertEqual(
      [],
      iossim_util.get_simulator_udids_by_platform_and_version(
        'iPhone 11', '13.1'
      ),
    )

  def test_get_platform_type_by_platform(self, _, _2):
    """Ensures correctess of filter."""
    self.assertEqual(
      constants.IOSPlatformType.IPHONEOS,
      iossim_util.get_platform_type_by_platform('iPhone 11'),
    )
    self.assertEqual(
      constants.IOSPlatformType.TVOS,
      iossim_util.get_platform_type_by_platform('Apple TV 4K (3rd generation)'),
    )

  def test_get_platform_type_by_platform_unsupported(self, _, _2):
    """Ensures that UnsupportedDeviceTypeError is raised for unsupported
    target platforms."""
    with self.assertRaises(test_runner.UnsupportedDeviceTypeError) as context:
      iossim_util.get_platform_type_by_platform('Apple Watch Series 10 (46mm)')
    expected_message = (
      'Unsupported device type: com.apple.CoreSimulator.SimDeviceType.'
      'Apple-Watch-Series-10-46mm'
    )
    self.assertTrue(expected_message in str(context.exception))

  def test_get_platform_type_by_platform_not_found(self, _, _2):
    """Ensures that SimulatorNotFoundError is raised if a platform cannot be
    found."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_platform_type_by_platform('Foo')
    expected_message = (
      'Simulator does not exist: Not found device "Foo" in devicetypes'
    )
    self.assertTrue(expected_message in str(context.exception))

  @mock.patch('subprocess.check_output', autospec=True)
  def test_create_device_by_platform_and_version_no_cache(
    self, subprocess_mock, _, _2
  ):
    """Ensures that command is correct."""
    subprocess_mock.return_value = b'NEW_UDID'
    self.assertEqual(
      'NEW_UDID',
      iossim_util.create_device_by_platform_and_version('iPhone 11', '13.2.2'),
    )
    self.assertEqual(
      [
        'xcrun',
        'simctl',
        'create',
        'iPhone 11 13.2.2 test simulator',
        'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
      ],
      subprocess_mock.call_args[0][0],
    )

  @mock.patch.object(iossim_util, 'delete_simulator_by_udid', autospec=True)
  @mock.patch.object(iossim_util, 'shutdown_simulator_by_udid', autospec=True)
  @mock.patch.object(
    iossim_util, 'ensure_simulator_fully_booted', autospec=True
  )
  @mock.patch.object(
    iossim_util, '_create_device_by_platform_and_version', autospec=True
  )
  @mock.patch.object(iossim_util, 'clone_simulator_by_udid', autospec=True)
  @mock.patch.object(
    iossim_util, 'get_simulator_udids_by_platform_and_version', autospec=True
  )
  @mock.patch('os.makedirs', autospec=True)
  def test_create_device_by_platform_and_version_with_cache(
    self,
    mock_makedirs,
    mock_get_simulator_udids_by_platform_and_version,
    mock_clone_simulator_by_udid,
    mock_create_device_by_platform_and_version,
    mock_ensure_simulator_fully_booted,
    mock_shutdown_simulator_by_udid,
    mock_delete_simulator_by_udid,
    _,
    _2,
  ):
    """Tests logic creating a new simulator when caching is enabled"""

    def _reset_mocks():
      mock_makedirs.reset_mock()
      mock_get_simulator_udids_by_platform_and_version.reset_mock()
      mock_clone_simulator_by_udid.reset_mock()
      mock_create_device_by_platform_and_version.reset_mock()
      mock_ensure_simulator_fully_booted.reset_mock()
      mock_shutdown_simulator_by_udid.reset_mock()
      mock_delete_simulator_by_udid.reset_mock()

    ### Simulator exists in Cache
    mock_get_simulator_udids_by_platform_and_version.return_value = ['UDID']
    mock_clone_simulator_by_udid.return_value = 'clone-UDID'

    result = iossim_util.create_device_by_platform_and_version(
      'iPhone 11', '13.2.2', use_cache=True
    )
    self.assertEqual(result, 'clone-UDID')
    mock_makedirs.assert_called_with(
      iossim_util.SIMULATOR_CACHE_PATH, exist_ok=True
    )
    mock_get_simulator_udids_by_platform_and_version.assert_called_with(
      'iPhone 11', '13.2.2', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_clone_simulator_by_udid.assert_called_with(
      'UDID',
      'iPhone 11 13.2.2 test simulator',
      path=iossim_util.SIMULATOR_CACHE_PATH,
      dest_path=iossim_util.SIMULATOR_DEFAULT_PATH,
    )
    _reset_mocks

    ### Simulator does not exist in Cache
    mock_get_simulator_udids_by_platform_and_version.return_value = []
    mock_create_device_by_platform_and_version.return_value = 'UDID'
    mock_ensure_simulator_fully_booted.return_value = True
    mock_clone_simulator_by_udid.return_value = 'clone-UDID'

    result = iossim_util.create_device_by_platform_and_version(
      'iPhone 11', '13.2.2', use_cache=True
    )
    self.assertEqual(result, 'clone-UDID')
    mock_makedirs.assert_called_with(
      iossim_util.SIMULATOR_CACHE_PATH, exist_ok=True
    )
    mock_get_simulator_udids_by_platform_and_version.assert_called_with(
      'iPhone 11', '13.2.2', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_create_device_by_platform_and_version.assert_called_with(
      'iPhone 11', '13.2.2', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_ensure_simulator_fully_booted.assert_called_with(
      'UDID', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_shutdown_simulator_by_udid.assert_called_with(
      'UDID', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_clone_simulator_by_udid.assert_called_with(
      'UDID',
      'iPhone 11 13.2.2 test simulator',
      path=iossim_util.SIMULATOR_CACHE_PATH,
      dest_path=iossim_util.SIMULATOR_DEFAULT_PATH,
    )
    _reset_mocks()

    ### Failure to launch simulator, default back to no caching
    mock_get_simulator_udids_by_platform_and_version.return_value = []
    mock_create_device_by_platform_and_version.return_value = 'UDID'
    mock_ensure_simulator_fully_booted.return_value = False

    result = iossim_util.create_device_by_platform_and_version(
      'iPhone 11', '13.2.2', use_cache=True
    )
    self.assertEqual(result, 'UDID')
    mock_makedirs.assert_called_with(
      iossim_util.SIMULATOR_CACHE_PATH, exist_ok=True
    )
    mock_get_simulator_udids_by_platform_and_version.assert_called_with(
      'iPhone 11', '13.2.2', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_ensure_simulator_fully_booted.assert_called_with(
      'UDID', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_shutdown_simulator_by_udid.assert_called_with(
      'UDID', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_delete_simulator_by_udid.assert_called_with(
      'UDID', path=iossim_util.SIMULATOR_CACHE_PATH
    )
    mock_create_device_by_platform_and_version.assert_called_with(
      'iPhone 11', '13.2.2'
    )

  @mock.patch('subprocess.check_output', autospec=True)
  def test_clone_simulator_by_udid(self, subprocess_mock, _, _2):
    """Ensures simctl command is correct."""
    subprocess_mock.return_value = b'CLONE_UDID'
    self.assertEqual(
      iossim_util.clone_simulator_by_udid(
        'UDID', 'iPhone 11 13.2.2 test simulator'
      ),
      'CLONE_UDID',
    )
    subprocess_mock.assert_called_with(
      ['xcrun', 'simctl', 'clone', 'UDID', 'iPhone 11 13.2.2 test simulator']
    )

    subprocess_mock.reset_mock()

    self.assertEqual(
      iossim_util.clone_simulator_by_udid(
        'UDID',
        'iPhone 11 13.2.2 test simulator',
        path='/src',
        dest_path='/dest',
      ),
      'CLONE_UDID',
    )
    subprocess_mock.assert_called_with(
      [
        'xcrun',
        'simctl',
        '--set',
        '/src',
        'clone',
        'UDID',
        'iPhone 11 13.2.2 test simulator',
        '/dest',
      ]
    )

  @mock.patch.object(
    iossim_util, 'is_device_with_udid_simulator', autospec=True
  )
  @mock.patch('subprocess.check_output', autospec=True)
  def test_delete_simulator_by_udid(
    self, subprocess_mock, is_simulator_mock, _, _2
  ):
    """Ensures that command is correct."""
    iossim_util.delete_simulator_by_udid('UDID')
    self.assertEqual(
      ['xcrun', 'simctl', 'delete', 'UDID'], subprocess_mock.call_args[0][0]
    )
    is_simulator_mock.cache_clear.assert_called_with()

  @mock.patch('subprocess.check_output', autospec=True)
  def test_uninstall_app(self, subprocess_mock, _, _2):
    """Tests that uninstall_app invokes `xcrun simctl uninstall`."""
    iossim_util.uninstall_app('UDID', 'org.chromium.runner')
    subprocess_mock.assert_called_once_with(
      ['xcrun', 'simctl', 'uninstall', 'UDID', 'org.chromium.runner'],
      stderr=subprocess.STDOUT,
    )

  @mock.patch('subprocess.check_output', autospec=True)
  def test_uninstall_app_with_path(self, subprocess_mock, _, _2):
    """Tests that uninstall_app passes `--set` when `path` is provided."""
    iossim_util.uninstall_app('UDID', 'org.chromium.runner', path='/simset')
    subprocess_mock.assert_called_once_with(
      [
        'xcrun',
        'simctl',
        '--set',
        '/simset',
        'uninstall',
        'UDID',
        'org.chromium.runner',
      ],
      stderr=subprocess.STDOUT,
    )

  @mock.patch('subprocess.check_output', autospec=True)
  def test_uninstall_app_ignores_error(self, subprocess_mock, _, _2):
    """Tests that uninstall_app logs a warning instead of raising on error."""
    subprocess_mock.side_effect = subprocess.CalledProcessError(
      1, ['xcrun', 'simctl', 'uninstall'], output=b'not installed'
    )
    with self.assertLogs(iossim_util.LOGGER, level='WARNING') as logs:
      iossim_util.uninstall_app('UDID', 'org.chromium.runner')
    self.assertTrue(any('not installed' in msg for msg in logs.output))

  @mock.patch('subprocess.check_call', autospec=True)
  @mock.patch.object(iossim_util, 'shutdown_simulator_by_udid', autospec=True)
  def test_wipe_simulator_by_platform_and_version(
    self, mock_shutdown, subprocess_mock, _, _2
  ):
    """Ensures that command is correct."""
    iossim_util.wipe_simulator_by_udid('A4E66321-177A-450A-9BA1-488D85B7278E')
    self.assertEqual(
      ['xcrun', 'simctl', 'erase', 'A4E66321-177A-450A-9BA1-488D85B7278E'],
      subprocess_mock.call_args[0][0],
    )
    mock_shutdown.assert_called_with(
      'A4E66321-177A-450A-9BA1-488D85B7278E', None
    )

  @mock.patch('subprocess.check_output', autospec=True)
  def test_get_home_directory(self, subprocess_mock, _, _2):
    """Ensures that command is correct."""
    subprocess_mock.return_value = b'HOME_DIRECTORY'
    self.assertEqual(
      'HOME_DIRECTORY', iossim_util.get_home_directory('iPhone 11', '13.2.2')
    )
    self.assertEqual(
      [
        'xcrun',
        'simctl',
        'getenv',
        'A4E66321-177A-450A-9BA1-488D85B7278E',
        'HOME',
      ],
      subprocess_mock.call_args[0][0],
    )

  @mock.patch.object(iossim_util, 'create_device_by_platform_and_version')
  def test_no_new_sim_created_when_one_exists(self, mock_create, _, _2):
    """Ensures no simulator is created when one in desired dimension exists."""
    self.assertEqual(
      'A4E66321-177A-450A-9BA1-488D85B7278E',
      iossim_util.get_simulator('iPhone 11', '13.2.2'),
    )
    self.assertFalse(mock_create.called)

  def test_copy_cert(self, _, _2):
    """Ensures right commands are issued to copy cert"""
    self.mock(os.path, 'exists', lambda *args: True)

    check_call_mock = mock.Mock()
    self.mock(subprocess, 'check_call', check_call_mock)
    iossim_util.copy_trusted_certificate('test/cert', 'UDID')

    calls = [
      mock.call(['xcrun', 'simctl', 'boot', 'UDID']),
      mock.call(
        ['xcrun', 'simctl', 'keychain', 'UDID', 'add-root-cert', 'test/cert']
      ),
      mock.call(['xcrun', 'simctl', 'shutdown', 'UDID']),
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

  def test_get_simulator_runtime_info(self, mock_runtime_list, _2):
    runtime = iossim_util.get_simulator_runtime_info(
      constants.IOSPlatformType.IPHONEOS, '13.1'
    )
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['version'], '13.1')

    runtime = iossim_util.get_simulator_runtime_info(
      constants.IOSPlatformType.TVOS, '13.1'
    )
    self.assertIsNone(runtime)

    runtime = iossim_util.get_simulator_runtime_info(
      constants.IOSPlatformType.TVOS, '18.2'
    )
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['version'], '18.2')

    runtimes_with_variants = deepcopy(RUNTIMES_LIST)
    runtimes_with_variants['HHHHHH'] = {
      'build': '24B91016s',
      'deletable': True,
      'identifier': 'HHHHHH',
      'kind': 'Patchable Cryptex Disk Image',
      'platformIdentifier': 'com.apple.platform.iphonesimulator',
      'runtimeBundlePath': 'path/to/bundle/runtime/iOS 27.2.simruntime',
      'runtimeIdentifier': (
        'com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4'
      ),
      'signatureState': 'Verified',
      'sizeBytes': 7800000000,
      'state': 'Ready',
      'version': '27.2',
    }
    runtimes_with_variants['IIIIII'] = {
      'build': '24B114',
      'deletable': True,
      'identifier': 'IIIIII',
      'kind': 'Patchable Cryptex Disk Image',
      'platformIdentifier': 'com.apple.platform.iphonesimulator',
      'runtimeBundlePath': 'path/to/bundle/runtime/iOS 27.2.simruntime',
      'runtimeIdentifier': 'com.apple.CoreSimulator.SimRuntime.iOS-27-2',
      'signatureState': 'Verified',
      'sizeBytes': 7700000000,
      'state': 'Ready',
      'version': '27.2',
    }
    mock_runtime_list.return_value = runtimes_with_variants

    # Ensure standard 27.2 query skips the iPhone19-4 variant even when listed
    # first in RUNTIMES_LIST.
    runtime = iossim_util.get_simulator_runtime_info(
      constants.IOSPlatformType.IPHONEOS, '27.2'
    )
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['identifier'], 'IIIIII')
    self.assertEqual(
      runtime['runtimeIdentifier'],
      'com.apple.CoreSimulator.SimRuntime.iOS-27-2',
    )

    # Ensure variant-suffixed query returns only the iPhone19-4 variant.
    runtime = iossim_util.get_simulator_runtime_info(
      constants.IOSPlatformType.IPHONEOS, '27.2-iphone19-4'
    )
    self.assertIsNotNone(runtime)
    self.assertEqual(runtime['identifier'], 'HHHHHH')
    self.assertEqual(
      runtime['runtimeIdentifier'],
      'com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4',
    )

  @mock.patch.object(iossim_util, 'get_simulator_runtime_match_list')
  def test_override_default_iphonesim_runtime(
    self, override_runtime_mock, _, _2
  ):
    check_call_mock = mock.Mock()
    self.mock(subprocess, 'check_call', check_call_mock)
    override_runtime_mock.return_value = RUNTIMES_MATCH_LIST
    iossim_util.override_default_iphonesim_runtime(ADD_RUNTIME_OUTPUT, '17.0')
    calls = [
      mock.call(
        [
          'xcrun',
          'simctl',
          'runtime',
          'match',
          'set',
          'iphoneos17.0',
          '21A111111',
          '--sdkBuild',
          '21A111112',
        ]
      ),
    ]
    check_call_mock.assert_has_calls(calls)

  @mock.patch.object(iossim_util, 'get_simulator_runtime_match_list')
  def test_override_default_iphonesim_runtime_not_found(
    self, override_runtime_mock, _, _2
  ):
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
      mock.call(
        ['xcrun', 'simctl', 'runtime', 'add', dmg_path, '--verbose'],
        stderr=subprocess.STDOUT,
      ),
    ]

    check_output_mock.assert_has_calls(calls)
    self.assertEqual(check_output_mock.call_count, 1)

  def test_delete_simulator_runtime(self, _, _2):
    check_output_mock = mock.Mock()
    self.mock(subprocess, 'check_output', check_output_mock)
    runtime_id = '111111'
    iossim_util.delete_simulator_runtime(runtime_id)

    calls = [
      mock.call(
        ['xcrun', 'simctl', 'runtime', 'delete', runtime_id],
        stderr=subprocess.STDOUT,
      ),
    ]

    check_output_mock.assert_has_calls(calls)
    self.assertEqual(check_output_mock.call_count, 1)

  def test_delete_simulator_runtime_already_deleted(self, _, _2):
    error_output = (
      b'Cannot stage disk image or bundle for delete '
      b'Underlying error (domain=NSCocoaErrorDomain, code=4):'
      b'The file doesn\xe2\x80\x99t exist.'
    )
    check_output_mock = mock.Mock(
      side_effect=subprocess.CalledProcessError(1, 'cmd', error_output)
    )
    self.mock(subprocess, 'check_output', check_output_mock)
    runtime_id = '111111'
    # This should not raise an exception.
    iossim_util.delete_simulator_runtime(runtime_id)

  def test_delete_simulator_runtime_other_error(self, _, _2):
    error_output = b'Some other error'
    check_output_mock = mock.Mock(
      side_effect=subprocess.CalledProcessError(1, 'cmd', error_output)
    )
    self.mock(subprocess, 'check_output', check_output_mock)
    runtime_id = '111111'
    with self.assertRaises(subprocess.CalledProcessError):
      iossim_util.delete_simulator_runtime(runtime_id)

  def test_delete_least_recently_used_simulator_runtimes(self, _, _2):
    with mock.patch(
      'iossim_util.delete_simulator_runtime'
    ) as mock_delete_simulator_runtime:
      iossim_util.delete_least_recently_used_simulator_runtimes(1)

      calls = [
        mock.call('444444', True),
        mock.call('555555', True),
      ]

      self.assertEqual(mock_delete_simulator_runtime.call_count, 2)
      mock_delete_simulator_runtime.assert_has_calls(calls, any_order=True)

  def test_delete_least_recently_used_simulator_runtimes_no_op(self, _, _2):
    with mock.patch(
      'iossim_util.delete_simulator_runtime'
    ) as mock_delete_simulator_runtime:
      iossim_util.delete_least_recently_used_simulator_runtimes()

      self.assertEqual(mock_delete_simulator_runtime.call_count, 0)

  def test_delete_stale_simulator_runtimes(self, _, _2):
    with mock.patch(
      'iossim_util.delete_simulator_runtime'
    ) as mock_delete_simulator_runtime:
      iossim_util.delete_stale_simulator_runtimes()

      calls = [  # noqa: F841
        mock.call('GGGGGG', True),
      ]

      self.assertEqual(mock_delete_simulator_runtime.call_count, 1)

  @mock.patch('os.path.isdir')
  @mock.patch('subprocess.check_call')
  def test_remove_stale_simulators_from_cache(
    self, mock_check_call, mock_isdir, _, _2
  ):
    mock_isdir.return_value = True
    iossim_util.remove_stale_simulators_from_cache()
    mock_isdir.assert_called_once_with(iossim_util.SIMULATOR_CACHE_PATH)
    mock_check_call.assert_called_once_with(
      [
        'xcrun',
        'simctl',
        '--set',
        iossim_util.SIMULATOR_CACHE_PATH,
        'delete',
        'unavailable',
      ]
    )

  @mock.patch('subprocess.check_call')
  def test_update_dyld_shared_cache(self, mock_check_call, _, _2):
    iossim_util.update_dyld_shared_cache()
    mock_check_call.assert_called_once_with(
      ['xcrun', 'simctl', 'runtime', 'dyld_shared_cache', 'update', '--all']
    )

  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch.object(plistlib, 'dump')
  @mock.patch.object(plistlib, 'load')
  def test_disable_hardware_keyboard(
    self, mock_load, mock_dump, mock_open, _, _2
  ):
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
      'DevicePreferences': {udid: {'SimulatorExternalDisplay': 2114}}
    }
    expected = deepcopy(load_return_val)
    expected['DevicePreferences'][udid]['ConnectHardwareKeyboard'] = False
    mock_load.return_value = load_return_val
    iossim_util.disable_hardware_keyboard(udid)
    mock_load.assert_called()
    mock_dump.assert_called_with(expected, mock_open(), fmt=plistlib.FMT_BINARY)

  def test_ensure_simulator_fully_booted(self, _, _2):
    with mock.patch('iossim_util.is_device_with_udid_simulator') as mock_is_sim:
      # Check to ensure correct calls made when simulator exists
      mock_is_sim.return_value = True

      udid = 'E4E66320-177A-450A-9BA1-488D85B7278E'

      check_call_mock = mock.Mock()
      self.mock(subprocess, 'check_call', check_call_mock)
      runtime = 'com.apple.CoreSimulator.SimRuntime.iOS-11-4'
      dyld_cmd = [
        'xcrun',
        'simctl',
        'runtime',
        'dyld_shared_cache',
        'update',
        runtime,
      ]
      check_calls = [
        mock.call(dyld_cmd),
        mock.call(['xcrun', 'simctl', 'bootstatus', udid, '-b'], timeout=120),
        mock.call(dyld_cmd),
        mock.call(
          ['xcrun', 'simctl', '--set', '/path', 'bootstatus', udid, '-b'],
          timeout=120,
        ),
      ]
      self.assertTrue(iossim_util.ensure_simulator_fully_booted(udid))
      self.assertTrue(
        iossim_util.ensure_simulator_fully_booted(udid, path='/path')
      )

      check_call_mock.assert_has_calls(check_calls)

      # Ensure exception raised when simulator doesn't exist
      mock_is_sim.return_value = False
      with self.assertRaises(test_runner.SimulatorNotFoundError):
        iossim_util.ensure_simulator_fully_booted(udid)

      # Ensure hitting timeout does not prohibit program flow.
      mock_is_sim.return_value = True

      # Mock wipe and kill
      mock_wipe = mock.Mock()
      self.mock(iossim_util, 'wipe_simulator_by_udid', mock_wipe)
      mock_kill = mock.Mock()
      self.mock(test_runner.SimulatorTestRunner, 'kill_simulators', mock_kill)

      check_call_mock.side_effect = subprocess.TimeoutExpired(
        cmd=["cmd"], timeout=120
      )

      self.assertFalse(iossim_util.ensure_simulator_fully_booted(udid))
      self.assertEqual(mock_wipe.call_count, 1)
      self.assertEqual(mock_kill.call_count, 1)

      mock_wipe.reset_mock()
      mock_kill.reset_mock()

      self.assertFalse(
        iossim_util.ensure_simulator_fully_booted(udid, num_attempts=2)
      )
      self.assertEqual(mock_wipe.call_count, 2)
      self.assertEqual(mock_kill.call_count, 2)

  def test_disable_simulator_keyboard_tutorial(self, _, _2):
    with mock.patch(
      'iossim_util.boot_simulator_if_not_booted'
    ) as mock_boot_simulator:
      # boots successfully
      mock_boot_simulator.return_value = None

      check_call_mock = mock.Mock()
      self.mock(subprocess, 'check_call', check_call_mock)
      udid = "1111111"
      check_calls = [
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'DidShowContinuousPathIntroduction',
            '1',
          ]
        ),
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'KeyboardDidShowProductivityTutorial',
            '1',
          ]
        ),
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'DidShowGestureKeyboardIntroduction',
            '1',
          ]
        ),
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'UIKeyboardDidShowInternationalInfoIntroduction',
            '1',
          ]
        ),
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'KeyboardAutocorrection',
            '0',
          ]
        ),
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'KeyboardPrediction',
            '0',
          ]
        ),
        mock.call(
          [
            'xcrun',
            'simctl',
            'spawn',
            udid,
            'defaults',
            'write',
            'com.apple.keyboard.preferences',
            'KeyboardShowPredictionBar',
            '0',
          ]
        ),
      ]

      iossim_util.disable_simulator_keyboard_tutorial(udid)
      check_call_mock.assert_has_calls(check_calls)


class DisableSimulatorDaemonsTest(test_runner_test.TestCase):
  """Tests for per-device simulator launchd daemon disabling."""

  def setUp(self):
    super(DisableSimulatorDaemonsTest, self).setUp()
    iossim_util.set_simulator_daemon_disabling_override(None)
    self.addCleanup(iossim_util.set_simulator_daemon_disabling_override, None)
    env_patcher = mock.patch.dict(os.environ, {}, clear=False)
    env_patcher.start()
    self.addCleanup(env_patcher.stop)
    os.environ.pop(iossim_util.IOS_DISABLE_SIMULATOR_DAEMONS_ENV, None)
    self.mock(
      iossim_util, '_is_simulator_booted', lambda *args, **kwargs: False
    )

  def _wait_for_path(self, path: str, timeout: float = 1.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
      if os.path.exists(path):
        return True
      time.sleep(0.01)
    return os.path.exists(path)

  def test_disabled_jobs_invariants(self):
    """Verifies allowlist invariants and exclusion of required jobs."""
    jobs = iossim_util.DISABLED_SIMULATOR_LAUNCHD_JOBS
    self.assertIsInstance(jobs, tuple)
    self.assertGreater(len(jobs), 0)
    self.assertEqual(len(jobs), len(set(jobs)))
    for job in jobs:
      self.assertTrue(job.startswith('com.apple.'), job)

    self.assertIn('com.apple.PosterBoard', jobs)

    excluded = (
      'com.apple.ScreenTimeAgent',
      'com.apple.ScreenTimeSettingsAgent',
      'com.apple.passd',
      'com.apple.financed',
      'com.apple.assetsd',
      'com.apple.medialibraryd',
      'com.apple.photoanalysisd',
      'com.apple.contactsd',
      'com.apple.calaccessd',
      'com.apple.searchd',
      'com.apple.searchtoold',
      'com.apple.corespotlightservice',
      'com.apple.sharingd',
      'com.apple.gamed',
      'com.apple.assistantd',
      'com.apple.intelligenceplatformd',
      'com.apple.sleepd',
      'com.apple.nanoregistryd',
      'com.apple.nanoregistrylaunchd',
      'com.apple.mobiletimerd',
      'com.apple.routined',
      'com.apple.biomed',
      'com.apple.donotdisturbd',
      'com.apple.appprotectiond',
      'com.apple.managedconfiguration.profiled',
    )
    for job in excluded:
      self.assertNotIn(job, jobs)

  def test_disable_simulator_daemons_writes_and_is_idempotent(self):
    """Writes valid XML `disabled.plist` and skips rewriting when unchanged."""
    udid = 'A4E66321-177A-450A-9BA1-488D85B7278E'
    with tempfile.TemporaryDirectory() as tmpdir:
      self.assertTrue(
        iossim_util.disable_simulator_daemons(udid, store_root=tmpdir)
      )
      plist_path = os.path.join(
        tmpdir, f'com.apple.CoreSimulator.SimDevice.{udid}', 'disabled.plist'
      )
      self.assertTrue(os.path.exists(plist_path))
      self.assertEqual(os.stat(plist_path).st_mode & 0o777, 0o644)

      with open(plist_path, 'rb') as f:
        raw = f.read()
        self.assertIn(b'<?xml', raw)
        f.seek(0)
        parsed = plistlib.load(f)

      for job in iossim_util.DISABLED_SIMULATOR_LAUNCHD_JOBS:
        self.assertIs(parsed[job], True)

      mtime_before = os.stat(plist_path).st_mtime_ns
      self.assertFalse(
        iossim_util.disable_simulator_daemons(udid, store_root=tmpdir)
      )
      self.assertEqual(os.stat(plist_path).st_mtime_ns, mtime_before)

  def test_disable_simulator_daemons_preserves_existing_entries(self):
    """Preserves pre-existing `<false/>` and unmanaged entries across merges."""
    udid = 'E4E66320-177A-450A-9BA1-488D85B7278E'
    with tempfile.TemporaryDirectory() as tmpdir:
      store_dir = os.path.join(
        tmpdir, f'com.apple.CoreSimulator.SimDevice.{udid}'
      )
      os.makedirs(store_dir)
      plist_path = os.path.join(store_dir, 'disabled.plist')
      initial = {
        'com.apple.assistantd': False,
        'com.apple.custom.service': False,
        'com.apple.PosterBoard': False,
      }
      with open(plist_path, 'wb') as f:
        plistlib.dump(initial, f, fmt=plistlib.FMT_XML, sort_keys=True)

      self.assertTrue(
        iossim_util.disable_simulator_daemons(udid, store_root=tmpdir)
      )
      with open(plist_path, 'rb') as f:
        merged = plistlib.load(f)

      self.assertIs(merged['com.apple.assistantd'], False)
      self.assertIs(merged['com.apple.custom.service'], False)
      self.assertIs(merged['com.apple.PosterBoard'], True)

  def test_disable_simulator_daemons_invalid_udid_and_platform_gate(self):
    """Rejects non-UUID inputs and defaults to no-op outside macOS."""
    with tempfile.TemporaryDirectory() as tmpdir:
      self.assertFalse(
        iossim_util.disable_simulator_daemons('', store_root=tmpdir)
      )
      self.assertFalse(
        iossim_util.disable_simulator_daemons('UDID', store_root=tmpdir)
      )
      self.assertFalse(
        iossim_util.disable_simulator_daemons('not-a-uuid', store_root=tmpdir)
      )
      self.assertEqual(os.listdir(tmpdir), [])

      valid_udid = '11111111-2222-3333-4444-555555555555'
      with mock.patch.object(iossim_util.sys, 'platform', 'linux'):
        self.assertFalse(iossim_util.disable_simulator_daemons(valid_udid))

      with (
        mock.patch.object(iossim_util.sys, 'platform', 'darwin'),
        mock.patch.object(iossim_util, 'SIMULATOR_DISABLED_STORE_ROOT', tmpdir),
      ):
        self.assertTrue(iossim_util.disable_simulator_daemons(valid_udid))
        plist_path = os.path.join(
          tmpdir,
          f'com.apple.CoreSimulator.SimDevice.{valid_udid}',
          'disabled.plist',
        )
        self.assertTrue(os.path.exists(plist_path))

  def test_disable_simulator_daemons_swallows_errors(self):
    """Logs and returns False on filesystem or plist parse errors."""
    udid = '11111111-2222-3333-4444-555555555555'
    with tempfile.TemporaryDirectory() as tmpdir:
      store_dir = os.path.join(
        tmpdir, f'com.apple.CoreSimulator.SimDevice.{udid}'
      )
      os.makedirs(store_dir)
      plist_path = os.path.join(store_dir, 'disabled.plist')
      with open(plist_path, 'wb') as f:
        f.write(b'not valid plist xml')

      self.assertFalse(
        iossim_util.disable_simulator_daemons(udid, store_root=tmpdir)
      )

  @mock.patch.object(iossim_util, 'disable_simulator_daemons', autospec=True)
  def test_get_simulator_disables_daemons(self, mock_disable):
    """Calls `disable_simulator_daemons` when reusing an existing simulator."""
    sim_udid = 'A4E66321-177A-450A-9BA1-488D85B7278E'
    with mock.patch.object(
      iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST
    ):
      self.assertEqual(
        iossim_util.get_simulator('iPhone 11', '13.2.2'), sim_udid
      )
    mock_disable.assert_called_once_with(sim_udid)

  @mock.patch.object(iossim_util, 'disable_simulator_daemons', autospec=True)
  def test_create_device_disables_daemons(self, mock_disable):
    """Calls `disable_simulator_daemons` when creating a simulator."""
    new_udid = '11111111-1111-1111-1111-111111111111'
    with (
      mock.patch.object(
        iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST
      ),
      mock.patch.object(
        iossim_util, 'is_device_with_udid_simulator', return_value=True
      ),
      mock.patch('subprocess.check_output', return_value=new_udid.encode()),
    ):
      created = iossim_util._create_device_by_platform_and_version(
        'iPhone 11', '13.2.2', path='/tmp/set'
      )
    self.assertEqual(created, new_udid)
    mock_disable.assert_called_once_with(
      new_udid, simset_path='/tmp/set', check_booted=False
    )

  @mock.patch.object(iossim_util, 'disable_simulator_daemons', autospec=True)
  def test_clone_simulator_disables_daemons(self, mock_disable):
    """Calls `disable_simulator_daemons` for cloned simulators."""
    sim_udid = 'A4E66321-177A-450A-9BA1-488D85B7278E'
    clone_udid = '22222222-2222-2222-2222-222222222222'
    with mock.patch(
      'subprocess.check_output', return_value=clone_udid.encode()
    ):
      cloned = iossim_util.clone_simulator_by_udid(
        sim_udid, 'clone', path='/src', dest_path='/dest'
      )
      self.assertEqual(cloned, clone_udid)
      mock_disable.assert_called_once_with(
        clone_udid, simset_path='/dest', check_booted=False
      )

      mock_disable.reset_mock()
      iossim_util.clone_simulator_by_udid(sim_udid, 'clone', path='/src')
      mock_disable.assert_called_once_with(
        clone_udid, simset_path='/src', check_booted=False
      )

  @mock.patch.object(iossim_util, 'disable_simulator_daemons', autospec=True)
  def test_boot_simulator_if_not_booted_disables_daemons(self, mock_disable):
    """Configures daemons before booting Shutdown simulators only."""
    sim_udid = 'A4E66321-177A-450A-9BA1-488D85B7278E'
    with (
      mock.patch.object(
        iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST
      ),
      mock.patch('subprocess.check_output', return_value=b''),
    ):
      iossim_util.boot_simulator_if_not_booted(sim_udid, path='/tmp/set')
    mock_disable.assert_called_once_with(
      sim_udid, simset_path='/tmp/set', check_booted=False
    )

    mock_disable.reset_mock()
    booted_list = deepcopy(SIMULATORS_LIST)
    booted_list['devices']['com.apple.CoreSimulator.SimRuntime.iOS-13-2'][1][
      'state'
    ] = 'Booted'
    with mock.patch.object(
      iossim_util, 'get_simulator_list', return_value=booted_list
    ):
      iossim_util.boot_simulator_if_not_booted(sim_udid, path='/tmp/set')
    self.assertFalse(mock_disable.called)

  @mock.patch.object(iossim_util, 'disable_simulator_daemons', autospec=True)
  def test_ensure_simulator_fully_booted_disables_daemons(self, mock_disable):
    """Calls `disable_simulator_daemons` before running `bootstatus`."""
    sim_udid = 'A4E66321-177A-450A-9BA1-488D85B7278E'
    with (
      mock.patch.object(
        iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST
      ),
      mock.patch.object(
        iossim_util, 'is_device_with_udid_simulator', return_value=True
      ),
      mock.patch.object(iossim_util, 'update_dyld_shared_cache'),
      mock.patch('subprocess.check_call'),
    ):
      self.assertTrue(
        iossim_util.ensure_simulator_fully_booted(sim_udid, path='/tmp/set')
      )
    mock_disable.assert_called_once_with(sim_udid, simset_path='/tmp/set')

  @mock.patch.object(iossim_util, 'disable_simulator_daemons', autospec=True)
  def test_copy_trusted_certificate_disables_daemons(self, mock_disable):
    """Calls `disable_simulator_daemons` before booting to add a root cert."""
    sim_udid = 'A4E66321-177A-450A-9BA1-488D85B7278E'
    with (
      mock.patch('os.path.exists', return_value=True),
      mock.patch('subprocess.check_call'),
    ):
      iossim_util.copy_trusted_certificate('test/cert', sim_udid)
    mock_disable.assert_called_once_with(sim_udid)

  def test_disable_simulator_daemons_skips_when_booted(self):
    """Skips touching `disabled.plist` when `check_booted` detects Booted."""
    udid = '11111111-2222-3333-4444-555555555555'
    with tempfile.TemporaryDirectory() as tmpdir:
      plist_path = os.path.join(
        tmpdir, f'com.apple.CoreSimulator.SimDevice.{udid}', 'disabled.plist'
      )
      with mock.patch.object(
        iossim_util, '_is_simulator_booted', return_value=True
      ) as mock_booted:
        self.assertFalse(
          iossim_util.disable_simulator_daemons(
            udid, store_root=tmpdir, simset_path='/tmp/set', check_booted=True
          )
        )
        mock_booted.assert_called_once_with(udid, simset_path='/tmp/set')
        self.assertFalse(os.path.exists(plist_path))

        mock_booted.reset_mock()
        self.assertTrue(
          iossim_util.disable_simulator_daemons(
            udid, store_root=tmpdir, simset_path='/tmp/set', check_booted=False
          )
        )
        mock_booted.assert_not_called()
        self.assertTrue(os.path.exists(plist_path))

  def test_is_simulator_booted_plist_and_simctl(self):
    """Checks `device.plist` `Booted` state, `simctl` JSON, and debug logs."""
    self.unmock(iossim_util, '_is_simulator_booted')
    udid = '11111111-2222-3333-4444-555555555555'

    with tempfile.TemporaryDirectory() as simset_dir:
      dev_dir = os.path.join(simset_dir, udid)
      os.makedirs(dev_dir)
      device_plist = os.path.join(dev_dir, 'device.plist')

      # Fast path via device.plist state == SIMULATOR_PLIST_STATE_BOOTED (3).
      with open(device_plist, 'wb') as f:
        plistlib.dump({'state': iossim_util.SIMULATOR_PLIST_STATE_BOOTED}, f)
      with mock.patch('subprocess.check_output') as mock_subproc:
        self.assertTrue(
          iossim_util._is_simulator_booted(udid, simset_path=simset_dir)
        )
        mock_subproc.assert_not_called()

      # Corrupt device.plist logs at DEBUG and falls back to simctl.
      for corrupt_bytes in (b'not valid plist xml', b'<plist><dict></plist>'):
        with open(device_plist, 'wb') as f:
          f.write(corrupt_bytes)
        booted_json = json.dumps(
          {'devices': {'rt': [{'udid': udid, 'state': 'Booted'}]}}
        ).encode('utf-8')
        with (
          mock.patch('subprocess.check_output', return_value=booted_json),
          self.assertLogs(iossim_util.LOGGER, level='DEBUG') as cm,
        ):
          self.assertTrue(
            iossim_util._is_simulator_booted(udid, simset_path=simset_dir)
          )
        self.assertTrue(any('device.plist' in msg for msg in cm.output))

      # Shutdown device.plist falls through to simctl and handles errors.
      with open(device_plist, 'wb') as f:
        plistlib.dump({'state': 1}, f)
      shutdown_json = json.dumps(
        {'devices': {'rt': [{'udid': udid, 'state': 'Shutdown'}]}}
      ).encode('utf-8')
      with mock.patch(
        'subprocess.check_output', return_value=shutdown_json
      ) as mock_subproc:
        self.assertFalse(
          iossim_util._is_simulator_booted(udid, simset_path=simset_dir)
        )
        mock_subproc.assert_called_once_with(
          [
            'xcrun',
            'simctl',
            '--set',
            simset_dir,
            'list',
            'devices',
            'booted',
            '-j',
          ],
          stderr=subprocess.DEVNULL,
        )

      for err in (
        OSError('no simctl'),
        subprocess.CalledProcessError(1, 'simctl'),
        b'not json',
      ):
        if isinstance(err, bytes):
          patch_ctx = mock.patch('subprocess.check_output', return_value=err)
        else:
          patch_ctx = mock.patch('subprocess.check_output', side_effect=err)
        with (
          patch_ctx,
          self.assertLogs(iossim_util.LOGGER, level='DEBUG') as cm,
        ):
          self.assertFalse(
            iossim_util._is_simulator_booted(udid, simset_path=simset_dir)
          )
        self.assertTrue(any('simctl booted state' in msg for msg in cm.output))

    # Missing simset_path returns False without invoking simctl.
    with mock.patch('subprocess.check_output') as mock_subproc:
      self.assertFalse(
        iossim_util._is_simulator_booted(
          udid, simset_path=os.path.join(simset_dir, 'missing')
        )
      )
      mock_subproc.assert_not_called()

    # Default simset (simset_path=None) runs simctl without --set.
    booted_json = json.dumps(
      {'devices': {'rt': [{'udid': udid, 'state': 'Booted'}]}}
    ).encode('utf-8')
    with mock.patch(
      'subprocess.check_output', return_value=booted_json
    ) as mock_subproc:
      self.assertTrue(iossim_util._is_simulator_booted(udid))
      mock_subproc.assert_called_once_with(
        ['xcrun', 'simctl', 'list', 'devices', 'booted', '-j'],
        stderr=subprocess.DEVNULL,
      )

  def test_watch_simulator_clones_polling(self):
    """Watches clone directory via polling fallback and cleans up thread."""
    with (
      tempfile.TemporaryDirectory() as device_set_dir,
      tempfile.TemporaryDirectory() as store_root,
    ):
      non_uuid_dir = os.path.join(device_set_dir, 'not-a-uuid')
      os.makedirs(non_uuid_dir)
      uuid_file = os.path.join(
        device_set_dir, '33333333-3333-3333-3333-333333333333'
      )
      with open(uuid_file, 'w') as f:
        f.write('')

      pre_clone = '44444444-4444-4444-4444-444444444444'
      os.makedirs(os.path.join(device_set_dir, pre_clone))

      fake_select = types.SimpleNamespace()
      created_threads: list[threading.Thread] = []
      real_thread = threading.Thread

      def _track_thread(*args, **kwargs):
        t = real_thread(*args, **kwargs)
        created_threads.append(t)
        return t

      new_clone = str(uuid.uuid4()).upper()
      with (
        mock.patch.object(iossim_util, 'select', fake_select),
        mock.patch.object(threading, 'Thread', side_effect=_track_thread),
        iossim_util.watch_simulator_clones_for_daemon_disabling(
          device_set_dir, store_root=store_root, poll_interval=0.01
        ),
      ):
        pre_plist = os.path.join(
          store_root,
          f'com.apple.CoreSimulator.SimDevice.{pre_clone}',
          'disabled.plist',
        )
        self.assertTrue(self._wait_for_path(pre_plist))

        os.makedirs(os.path.join(device_set_dir, new_clone))
        new_plist = os.path.join(
          store_root,
          f'com.apple.CoreSimulator.SimDevice.{new_clone}',
          'disabled.plist',
        )
        self.assertTrue(self._wait_for_path(new_plist))

      self.assertEqual(len(created_threads), 1)
      self.assertFalse(created_threads[0].is_alive())
      self.assertFalse(
        os.path.exists(
          os.path.join(
            store_root, 'com.apple.CoreSimulator.SimDevice.not-a-uuid'
          )
        )
      )
      self.assertFalse(
        os.path.exists(
          os.path.join(
            store_root,
            'com.apple.CoreSimulator.SimDevice.'
            '33333333-3333-3333-3333-333333333333',
          )
        )
      )

  def test_watch_simulator_clones_kqueue(self):
    """Registers VNODE write kqueue filter and closes descriptors on exit."""
    with (
      tempfile.TemporaryDirectory() as device_set_dir,
      tempfile.TemporaryDirectory() as store_root,
    ):
      clone_udid = str(uuid.uuid4()).upper()

      class _FakeKqueue:
        def __init__(self):
          self.closed = False
          self.registered = []
          self.wait_calls = 0

        def control(self, changelist, nevents, timeout=None):
          if changelist is not None:
            self.registered.extend(changelist)
            return []
          self.wait_calls += 1
          if self.wait_calls == 1:
            os.makedirs(os.path.join(device_set_dir, clone_udid), exist_ok=True)
            return ['write-event']
          time.sleep(0.02)
          return []

        def close(self):
          self.closed = True

      fake_kq = _FakeKqueue()
      fake_select = types.SimpleNamespace(
        kqueue=lambda: fake_kq,
        kevent=lambda fd, filter=0, flags=0, fflags=0: (
          fd,
          filter,
          flags,
          fflags,
        ),
        KQ_FILTER_VNODE=1,
        KQ_EV_ADD=2,
        KQ_EV_CLEAR=4,
        KQ_NOTE_WRITE=8,
      )

      real_open = os.open
      real_close = os.close
      opened_dir_fds: list[int] = []
      closed_fds: list[int] = []

      def _fake_open(path, flags, *args, **kwargs):
        if path == device_set_dir:
          opened_dir_fds.append(999)
          return 999
        return real_open(path, flags, *args, **kwargs)

      def _fake_close(fd):
        if fd == 999:
          closed_fds.append(fd)
          return None
        return real_close(fd)

      with (
        mock.patch.object(iossim_util, 'select', fake_select),
        mock.patch('os.open', side_effect=_fake_open),
        mock.patch('os.close', side_effect=_fake_close),
        iossim_util.watch_simulator_clones_for_daemon_disabling(
          device_set_dir, store_root=store_root
        ),
      ):
        plist_path = os.path.join(
          store_root,
          f'com.apple.CoreSimulator.SimDevice.{clone_udid}',
          'disabled.plist',
        )
        self.assertTrue(self._wait_for_path(plist_path))

      self.assertEqual(opened_dir_fds, [999])
      self.assertEqual(closed_fds, [999])
      self.assertTrue(fake_kq.closed)
      self.assertEqual(fake_kq.registered, [(999, 1, 2 | 4, 8)])

  def test_watch_simulator_clones_no_op_non_darwin_or_empty_path(self):
    """Skips watching outside macOS without `store_root` or device path."""
    with (
      mock.patch.object(iossim_util.sys, 'platform', 'linux'),
      mock.patch('os.makedirs') as mock_makedirs,
      mock.patch.object(threading, 'Thread') as mock_thread,
    ):
      with iossim_util.watch_simulator_clones_for_daemon_disabling('/tmp/x'):
        pass
      with iossim_util.watch_simulator_clones_for_daemon_disabling(
        '', store_root='/tmp/store'
      ):
        pass
      mock_makedirs.assert_not_called()
      mock_thread.assert_not_called()

  def test_daemon_disabling_default_on_and_opt_out_strips_stale_entries(self):
    """Enables disabling by default and strips stale entries on opt-out."""
    self.assertIs(iossim_util.DEFAULT_DISABLE_SIMULATOR_DAEMONS, True)
    udid = '11111111-2222-3333-4444-555555555555'
    clone_shutdown = '22222222-2222-3333-4444-555555555555'
    clone_booted = '33333333-2222-3333-4444-555555555555'
    override_udid = '99999999-2222-3333-4444-555555555555'
    with (
      tempfile.TemporaryDirectory() as device_set_dir,
      tempfile.TemporaryDirectory() as store_root,
    ):
      # Watch clones is active by default when neither env nor override is set.
      default_clone = 'AAAAAAAA-2222-3333-4444-555555555555'
      fake_select = types.SimpleNamespace()
      with (
        mock.patch.object(iossim_util, 'select', fake_select),
        iossim_util.watch_simulator_clones_for_daemon_disabling(
          device_set_dir, store_root=store_root, poll_interval=0.01
        ),
      ):
        os.makedirs(os.path.join(device_set_dir, default_clone))
        default_plist = os.path.join(
          store_root,
          f'com.apple.CoreSimulator.SimDevice.{default_clone}',
          'disabled.plist',
        )
        self.assertTrue(self._wait_for_path(default_plist))
      self.assertFalse(os.path.exists(default_plist))

      # Pre-populate Shutdown simulator, override target, and clones with no
      # env var or override set (enabled by default).
      for target in (udid, override_udid, clone_shutdown, clone_booted):
        self.assertTrue(
          iossim_util.disable_simulator_daemons(target, store_root=store_root)
        )
      os.makedirs(os.path.join(device_set_dir, clone_shutdown))
      os.makedirs(os.path.join(device_set_dir, clone_booted))

      # Reused simulator with unmanaged <false/> watch keys keeps only those.
      reused_udid = '44444444-2222-3333-4444-555555555555'
      reused_dir = os.path.join(
        store_root, f'com.apple.CoreSimulator.SimDevice.{reused_udid}'
      )
      reused_plist = os.path.join(reused_dir, 'disabled.plist')
      os.makedirs(reused_dir)
      with open(reused_plist, 'wb') as f:
        plistlib.dump(
          {
            'com.apple.PosterBoard': True,
            'com.apple.chronod': True,
            'com.apple.custom.watch': False,
          },
          f,
          fmt=plistlib.FMT_XML,
        )

      # Simulator with non-empty dir (e.g. syslogsock) keeps dir when empty.
      sock_udid = '55555555-2222-3333-4444-555555555555'
      self.assertTrue(
        iossim_util.disable_simulator_daemons(sock_udid, store_root=store_root)
      )
      sock_dir = os.path.join(
        store_root, f'com.apple.CoreSimulator.SimDevice.{sock_udid}'
      )
      with open(os.path.join(sock_dir, 'syslogsock'), 'w') as f:
        f.write('')

      with (
        mock.patch.dict(
          os.environ, {iossim_util.IOS_DISABLE_SIMULATOR_DAEMONS_ENV: '0'}
        ),
        mock.patch.object(threading, 'Thread') as mock_thread,
      ):
        # Clean simulator returns False with zero booted checks.
        missing_udid = '66666666-2222-3333-4444-555555555555'
        with mock.patch.object(
          iossim_util, '_is_simulator_booted'
        ) as mock_booted:
          self.assertFalse(
            iossim_util.disable_simulator_daemons(
              missing_udid, store_root=store_root
            )
          )
          mock_booted.assert_not_called()

        # Non-existent clone watch dir is a fast no-op with zero threads.
        missing_set = os.path.join(device_set_dir, 'missing')
        with iossim_util.watch_simulator_clones_for_daemon_disabling(
          missing_set, store_root=store_root
        ):
          pass
        self.assertFalse(os.path.exists(missing_set))

        # Reused Shutdown simulator removes empty store dir when only managed
        # entries existed.
        with self.assertLogs(iossim_util.LOGGER, level='INFO') as cm:
          self.assertFalse(
            iossim_util.disable_simulator_daemons(udid, store_root=store_root)
          )
        self.assertTrue(
          any('(daemon disabling not enabled)' in msg for msg in cm.output)
        )
        self.assertFalse(
          os.path.exists(
            os.path.join(
              store_root, f'com.apple.CoreSimulator.SimDevice.{udid}'
            )
          )
        )

        # Unlinks disabled.plist while preserving dir when extra files exist.
        self.assertFalse(
          iossim_util.disable_simulator_daemons(
            sock_udid, store_root=store_root
          )
        )
        self.assertFalse(
          os.path.exists(os.path.join(sock_dir, 'disabled.plist'))
        )
        self.assertTrue(os.path.isdir(sock_dir))

        # Preserves unmanaged <false/> entries when stripping managed keys.
        self.assertFalse(
          iossim_util.disable_simulator_daemons(
            reused_udid, store_root=store_root
          )
        )
        with open(reused_plist, 'rb') as f:
          self.assertEqual(plistlib.load(f), {'com.apple.custom.watch': False})

        # Skips Booted clones and cleans pre-existing Shutdown clones once
        # without spawning a watcher thread.
        with mock.patch.object(
          iossim_util,
          '_is_simulator_booted',
          side_effect=lambda u, simset_path=None: u == clone_booted,
        ):
          with iossim_util.watch_simulator_clones_for_daemon_disabling(
            device_set_dir, store_root=store_root
          ):
            pass
        mock_thread.assert_not_called()
        self.assertFalse(
          os.path.exists(
            os.path.join(
              store_root,
              f'com.apple.CoreSimulator.SimDevice.{clone_shutdown}',
            )
          )
        )
        self.assertTrue(
          os.path.exists(
            os.path.join(
              store_root,
              f'com.apple.CoreSimulator.SimDevice.{clone_booted}',
              'disabled.plist',
            )
          )
        )

      # Opt-out via set_simulator_daemon_disabling_override(False) also strips
      # pre-existing managed keys and skips clean stores.
      iossim_util.set_simulator_daemon_disabling_override(False)
      self.assertFalse(
        iossim_util.disable_simulator_daemons(
          override_udid, store_root=store_root
        )
      )
      self.assertFalse(
        os.path.exists(
          os.path.join(
            store_root, f'com.apple.CoreSimulator.SimDevice.{override_udid}'
          )
        )
      )
      clean_udid = 'BBBBBBBB-2222-3333-4444-555555555555'
      self.assertFalse(
        iossim_util.disable_simulator_daemons(clean_udid, store_root=store_root)
      )
      self.assertFalse(
        os.path.exists(
          os.path.join(
            store_root, f'com.apple.CoreSimulator.SimDevice.{clean_udid}'
          )
        )
      )

  def test_daemon_disabling_env_and_override_precedence(self):
    """Applies env var opt-in/out and gives Python override precedence."""
    env_name = iossim_util.IOS_DISABLE_SIMULATOR_DAEMONS_ENV
    udid_a = '77777777-2222-3333-4444-555555555555'
    udid_b = '88888888-2222-3333-4444-555555555555'
    udid_c = 'CCCCCCCC-2222-3333-4444-555555555555'
    with (
      tempfile.TemporaryDirectory() as store_root,
      mock.patch.dict(os.environ, {}, clear=False),
    ):
      os.environ[env_name] = '0'
      self.assertFalse(
        iossim_util.disable_simulator_daemons(udid_a, store_root=store_root)
      )

      iossim_util.set_simulator_daemon_disabling_override(True)
      self.assertTrue(
        iossim_util.disable_simulator_daemons(udid_a, store_root=store_root)
      )

      os.environ[env_name] = '1'
      iossim_util.set_simulator_daemon_disabling_override(False)
      self.assertFalse(
        iossim_util.disable_simulator_daemons(udid_b, store_root=store_root)
      )

      iossim_util.set_simulator_daemon_disabling_override(None)
      self.assertTrue(
        iossim_util.disable_simulator_daemons(udid_b, store_root=store_root)
      )

      os.environ.pop(env_name, None)
      self.assertTrue(
        iossim_util.disable_simulator_daemons(udid_c, store_root=store_root)
      )

  def test_cleanup_simulator_disabled_store_and_booted_guard(self):
    """Cleans up store dirs only when simulator is valid and not Booted."""
    udid = '11111111-2222-3333-4444-555555555555'
    with tempfile.TemporaryDirectory() as store_root:
      self.assertFalse(
        iossim_util.cleanup_simulator_disabled_store(
          'not-a-uuid', store_root=store_root
        )
      )
      self.assertFalse(
        iossim_util.cleanup_simulator_disabled_store(
          udid, store_root=store_root
        )
      )

      self.assertTrue(
        iossim_util.disable_simulator_daemons(udid, store_root=store_root)
      )
      store_dir = os.path.join(
        store_root, f'com.apple.CoreSimulator.SimDevice.{udid}'
      )
      self.assertTrue(os.path.isdir(store_dir))

      with mock.patch.object(
        iossim_util, '_is_simulator_booted', return_value=True
      ):
        self.assertFalse(
          iossim_util.cleanup_simulator_disabled_store(
            udid, store_root=store_root
          )
        )
        self.assertTrue(os.path.isdir(store_dir))

      with mock.patch.object(
        iossim_util, '_is_simulator_booted', return_value=False
      ):
        self.assertTrue(
          iossim_util.cleanup_simulator_disabled_store(
            udid, store_root=store_root
          )
        )
        self.assertFalse(os.path.exists(store_dir))

  def test_delete_simulator_by_udid_cleans_disabled_store(self):
    """Cleans disabled store after delete succeeds and skips on error."""
    udid = '11111111-2222-3333-4444-555555555555'
    with (
      mock.patch.object(
        iossim_util, 'cleanup_simulator_disabled_store'
      ) as mock_cleanup,
      mock.patch.object(
        iossim_util, 'is_device_with_udid_simulator'
      ) as is_simulator_mock,
      mock.patch(
        'subprocess.check_output', return_value=b''
      ) as subprocess_mock,
    ):
      iossim_util.delete_simulator_by_udid(udid, path='/tmp/set')
      mock_cleanup.assert_called_once_with(udid, simset_path='/tmp/set')
      is_simulator_mock.cache_clear.assert_called_once_with()

      mock_cleanup.reset_mock()
      subprocess_mock.side_effect = subprocess.CalledProcessError(1, 'simctl')
      iossim_util.delete_simulator_by_udid(udid)
      mock_cleanup.assert_not_called()

  def test_watch_simulator_clones_kqueue_error_falls_back_to_polling(self):
    """Falls back to polling on `kqueue.control` `OSError` and cleans clones."""
    with (
      tempfile.TemporaryDirectory() as device_set_dir,
      tempfile.TemporaryDirectory() as store_root,
    ):
      poll_clone = str(uuid.uuid4()).upper()
      booted_clone = str(uuid.uuid4()).upper()

      booted_clone_dir = os.path.join(device_set_dir, booted_clone)
      os.makedirs(booted_clone_dir)
      with open(os.path.join(booted_clone_dir, 'device.plist'), 'wb') as f:
        plistlib.dump({'state': iossim_util.SIMULATOR_PLIST_STATE_BOOTED}, f)
      booted_store_dir = os.path.join(
        store_root, f'com.apple.CoreSimulator.SimDevice.{booted_clone}'
      )
      os.makedirs(booted_store_dir)
      with open(os.path.join(booted_store_dir, 'syslogsock'), 'w') as f:
        f.write('')

      class _FailingWaitKqueue:
        def __init__(self):
          self.closed = False
          self.wait_called = threading.Event()

        def control(self, changelist, nevents, timeout=None):
          if changelist is not None:
            return []
          self.wait_called.set()
          raise OSError('mock kqueue error')

        def close(self):
          self.closed = True

      fake_kq = _FailingWaitKqueue()
      fake_select = types.SimpleNamespace(
        kqueue=lambda: fake_kq,
        kevent=lambda fd, filter=0, flags=0, fflags=0: (
          fd,
          filter,
          flags,
          fflags,
        ),
        KQ_FILTER_VNODE=1,
        KQ_EV_ADD=2,
        KQ_EV_CLEAR=4,
        KQ_NOTE_WRITE=8,
      )

      real_open = os.open
      real_close = os.close
      closed_fds: list[int] = []

      def _fake_open(path, flags, *args, **kwargs):
        if path == device_set_dir:
          return 998
        return real_open(path, flags, *args, **kwargs)

      def _fake_close(fd):
        if fd == 998:
          closed_fds.append(fd)
          return None
        return real_close(fd)

      with (
        mock.patch.object(iossim_util, 'select', fake_select),
        mock.patch('os.open', side_effect=_fake_open),
        mock.patch('os.close', side_effect=_fake_close),
        mock.patch.object(
          iossim_util,
          '_is_simulator_booted',
          side_effect=lambda u, simset_path=None: u == booted_clone,
        ),
        iossim_util.watch_simulator_clones_for_daemon_disabling(
          device_set_dir, store_root=store_root, poll_interval=0.01
        ),
      ):
        self.assertTrue(fake_kq.wait_called.wait(timeout=1.0))
        os.makedirs(os.path.join(device_set_dir, poll_clone))
        poll_plist = os.path.join(
          store_root,
          f'com.apple.CoreSimulator.SimDevice.{poll_clone}',
          'disabled.plist',
        )
        self.assertTrue(self._wait_for_path(poll_plist))

      self.assertEqual(closed_fds, [998])
      self.assertTrue(fake_kq.closed)
      # Non-booted clone disabled store removed on exit; booted clone kept
      # untouched without writing disabled.plist.
      self.assertFalse(
        os.path.exists(
          os.path.join(
            store_root, f'com.apple.CoreSimulator.SimDevice.{poll_clone}'
          )
        )
      )
      self.assertTrue(
        os.path.exists(os.path.join(booted_store_dir, 'syslogsock'))
      )
      self.assertFalse(
        os.path.exists(os.path.join(booted_store_dir, 'disabled.plist'))
      )

  def test_watch_simulator_clones_retries_transient_error_and_ignores_booted(
    self,
  ):
    """Retries failed writes without re-checking Booted or finished clones."""
    with (
      tempfile.TemporaryDirectory() as device_set_dir,
      tempfile.TemporaryDirectory() as store_root,
    ):
      booted_clone = str(uuid.uuid4()).upper()
      os.makedirs(os.path.join(device_set_dir, booted_clone))
      booted_store_dir = os.path.join(
        store_root, f'com.apple.CoreSimulator.SimDevice.{booted_clone}'
      )
      os.makedirs(booted_store_dir)
      with open(os.path.join(booted_store_dir, 'syslogsock'), 'w') as f:
        f.write('')

      retry_clone = str(uuid.uuid4()).upper()
      fake_select = types.SimpleNamespace()
      real_write = iossim_util._write_plist_atomically
      write_attempts = 0
      retry_succeeded = threading.Event()
      booted_checks: list[str] = []

      def _flaky_write(store_dir, store_path, payload):
        nonlocal write_attempts
        write_attempts += 1
        if write_attempts == 1:
          raise OSError('transient write failure')
        real_write(store_dir, store_path, payload)
        retry_succeeded.set()

      def _check_booted(udid, simset_path=None):
        booted_checks.append(udid)
        return udid == booted_clone

      with (
        mock.patch.object(iossim_util, 'select', fake_select),
        mock.patch.object(
          iossim_util, '_is_simulator_booted', side_effect=_check_booted
        ),
        mock.patch.object(
          iossim_util, '_write_plist_atomically', side_effect=_flaky_write
        ),
        iossim_util.watch_simulator_clones_for_daemon_disabling(
          device_set_dir, store_root=store_root, poll_interval=0.01
        ),
      ):
        os.makedirs(os.path.join(device_set_dir, retry_clone))
        self.assertTrue(retry_succeeded.wait(timeout=2.0))
        # Allow multiple subsequent poll intervals to verify no repeat work.
        time.sleep(0.05)
        self.assertEqual(booted_checks, [booted_clone])

      self.assertEqual(write_attempts, 2)
      self.assertEqual(booted_checks, [booted_clone, retry_clone])
      self.assertTrue(
        os.path.exists(os.path.join(booted_store_dir, 'syslogsock'))
      )
      self.assertFalse(
        os.path.exists(
          os.path.join(
            store_root, f'com.apple.CoreSimulator.SimDevice.{retry_clone}'
          )
        )
      )

  def test_watch_simulator_clones_skips_cleanup_if_watcher_alive(self):
    """Skips disabled store cleanup with warning if watcher stays alive."""
    with (
      tempfile.TemporaryDirectory() as device_set_dir,
      tempfile.TemporaryDirectory() as store_root,
    ):
      clone_udid = str(uuid.uuid4()).upper()
      os.makedirs(os.path.join(device_set_dir, clone_udid))
      fake_select = types.SimpleNamespace()
      real_thread_cls = threading.Thread
      real_threads: list[threading.Thread] = []

      class _AliveThreadProxy:
        def __init__(self, real_thread):
          self._real = real_thread

        def start(self):
          self._real.start()

        def join(self, timeout=None):
          pass

        def is_alive(self):
          return True

      def _make_proxy_thread(*args, **kwargs):
        real_thread = real_thread_cls(*args, **kwargs)
        real_threads.append(real_thread)
        return _AliveThreadProxy(real_thread)

      try:
        with (
          mock.patch.object(iossim_util, 'select', fake_select),
          mock.patch.object(
            threading, 'Thread', side_effect=_make_proxy_thread
          ),
          self.assertLogs(iossim_util.LOGGER, level='WARNING') as cm,
        ):
          with iossim_util.watch_simulator_clones_for_daemon_disabling(
            device_set_dir, store_root=store_root, poll_interval=0.01
          ):
            plist_path = os.path.join(
              store_root,
              f'com.apple.CoreSimulator.SimDevice.{clone_udid}',
              'disabled.plist',
            )
            self.assertTrue(self._wait_for_path(plist_path))
      finally:
        for t in real_threads:
          t.join(timeout=2.0)

      self.assertTrue(
        any('skipping disabled store cleanup' in msg for msg in cm.output)
      )
      self.assertTrue(os.path.exists(plist_path))


if __name__ == '__main__':
  unittest.main()
