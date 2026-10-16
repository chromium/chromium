# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import unittest

from gpu_path_util import setup_telemetry_paths  # noqa: F401
from gpu_path_util import setup_tools_perf_paths  # noqa: F401

from telemetry.internal.platform import gpu_info as gi

from gpu_tests import info_collection_test


class InfoCollectionTestUnittest(unittest.TestCase):
  def testAddCommandlineArgsMultipleExpectedVendorIds(self) -> None:
    parser = argparse.ArgumentParser()
    info_collection_test.InfoCollectionTest.AddCommandlineArgs(parser)
    args = parser.parse_args(
      [
        '--expected-vendor-id',
        '1ae0',
        '--expected-vendor-id',
        '10005',
        '--expected-device-id',
        'c0de',
        '--expected-device-id',
        '0',
      ]
    )
    self.assertEqual(args.expected_vendor_ids, ['1ae0', '10005'])
    self.assertEqual(args.expected_device_ids, ['c0de', '0'])

  def testRunBasicTestWithMultipleExpectedVendorAndDeviceIds(self) -> None:
    test_instance = info_collection_test.InfoCollectionTest('runTest')
    gpu_info = gi.GPUInfo.FromDict(
      {
        'devices': [
          {
            'vendor_id': 0x10005,
            'device_id': 0x0,
            'vendor_string': '',
            'device_string': '',
          }
        ]
      }
    )
    test_args = info_collection_test.InfoCollectionTestArgs(
      expected_vendor_id_strs=['1ae0', '10005'],
      expected_device_id_strs=['c0de', '0'],
      gpu=gpu_info,
    )
    test_instance._RunBasicTest(test_args)

  def testRunBasicTestVendorMismatch(self) -> None:
    test_instance = info_collection_test.InfoCollectionTest('runTest')
    gpu_info = gi.GPUInfo.FromDict(
      {
        'devices': [
          {
            'vendor_id': 0x8086,
            'device_id': 0x0,
            'vendor_string': '',
            'device_string': '',
          }
        ]
      }
    )
    test_args = info_collection_test.InfoCollectionTestArgs(
      expected_vendor_id_strs=['1ae0', '10005'],
      expected_device_id_strs=['c0de', '0'],
      gpu=gpu_info,
    )
    with self.assertRaises(AssertionError) as ctx:
      test_instance._RunBasicTest(test_args)
    self.assertIn(
      'Vendor ID mismatch, expected one of [0x1ae0, 0x10005] but got 0x8086',
      str(ctx.exception),
    )

  def testRunBasicTestDeviceMismatch(self) -> None:
    test_instance = info_collection_test.InfoCollectionTest('runTest')
    gpu_info = gi.GPUInfo.FromDict(
      {
        'devices': [
          {
            'vendor_id': 0x10005,
            'device_id': 0x1234,
            'vendor_string': '',
            'device_string': '',
          }
        ]
      }
    )
    test_args = info_collection_test.InfoCollectionTestArgs(
      expected_vendor_id_strs=['1ae0', '10005'],
      expected_device_id_strs=['c0de', '0'],
      gpu=gpu_info,
    )
    with self.assertRaises(AssertionError) as ctx:
      test_instance._RunBasicTest(test_args)
    self.assertIn(
      'Device ID mismatch, expected one of [0xc0de, 0x0] but got 0x1234',
      str(ctx.exception),
    )


if __name__ == '__main__':
  unittest.main()
