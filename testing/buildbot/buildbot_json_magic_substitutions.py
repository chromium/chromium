# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A set of functions to programmatically substitute test arguments.

Arguments for a test that start with $$MAGIC_SUBSTITUTION_ will be replaced with
the output of the corresponding function in this file. For example,
$$MAGIC_SUBSTITUTION_Foo would be replaced with the return value of the Foo()
function.

This is meant as an alternative to many entries in test_suite_exceptions.pyl if
the differentiation can be done programmatically.
"""

import collections

# LINT.IfChange

MAGIC_SUBSTITUTION_PREFIX = '$$MAGIC_SUBSTITUTION_'

GpuDevice = collections.namedtuple('GpuDevice', ['vendor', 'device'])
CROS_BOARD_GPUS = {
    'volteer': GpuDevice('8086', '9a49'),
}

VENDOR_SUBSTITUTIONS = {
    'apple': '106b',
    'qcom': '4d4f4351',
}
DEVICE_SUBSTITUTIONS = {
    'm1': '0',
    'm2': '0',
    # Qualcomm Adreno 680/685/690 and 741 on Windows arm64. The approach
    # swarming uses to find GPUs (looking for all Win32_VideoController WMI
    # objects) results in different output than what Chrome sees.
    # 043a = Adreno 680/685/690 GPU (such as Surface Pro X, Dell trybots)
    # 0636 = Adreno 690 GPU (such as Surface Pro 9 5G)
    # 0c36 = Adreno 741 GPU (such as Surface Pro 11th Edition)
    '043a': '41333430',
    '0636': '36333630',
    '0c36': '36334330',
}
ANDROID_VULKAN_DEVICES = {
    # Pixel 6 phones map to multiple GPU models.
    'oriole': GpuDevice('13b5', '92020010,92020000'),
    'dm1q': GpuDevice('5143', '43050a01'),
    'a23': GpuDevice('5143', '6010001'),
}

def ChromeOSTelemetryRemote(test_config, _, tester_config):
  """Substitutes the correct CrOS remote Telemetry arguments.

  VMs use a hard-coded remote address and port, while physical hardware use
  a magic hostname.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
    tester_config: A dict containing the configuration for the builder
        that |test_config| is for.
  """
  if _IsSkylabBot(tester_config):
    # Skylab bots will automatically add the --remote argument with the correct
    # hostname.
    return []
  if _GetChromeOSBoardName(test_config) == 'amd64-generic':
    return [
        '--remote=127.0.0.1',
        # By default, CrOS VMs' ssh servers listen on local port 9222.
        '--remote-ssh-port=9222',
    ]
  return [
      # Magic hostname that resolves to a CrOS device in the test lab.
      '--remote=variable_chromeos_device_hostname',
  ]


def ChromeOSGtestFilterFile(test_config, _, tester_config):
  """Substitutes the correct CrOS filter file for gtests."""
  if _IsSkylabBot(tester_config):
    board = test_config['cros_board']
  else:
    board = _GetChromeOSBoardName(test_config)
  test_name = test_config['name']
  # Strip off the variant suffix if it's present.
  if 'variant_id' in test_config:
    test_name = test_name.replace(test_config['variant_id'], '')
    test_name = test_name.strip()
  filter_file = 'chromeos.%s.%s.filter' % (board, test_name)
  return [
      '--test-launcher-filter-file=../../testing/buildbot/filters/' +
      filter_file
  ]


def _GetChromeOSBoardName(test_config):
  """Helper function to determine what ChromeOS board is being used."""

  def StringContainsSubstring(s, sub_strs):
    for sub_str in sub_strs:
      if sub_str in s:
        return True
    return False

  TEST_POOLS = [
      'chrome.tests',
      'chromium.tests',
  ]
  dimensions = test_config.get('swarming', {}).get('dimensions')
  assert dimensions is not None
  pool = dimensions.get('pool')
  if not pool:
    raise RuntimeError(
        'No pool set for CrOS test, unable to determine whether running on '
        'a VM or physical hardware.')

  if not StringContainsSubstring(pool, TEST_POOLS):
    raise RuntimeError('Unknown CrOS pool %s' % pool)

  return dimensions.get('device_type', 'amd64-generic')


def _IsSkylabBot(tester_config):
  """Helper function to determine if a bot is a Skylab ChromeOS bot."""
  return (tester_config.get('browser_config') == 'cros-chrome'
          and not tester_config.get('use_swarming', True))


def _IsAndroid(tester_config):
  return 'os_type' in tester_config and tester_config['os_type'] == 'android'


def GPUExpectedVendorId(test_config, _, tester_config):
  """Substitutes the correct expected GPU vendor for certain GPU tests.

  We only ever trigger tests on a single vendor type per builder definition,
  so multiple found vendors is an error.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
    tester_config: A dict containing the configuration for the builder
        that |test_config| is for.
  """
  if _IsSkylabBot(tester_config):
    return _GPUExpectedVendorIdSkylab(test_config)
  dimensions = test_config.get('swarming', {}).get('dimensions')
  assert dimensions is not None
  dimensions = dimensions or {}
  gpus = []
  # Split up multiple GPU/driver combinations if the swarming OR operator is
  # being used.
  if 'gpu' in dimensions:
    gpus.extend(dimensions['gpu'].split('|'))
  elif _IsAndroid(tester_config) and 'device_type' in dimensions:
    vulkan_device = ANDROID_VULKAN_DEVICES.get(dimensions['device_type'])
    if vulkan_device:
      return ['--expected-vendor-id', vulkan_device.vendor]

  # We don't specify GPU on things like Android and certain CrOS devices, so
  # default to 0.
  if not gpus:
    return ['--expected-vendor-id', '0']

  vendor_ids = set()
  for gpu_and_driver in gpus:
    # In the form vendor:device-driver.
    vendor = gpu_and_driver.split(':')[0]
    vendor = VENDOR_SUBSTITUTIONS.get(vendor, vendor)
    vendor_ids.add(vendor)
  assert len(vendor_ids) == 1

  return ['--expected-vendor-id', vendor_ids.pop()]


def _GPUExpectedVendorIdSkylab(test_config):
  cros_board = test_config.get('cros_board')
  assert cros_board is not None
  gpu_device = CROS_BOARD_GPUS.get(cros_board, GpuDevice('0', '0'))
  return ['--expected-vendor-id', gpu_device.vendor]


def GPUExpectedDeviceId(test_config, _, tester_config):
  """Substitutes the correct expected GPU(s) for certain GPU tests.

  Most configurations only need one expected GPU, but heterogeneous pools (e.g.
  HD 630 and UHD 630 machines) require multiple.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
    tester_config: A dict containing the configuration for the builder
        that |test_config| is for.
  """
  if _IsSkylabBot(tester_config):
    return _GPUExpectedDeviceIdSkylab(test_config)
  dimensions = test_config.get('swarming', {}).get('dimensions')
  assert dimensions is not None
  dimensions = dimensions or {}
  gpus = []
  # Split up multiple GPU/driver combinations if the swarming OR operator is
  # being used.
  if 'gpu' in dimensions:
    gpus.extend(dimensions['gpu'].split('|'))
  elif _IsAndroid(tester_config) and 'device_type' in dimensions:
    vulkan_device = ANDROID_VULKAN_DEVICES.get(dimensions['device_type'])
    if vulkan_device:
      device_ids = vulkan_device.device.split(',')
      commands = []
      for index, device_id in enumerate(device_ids):
        commands.append('--expected-device-id')
        commands.append(device_ids[index])
      return commands

  # We don't specify GPU on things like Android/CrOS devices, so default to 0.
  if not gpus:
    return ['--expected-device-id', '0']

  device_ids = set()
  for gpu_and_driver in gpus:
    # In the form vendor:device-driver.
    device = gpu_and_driver.split('-')[0].split(':')[1]
    device = DEVICE_SUBSTITUTIONS.get(device, device)
    device_ids.add(device)

  retval = []
  for device_id in sorted(device_ids):
    retval.extend(['--expected-device-id', device_id])
  return retval


def _GPUExpectedDeviceIdSkylab(test_config):
  cros_board = test_config.get('cros_board')
  assert cros_board is not None
  gpu_device = CROS_BOARD_GPUS.get(cros_board, GpuDevice('0', '0'))
  return ['--expected-device-id', gpu_device.device]


def _GetGpusFromTestConfig(test_config):
  """Generates all GPU dimension strings from a test config.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
  """
  dimensions = test_config.get('swarming', {}).get('dimensions')
  assert dimensions is not None
  # Split up multiple GPU/driver combinations if the swarming OR operator is
  # being used.
  if 'gpu' in dimensions:
    gpus = dimensions['gpu'].split('|')
    for gpu in gpus:
      yield gpu


def GPUParallelJobs(test_config, tester_name, tester_config):
  """Substitutes the correct number of jobs for GPU tests.

  Linux/Mac/Windows can run tests in parallel since multiple windows can be open
  but other platforms cannot.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
    tester_name: A string containing the name of the builder that |test_config|
        is for.
    tester_config: A dict containing the configuration for the builder
        that |test_config| is for.
  """
  os_type = tester_config.get('os_type')
  assert os_type

  test_name = test_config.get('name', '')

  # Return --jobs=1 for Windows Intel bots running the WebGPU CTS
  # These bots can't handle parallel tests. See crbug.com/1353938.
  # The load can also negatively impact WebGL tests, so reduce the number of
  # jobs there.
  # TODO(crbug.com/40233910): Try removing the Windows/Intel special casing once
  # we swap which machines we're using.
  is_webgpu_cts = test_name.startswith('webgpu_cts') or test_config.get(
      'telemetry_test_name') == 'webgpu_cts'
  is_webgl_cts = (any(n in test_name
                      for n in ('webgl_conformance', 'webgl1_conformance',
                                'webgl2_conformance'))
                  or test_config.get('telemetry_test_name')
                  in ('webgl1_conformance', 'webgl2_conformance'))
  if os_type == 'win' and (is_webgl_cts or is_webgpu_cts):
    for gpu in _GetGpusFromTestConfig(test_config):
      if gpu.startswith('8086'):
        # Especially flaky on '8086:9bc5' per crbug.com/1392149
        if is_webgpu_cts or gpu.startswith('8086:9bc5'):
          return ['--jobs=1']
        return ['--jobs=2']
  # Similarly, the NVIDIA Macbooks are quite old and slow, so reduce the number
  # of jobs there as well.
  if os_type == 'mac' and is_webgl_cts:
    for gpu in _GetGpusFromTestConfig(test_config):
      if gpu.startswith('10de'):
        return ['--jobs=3']

  # Slow Mac configs have issues with flakiness when running tests in parallel.
  is_pixel_test = (test_name == 'pixel_skia_gold_test'
                   or test_config.get('telemetry_test_name') == 'pixel')
  is_webcodecs_test = (test_name == 'webcodecs_tests'
                       or test_config.get('telemetry_test_name') == 'webcodecs')
  is_debug = any(s in tester_name.lower() for s in ('debug', 'dbg'))
  if os_type == 'mac' and (is_pixel_test or is_webcodecs_test):
    if is_debug:
      return ['--jobs=1']
    for gpu in _GetGpusFromTestConfig(test_config):
      if gpu.startswith('10de'):
        return ['--jobs=1']

  if os_type in ['lacros', 'linux', 'mac', 'win']:
    return ['--jobs=4']
  return ['--jobs=1']


def GPUTelemetryNoRootForUnrootedDevices(test_config, _, tester_config):
  """Disables Telemetry's root requests for unrootable Android devices.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
    tester_config: A dict containing the configuration for the builder
        that |test_config| is for.
  """
  os_type = tester_config.get('os_type')
  assert os_type
  if os_type != 'android':
    return []

  unrooted_devices = {
      'a13',
      'a23',
      'dm1q',  # Samsung S23.
      'devonn',  # Motorola Moto G Power 5G.
  }
  dimensions = test_config.get('swarming', {}).get('dimensions')
  assert dimensions is not None
  device_type = dimensions.get('device_type')
  if device_type in unrooted_devices:
    return ['--compatibility-mode=dont-require-rooted-device']
  return []


def GPUWebGLRuntimeFile(test_config, _, tester_config):
  """Gets the correct WebGL runtime file for a tester.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
    tester_config: A dict containing the configuration for the builder
        that |test_config| is for.
  """
  os_type = tester_config.get('os_type')
  assert os_type
  suite = test_config.get('telemetry_test_name')
  assert suite in ('webgl1_conformance', 'webgl2_conformance')

  # Default to using Linux's file if we're on a platform that we don't actively
  # maintain runtime files for.
  chosen_os = os_type
  if chosen_os not in ('android', 'linux', 'mac', 'win'):
    chosen_os = 'linux'

  runtime_filepath = (
      f'../../content/test/data/gpu/{suite}_{chosen_os}_runtimes.json')
  return [f'--read-abbreviated-json-results-from={runtime_filepath}']

# LINT.ThenChange(//infra/config/lib/targets-internal/magic_args.star)

def TestOnlySubstitution(_, __, ___):
  """Magic substitution used for unittests."""
  return ['--magic-substitution-success']
