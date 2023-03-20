#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
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
        magic_substitutions.ChromeOSTelemetryRemote(test_config, None, {}), [
            '--remote=127.0.0.1',
            '--remote-ssh-port=9222',
        ])

  def testPhysicalHardwareSubstitutions(self):
    test_config = CreateConfigWithPool('chromium.tests', device_type='eve')
    self.assertEqual(
        magic_substitutions.ChromeOSTelemetryRemote(test_config, None, {}),
        ['--remote=variable_chromeos_device_hostname'])

  def testSkylabSubstitutions(self):
    tester_config = {'browser_config': 'cros-chrome', 'use_swarming': False}
    self.assertEqual(
        magic_substitutions.ChromeOSTelemetryRemote({}, None, tester_config),
        [])

  def testNoPool(self):
    test_config = CreateConfigWithPool(None)
    with self.assertRaisesRegex(RuntimeError, 'No pool *'):
      magic_substitutions.ChromeOSTelemetryRemote(test_config, None, {})

  def testUnknownPool(self):
    test_config = CreateConfigWithPool('totally-legit-pool')
    with self.assertRaisesRegex(RuntimeError, 'Unknown CrOS pool *'):
      magic_substitutions.ChromeOSTelemetryRemote(test_config, None, {})


class ChromeOSGtestFilterFileTest(unittest.TestCase):
  def testVirtualMachineFile(self):
    test_config = CreateConfigWithPool('chromium.tests.cros.vm')
    self.assertEqual(
        magic_substitutions.ChromeOSGtestFilterFile(test_config, None, {}), [
            '--test-launcher-filter-file=../../testing/buildbot/filters/'
            'chromeos.amd64-generic.test_name.filter',
        ])

  def testPhysicalHardwareFile(self):
    test_config = CreateConfigWithPool('chromium.tests', device_type='eve')
    self.assertEqual(
        magic_substitutions.ChromeOSGtestFilterFile(test_config, None, {}), [
            '--test-launcher-filter-file=../../testing/buildbot/filters/'
            'chromeos.eve.test_name.filter',
        ])

  def testSkylab(self):
    test_config = {'name': 'test_name', 'cros_board': 'eve'}
    tester_config = {'browser_config': 'cros-chrome', 'use_swarming': False}
    self.assertEqual(
        magic_substitutions.ChromeOSGtestFilterFile(test_config, None,
                                                    tester_config),
        [
            '--test-launcher-filter-file=../../testing/buildbot/filters/'
            'chromeos.eve.test_name.filter',
        ])

  def testNoPool(self):
    test_config = CreateConfigWithPool(None)
    with self.assertRaisesRegex(RuntimeError, 'No pool *'):
      magic_substitutions.ChromeOSGtestFilterFile(test_config, None, {})

  def testUnknownPool(self):
    test_config = CreateConfigWithPool('totally-legit-pool')
    with self.assertRaisesRegex(RuntimeError, 'Unknown CrOS pool *'):
      magic_substitutions.ChromeOSGtestFilterFile(test_config, None, {})


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
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}),
        ['device1'])

  def testSingleGpuDoubleDimension(self):
    test_config = CreateConfigWithGpus(
        ['vendor:device1-driver', 'vendor:device2-driver'])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}),
        ['device1', 'device2'])

  def testDoubleGpuSingleDimension(self):
    test_config = CreateConfigWithGpus(
        ['vendor:device1-driver|vendor:device2-driver'])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}),
        ['device1', 'device2'])

  def testDoubleGpuDoubleDimension(self):
    test_config = CreateConfigWithGpus([
        'vendor:device1-driver|vendor:device2-driver',
        'vendor:device1-driver|vendor:device3-driver'
    ])
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}),
        ['device1', 'device2', 'device3'])

  def testNoGpu(self):
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(
            {'swarming': {
                'dimension_sets': [{}]
            }}, None, {}), ['0'])

  def testNoDimensions(self):
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUExpectedDeviceId({}, None, {})


class GPUParallelJobs(unittest.TestCase):
  def testNoOsType(self):
    test_config = CreateConfigWithGpus(['vendor:device1-driver'])
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUParallelJobs(test_config, None, {})

  def testParallelJobs(self):
    test_config = CreateConfigWithGpus(['vendor:device1-driver'])
    for os_type in ['lacros', 'linux', 'mac', 'win']:
      retval = magic_substitutions.GPUParallelJobs(test_config, None,
                                                   {'os_type': os_type})
      self.assertEqual(retval, ['--jobs=4'])

  def testSerialJobs(self):
    test_config = CreateConfigWithGpus(['vendor:device1-driver'])
    for os_type in ['android', 'chromeos', 'fuchsia']:
      retval = magic_substitutions.GPUParallelJobs(test_config, None,
                                                   {'os_type': os_type})
      self.assertEqual(retval, ['--jobs=1'])

  def testWebGPUCTSWindowsIntelSerialJobs(self):
    intel_config = CreateConfigWithGpus(['8086:device1-driver'])
    amd_config = CreateConfigWithGpus(['1002:device1-driver'])

    for gpu_config in [intel_config, amd_config]:
      for name, telemetry_test_name in [('webgpu_cts', None),
                                        (None, 'webgpu_cts')]:
        is_intel = intel_config == gpu_config
        c = gpu_config.copy()
        if name:
          c['name'] = name
        if telemetry_test_name:
          c['telemetry_test_name'] = telemetry_test_name
        for os_type in ['lacros', 'linux', 'mac', 'win']:
          retval = magic_substitutions.GPUParallelJobs(c, None,
                                                       {'os_type': os_type})
          if is_intel and os_type == 'win':
            self.assertEqual(retval, ['--jobs=1'])
          else:
            self.assertEqual(retval, ['--jobs=4'])

  def testWebGLWindowsIntelParallelJobs(self):
    intel_config = CreateConfigWithGpus(['8086:device1-driver'])
    amd_config = CreateConfigWithGpus(['1002:device1-driver'])
    for gpu_config in [intel_config, amd_config]:
      for name, telemetry_test_name in [('webgl_conformance', None),
                                        ('webgl1_conformance', None),
                                        ('webgl2_conformance', None),
                                        (None, 'webgl1_conformance'),
                                        (None, 'webgl2_conformance')]:
        is_intel = intel_config == gpu_config
        c = gpu_config.copy()
        if name:
          c['name'] = name
        if telemetry_test_name:
          c['telemetry_test_name'] = telemetry_test_name
        for os_type in ['lacros', 'linux', 'mac', 'win']:
          retval = magic_substitutions.GPUParallelJobs(c, None,
                                                       {'os_type': os_type})
          if is_intel and os_type == 'win':
            self.assertEqual(retval, ['--jobs=2'])
          else:
            self.assertEqual(retval, ['--jobs=4'])

  def testWebGLMacNvidiaParallelJobs(self):
    amd_config = CreateConfigWithGpus(['1002:device1-driver'])
    nvidia_config = CreateConfigWithGpus(['10de:device1-driver'])

    for gpu_config in [nvidia_config, amd_config]:
      for name, telemetry_test_name in [('webgl_conformance', None),
                                        (None, 'webgl_conformance')]:
        is_nvidia = gpu_config == nvidia_config
        c = gpu_config.copy()
        if name:
          c['name'] = name
        if telemetry_test_name:
          c['telemetry_test_name'] = telemetry_test_name
        for os_type in ['lacros', 'linux', 'mac', 'win']:
          retval = magic_substitutions.GPUParallelJobs(c, None,
                                                       {'os_type': os_type})
          if is_nvidia and os_type == 'mac':
            self.assertEqual(retval, ['--jobs=3'])
          else:
            self.assertEqual(retval, ['--jobs=4'])


def CreateConfigWithDeviceTypes(device_types):
  dimension_sets = []
  for d in device_types:
    dimension_sets.append({'device_type': d})
  return {
      'swarming': {
          'dimension_sets': dimension_sets,
      },
  }


class GPUTelemetryNoRootForUnrootedDevices(unittest.TestCase):
  def testNoOsType(self):
    test_config = CreateConfigWithDeviceTypes(['a13'])
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
          test_config, None, {})

  def testNonAndroidOs(self):
    retval = magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
        {}, None, {'os_type': 'linux'})
    self.assertEqual(retval, [])

  def testUnrootedDevices(self):
    devices = ('a13', 'a23')
    for d in devices:
      test_config = CreateConfigWithDeviceTypes([d])
      retval = magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
          test_config, None, {'os_type': 'android'})
      self.assertEqual(retval,
                       ['--compatibility-mode=dont-require-rooted-device'])

  def testMixedDevices(self):
    test_config = CreateConfigWithDeviceTypes(['hammerhead', 'a13'])
    with self.assertRaises(RuntimeError):
      magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
          test_config, None, {'os_type': 'android'})

  def testRootedDevices(self):
    test_config = CreateConfigWithDeviceTypes(['hammerhead'])
    retval = magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
        test_config, None, {'os_type': 'android'})
    self.assertEqual(retval, [])


if __name__ == '__main__':
  unittest.main()
