#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for iossim_util.py."""

import mock
import unittest

import iossim_util
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
        }]
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
    ],
    'pairs': [],
    'runtimes': [
        {
            "buildversion": "15F79",
            "bundlePath": "/path/Runtimes/iOS 11.4.simruntime",
            "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-11-4",
            "isAvailable": True,
            "name": "iOS 11.4",
            "version": "11.4"
        },
        {
            "buildversion": "17A844",
            "bundlePath": "/path/Runtimes/iOS 13.1.simruntime",
            "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-1",
            "isAvailable": True,
            "name": "iOS 13.1",
            "version": "13.1"
        },
        {
            "buildversion": "17B102",
            "bundlePath": "/path/Runtimes/iOS.simruntime",
            "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-13-2",
            "isAvailable": True,
            "name": "iOS 13.2",
            "version": "13.2.2"
        },
    ]
}


@mock.patch.object(
    iossim_util, 'get_simulator_list', return_value=SIMULATORS_LIST)
class GetiOSSimUtil(test_runner_test.TestCase):
  """Tests for iossim_util.py."""

  def setUp(self):
    super(GetiOSSimUtil, self).setUp()

  def test_get_simulator_runtime_by_version(self, _):
    """Ensures correctness of filter."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
        iossim_util.get_simulator_runtime_by_version(
            iossim_util.get_simulator_list(), '13.2.2'))

  def test_get_simulator_runtime_by_version_not_found(self, _):
    """Ensures that SimulatorNotFoundError raises if no runtime."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_runtime_by_version(
          iossim_util.get_simulator_list(), '13.2')
    expected_message = ('Simulator does not exist: Not found '
                        '"13.2" SDK in runtimes')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_device_type_by_platform(self, _):
    """Ensures correctness of filter."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimDeviceType.iPhone-11',
        iossim_util.get_simulator_device_type_by_platform(
            iossim_util.get_simulator_list(), 'iPhone 11'))

  def test_get_simulator_device_type_by_platform_not_found(self, _):
    """Ensures that SimulatorNotFoundError raises if no platform."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_device_type_by_platform(
          iossim_util.get_simulator_list(), 'iPhone XI')
    expected_message = ('Simulator does not exist: Not found device '
                        '"iPhone XI" in devicetypes')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_runtime_by_device_udid(self, _):
    """Ensures correctness of filter."""
    self.assertEqual(
        'com.apple.CoreSimulator.SimRuntime.iOS-13-2',
        iossim_util.get_simulator_runtime_by_device_udid(
            'E4E66321-177A-450A-9BA1-488D85B7278E'))

  def test_get_simulator_runtime_by_device_udid_not_found(self, _):
    """Ensures that SimulatorNotFoundError raises if no device with UDID."""
    with self.assertRaises(test_runner.SimulatorNotFoundError) as context:
      iossim_util.get_simulator_runtime_by_device_udid('non_existing_UDID')
    expected_message = ('Simulator does not exist: Not found simulator with '
                        '"non_existing_UDID" UDID in devices')
    self.assertTrue(expected_message in str(context.exception))

  def test_get_simulator_udids_by_platform_and_version(self, _):
    """Ensures correctness of filter."""
    self.assertEqual(['A4E66321-177A-450A-9BA1-488D85B7278E'],
                     iossim_util.get_simulator_udids_by_platform_and_version(
                         'iPhone 11', '13.2.2'))

  def test_get_simulator_udids_by_platform_and_version_not_found(self, _):
    """Ensures that filter returns empty list if no device with version."""
    self.assertEqual([],
                     iossim_util.get_simulator_udids_by_platform_and_version(
                         'iPhone 11', '13.1'))

  @mock.patch('subprocess.check_output', autospec=True)
  def test_create_device_by_platform_and_version(self, subprocess_mock, _):
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
  def test_delete_simulator_by_udid(self, subprocess_mock, _):
    """Ensures that command is correct."""
    iossim_util.delete_simulator_by_udid('UDID')
    self.assertEqual(['xcrun', 'simctl', 'delete', 'UDID'],
                     subprocess_mock.call_args[0][0])

  @mock.patch('subprocess.check_call', autospec=True)
  def test_wipe_simulator_by_platform_and_version(self, subprocess_mock, _):
    """Ensures that command is correct."""
    iossim_util.wipe_simulator_by_udid('A4E66321-177A-450A-9BA1-488D85B7278E')
    self.assertEqual(
        ['xcrun', 'simctl', 'erase', 'A4E66321-177A-450A-9BA1-488D85B7278E'],
        subprocess_mock.call_args[0][0])

  @mock.patch('subprocess.check_output', autospec=True)
  def test_get_home_directory(self, subprocess_mock, _):
    """Ensures that command is correct."""
    subprocess_mock.return_value = b'HOME_DIRECTORY'
    self.assertEqual('HOME_DIRECTORY',
                     iossim_util.get_home_directory('iPhone 11', '13.2.2'))
    self.assertEqual([
        'xcrun', 'simctl', 'getenv', 'A4E66321-177A-450A-9BA1-488D85B7278E',
        'HOME'
    ], subprocess_mock.call_args[0][0])

  @mock.patch.object(iossim_util, 'create_device_by_platform_and_version')
  def test_no_new_sim_created_when_one_exists(self, mock_create, _):
    """Ensures no simulator is created when one in desired dimension exists."""
    self.assertEqual('A4E66321-177A-450A-9BA1-488D85B7278E',
                     iossim_util.get_simulator('iPhone 11', '13.2.2'))
    self.assertFalse(mock_create.called)

  def test_copy_cert(self, _):
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


if __name__ == '__main__':
  unittest.main()
