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
          'dimensions': {
              'pool': pool,
          },
      },
  }
  if device_type:
    dims['swarming']['dimensions']['device_type'] = device_type
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

  def testSkylabWithVariant(self):
    test_config = {
        'name': 'test_name SOME_VARIANT',
        'cros_board': 'eve',
        'variant_id': 'SOME_VARIANT',
    }
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


def CreateConfigWithGpu(gpu):
  return {
      'swarming': {
          'dimensions': {
              'gpu': gpu,
          },
      },
  }


class GPUExpectedVendorId(unittest.TestCase):
  def testSingleGpuSingleDimension(self):
    test_config = CreateConfigWithGpu('vendor:device1-driver')
    self.assertEqual(
        magic_substitutions.GPUExpectedVendorId(test_config, None, {}),
        ['--expected-vendor-id', 'vendor'])

  def testDoubleGpuSingleDimension(self):
    test_config = CreateConfigWithGpu(
        'vendor:device1-driver|vendor:device2-driver')
    self.assertEqual(
        magic_substitutions.GPUExpectedVendorId(test_config, None, {}),
        ['--expected-vendor-id', 'vendor'])

  def testDoubleGpuSingleDimensionDifferentVendors(self):
    test_config = CreateConfigWithGpu(
        'vendor:device1-driver|vendor2:device2-driver')
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUExpectedVendorId(test_config, None, {})

  def testAppleSilicon(self):
    test_config = CreateConfigWithGpu('apple:m1')
    self.assertEqual(
        magic_substitutions.GPUExpectedVendorId(test_config, None, {}),
        ['--expected-vendor-id', '106b'])

  def testNoGpu(self):
    test_config = {
        'swarming': {
            'dimensions': {},
        },
    }
    self.assertEqual(
        magic_substitutions.GPUExpectedVendorId(test_config, None, {}),
        ['--expected-vendor-id', '0'])

  def testNoDimensions(self):
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUExpectedVendorId({}, None, {})

  def testSkylabKnownBoard(self):
    test_config = {
        'name': 'test_name',
        'cros_board': 'volteer',
    }
    tester_config = {
        'browser_config': 'cros-chrome',
        'use_swarming': False,
    }
    self.assertEqual(
        magic_substitutions.GPUExpectedVendorId(test_config, None,
                                                tester_config),
        ['--expected-vendor-id', '8086'])

  def testSkylabUnknownBoard(self):
    test_config = {
        'name': 'test_name',
        'cros_board': 'fancy_new_board',
    }
    tester_config = {
        'browser_config': 'cros-chrome',
        'use_swarming': False,
    }
    self.assertEqual(
        magic_substitutions.GPUExpectedVendorId(test_config, None,
                                                tester_config),
        ['--expected-vendor-id', '0'])


class GPUExpectedDeviceId(unittest.TestCase):
  def assertDeviceIdCorrectness(self, retval, device_ids):
    self.assertEqual(len(retval), 2 * len(device_ids))
    for i in range(0, len(retval), 2):
      self.assertEqual(retval[i], '--expected-device-id')
    for d in device_ids:
      self.assertIn(d, retval)

  def testSingleGpuSingleDimension(self):
    test_config = CreateConfigWithGpu('vendor:device1-driver')
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}),
        ['device1'])

  def testDoubleGpuSingleDimension(self):
    test_config = CreateConfigWithGpu(
        'vendor:device1-driver|vendor:device2-driver')
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}),
        ['device1', 'device2'])

  def testAppleSilicon(self):
    test_config = CreateConfigWithGpu('apple:m1')
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None, {}), ['0'])

  def testNoGpu(self):
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(
            {'swarming': {
                'dimensions': {}
            }}, None, {}), ['0'])

  def testNoDimensions(self):
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUExpectedDeviceId({}, None, {})

  def testSkylabKnownBoard(self):
    test_config = {
        'name': 'test_name',
        'cros_board': 'volteer',
    }
    tester_config = {
        'browser_config': 'cros-chrome',
        'use_swarming': False,
    }
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None,
                                                tester_config), ['9a49'])

  def testSkylabUnknownBoard(self):
    test_config = {
        'name': 'test_name',
        'cros_board': 'fancy_new_board',
    }
    tester_config = {
        'browser_config': 'cros-chrome',
        'use_swarming': False,
    }
    self.assertDeviceIdCorrectness(
        magic_substitutions.GPUExpectedDeviceId(test_config, None,
                                                tester_config), ['0'])


class GPUParallelJobs(unittest.TestCase):
  def testNoOsType(self):
    test_config = CreateConfigWithGpu('vendor:device1-driver')
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUParallelJobs(test_config, 'name', {})

  def testParallelJobs(self):
    test_config = CreateConfigWithGpu('vendor:device1-driver')
    for os_type in ['lacros', 'linux', 'mac', 'win']:
      retval = magic_substitutions.GPUParallelJobs(test_config, 'name',
                                                   {'os_type': os_type})
      self.assertEqual(retval, ['--jobs=4'])

  def testSerialJobs(self):
    test_config = CreateConfigWithGpu('vendor:device1-driver')
    for os_type in ['android', 'chromeos', 'fuchsia']:
      retval = magic_substitutions.GPUParallelJobs(test_config, 'name',
                                                   {'os_type': os_type})
      self.assertEqual(retval, ['--jobs=1'])

  def testWebGPUCTSWindowsIntelSerialJobs(self):
    intel_config = CreateConfigWithGpu('8086:device1-driver')
    amd_config = CreateConfigWithGpu('1002:device1-driver')

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
          retval = magic_substitutions.GPUParallelJobs(c, 'name',
                                                       {'os_type': os_type})
          if is_intel and os_type == 'win':
            self.assertEqual(retval, ['--jobs=1'])
          else:
            self.assertEqual(retval, ['--jobs=4'])

  def testWebGLWindowsIntelParallelJobs(self):
    intel_config = CreateConfigWithGpu('8086:device1-driver')
    amd_config = CreateConfigWithGpu('1002:device1-driver')
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
          retval = magic_substitutions.GPUParallelJobs(c, 'name',
                                                       {'os_type': os_type})
          if is_intel and os_type == 'win':
            self.assertEqual(retval, ['--jobs=2'])
          else:
            self.assertEqual(retval, ['--jobs=4'])

  def testWebGLMacNvidiaParallelJobs(self):
    amd_config = CreateConfigWithGpu('1002:device1-driver')
    nvidia_config = CreateConfigWithGpu('10de:device1-driver')

    for gpu_config in [nvidia_config, amd_config]:
      for name, telemetry_test_name in [('webgl1_conformance', None),
                                        (None, 'webgl1_conformance')]:
        is_nvidia = gpu_config == nvidia_config
        c = gpu_config.copy()
        if name:
          c['name'] = name
        if telemetry_test_name:
          c['telemetry_test_name'] = telemetry_test_name
        for os_type in ['lacros', 'linux', 'mac', 'win']:
          retval = magic_substitutions.GPUParallelJobs(c, 'name',
                                                       {'os_type': os_type})
          if is_nvidia and os_type == 'mac':
            self.assertEqual(retval, ['--jobs=3'])
          else:
            self.assertEqual(retval, ['--jobs=4'])

  def testPixelMacDebugParallelJobs(self):
    gpu_config = CreateConfigWithGpu('1002:device1-driver')
    for name, telemetry_test_name in [('pixel_skia_gold_test', None),
                                      (None, 'pixel')]:
      c = gpu_config.copy()
      if name:
        c['name'] = name
      if telemetry_test_name:
        c['telemetry_test_name'] = telemetry_test_name
      for os_type in ['lacros', 'linux', 'mac', 'win']:
        for tester_name in ('Name Debug', 'Name Dbg', 'name debug', 'name dbg'):
          retval = magic_substitutions.GPUParallelJobs(c, tester_name,
                                                       {'os_type': os_type})
          if os_type == 'mac':
            self.assertEqual(retval, ['--jobs=1'])
          else:
            self.assertEqual(retval, ['--jobs=4'])
      # Double check that non-debug Mac pixel tests still get parallelized.
      retval = magic_substitutions.GPUParallelJobs(c, 'name release',
                                                   {'os_type': 'mac'})
      self.assertEqual(retval, ['--jobs=4'])

  def testPixelMacNvidiaParallelJobs(self):
    gpu_config = CreateConfigWithGpu('10de:device1-driver')
    for name, telemetry_test_name in [('pixel_skia_gold_test', None),
                                      (None, 'pixel')]:
      c = gpu_config.copy()
      if name:
        c['name'] = name
      if telemetry_test_name:
        c['telemetry_test_name'] = telemetry_test_name
      for os_type in ['lacros', 'linux', 'mac', 'win']:
        retval = magic_substitutions.GPUParallelJobs(c, 'name',
                                                     {'os_type': os_type})
        if os_type == 'mac':
          self.assertEqual(retval, ['--jobs=1'])
        else:
          self.assertEqual(retval, ['--jobs=4'])


def CreateConfigWithDeviceType(device_type):
  return {
      'swarming': {
          'dimensions': {
              'device_type': device_type,
          },
      },
  }


class GPUTelemetryNoRootForUnrootedDevices(unittest.TestCase):
  def testNoOsType(self):
    test_config = CreateConfigWithDeviceType('a13')
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
          test_config, None, {})

  def testNonAndroidOs(self):
    retval = magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
        {}, None, {'os_type': 'linux'})
    self.assertEqual(retval, [])

  def testUnrootedDevices(self):
    devices = ('a13', 'a23', 'dm1q', 'devonn')
    for d in devices:
      test_config = CreateConfigWithDeviceType(d)
      retval = magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
          test_config, None, {'os_type': 'android'})
      self.assertEqual(retval,
                       ['--compatibility-mode=dont-require-rooted-device'])

  def testRootedDevices(self):
    test_config = CreateConfigWithDeviceType('hammerhead')
    retval = magic_substitutions.GPUTelemetryNoRootForUnrootedDevices(
        test_config, None, {'os_type': 'android'})
    self.assertEqual(retval, [])


class GPUWebGLRuntimeFile(unittest.TestCase):
  def testNoOsType(self):
    test_config = {'telemetry_test_name': 'webgl1_conformance'}
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUWebGLRuntimeFile(test_config, None, {})

  def testNoSuite(self):
    tester_config = {'os_type': 'linux'}
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUWebGLRuntimeFile({}, None, tester_config)

  def testUnknownSuite(self):
    test_config = {'telemetry_test_name': 'foo'}
    tester_config = {'os_type': 'linux'}
    with self.assertRaises(AssertionError):
      magic_substitutions.GPUWebGLRuntimeFile(test_config, None, tester_config)

  def testKnownOsTypes(self):
    for os_type in ('android', 'linux', 'mac', 'win'):
      for suite in ('webgl1_conformance', 'webgl2_conformance'):
        retval = magic_substitutions.GPUWebGLRuntimeFile(
            {'telemetry_test_name': suite}, None, {'os_type': os_type})
        self.assertEqual(retval, [
            '--read-abbreviated-json-results-from=../../content/test/data/gpu/'
            f'{suite}_{os_type}_runtimes.json'
        ])

  def testUnknownOsType(self):
    for suite in ('webgl1_conformance', 'webgl2_conformance'):
      retval = magic_substitutions.GPUWebGLRuntimeFile(
          {'telemetry_test_name': suite}, None, {'os_type': 'foo'})
      self.assertEqual(retval, [
          '--read-abbreviated-json-results-from=../../content/test/data/gpu/'
          f'{suite}_linux_runtimes.json'
      ])


if __name__ == '__main__':
  unittest.main()
