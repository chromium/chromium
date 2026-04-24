# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import json
import logging
import os
import plistlib
import subprocess
import time
import typing
import functools
import sys

import constants
import test_runner
import test_runner_errors
import mac_util

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.append(
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')))
import measures


LOGGER = logging.getLogger(__name__)

MAX_WAIT_TIME_TO_DELETE_RUNTIME = 60  # 60 seconds

SIMULATOR_DEFAULT_PATH = os.path.expanduser(
    '~/Library/Developer/CoreSimulator/Devices')
SIMULATOR_CACHE_PATH = os.path.expanduser('~/Library/Developer/SimulatorCache')

# TODO(crbug.com/40910268): remove Legacy Download once iOS 15.5 is deprecated
IOS_SIM_RUNTIME_BUILTIN_STATE = ['Legacy Download', 'Bundled with Xcode']


def _compose_simulator_name(platform, version):
  """Composes the name of simulator of platform and version strings."""
  return '%s %s test simulator' % (platform, version)


def _compose_simctl_cmd(cmd: list[str], path: str = None) -> list[str]:
  """Composes a simctl command"""
  result = ['xcrun', 'simctl']
  if path:
    result += ['--set', path]
  result += cmd
  return result


def get_simulator_list(path: str = None):
  """Gets list of available simulator as a dictionary.

  Args:
    path: (str) Path to be passed to '--set' option.
  """
  return json.loads(
      subprocess.check_output(_compose_simctl_cmd(['list', '-j'],
                                                  path)).decode('utf-8'))


def get_simulator(platform, version, out_dir=None, use_cache=False):
  """Gets a simulator or creates a new one if not exist by platform and version.

  Args:
    platform: (str) A platform name, e.g. "iPhone 11 Pro"
    version: (str) A version name, e.g. "13.4"
    use_cache: (bool) Whether to use cache of prebooted simulators if a
      simulator must be created.

  Returns:
    A udid of a simulator device.
  """
  udids = get_simulator_udids_by_platform_and_version(platform, version,
                                                      out_dir)
  if udids:
    return udids[0]
  return create_device_by_platform_and_version(platform, version, use_cache)


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


def debug_missing_simulator(checked_runtimes, out_dir=None):
  if out_dir == None:
    return
  # where we looked and didn't find the given version
  checked_runtimes_path = os.path.join(
      os.path.abspath(out_dir), 'checked_runtimes.json')
  with open(checked_runtimes_path, "w") as f:
    f.write(json.dumps(checked_runtimes, indent=2))

  # sanity check of 'xcrun simctl runtime list -j'
  runtimes_path = os.path.join(os.path.abspath(out_dir), 'runtimes.json')
  runtimes = subprocess.check_output(
      ['xcrun', 'simctl', 'runtime', 'list', '-j']).decode('utf-8')
  with open(runtimes_path, "w") as f:
    f.write(runtimes)

  # is the runtime DMG still mounted?
  coresim_volumes_path = os.path.join(
      os.path.abspath(out_dir), 'coresim_volumes.txt')
  target_vol_path = '/Library/Developer/CoreSimulator/Volumes'
  if os.path.exists(target_vol_path):
    coresim_volumes_list = os.listdir(target_vol_path)
    coresim_volumes_str = "\n".join(
        coresim_volumes_list)  # Convert list to string
  else:
    coresim_volumes_str = "DIRECTORY MISSING"
  try:
    all_mounts = subprocess.check_output(['mount']).decode('utf-8')
    # Filter for the relevant lines in Python
    relevant_mounts = [
        line for line in all_mounts.splitlines()
        if '/Library/Developer/CoreSimulator/Volumes' in line
    ]
    coresim_mounts_str = "\n".join(relevant_mounts)
  except subprocess.CalledProcessError as e:
    coresim_mounts_str = f"Error running mount: {str(e)}"

  with open(coresim_volumes_path, "w") as f:
    f.write("--- os.listdir contents ---\n")
    f.write(coresim_volumes_str)
    f.write("\n\n--- 'mount' command output ---\n")
    f.write(coresim_mounts_str)


def get_simulator_runtime_by_platform_and_version(simulators,
                                                  platform,
                                                  version,
                                                  out_dir=None):
  """Finds the simulator runtime identifier for a given platform and OS version.

  Args:
    simulators: (dict) A list of available simulators.
    platform: (str) A platform name, e.g. "iPhone 11"
    version: (str) A version name, e.g. "13.4"

  Returns:
    Simulator runtime identifier string of the version.
    e.g. 'com.apple.CoreSimulator.SimRuntime.iOS-13-4'

  Raises:
    test_runner.SimulatorNotFoundError when the version can't be found.
  """
  runtimes = simulators['runtimes']
  max_retries = 2
  for attempt in range(0, max_retries):
    version_found = False
    for runtime in runtimes:
      # The output might use version with a patch number (e.g. 17.0.1)
      # but the passed in version does not have a patch number (e.g. 17.0)
      # Therefore, we should use startswith for substring match.
      if runtime['version'].startswith(version):
        version_found = True
        if any(supported_device_type['name'] == platform
               for supported_device_type in runtime['supportedDeviceTypes']):
          return runtime.get('identifier') or runtime.get('runtimeIdentifier')
    LOGGER.error(f'(attempt {attempt + 1} of {max_retries}) failed to find '
                 f'simulator matching version: {version} and platform: '
                 f'{platform}.')
    if version_found:
      LOGGER.error('Version found, but not platform.')
    if attempt + 1 < max_retries:
      # try again after sleeping
      time.sleep(5)
      runtimes = get_simulator_list().get('runtimes', [])
  # TODO(crbug.com/454911750): remove debugging after bug is resolved
  debug_missing_simulator(runtimes, out_dir)
  raise test_runner.SimulatorNotFoundError('Not found "%s" SDK in runtimes %s' %
                                           (version, runtimes))


def get_simulator_runtime_by_device_udid(simulator_udid, path=None):
  """Gets simulator runtime based on simulator UDID.

  Args:
    simulator_udid: (str) UDID of a simulator.
    path: path to simulator directory, passed to --set option of xcrun simctl
  """
  simulator_list = get_simulator_list(path)['devices']
  for runtime, simulators in simulator_list.items():
    for device in simulators:
      if simulator_udid == device['udid']:
        return runtime
  raise test_runner.SimulatorNotFoundError(
      'Not found simulator with "%s" UDID in devices %s' % (simulator_udid,
                                                            simulator_list))


def get_simulator_udids_by_platform_and_version(platform,
                                                version,
                                                out_dir=None,
                                                path=None):
  """Gets list of simulators UDID based on platform name and iOS version.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
      version: (str) A version name, e.g. "13.2.2"
  """
  simulators = get_simulator_list(path=path)
  devices = simulators['devices']
  sdk_id = get_simulator_runtime_by_platform_and_version(
      simulators, platform, version, out_dir)
  results = []
  for device in devices.get(sdk_id, []):
    if device['name'] == _compose_simulator_name(platform, version):
      results.append(device['udid'])
  return results


def get_platform_type_by_platform(platform) -> constants.IOSPlatformType:
  """Returns the iOS-based target platform (e.g. iOS, tvOS) based on a given
  platform name.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
  """
  device_type = get_simulator_device_type_by_platform(get_simulator_list(),
                                                      platform)
  if device_type.startswith('com.apple.CoreSimulator.SimDeviceType.Apple-TV'):
    return constants.IOSPlatformType.TVOS
  elif (device_type.startswith('com.apple.CoreSimulator.SimDeviceType.iPad') or
        device_type.startswith('com.apple.CoreSimulator.SimDeviceType.iPhone')):
    return constants.IOSPlatformType.IPHONEOS
  raise test_runner.UnsupportedDeviceTypeError(device_type)


def _create_device_by_platform_and_version(platform, version, path=None):
  """Creates a simulator at the given path, returning its udid"""
  name = _compose_simulator_name(platform, version)
  LOGGER.info('Creating simulator %s', name)
  simulators = get_simulator_list(path)
  device_type = get_simulator_device_type_by_platform(simulators, platform)
  runtime = get_simulator_runtime_by_platform_and_version(
      simulators, platform, version)
  try:
    udid = subprocess.check_output(
        _compose_simctl_cmd(['create', name, device_type, runtime],
                            path)).decode('utf-8').rstrip()
    LOGGER.info('Created simulator in first attempt with UDID: %s', udid)
    # Sometimes above command fails to create a simulator. Verify it and retry
    # once if first attempt failed.
    if not is_device_with_udid_simulator(udid, path=path):
      # Try to delete once to avoid duplicate in case of race condition.
      delete_simulator_by_udid(udid, path=path)
      udid = subprocess.check_output(
          _compose_simctl_cmd(['create', name, device_type, runtime],
                              path)).decode('utf-8').rstrip()
      LOGGER.info('Created simulator in second attempt with UDID: %s', udid)
    return udid
  except subprocess.CalledProcessError as e:
    LOGGER.error('Error when creating simulator "%s": %s' % (name, e.output))
    raise e


def create_device_by_platform_and_version(platform, version, use_cache=False):
  """Creates a simulator and returns UDID of it.

    Args:
      platform: (str) A platform name, e.g. "iPhone 11"
      version: (str) A version name, e.g. "13.2.2"
      use_cache: (bool) Whether to try to use a clone of a prebooted simulator
        in the cache.
  """
  enabled_datapoint = measures.data_points('simulator_caching_enabled')
  if not use_cache:
    LOGGER.info("Simulator caching not enabled. Creating disposable simulator "
                "in the default set.")
    enabled_datapoint.record(False)
    return _create_device_by_platform_and_version(platform, version)

  enabled_datapoint.record(True)

  # Ensure Cache Path Exists
  os.makedirs(SIMULATOR_CACHE_PATH, exist_ok=True)

  LOGGER.info(f"Simulator caching is enabled. "
              f"Checking if a {version} {platform} simulator is in cache")
  cache_udids = get_simulator_udids_by_platform_and_version(
      platform, version, path=SIMULATOR_CACHE_PATH)

  cache_hit_datapoint = measures.data_points('simulator_cache_hit')
  if cache_udids:
    LOGGER.info("Simulator found in cache. Cloning into default set")
    cache_hit_datapoint.record(True)
    with measures.time_consumption('Simulator clone', 'cache to working set'):
      udid = clone_simulator_by_udid(
          cache_udids[0],
          _compose_simulator_name(platform, version),
          path=SIMULATOR_CACHE_PATH,
          dest_path=SIMULATOR_DEFAULT_PATH)
    return udid

  cache_hit_datapoint.record(False)
  LOGGER.info("Simulator not found in cache, attempting to create")

  max_attempts = 2
  for attempt in range(max_attempts):
    udid = _create_device_by_platform_and_version(
        platform, version, path=SIMULATOR_CACHE_PATH)

    # Run first boot of the simulator to ensure that costly data migrations
    # have completed, then shutdown simulator so it can be cloned for future
    # use.

    with measures.time_consumption('Simulator full boot', 'iossim_util',
                                   'Pre launch for cache creation',
                                   f'attempt {attempt}'):
      booted = ensure_simulator_fully_booted(udid, path=SIMULATOR_CACHE_PATH)
    if booted:
      shutdown_simulator_by_udid(udid, path=SIMULATOR_CACHE_PATH)
      LOGGER.info(
          f"Attempt {attempt} to create simulator and boot it succeeded. "
          f"Cloning simulator into the default set.")
      with measures.time_consumption('Simulator clone', 'cache to working set'):
        udid = clone_simulator_by_udid(
            udid,
            _compose_simulator_name(platform, version),
            path=SIMULATOR_CACHE_PATH,
            dest_path=SIMULATOR_DEFAULT_PATH,
        )
      return udid
    LOGGER.info(f"Attempt {attempt} to create simulator in cache failed.")
    shutdown_simulator_by_udid(udid, path=SIMULATOR_CACHE_PATH)
    delete_simulator_by_udid(udid, path=SIMULATOR_CACHE_PATH)
  else:
    LOGGER.info(
        f"Unable to create cached simulator in {max_attempts} attempts. "
        f"Creating a disposable simulator in default set.")

    return _create_device_by_platform_and_version(platform, version)


def clone_simulator_by_udid(udid: str,
                            name: str,
                            path: str = None,
                            dest_path: str = None):
  """Clones given simulator located at path into dest_path.

  Args:
    udid: (str) UDID of simulator to clone.
    name (str) name to give to the newly cloned simulator.
    path: (str) Path where the source simulator is located.
    dest_path: (str) Path to place the clone of the simulator. If not provided
      will default to xcrun simctl clone's default behavior which is to create
      the clone in the same path as the source simulator.

  Returns:
    UDID of newly cloned simulator
  """

  command = _compose_simctl_cmd(['clone', udid, name], path=path)

  if dest_path:
    command += [dest_path]

  return subprocess.check_output(command).decode('utf-8').strip()


def shutdown_simulator_by_udid(udid: str, path: str = None):
  """Shuts down a simulator by its udid

  Args:
    udid: (str) UDID of simulator
    path: (str) path of the simulator
  """
  for _, devices in get_simulator_list(path)['devices'].items():
    for device in devices:
      if device['udid'] != udid:
        continue
      try:
        LOGGER.info('Shutdown simulator %s ', device)
        if device['state'] != 'Shutdown':
          subprocess.check_call(
              _compose_simctl_cmd(['shutdown', device['udid']], path))
        break
      except subprocess.CalledProcessError as ex:
        LOGGER.error('Shutdown failed %s ', ex)


def delete_simulator_by_udid(udid, path: str = None):
  """Deletes simulator by its udid.

  Args:
    udid: (str) UDID of simulator.
  """
  LOGGER.info('Deleting simulator %s', udid)
  try:
    subprocess.check_output(
        _compose_simctl_cmd(['delete', udid], path=path),
        stderr=subprocess.STDOUT).decode('utf-8')
    is_device_with_udid_simulator.cache_clear()
  except subprocess.CalledProcessError as e:
    # Logging error instead of throwing so we don't cause failures in case
    # this was indeed failing to clean up.
    message = 'Failed to delete simulator %s with error %s' % (udid, e.output)
    LOGGER.error(message)


def wipe_simulator_by_udid(udid, path: str = None):
  """Wipes simulators by its udid.

  Args:
    udid: (str) UDID of simulator.
    path: (str) Path where simulator is located.
  """
  shutdown_simulator_by_udid(udid, path)
  subprocess.check_call(_compose_simctl_cmd(['erase', udid], path))


def get_home_directory(platform, version):
  """Gets directory where simulators are stored.

  Args:
    platform: (str) A platform name, e.g. "iPhone 11"
    version: (str) A version name, e.g. "13.2.2"
  """
  return subprocess.check_output(
      ['xcrun', 'simctl', 'getenv',
       get_simulator(platform, version), 'HOME']).decode('utf-8').rstrip()


def boot_simulator_if_not_booted(sim_udid, path=SIMULATOR_DEFAULT_PATH):
  """Boots the simulator of given udid.

  Args:
    sim_udid: (str) UDID of the simulator.

  Raises:
    test_runner.SimulatorNotFoundError if the sim_udid is not found on machine.
  """
  simulator_list = get_simulator_list(path=path)
  for _, devices in simulator_list['devices'].items():
    for device in devices:
      if device['udid'] != sim_udid:
        continue
      if device['state'] == 'Booted':
        return
      subprocess.check_output(
          ['xcrun', 'simctl', '--set', path, 'boot', sim_udid]).decode('utf-8')
      return
  raise test_runner.SimulatorNotFoundError(
      'Not found simulator with "%s" UDID in devices %s' %
      (sim_udid, simulator_list['devices']))


def update_dyld_shared_cache(runtime=None):
  """Updates dyld_shared_cache before starting to boot simulators.

  Args:
    runtime: (str) simulator runtime. E.g. "com.apple.CoreSimulator.SimRuntime.iOS-18-5".
      If None, updates all runtimes.
  """
  LOGGER.info('Updating dyld_shared_cache')
  cmd = ['xcrun', 'simctl', 'runtime', 'dyld_shared_cache', 'update']
  cmd.append(runtime if runtime else '--all')
  try:
    subprocess.check_call(cmd)
  except subprocess.CalledProcessError as e:
    # This command was introduced in Xcode 26.1 as a workaround for
    # simulator boot failures (see upstream commit b4a2fed92851 and
    # https://developer.apple.com/documentation/xcode-release-notes/xcode-26_1-release-notes).
    # On older Xcode versions (e.g. 15.2) this subcommand doesn't exist
    # and returns a non-zero exit code. We gracefully skip it so tests
    # can still run on machines with older Xcode installations.
    LOGGER.warning('Failed to update dyld_shared_cache. Error: %s', e.output)


def ensure_simulator_fully_booted(sim_udid: str, path=None, num_attempts=1):
  """Ensures simulator of given udid is fully booted.

  `xcrun simctl boot` launches only background processes and does not ensure the
  entire boot process runs. Running `xcrun simctl bootstatus UDID -b` monitors
  the specified device until the device finishes booting exiting with return
  code 0 when the device is fully booted.

  Args:
    sim_udid: (str) UDID of simulator
    path: path to simulator directory, passed to --set option of xcrun simctl

  Raises:
    test_runner.SimulatorNotFoundError if the sim_udid is not on machine

  Returns:
    True if the simulator was successfully booted, false otherwise.
  """
  if not is_device_with_udid_simulator(sim_udid, path=path):
    raise test_runner.SimulatorNotFoundError(
        f"Not found simulator with UDID: {sim_udid}")

    # Ensure data migrations are run
  cmd = _compose_simctl_cmd([
      'bootstatus',
      sim_udid,
      '-bd',
  ], path)
  runtime = get_simulator_runtime_by_device_udid(sim_udid, path=path)
  for boot_attempt in range(num_attempts):
    try:
      update_dyld_shared_cache(runtime)
      subprocess.check_call(cmd, timeout=120)
      return True
    except subprocess.TimeoutExpired as e:
      msg = f"Manually booting simulator timed out after 120 seconds."
      LOGGER.info(msg)
      msg_again = " again" if boot_attempt > 0 else ""
      msg_action = "continuing" if boot_attempt == num_attempts - 1 else "retrying"
      LOGGER.info(f"Failed to manually boot simulator{msg_again}. "
                  f"Wiping simulator and {msg_action}.")
      wipe_simulator_by_udid(sim_udid, path)
      test_runner.SimulatorTestRunner.kill_simulators()

  return False



def get_app_data_directory(app_bundle_id, sim_udid):
  """Returns app data directory for a given app on a given simulator.

  Args:
    app_bundle_id: (str) Bundle id of application.
    sim_udid: (str) UDID of the simulator.
  """
  return subprocess.check_output(
      ['xcrun', 'simctl', 'get_app_container', sim_udid, app_bundle_id,
       'data']).decode('utf-8').rstrip()


@functools.cache
def is_device_with_udid_simulator(device_udid, path: str = None):
  """Checks whether a device with udid is simulator or not.

  Args:
    device_udid: (str) UDID of a device.
    path: (str) path to simulator directory, passed to --set option
      of xcrun simctl.
  """
  simulator_list = get_simulator_list(path=path)['devices']
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
    build = runtime.get('build')
    if build and build.lower() == runtime_build.lower():
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
    runtime_id = runtime.get('identifier')
    if runtime_id and runtime_id.lower() == identifier.lower():
      return runtime
  return None


def get_simulator_runtime_info(platform_type: constants.IOSPlatformType,
                               platform_version: str):
  """Gets runtime object based on iOS version.

  Args:
    platform_type: (IOSPlatformType) iOS-based platform in use
    platform_version: (str) A version name, e.g. "13.4"

  Returns:
    a simulator runtime json object that contains all the info of an
    iOS/tvOS runtime
    e.g.
    {
      "build" : "19F70",
      "deletable" : true,
      "identifier" : "FD9ED7F9-96A7-4621-B328-4C317893EC8A",
      etc...
    }
    if no runtime for the corresponding iOS/tvOS version is found, then
    return None.
  """
  if platform_type == constants.IOSPlatformType.IPHONEOS:
    platform_identifier = "com.apple.platform.iphonesimulator"
  elif platform_type == constants.IOSPlatformType.TVOS:
    platform_identifier = "com.apple.platform.appletvsimulator"
  else:
    raise ValueError('Invalid platform_type value: %s' % platform_type)

  runtimes = get_simulator_runtime_list()
  for runtime in runtimes.values():
    # The output might use version with a patch number (e.g. 17.0.1)
    # but the passed in version does not have a patch number (e.g. 17.0)
    # Therefore, we should use startswith for substring match.
    version = runtime.get('version')
    if version and version.startswith(platform_version) and runtime.get(
        'platformIdentifier') == platform_identifier:
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
        'Unable to find the simulator runtime build number to be overridden...')
    return
  cmd = [
      'xcrun', 'simctl', 'runtime', 'match', 'set', iphone_sdk_key,
      overriding_build, '--sdkBuild', sdk_build
  ]
  LOGGER.debug('Overriding default runtime with command %s' % cmd)
  subprocess.check_call(cmd)


def add_simulator_runtime(runtime_dmg_path):
  cmd = ['xcrun', 'simctl', 'runtime', 'add', runtime_dmg_path, '--verbose']
  LOGGER.debug('Adding runtime with command %s' % cmd)
  return subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')


def delete_simulator_runtime(runtime_id, should_wait=False):
  cmd = ['xcrun', 'simctl', 'runtime', 'delete', runtime_id]
  LOGGER.debug('Deleting runtime with command %s' % cmd)
  try:
    subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    # The error message contains "Cannot stage disk image" when trying to
    # delete a runtime that is already deleted.
    if (b'Cannot stage disk image or bundle for delete' in e.output):
      LOGGER.warning(
          'Error when deleting runtime %s. It may have been already deleted. '
          'Error: %s', runtime_id, e.output.decode('utf-8', 'ignore'))
      return
    else:
      raise

  if should_wait:
    # runtime takes a few seconds to delete
    time_waited = 0
    runtime_to_delete = get_simulator_runtime_info_by_id(runtime_id)
    while runtime_to_delete is not None:
      LOGGER.debug('Waiting for runtime to be deleted. Current state is %s' %
                   runtime_to_delete['state'])
      time.sleep(1)
      time_waited += 1
      if (time_waited > MAX_WAIT_TIME_TO_DELETE_RUNTIME):
        raise test_runner_errors.SimRuntimeDeleteTimeoutError(runtime_id)
      runtime_to_delete = get_simulator_runtime_info_by_id(runtime_id)
    LOGGER.debug('Runtime successfully deleted!')


def delete_least_recently_used_simulator_runtimes(
    max_to_keep=constants.MAX_RUNTIME_KEPT_COUNT):
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
      LOGGER.debug('Runtime %s should be kept. Current runtime count %s', value,
                   keep_count)
      keep_count += 1
    else:
      LOGGER.debug(
          'Runtime %s should be deleted due to exceeding max runtime count %s',
          value, max_to_keep)
      delete_simulator_runtime(runtime_id, True)
      remove_stale_simulators_from_cache()


def delete_stale_simulator_runtimes():
  """Delete stale simulator runtimes.

  Delete simulator runtimes that are unusable

  """

  runtimes = get_simulator_runtime_list()

  for runtime_id, value in runtimes.items():
    if value['state'] == "Unusable":
      LOGGER.debug('Runtime %s should be deleted due to stale state', value)
      delete_simulator_runtime(runtime_id, True)
      remove_stale_simulators_from_cache()


def remove_stale_simulators_from_cache():
  """Removes stale simulators from the cache.

  Once a simulator runtime has been removed the simulators in the cache that use
  that runtime will be marked as unavailable. This function should run
  periodically to clean up disk space.
  """

  if os.path.isdir(SIMULATOR_CACHE_PATH):
    subprocess.check_call(
        _compose_simctl_cmd(['delete', 'unavailable'],
                            path=SIMULATOR_CACHE_PATH))
    is_device_with_udid_simulator.cache_clear()


def shutdown_all_simulators(path=None):
  """Shutdown all simulator devices.

  Args:
    path: (str) A path to the directory containing the simulators.
  """
  try:
    subprocess.check_call(_compose_simctl_cmd(['shutdown', 'all'], path))
  except subprocess.CalledProcessError as e:
    LOGGER.error('Failed to shutdown all simulators. Error: %s' % e.output)


def delete_all_simulators(path=None):
  """Deletes all simulator devices.

  Args:
    path: (str) A path to the directory containing the simulators.
  """
  try:
    subprocess.check_call(_compose_simctl_cmd(['delete', 'all'], path))
  except subprocess.CalledProcessError as e:
    LOGGER.error('Failed to delete all simulators. Error: %s' % e.output)
  finally:
    is_device_with_udid_simulator.cache_clear()


def erase_all_simulators(path=None):
  """Erases all simulator devices.

  Args:
    path: (str) A path to the directory containing the simulators.
  """
  try:
    subprocess.check_call(_compose_simctl_cmd(['erase', 'all'], path))
  except subprocess.CalledProcessError as e:
    LOGGER.error('Failed to erase all simulators. Error: %s' % e.output)


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
    plist = {}
    if os.path.exists(path):
      with open(path, 'rb') as f:
        plist = plistlib.load(f, fmt=plistlib.FMT_BINARY)
    prefs_val = plist.setdefault('DevicePreferences', {})
    udid_val = prefs_val.setdefault(udid, {})
    udid_val['ConnectHardwareKeyboard'] = False
    with open(path, 'wb') as f:
      plistlib.dump(plist, f, fmt=plistlib.FMT_BINARY)
  except Exception:
    LOGGER.exception('Failed to disable hardware keyboard.')

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
