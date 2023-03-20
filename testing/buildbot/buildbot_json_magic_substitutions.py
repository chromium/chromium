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

MAGIC_SUBSTITUTION_PREFIX = '$$MAGIC_SUBSTITUTION_'


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
  dimensions = test_config.get('swarming', {}).get('dimension_sets', [])
  assert len(dimensions)
  pool = dimensions[0].get('pool')
  if not pool:
    raise RuntimeError(
        'No pool set for CrOS test, unable to determine whether running on '
        'a VM or physical hardware.')

  if not StringContainsSubstring(pool, TEST_POOLS):
    raise RuntimeError('Unknown CrOS pool %s' % pool)

  return dimensions[0].get('device_type', 'amd64-generic')


def _IsSkylabBot(tester_config):
  """Helper function to determine if a bot is a Skylab ChromeOS bot."""
  return (tester_config.get('browser_config') == 'cros-chrome'
          and not tester_config.get('use_swarming', True))


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
  dimensions = test_config.get('swarming', {}).get('dimension_sets', [])
  assert dimensions or _IsSkylabBot(tester_config)
  gpus = []
  for d in dimensions:
    # Split up multiple GPU/driver combinations if the swarming OR operator is
    # being used.
    if 'gpu' in d:
      gpus.extend(d['gpu'].split('|'))

  # We don't specify GPU on things like Android/CrOS devices, so default to 0.
  if not gpus:
    return ['--expected-device-id', '0']

  device_ids = set()
  for gpu_and_driver in gpus:
    # In the form vendor:device-driver.
    device = gpu_and_driver.split('-')[0].split(':')[1]
    device_ids.add(device)

  retval = []
  for device_id in sorted(device_ids):
    retval.extend(['--expected-device-id', device_id])
  return retval


def _GetGpusFromTestConfig(test_config):
  """Generates all GPU dimension strings from a test config.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
  """
  dimensions = test_config.get('swarming', {}).get('dimension_sets', [])
  assert dimensions
  for d in dimensions:
    # Split up multiple GPU/driver combinations if the swarming OR operator is
    # being used.
    if 'gpu' in d:
      gpus = d['gpu'].split('|')
      for gpu in gpus:
        yield gpu


def GPUParallelJobs(test_config, _, tester_config):
  """Substitutes the correct number of jobs for GPU tests.

  Linux/Mac/Windows can run tests in parallel since multiple windows can be open
  but other platforms cannot.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
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
  # TODO(crbug.com/1349828): Try removing the Windows/Intel special casing once
  # we swap which machines we're using.
  is_webgpu_cts = test_name.startswith('webgpu_cts') or test_config.get(
      'telemetry_test_name') == 'webgpu_cts'
  is_webgl_cts = (any(test_name in n
                      for n in ('webgl_conformance', 'webgl1_conformance',
                                'webgl2_conformance'))
                  or test_config.get('telemetry_test_name') in (
                      'webgl1_conformance', 'webgl2_conformance'))
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

  unrooted_devices = {'a13', 'a23'}
  dimensions = test_config.get('swarming', {}).get('dimension_sets', [])
  assert dimensions
  num_unrooted_devices = 0
  for d in dimensions:
    device_type = d.get('device_type')
    if device_type in unrooted_devices:
      num_unrooted_devices += 1
  # All devices should be either rooted or unrooted.
  if num_unrooted_devices == 0:
    return []
  if num_unrooted_devices == len(dimensions):
    return ['--compatibility-mode=dont-require-rooted-device']
  raise RuntimeError('All devices must be either rooted or unrooted')


def TestOnlySubstitution(_, __, ___):
  """Magic substitution used for unittests."""
  return ['--magic-substitution-success']
