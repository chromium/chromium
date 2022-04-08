#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import buildbot_json_magic_substitutions as magic_substitutions


def CreateConfigWithPool(pool, device_type=None):
  dims = {
      'name': 'test_name',
      'swarming': {
          'dimension_sets': [
              {
                  'pool': pool,
              },
          ],
      },
  }
  if device_type:
    dims['swarming']['dimension_sets'][0]['device_type'] = device_type
  return dims


class ChromeOSTelemetryRemoteTest(unittest.TestCase):

  def testVirtualMachineSubstitutions(self):
    test_config = CreateConfigWithPool('chromium.tests.cros.vm')
    self.assertEqual(
        magic_substitutions.ChromeOSTelemetryRemote(test_config, None), [
            '--remote=127.0.0.1',
            '--remote-ssh-port=9222',
        ])

  def testPhysicalHardwareSubstitutions(self):
    test_config = CreateConfigWithPool('chromium.tests', device_type='eve')
    self.assertEqual(
        magic_substitutions.ChromeOSTelemetryRemote(test_config, None),
        ['--remote=variable_chromeos_device_hostname'])

  def testNoPool(self):
    test_config = CreateConfigWithPool(None)
    with self.assertRaisesRegex(RuntimeError, 'No pool *'):
      magic_substitutions.ChromeOSTelemetryRemote(test_config, None)

  def testUnknownPool(self):
    test_config = CreateConfigWithPool('totally-legit-pool')
    with self.assertRaisesRegex(RuntimeError, 'Unknown CrOS pool *'):
      magic_substitutions.ChromeOSTelemetryRemote(test_config, None)


class ChromeOSGtestFilterFileTest(unittest.TestCase):
  def testVirtualMachineFile(self):
    test_config = CreateConfigWithPool('chromium.tests.cros.vm')
    self.assertEqual(
        magic_substitutions.ChromeOSGtestFilterFile(test_config, None), [
            '--test-launcher-filter-file=../../testing/buildbot/filters/'
            'chromeos.amd64-generic.test_name.filter',
        ])

  def testPhysicalHardwareFile(self):
    test_config = CreateConfigWithPool('chromium.tests', device_type='eve')
    self.assertEqual(
        magic_substitutions.ChromeOSGtestFilterFile(test_config, None), [
            '--test-launcher-filter-file=../../testing/buildbot/filters/'
            'chromeos.eve.test_name.filter',
        ])

  def testNoPool(self):
    test_config = CreateConfigWithPool(None)
    with self.assertRaisesRegex(RuntimeError, 'No pool *'):
      magic_substitutions.ChromeOSTelemetryRemote(test_config, None)

  def testUnknownPool(self):
    test_config = CreateConfigWithPool('totally-legit-pool')
    with self.assertRaisesRegex(RuntimeError, 'Unknown CrOS pool *'):
      magic_substitutions.ChromeOSTelemetryRemote(test_config, None)


def CreateConfigWithGpus(gpus):
  dimension_sets = []
  for g in gpus:
    dimension_sets.append({'gpu': g})
  return {
      'swarming': {
          'dimension_sets': dimension_sets,
      },
  }


class GPUExpectedDeviceId(unittest.TestCase):
  def assertDeviceIdCorrectness(self, retval, device_ids):
    self.assertEqual(len(retval), 2 * len(device_ids))
    for i in range(0, len(retval), 2):
      self.assertEqual(retval[i], '--expected-device-id')
    for d in device_ids:
      self.assertIn(d, retval)

  def testSingleGpuSingleDimension(self):
    test_config = CreateConfigWithGpus(['vendor:device1-driver'])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None), ['device1'])

  def testSingleGpuDoubleDimension(self):
    test_config = CreateConfigWithGpus(
        ['vendor:device1-driver', 'vendor:device2-driver'])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None),
        ['device1', 'device2'])

  def testDoubleGpuSingleDimension(self):
    test_config = CreateConfigWithGpus(
        ['vendor:device1-driver|vendor:device2-driver'])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None),
        ['device1', 'device2'])

  def testDoubleGpuDoubleDimension(self):
    test_config = CreateConfigWithGpus([
        'vendor:device1-driver|vendor:device2-driver',
        'vendor:device1-driver|vendor:device3-driver'
    ])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None),
        ['device1', 'device2', 'device3'])

  def testNoGpu(self):
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(
            {'swarming': {
                'dimension_sets': [{}]
            }}, None), ['0'])

  def testNoDimensions(self):
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUExpectedDeviceId({}, None)


if __name__ == '__main__':
  unittest.main()
