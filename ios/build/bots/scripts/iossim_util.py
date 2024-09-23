# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import subprocess
import time
import typing

import constants
import test_runner
import test_runner_errors
import mac_util

from collections import OrderedDict

LOGGER = logging.getLogger(__name__)

MAX_WAIT_TIME_TO_DELETE_RUNTIME = 45  # 45 seconds

SIMULATOR_DEFAULT_PATH = os.path.expanduser(
    '~/Library/Developer/CoreSimulator/Devices')

IOS18_SIM_RUNTIME_ID = 'com.apple.CoreSimulator.SimRuntime.iOS-18-0'

# TODO(crbug.com/40910268): remove Legacy Download once iOS 15.5 is deprecated
IOS_SIM_RUNTIME_BUILTIN_STATE = ['Legacy Download', 'Bundled with Xcode']


def _compose_simulator_name(platform, version):
  """Composes the name of simulator of platform and version strings."""
  return '%s %s test simulator' % (platform, version)


def get_simulator_list(path=SIMULATOR_DEFAULT_PATH):
  """Gets list of available simulator as a dictionary.

  Args:
    path: (str) Path to be passed to '--set' option.
  """
  return json.loads(
      subprocess.check_output(['xcrun', 'simctl', '--set', path, 'list',
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
    # The output might use version with a patch number (e.g. 17.0.1)
    # but the passed in version does not have a patch number (e.g. 17.0)
    # Therefore, we should use startswith for substring match.
    if runtime['version'].startswith(version) and 'iOS' in runtime['name']:
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
  # TODO(crbug.com/40234635): Update wpr runner to use this function.
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


def get_simulator_runtime_list():
  """Gets list of available simulator runtimes as a dictionary."""
  return json.loads(
      subprocess.check_output(['xcrun', 'simctl', 'runtime', 'list',
                               '-j']).decode('utf-8'))


def get_simulator_runtime_match_list():
  """Gets list of chosen simulator runtime for each simulator sdk type"""
  return json.loads(
      subprocess.check_output(
          ['xcrun', 'simctl', 'runtime', 'match', 'list',
           '-j']).decode('utf-8'))


def get_simulator_runtime_info_by_build(runtime_build):
  """Gets runtime object based on the runtime build.

  Args:
    runtime_build: (str) build id of the runtime, e.g. "20C52"

  Returns:
    a simulator runtime json object that contains all the info of an
    iOS runtime
    e.g.
    {
      "build" : "19F70",
      "deletable" : true,
      "identifier" : "FD9ED7F9-96A7-4621-B328-4C317893EC8A",
      etc...
    }
    if no runtime for the corresponding build id is found, then
    return None.
  """
  runtimes = get_simulator_runtime_list()
  for runtime in runtimes.values():
    if runtime['build'].lower() == runtime_build.lower():
      return runtime
  return None


def get_simulator_runtime_info_by_id(identifier):
  """Gets runtime object based on the runtime id.

  Args:
    identifier: (str) id of the runtime, e.g. "7A46A063-35D7"

  Returns:
    a simulator runtime json object that contains all the info of an
    iOS runtime
    e.g.
    {
      "build" : "19F70",
      "deletable" : true,
      "identifier" : "7A46A063-35D7",
      etc...
    }
    if no runtime for the corresponding id is found, then
    return None.
  """
  runtimes = get_simulator_runtime_list()
  for runtime in runtimes.values():
    if runtime['identifier'].lower() == identifier.lower():
      return runtime
  return None


def get_simulator_runtime_info(ios_version):
  """Gets runtime object based on iOS version.

  Args:
    version: (str) A version name, e.g. "13.4"

  Returns:
    a simulator runtime json object that contains all the info of an
    iOS runtime
    e.g.
    {
      "build" : "19F70",
      "deletable" : true,
      "identifier" : "FD9ED7F9-96A7-4621-B328-4C317893EC8A",
      etc...
    }
    if no runtime for the corresponding iOS version is found, then
    return None.
  """
  runtimes = get_simulator_runtime_list()
  for runtime in runtimes.values():
    # The output might use version with a patch number (e.g. 17.0.1)
    # but the passed in version does not have a patch number (e.g. 17.0)
    # Therefore, we should use startswith for substring match.
    if runtime['version'].startswith(ios_version):
      return runtime
  return None

def is_simulator_runtime_builtin(runtime):
  if (runtime is None or runtime['kind'] not in IOS_SIM_RUNTIME_BUILTIN_STATE):
    return False
  return True


def override_default_iphonesim_runtime(runtime_id, ios_version):
  """Overrides the default simulator runtime build version.

  The default simulator runtime build version that Xcode looks
  for might not be the same as what we downloaded. Therefore,
  this method gives the option for override the default with a
  different runtime build version (ideally the one we downloaded from cipd.

  Args:
    runtime_id: (str) the runtime id that we desire to use.
      The runtime build version will be extracted and override the
      default one.
    ios_version: the iOS version of the iphone sdk we want to
      override. e.g. 17.0
  """

  # find the runtime build number to override with
  overriding_build = None
  runtimes = get_simulator_runtime_list()
  for runtime_key in runtimes:
    if runtime_key in runtime_id:
      overriding_build = runtimes[runtime_key].get('build')
      break
  if overriding_build is None:
    LOGGER.debug(
        'Unable to find the simulator runtime build number to override with...')
    return

  # find the runtime build number to be overridden
  sdks = get_simulator_runtime_match_list()
  iphone_sdk_key = 'iphoneos' + ios_version
  sdk_build = sdks.get(iphone_sdk_key, {}).get("sdkBuild")
  if sdk_build is None:
    LOGGER.debug(
        'Unable to find the simulator runtime build number to be overriden...')
    return
  cmd = [
      'xcrun', 'simctl', 'runtime', 'match', 'set', iphone_sdk_key,
      overriding_build, '--sdkBuild', sdk_build
  ]
  LOGGER.debug('Overriding default runtime with command %s' % cmd)
  subprocess.check_call(cmd)


def add_simulator_runtime(runtime_dmg_path):
  cmd = ['xcrun', 'simctl', 'runtime', 'add', runtime_dmg_path]
  LOGGER.debug('Adding runtime with command %s' % cmd)
  return subprocess.check_output(cmd).decode('utf-8')


def delete_simulator_runtime(runtime_id, should_wait=False):
  cmd = ['xcrun', 'simctl', 'runtime', 'delete', runtime_id]
  LOGGER.debug('Deleting runtime with command %s' % cmd)
  subprocess.check_output(cmd)

  if should_wait:
    # runtime takes a few seconds to delete
    time_waited = 0
    runtime_to_delete = get_simulator_runtime_info_by_id(runtime_id)
    while (runtime_to_delete is not None):
      LOGGER.debug('Waiting for runtime to be deleted. Current state is %s' %
                   runtime_to_delete['state'])
      time.sleep(1)
      time_waited += 1
      if (time_waited > MAX_WAIT_TIME_TO_DELETE_RUNTIME):
        raise test_runner_errors.SimRuntimeDeleteTimeoutError(ios_version)
      runtime_to_delete = get_simulator_runtime_info_by_id(runtime_id)
    LOGGER.debug('Runtime successfully deleted!')


def delete_simulator_runtime_after_days(days):
  cmd = ['xcrun', 'simctl', 'runtime', 'delete', '--notUsedSinceDays', days]
  LOGGER.debug('Deleting unused runtime with command %s' % cmd)
  subprocess.run(cmd, check=False)


def delete_least_recently_used_simulator_runtimes(
    max_to_keep=constants.MAX_RUNTIME_KETP_COUNT):
  """Delete least recently used simulator runtimes.

  Delete simulator runtimes that are least recently used, based
  on the lastUsedAt field. iOS15.5 and runtimes bundled within Xcode
  are excluded.

  Args:
    max_to_keep: (int) max number of simulator runtimes to keep.
      All other simulator runtimes will be deleted based on lastUsedAt field.
  """

  runtimes = get_simulator_runtime_list()
  sorted_runtime_values = sorted(
      runtimes.values(), key=lambda x: x.get("lastUsedAt", ""), reverse=True)
  sorted_runtimes = OrderedDict(
      (item["identifier"], item) for item in sorted_runtime_values)

  keep_count = 0
  for runtime_id, value in sorted_runtimes.items():
    if is_simulator_runtime_builtin(value):
      LOGGER.debug('Built-in Runtime %s with iOS %s should not be deleted' %
                   (runtime_id, value['version']))
      continue
    if keep_count < max_to_keep:
      LOGGER.debug('Runtime %s with iOS %s should be kept undeleted' %
                   (runtime_id, value['version']))
      keep_count += 1
    else:
      delete_simulator_runtime(runtime_id, True)


def delete_simulator_runtime_and_wait(ios_version):
  runtime_to_delete = get_simulator_runtime_info(ios_version)
  if runtime_to_delete == None:
    LOGGER.debug('Runtime %s does not exist in Xcode, no need to cleanup...' %
                 ios_version)
    return

  delete_simulator_runtime(runtime_to_delete['identifier'], True)


def delete_other_ios18_runtimes(current_runtime_build_id: str):
  LOGGER.info(f'Deleting other iOS18 runtimes, i.e. with runtime identifier '
              f'{IOS18_SIM_RUNTIME_ID} and build NOT equal to '
              f'{current_runtime_build_id}')
  runtimes = get_simulator_runtime_list()
  for runtime in runtimes.values():
    if (runtime['runtimeIdentifier'] == IOS18_SIM_RUNTIME_ID and
        runtime['build'] != current_runtime_build_id):
      delete_simulator_runtime(runtime['identifier'], True)


def disable_hardware_keyboard(udid: str) -> None:
  """Disables hardware keyboard input for the given simulator.

  Exceptions are caught and logged but do not interrupt program flow. The result
  is that if the util is unable to change the HW keyboard pref for any reason
  the test will still run without changing the preference.

  Args:
    udid: (str) UDID of the simulator to disable hw keyboard for.
  """

  path = os.path.expanduser(
      '~/Library/Preferences/com.apple.iphonesimulator.plist')

  try:
    if not os.path.exists(path):
      subprocess.check_call(['plutil', '-create', 'binary1', path])

    plist, error = mac_util.plist_as_dict(path)
    if error:
      raise error

    if 'DevicePreferences' not in plist:
      subprocess.check_call(
          ['plutil', '-insert', 'DevicePreferences', '-dictionary', path])
      plist['DevicePreferences'] = {}

    if 'DevicePreferences' in plist and udid not in plist['DevicePreferences']:
      subprocess.check_call([
          'plutil', '-insert', 'DevicePreferences.{}'.format(udid),
          '-dictionary', path
      ])
      plist['DevicePreferences'][udid] = {}

    subprocess.check_call([
        'plutil', '-replace',
        'DevicePreferences.{}.ConnectHardwareKeyboard'.format(udid), '-bool',
        'NO', path
    ])

  except subprocess.CalledProcessError as e:
    message = 'Unable to disable hardware keyboard. Error: %s' % e.stderr
    LOGGER.error(message)
  except json.JSONDecodeError as e:
    message = 'Unable to disable hardware keyboard. Error: %s' % e.msg
    LOGGER.error(message)


def disable_simulator_keyboard_tutorial(udid):
  """Disables keyboard tutorial for the given simulator.

  Keyboard tutorial can cause flakes to EG tests as they are not expected.
  Exceptions are caught and logged but do not interrupt program flow.

  Args:
    udid: (str) UDID of the simulator.
  """
  boot_simulator_if_not_booted(udid)

  try:
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences', 'DidShowContinuousPathIntroduction',
        '1'
    ])
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences', 'KeyboardDidShowProductivityTutorial',
        '1'
    ])
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences', 'DidShowGestureKeyboardIntroduction',
        '1'
    ])
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences',
        'UIKeyboardDidShowInternationalInfoIntroduction', '1'
    ])
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences', 'KeyboardAutocorrection', '0'
    ])
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences', 'KeyboardPrediction', '0'
    ])
    subprocess.check_call([
        'xcrun', 'simctl', 'spawn', udid, 'defaults', 'write',
        'com.apple.keyboard.preferences', 'KeyboardShowPredictionBar', '0'
    ])
  except subprocess.CalledProcessError as e:
    message = 'Unable to disable keyboard tutorial: %s' % e.stderr
    LOGGER.error(message)
