# Copyright 2020 The Chromium Authors. All rights reserved.
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


def ChromeOSTelemetryRemote(test_config, _):
  """Substitutes the correct CrOS remote Telemetry arguments.

  VMs use a hard-coded remote address and port, while physical hardware use
  a magic hostname.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
  """
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


def ChromeOSGtestFilterFile(test_config, _):
  """Substitutes the correct CrOS filter file for gtests."""
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


def GPUExpectedDeviceId(test_config, _):
  """Substitutes the correct expected GPU(s) for certain GPU tests.

  Most configurations only need one expected GPU, but heterogeneous pools (e.g.
  HD 630 and UHD 630 machines) require multiple.

  Args:
    test_config: A dict containing a configuration for a specific test on a
        specific builder.
  """
  dimensions = test_config.get('swarming', {}).get('dimension_sets', [])
  assert dimensions
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


def TestOnlySubstitution(_, __):
  """Magic substitution used for unittests."""
  return ['--magic-substitution-success']
