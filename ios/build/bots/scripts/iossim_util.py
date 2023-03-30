# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import subprocess

import test_runner

LOGGER = logging.getLogger(__name__)


def _compose_simulator_name(platform, version):
  """Composes the name of simulator of platform and version strings."""
  return '%s %s test simulator' % (platform, version)


def get_simulator_list():
  """Gets list of available simulator as a dictionary."""
  return json.loads(
      subprocess.check_output(['xcrun', 'simctl', 'list',
                               '-j']).decode('utf-8'))


def get_simulator(platform, version):
  """Gets a simulator or creates a new one if not exist by platform and version.

  Args:
    platform: (str) A platform name, e.g. "iPhone 11 Pro"
    version: (str) A version name, e.g. "13.4"

  Returns:
    A udid of a simulator device.
  """
  udids = get_simulator_udids_by_platform_and_version(platform, version)
  if udids:
    return udids[0]
  return create_device_by_platform_and_version(platform, version)


def get_simulator_device_type_by_platform(simulators, platform):
  """Gets device type identifier for platform.

  Args:
    simulators: (dict) A list of available simulators.
    platform: (str) A platform name, e.g. "iPhone 11 Pro"

  Returns:
    Simulator device type identifier string of the platform.
    e.g. 'com.apple.CoreSimulator.SimDeviceType.iPhone-11-Pro'

  Raises:
    test_runner.SimulatorNotFoundError when the platform can't be found.
  """
  for devicetype in simulators['devicetypes']:
    if devicetype['name'] == platform:
      return devicetype['identifier']
  raise test_runner.SimulatorNotFoundError(
      'Not found device "%s" in devicetypes %s' %
      (platform, simulators['devicetypes']))


def get_simulator_runtime_by_version(simulators, version):
  """Gets runtime based on iOS version.

  Args:
    simulators: (dict) A list of available simulators.
    version: (str) A version name, e.g. "13.4"

  Returns:
    Simulator runtime identifier string of the version.
    e.g. 'com.apple.CoreSimulator.SimRuntime.iOS-13-4'

  Raises:
    test_runner.SimulatorNotFoundError when the version can't be found.
  """
  for runtime in simulators['runtimes']:
    if runtime['version'] == version and 'iOS' in runtime['name']:
      return runtime['identifier']
  raise test_runner.SimulatorNotFoundError('Not found "%s" SDK in runtimes %s' %
                                           (version, simulators['runtimes']))


def get_simulator_runtime_by_device_udid(simulator_udid):
  """Gets simulator runtime based on simulator UDID.

  Args:
    simulator_udid: (str) UDID of a simulator.
  """
  simulator_list = get_simulator_list()['devices']
  for runtime, simulators in simulator_list.items():
    for device in simulators:
      if simulator_udid == device['udid']:
        return runtime
  raise test_runner.SimulatorNotFoundError(
      'Not found simulator with "%s" UDID in devices %s' % (simulator_udid,
                                                            simulator_list))


def get_simulator_udids_by_platform_and_version(platform, version):
  """Gets list of simulators UDID based on platform name and iOS version.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
      version: (str) A version name, e.g. "13.2.2"
  """
  simulators = get_simulator_list()
  devices = simulators['devices']
  sdk_id = get_simulator_runtime_by_version(simulators, version)
  results = []
  for device in devices.get(sdk_id, []):
    if device['name'] == _compose_simulator_name(platform, version):
      results.append(device['udid'])
  return results


def create_device_by_platform_and_version(platform, version):
  """Creates a simulator and returns UDID of it.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
      version: (str) A version name, e.g. "13.2.2"
  """
  name = _compose_simulator_name(platform, version)
  LOGGER.info('Creating simulator %s', name)
  simulators = get_simulator_list()
  device_type = get_simulator_device_type_by_platform(simulators, platform)
  runtime = get_simulator_runtime_by_version(simulators, version)
  try:
    udid = subprocess.check_output(
        ['xcrun', 'simctl', 'create', name, device_type,
         runtime]).decode('utf-8').rstrip()
    LOGGER.info('Created simulator in first attempt with UDID: %s', udid)
    # Sometimes above command fails to create a simulator. Verify it and retry
    # once if first attempt failed.
    if not is_device_with_udid_simulator(udid):
      # Try to delete once to avoid duplicate in case of race condition.
      delete_simulator_by_udid(udid)
      udid = subprocess.check_output(
          ['xcrun', 'simctl', 'create', name, device_type,
           runtime]).decode('utf-8').rstrip()
      LOGGER.info('Created simulator in second attempt with UDID: %s', udid)
    return udid
  except subprocess.CalledProcessError as e:
    LOGGER.error('Error when creating simulator "%s": %s' % (name, e.output))
    raise e


def delete_simulator_by_udid(udid):
  """Deletes simulator by its udid.

  Args:
    udid: (str) UDID of simulator.
  """
  LOGGER.info('Deleting simulator %s', udid)
  try:
    subprocess.check_output(['xcrun', 'simctl', 'delete', udid],
                            stderr=subprocess.STDOUT).decode('utf-8')
  except subprocess.CalledProcessError as e:
    # Logging error instead of throwing so we don't cause failures in case
    # this was indeed failing to clean up.
    message = 'Failed to delete simulator %s with error %s' % (udid, e.output)
    LOGGER.error(message)


def wipe_simulator_by_udid(udid):
  """Wipes simulators by its udid.

  Args:
    udid: (str) UDID of simulator.
  """
  for _, devices in get_simulator_list()['devices'].items():
    for device in devices:
      if device['udid'] != udid:
        continue
      try:
        LOGGER.info('Shutdown simulator %s ', device)
        if device['state'] != 'Shutdown':
          subprocess.check_call(['xcrun', 'simctl', 'shutdown', device['udid']])
      except subprocess.CalledProcessError as ex:
        LOGGER.error('Shutdown failed %s ', ex)
      subprocess.check_call(['xcrun', 'simctl', 'erase', device['udid']])


def get_home_directory(platform, version):
  """Gets directory where simulators are stored.

  Args:
    platform: (str) A platform name, e.g. "iPhone 11"
    version: (str) A version name, e.g. "13.2.2"
  """
  return subprocess.check_output(
      ['xcrun', 'simctl', 'getenv',
       get_simulator(platform, version), 'HOME']).decode('utf-8').rstrip()


def boot_simulator_if_not_booted(sim_udid):
  """Boots the simulator of given udid.

  Args:
    sim_udid: (str) UDID of the simulator.

  Raises:
    test_runner.SimulatorNotFoundError if the sim_udid is not found on machine.
  """
  simulator_list = get_simulator_list()
  for _, devices in simulator_list['devices'].items():
    for device in devices:
      if device['udid'] != sim_udid:
        continue
      if device['state'] == 'Booted':
        return
      subprocess.check_output(['xcrun', 'simctl', 'boot',
                               sim_udid]).decode('utf-8')
      return
  raise test_runner.SimulatorNotFoundError(
      'Not found simulator with "%s" UDID in devices %s' %
      (sim_udid, simulator_list['devices']))


def get_app_data_directory(app_bundle_id, sim_udid):
  """Returns app data directory for a given app on a given simulator.

  Args:
    app_bundle_id: (str) Bundle id of application.
    sim_udid: (str) UDID of the simulator.
  """
  return subprocess.check_output(
      ['xcrun', 'simctl', 'get_app_container', sim_udid, app_bundle_id,
       'data']).decode('utf-8').rstrip()


def is_device_with_udid_simulator(device_udid):
  """Checks whether a device with udid is simulator or not.

  Args:
    device_udid: (str) UDID of a device.
  """
  simulator_list = get_simulator_list()['devices']
  for _, simulators in simulator_list.items():
    for device in simulators:
      if device_udid == device['udid']:
        return True
  return False


def copy_trusted_certificate(cert_path, udid):
  """Copies a cert into a simulator.

  This allows the simulator to install the input cert.

  Args:
    cert_path: (str) A path for the cert
    udid: (str) UDID of a simulator.
  """
  # TODO(crbug.com/1351820): Update wpr runner to use this function.
  if not os.path.exists(cert_path):
    LOGGER.error('Failed to find the cert path %s', cert_path)
    return

  LOGGER.info('Copying cert into %s', udid)
  # Try to boot first, if the simulator is already booted, continue.
  try:
    subprocess.check_call(['xcrun', 'simctl', 'boot', udid])
  except subprocess.CalledProcessError as e:
    if 'booted' not in str(e):
      # Logging error instead of throwing, so we don't cause failures in case
      # this was indeed failing to copy the cert.
      message = 'Failed to boot simulator before installing cert. ' \
                'Error: %s' % e.output
      LOGGER.error(message)
      return

  try:
    subprocess.check_call(
        ['xcrun', 'simctl', 'keychain', udid, 'add-root-cert', cert_path])
    subprocess.check_call(['xcrun', 'simctl', 'shutdown', udid])
  except subprocess.CalledProcessError as e:
    message = 'Failed to install cert. Error: %s' % e.output
    LOGGER.error(message)
