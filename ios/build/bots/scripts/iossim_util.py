# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import OrderedDict
import contextlib
import functools
import json
import logging
import os
import plistlib
import select
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import typing  # noqa: F401
import uuid
import xml.parsers.expat

import constants
import test_runner
import test_runner_errors
import mac_util  # noqa: F401

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.append(
  os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto'))
)
import measures  # noqa: E402


LOGGER = logging.getLogger(__name__)

MAX_WAIT_TIME_TO_DELETE_RUNTIME = 60  # 60 seconds

SIMULATOR_DEFAULT_PATH = os.path.expanduser(
  '~/Library/Developer/CoreSimulator/Devices'
)
SIMULATOR_CACHE_PATH = os.path.expanduser('~/Library/Developer/SimulatorCache')
SIMULATOR_XCTEST_DEVICES_PATH = os.path.expanduser(
  '~/Library/Developer/XCTestDevices'
)
SIMULATOR_DISABLED_STORE_ROOT = '/private/var/tmp'
SIMULATOR_PLIST_STATE_BOOTED = (
  3  # CoreSimulator SimDeviceState.Booted in device.plist.
)
DEFAULT_DISABLE_SIMULATOR_DAEMONS = True
IOS_DISABLE_SIMULATOR_DAEMONS_ENV = 'IOS_DISABLE_SIMULATOR_DAEMONS'
_DISABLE_SIMULATOR_DAEMONS_OVERRIDE: bool | None = None

# Background launchd jobs safe to disable in Chrome for iOS test simulators via
# per-UDID host-side `disabled.plist` overrides. Because `launchd_sim` runs on
# the host, these overrides reduce idle memory footprint and boot contention
# without mutating signed runtime volumes.
#
# Daemons required by Chrome, WebKit, or XCTest are intentionally excluded:
# - `com.apple.ScreenTimeAgent`, `com.apple.ScreenTimeSettingsAgent`:
#   `WebTestSuite::Initialize` blocks up to 120s waiting on ScreenTime XPC.
# - `com.apple.passd`, `com.apple.financed`: required by `PassKitEGTest`.
# - `com.apple.assetsd`, `com.apple.medialibraryd`, `com.apple.photoanalysisd`:
#   required by photo library / picker flows in `FileUploadPanelEGTest`.
# - `com.apple.contactsd`, `com.apple.calaccessd`: required by `VcardEGTest`.
# - `com.apple.searchd`, `com.apple.searchtoold`,
#   `com.apple.corespotlightservice`: required by `SpotlightInterface` and the
#   document picker.
# - `com.apple.sharingd`: required for system share sheets.
# - `com.apple.Safari.passwordbreachd`: required by tests using the "Turn on
#   AutoFill" flow.
# - `com.apple.gamed`: required for timely XCTest / XCUITest app launches.
# - com.apple.assistantd: kept running because it hosts dictation XPC
#   services; without it Spotlight leaks memory retrying connections.
# - `com.apple.intelligenceplatformd`: kept running because disabling it causes
#   `biomed`'s per-process `IntelligencePlatformComputeService` XPC service
#   (which ignores system `disabled.plist`) to spin for ~20s CPU on boot.
# - WebKit, MDM/policy, and CoreSimulator/SpringBoard-sensitive daemons
#   (`com.apple.webkit.*`, `com.apple.webprivacyd`, `com.apple.swcd`,
#   `com.apple.nanoregistry*`, `com.apple.sleepd`, `com.apple.mobiletimerd`,
#   `com.apple.routined`, `com.apple.biomed*`, `com.apple.donotdisturbd`,
#   `com.apple.appprotectiond`, `com.apple.managedconfiguration.profiled`).
DISABLED_SIMULATOR_LAUNCHD_JOBS: tuple[str, ...] = (
  # Wallpaper & widgets.
  'com.apple.PosterBoard',
  'com.apple.chronod',
  'com.apple.liveactivitiesd',
  'com.apple.navd',
  # Siri, speech & Apple Intelligence / on-device ML (excludes assistantd and
  # intelligenceplatformd).
  'com.apple.assistant_cdmd',
  'com.apple.assistant_service',
  'com.apple.siriactionsd',
  'com.apple.siriinferenced',
  'com.apple.siriknowledged',
  'com.apple.sirittsd',
  'com.apple.siri.context.service',
  'com.apple.siri.acousticsignature',
  'com.apple.corespeechd',
  'com.apple.voiced',
  'com.apple.voicebankingd',
  'com.apple.speechmodeltrainingd',
  'com.apple.intelligencecontextd',
  'com.apple.intelligenceflowd',
  'com.apple.intelligencetasksd',
  'com.apple.generativeexperiencesd',
  'com.apple.knowledgeconstructiond',
  'com.apple.naturallanguaged',
  'com.apple.textunderstandingd',
  'com.apple.modelcatalogd',
  'com.apple.modelmanagerd',
  'com.apple.mlhostd',
  'com.apple.mlruntimed',
  'com.apple.suggestd',
  'com.apple.parsecd',
  'com.apple.parsec-fbf',
  'com.apple.proactiveeventtrackerd',
  # Health, HomeKit & Fitness.
  'com.apple.healthd',
  'com.apple.healthappd',
  'com.apple.healthcontentd',
  'com.apple.healtheventsd',
  'com.apple.healthrecordsd',
  'com.apple.finhealthd',
  'com.apple.homed',
  'com.apple.homeeventsd',
  'com.apple.fitcore',
  'com.apple.fitcore.session',
  'com.apple.fitnesscoachingd',
  'com.apple.fitnessintelligenced',
  'com.apple.activityawardsd',
  'com.apple.activitysharingd',
  # iCloud, CloudKit & account sync.
  'com.apple.appleaccountd',
  'com.apple.appleaccounttransparencyd',
  'com.apple.appleidsetupd',
  'com.apple.akd',
  'com.apple.cloudd',
  'com.apple.cloudphotod',
  'com.apple.ckdiscretionaryd',
  'com.apple.cloudsettingssyncagent',
  'com.apple.bird',
  'com.apple.syncdefaultsd',
  'com.apple.cdpd',
  'com.apple.sosd',
  'com.apple.SecureBackupDaemon',
  'com.apple.TrustedPeersHelper',
  'com.apple.protectedcloudstorage.protectedcloudkeysyncing',
  'com.apple.icloudmailagent',
  'com.apple.icloudsubscriptionoptimizerd',
  'com.apple.ind',
  # Built-in consumer apps & media store.
  'com.apple.newsd',
  'com.apple.weatherd',
  'com.apple.Maps.mapssyncd',
  'com.apple.Maps.mapspushd',
  'com.apple.Maps.geocorrectiond',
  'com.apple.maps.destinationd',
  'com.apple.MapKit.SnapshotService',
  'com.apple.tipsd',
  'com.apple.jetpackassetd',
  'com.apple.gamesaved',
  'com.apple.GameController.gamecontrollerd',
  'com.apple.appstored',
  'com.apple.appstorecomponentsd',
  'com.apple.itunescloudd',
  'com.apple.itunesstored',
  'com.apple.storekitd',
  'com.apple.amsaccountsd',
  'com.apple.amsengagementd',
  'com.apple.amsondevicestoraged',
  'com.apple.musicd',
  'com.apple.videosubscriptionsd',
  'com.apple.assetsubscriptiond',
  'com.apple.apsd',
  # Messaging, FaceTime, Mail, Reminders & companion connectivity.
  'com.apple.identityservicesd',
  'com.apple.ids_simd',
  'com.apple.imautomatichistorydeletionagent',
  'com.apple.imcore.imtransferagent',
  'com.apple.imdpersistence.IMDPersistenceAgent',
  'com.apple.facetimemessagestored',
  'com.apple.telephonyutilities.callservicesd',
  'com.apple.email.maild',
  'com.apple.exchangesyncd',
  'com.apple.dataaccess.dataaccessd',
  'com.apple.remindd',
  'com.apple.contacts.postersyncd',
  'com.apple.peopled',
  'com.apple.rapportd',
  'com.apple.companiond',
  'com.apple.carkitd',
  'com.apple.wcd',
  'com.apple.tvremoted',
  'com.apple.avatarsd',
  'com.apple.stickersd',
  'com.apple.sociallayerd',
  'com.apple.announced',
  'com.apple.findmy.findmylocated',
  # Safari sync, media ML & background telemetry.
  'com.apple.SafariBookmarksSyncAgent',
  'com.apple.Safari.History',
  'com.apple.safarifetcherd',
  'com.apple.mediaanalysisd',
  'com.apple.mediaanalysisd.service',
  'com.apple.mediastream.mstreamd',
  'com.apple.ClipServices.clipserviced',
  'com.apple.accessibility.axassetsd',
  'com.apple.storagedatad',
  'com.apple.StatusKitAgent',
  'com.apple.businessservicesd',
  'com.apple.ap.adprivacyd',
  'com.apple.ap.promotedcontentd',
  'com.apple.diagnosticextensionsd',
  'com.apple.feedbackd',
  'com.apple.rtcreportingd',
  'com.apple.securityuploadd',
  'com.apple.geoanalyticsd',
  'com.apple.triald',
  'com.apple.followupd',
  'com.apple.purplebuddy.budd',
)

# TODO(crbug.com/40910268): remove Legacy Download once iOS 15.5 is deprecated
IOS_SIM_RUNTIME_BUILTIN_STATE = ['Legacy Download', 'Bundled with Xcode']


def _compose_simulator_name(platform, version):
  """Composes the name of simulator of platform and version strings."""
  return f'{platform} {version} test simulator'


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
    subprocess.check_output(_compose_simctl_cmd(['list', '-j'], path)).decode(
      'utf-8'
    )
  )


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
  udids = get_simulator_udids_by_platform_and_version(
    platform, version, out_dir
  )
  if udids:
    disable_simulator_daemons(udids[0])
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
    f'Not found device "{platform}" in devicetypes {simulators["devicetypes"]}'
  )


def debug_missing_simulator(checked_runtimes, out_dir=None):
  if out_dir == None:  # noqa: E711
    return
  # where we looked and didn't find the given version
  checked_runtimes_path = os.path.join(
    os.path.abspath(out_dir), 'checked_runtimes.json'
  )
  with open(checked_runtimes_path, "w") as f:
    f.write(json.dumps(checked_runtimes, indent=2))

  # sanity check of 'xcrun simctl runtime list -j'
  runtimes_path = os.path.join(os.path.abspath(out_dir), 'runtimes.json')
  runtimes = subprocess.check_output(
    ['xcrun', 'simctl', 'runtime', 'list', '-j']
  ).decode('utf-8')
  with open(runtimes_path, "w") as f:
    f.write(runtimes)

  # is the runtime DMG still mounted?
  coresim_volumes_path = os.path.join(
    os.path.abspath(out_dir), 'coresim_volumes.txt'
  )
  target_vol_path = '/Library/Developer/CoreSimulator/Volumes'
  if os.path.exists(target_vol_path):
    coresim_volumes_list = os.listdir(target_vol_path)
    coresim_volumes_str = "\n".join(
      coresim_volumes_list
    )  # Convert list to string
  else:
    coresim_volumes_str = "DIRECTORY MISSING"
  try:
    all_mounts = subprocess.check_output(['mount']).decode('utf-8')
    # Filter for the relevant lines in Python
    relevant_mounts = [
      line
      for line in all_mounts.splitlines()
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


def get_simulator_runtime_by_platform_and_version(
  simulators, platform, version, out_dir=None
):
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
  version_parts = version.split('-', 1)
  base_version = version_parts[0]
  variant_suffix = version_parts[1].lower() if len(version_parts) > 1 else None
  max_retries = 2
  for attempt in range(0, max_retries):
    version_found = False
    for runtime in runtimes:
      runtime_id = (
        runtime.get('identifier') or runtime.get('runtimeIdentifier') or ''
      )
      if variant_suffix and not runtime_id.lower().endswith(
        f'-{variant_suffix}'
      ):
        continue
      # The output might use version with a patch number (e.g. 17.0.1)
      # but the passed in version does not have a patch number (e.g. 17.0)
      # Therefore, we should use startswith for substring match.
      if runtime['version'].startswith(base_version):
        version_found = True
        if any(
          supported_device_type['name'] == platform
          for supported_device_type in runtime['supportedDeviceTypes']
        ):
          return runtime_id
    LOGGER.error(
      f'(attempt {attempt + 1} of {max_retries}) failed to find '
      f'simulator matching version: {version} and platform: '
      f'{platform}.'
    )
    if version_found:
      LOGGER.error('Version found, but not platform.')
    if attempt + 1 < max_retries:
      # try again after sleeping
      time.sleep(5)
      runtimes = get_simulator_list().get('runtimes', [])
  # TODO(crbug.com/454911750): remove debugging after bug is resolved
  debug_missing_simulator(runtimes, out_dir)
  raise test_runner.SimulatorNotFoundError(
    f'Not found "{version}" SDK in runtimes {runtimes}'
  )


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
    f'Not found simulator with "{simulator_udid}" UDID in devices {simulator_list}'
  )


def get_simulator_udids_by_platform_and_version(
  platform, version, out_dir=None, path=None
):
  """Gets list of simulators UDID based on platform name and iOS version.

  Args:
    platform: (str) A platform name, e.g. "iPhone 11"
    version: (str) A version name, e.g. "13.2.2"
  """
  simulators = get_simulator_list(path=path)
  devices = simulators['devices']
  sdk_id = get_simulator_runtime_by_platform_and_version(
    simulators, platform, version, out_dir
  )
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
  device_type = get_simulator_device_type_by_platform(
    get_simulator_list(), platform
  )
  if device_type.startswith('com.apple.CoreSimulator.SimDeviceType.Apple-TV'):
    return constants.IOSPlatformType.TVOS
  elif device_type.startswith(
    'com.apple.CoreSimulator.SimDeviceType.iPad'
  ) or device_type.startswith('com.apple.CoreSimulator.SimDeviceType.iPhone'):
    return constants.IOSPlatformType.IPHONEOS
  raise test_runner.UnsupportedDeviceTypeError(device_type)


def _create_device_by_platform_and_version(platform, version, path=None):
  """Creates a simulator at the given path, returning its udid"""
  name = _compose_simulator_name(platform, version)
  LOGGER.info('Creating simulator %s', name)
  simulators = get_simulator_list(path)
  device_type = get_simulator_device_type_by_platform(simulators, platform)
  runtime = get_simulator_runtime_by_platform_and_version(
    simulators, platform, version
  )
  try:
    udid = (
      subprocess.check_output(
        _compose_simctl_cmd(['create', name, device_type, runtime], path)
      )
      .decode('utf-8')
      .rstrip()
    )
    LOGGER.info('Created simulator in first attempt with UDID: %s', udid)
    # Sometimes above command fails to create a simulator. Verify it and retry
    # once if first attempt failed.
    if not is_device_with_udid_simulator(udid, path=path):
      # Try to delete once to avoid duplicate in case of race condition.
      delete_simulator_by_udid(udid, path=path)
      udid = (
        subprocess.check_output(
          _compose_simctl_cmd(['create', name, device_type, runtime], path)
        )
        .decode('utf-8')
        .rstrip()
      )
      LOGGER.info('Created simulator in second attempt with UDID: %s', udid)
    disable_simulator_daemons(udid, simset_path=path, check_booted=False)
    return udid
  except subprocess.CalledProcessError as e:
    LOGGER.error(f'Error when creating simulator "{name}": {e.output}')
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
    LOGGER.info(
      "Simulator caching not enabled. Creating disposable simulator "
      "in the default set."
    )
    enabled_datapoint.record(False)
    return _create_device_by_platform_and_version(platform, version)

  enabled_datapoint.record(True)

  # Ensure Cache Path Exists
  os.makedirs(SIMULATOR_CACHE_PATH, exist_ok=True)

  LOGGER.info(
    f"Simulator caching is enabled. "
    f"Checking if a {version} {platform} simulator is in cache"
  )
  cache_udids = get_simulator_udids_by_platform_and_version(
    platform, version, path=SIMULATOR_CACHE_PATH
  )

  cache_hit_datapoint = measures.data_points('simulator_cache_hit')
  if cache_udids:
    LOGGER.info("Simulator found in cache. Cloning into default set")
    cache_hit_datapoint.record(True)
    with measures.time_consumption('Simulator clone', 'cache to working set'):
      udid = clone_simulator_by_udid(
        cache_udids[0],
        _compose_simulator_name(platform, version),
        path=SIMULATOR_CACHE_PATH,
        dest_path=SIMULATOR_DEFAULT_PATH,
      )
    return udid

  cache_hit_datapoint.record(False)
  LOGGER.info("Simulator not found in cache, attempting to create")

  max_attempts = 2
  for attempt in range(max_attempts):
    udid = _create_device_by_platform_and_version(
      platform, version, path=SIMULATOR_CACHE_PATH
    )

    # Run first boot of the simulator to ensure that costly data migrations
    # have completed, then shutdown simulator so it can be cloned for future
    # use.

    with measures.time_consumption(
      'Simulator full boot',
      'iossim_util',
      'Pre launch for cache creation',
      f'attempt {attempt}',
    ):
      booted = ensure_simulator_fully_booted(udid, path=SIMULATOR_CACHE_PATH)
    if booted:
      shutdown_simulator_by_udid(udid, path=SIMULATOR_CACHE_PATH)
      LOGGER.info(
        f"Attempt {attempt} to create simulator and boot it succeeded. "
        f"Cloning simulator into the default set."
      )
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
      f"Creating a disposable simulator in default set."
    )

    return _create_device_by_platform_and_version(platform, version)


def clone_simulator_by_udid(
  udid: str, name: str, path: str = None, dest_path: str = None
):
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

  cloned_udid = subprocess.check_output(command).decode('utf-8').strip()
  disable_simulator_daemons(
    cloned_udid, simset_path=dest_path or path, check_booted=False
  )
  return cloned_udid


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
            _compose_simctl_cmd(['shutdown', device['udid']], path)
          )
        break
      except subprocess.CalledProcessError as ex:
        LOGGER.error('Shutdown failed %s ', ex)


def uninstall_app(udid: str, bundle_id: str, path: str | None = None) -> None:
  """Uninstalls an app from a simulator by UDID and bundle ID.

  Failures are logged as warnings and do not interrupt test execution.

  Args:
    udid: UDID of the simulator.
    bundle_id: Bundle identifier of the app to uninstall.
    path: Optional path to the simulator directory for `xcrun simctl --set`.
  """
  LOGGER.info('Uninstalling %s from simulator %s', bundle_id, udid)
  try:
    subprocess.check_output(
      _compose_simctl_cmd(['uninstall', udid, bundle_id], path=path),
      stderr=subprocess.STDOUT,
    )
  except subprocess.CalledProcessError as e:
    output = (
      e.output.decode('utf-8', errors='replace')
      if isinstance(e.output, bytes)
      else e.output
    )
    if isinstance(output, str):
      output = output.strip()
    LOGGER.warning(
      'Failed to uninstall %s from simulator %s: %s', bundle_id, udid, output
    )


def delete_simulator_by_udid(udid, path: str = None):
  """Deletes simulator by its udid.

  Args:
    udid: (str) UDID of simulator.
  """
  LOGGER.info('Deleting simulator %s', udid)
  try:
    subprocess.check_output(
      _compose_simctl_cmd(['delete', udid], path=path), stderr=subprocess.STDOUT
    ).decode('utf-8')
    is_device_with_udid_simulator.cache_clear()
    cleanup_simulator_disabled_store(udid, simset_path=path)
  except subprocess.CalledProcessError as e:
    # Logging error instead of throwing so we don't cause failures in case
    # this was indeed failing to clean up.
    message = f'Failed to delete simulator {udid} with error {e.output}'
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
  return (
    subprocess.check_output(
      ['xcrun', 'simctl', 'getenv', get_simulator(platform, version), 'HOME']
    )
    .decode('utf-8')
    .rstrip()
  )


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
        LOGGER.debug(
          'Simulator %s is already Booted; skipping disabled.plist update.',
          sim_udid,
        )
        return
      disable_simulator_daemons(sim_udid, simset_path=path, check_booted=False)
      subprocess.check_output(
        ['xcrun', 'simctl', '--set', path, 'boot', sim_udid]
      ).decode('utf-8')
      return
  raise test_runner.SimulatorNotFoundError(
    f'Not found simulator with "{sim_udid}" UDID in devices {simulator_list["devices"]}'
  )


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
      f"Not found simulator with UDID: {sim_udid}"
    )

    # Ensure data migrations are run
  cmd = _compose_simctl_cmd(
    [
      'bootstatus',
      sim_udid,
      '-b',
    ],
    path,
  )
  runtime = get_simulator_runtime_by_device_udid(sim_udid, path=path)
  for boot_attempt in range(num_attempts):
    try:
      disable_simulator_daemons(sim_udid, simset_path=path)
      update_dyld_shared_cache(runtime)
      subprocess.check_call(cmd, timeout=120)
      return True
    except subprocess.TimeoutExpired:
      msg = "Manually booting simulator timed out after 120 seconds."
      LOGGER.info(msg)
      msg_again = " again" if boot_attempt > 0 else ""
      msg_action = (
        "continuing" if boot_attempt == num_attempts - 1 else "retrying"
      )
      LOGGER.info(
        f"Failed to manually boot simulator{msg_again}. "
        f"Wiping simulator and {msg_action}."
      )
      wipe_simulator_by_udid(sim_udid, path)
      test_runner.SimulatorTestRunner.kill_simulators()

  return False


def get_app_data_directory(app_bundle_id, sim_udid):
  """Returns app data directory for a given app on a given simulator.

  Args:
    app_bundle_id: (str) Bundle id of application.
    sim_udid: (str) UDID of the simulator.
  """
  return (
    subprocess.check_output(
      ['xcrun', 'simctl', 'get_app_container', sim_udid, app_bundle_id, 'data']
    )
    .decode('utf-8')
    .rstrip()
  )


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
  disable_simulator_daemons(udid)
  # Try to boot first, if the simulator is already booted, continue.
  try:
    subprocess.check_call(['xcrun', 'simctl', 'boot', udid])
  except subprocess.CalledProcessError as e:
    if 'booted' not in str(e):
      # Logging error instead of throwing, so we don't cause failures in case
      # this was indeed failing to copy the cert.
      message = (
        f'Failed to boot simulator before installing cert. Error: {e.output}'
      )
      LOGGER.error(message)
      return

  try:
    subprocess.check_call(
      ['xcrun', 'simctl', 'keychain', udid, 'add-root-cert', cert_path]
    )
    subprocess.check_call(['xcrun', 'simctl', 'shutdown', udid])
  except subprocess.CalledProcessError as e:
    message = f'Failed to install cert. Error: {e.output}'
    LOGGER.error(message)


def get_simulator_runtime_list():
  """Gets list of available simulator runtimes as a dictionary."""
  return json.loads(
    subprocess.check_output(
      ['xcrun', 'simctl', 'runtime', 'list', '-j']
    ).decode('utf-8')
  )


def get_simulator_runtime_match_list():
  """Gets list of chosen simulator runtime for each simulator sdk type"""
  return json.loads(
    subprocess.check_output(
      ['xcrun', 'simctl', 'runtime', 'match', 'list', '-j']
    ).decode('utf-8')
  )


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


def get_simulator_runtime_info(
  platform_type: constants.IOSPlatformType, platform_version: str
):
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
    raise ValueError(f'Invalid platform_type value: {platform_type}')

  version_parts = platform_version.split('-', 1)
  base_version = version_parts[0]
  variant_suffix = version_parts[1].lower() if len(version_parts) > 1 else None

  runtimes = get_simulator_runtime_list()
  for runtime in runtimes.values():
    # The output might use version with a patch number (e.g. 17.0.1)
    # but the passed in version does not have a patch number (e.g. 17.0)
    # Therefore, we should use startswith for substring match.
    version = runtime.get('version')
    if (
      not version
      or not version.startswith(base_version)
      or runtime.get('platformIdentifier') != platform_identifier
    ):
      continue
    runtime_id = runtime.get('runtimeIdentifier', '')
    if variant_suffix:
      if not runtime_id.lower().endswith(f'-{variant_suffix}'):
        continue
    elif runtime_id:
      # Exclude device-specific runtime variants (e.g.
      # com.apple.CoreSimulator.SimRuntime.iOS-27-2-iPhone19-4) when querying
      # the standard OS version.
      if not all(part.isdigit() for part in runtime_id.split('-')[1:]):
        continue
    return runtime
  return None


def is_simulator_runtime_builtin(runtime):
  if runtime is None or runtime['kind'] not in IOS_SIM_RUNTIME_BUILTIN_STATE:
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
      'Unable to find the simulator runtime build number to override with...'
    )
    return

  # find the runtime build number to be overridden
  sdks = get_simulator_runtime_match_list()
  iphone_sdk_key = 'iphoneos' + ios_version
  sdk_build = sdks.get(iphone_sdk_key, {}).get("sdkBuild")
  if sdk_build is None:
    LOGGER.debug(
      'Unable to find the simulator runtime build number to be overridden...'
    )
    return
  cmd = [
    'xcrun',
    'simctl',
    'runtime',
    'match',
    'set',
    iphone_sdk_key,
    overriding_build,
    '--sdkBuild',
    sdk_build,
  ]
  LOGGER.debug(f'Overriding default runtime with command {cmd}')
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
    if b'Cannot stage disk image or bundle for delete' in e.output:
      LOGGER.warning(
        'Error when deleting runtime %s. It may have been already deleted. '
        'Error: %s',
        runtime_id,
        e.output.decode('utf-8', 'ignore'),
      )
      return
    else:
      raise

  if should_wait:
    # runtime takes a few seconds to delete
    time_waited = 0
    runtime_to_delete = get_simulator_runtime_info_by_id(runtime_id)
    while runtime_to_delete is not None:
      LOGGER.debug(
        f'Waiting for runtime to be deleted. Current state is {runtime_to_delete["state"]}'
      )
      time.sleep(1)
      time_waited += 1
      if time_waited > MAX_WAIT_TIME_TO_DELETE_RUNTIME:
        raise test_runner_errors.SimRuntimeDeleteTimeoutError(runtime_id)
      runtime_to_delete = get_simulator_runtime_info_by_id(runtime_id)
    LOGGER.debug('Runtime successfully deleted!')


def delete_least_recently_used_simulator_runtimes(
  max_to_keep=constants.MAX_RUNTIME_KEPT_COUNT,
):
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
    runtimes.values(), key=lambda x: x.get("lastUsedAt", ""), reverse=True
  )
  sorted_runtimes = OrderedDict(
    (item["identifier"], item) for item in sorted_runtime_values
  )

  keep_count = 0
  for runtime_id, value in sorted_runtimes.items():
    if is_simulator_runtime_builtin(value):
      LOGGER.debug(
        f'Built-in Runtime {runtime_id} with iOS {value["version"]} should not be deleted'
      )
      continue
    if keep_count < max_to_keep:
      LOGGER.debug(
        'Runtime %s should be kept. Current runtime count %s', value, keep_count
      )
      keep_count += 1
    else:
      LOGGER.debug(
        'Runtime %s should be deleted due to exceeding max runtime count %s',
        value,
        max_to_keep,
      )
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
      _compose_simctl_cmd(['delete', 'unavailable'], path=SIMULATOR_CACHE_PATH)
    )
    is_device_with_udid_simulator.cache_clear()


def shutdown_all_simulators(path=None):
  """Shutdown all simulator devices.

  Args:
    path: (str) A path to the directory containing the simulators.
  """
  try:
    subprocess.check_call(_compose_simctl_cmd(['shutdown', 'all'], path))
  except subprocess.CalledProcessError as e:
    LOGGER.error(f'Failed to shutdown all simulators. Error: {e.output}')


def delete_all_simulators(path=None):
  """Deletes all simulator devices.

  Args:
    path: (str) A path to the directory containing the simulators.
  """
  try:
    subprocess.check_call(_compose_simctl_cmd(['delete', 'all'], path))
  except subprocess.CalledProcessError as e:
    LOGGER.error(f'Failed to delete all simulators. Error: {e.output}')
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
    LOGGER.error(f'Failed to erase all simulators. Error: {e.output}')


def disable_hardware_keyboard(udid: str) -> None:
  """Disables hardware keyboard input for the given simulator.

  Exceptions are caught and logged but do not interrupt program flow. The result
  is that if the util is unable to change the HW keyboard pref for any reason
  the test will still run without changing the preference.

  Args:
    udid: (str) UDID of the simulator to disable hw keyboard for.
  """
  path = os.path.expanduser(
    '~/Library/Preferences/com.apple.iphonesimulator.plist'
  )
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
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'DidShowContinuousPathIntroduction',
        '1',
      ]
    )
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'KeyboardDidShowProductivityTutorial',
        '1',
      ]
    )
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'DidShowGestureKeyboardIntroduction',
        '1',
      ]
    )
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'UIKeyboardDidShowInternationalInfoIntroduction',
        '1',
      ]
    )
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'KeyboardAutocorrection',
        '0',
      ]
    )
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'KeyboardPrediction',
        '0',
      ]
    )
    subprocess.check_call(
      [
        'xcrun',
        'simctl',
        'spawn',
        udid,
        'defaults',
        'write',
        'com.apple.keyboard.preferences',
        'KeyboardShowPredictionBar',
        '0',
      ]
    )
  except subprocess.CalledProcessError as e:
    message = f'Unable to disable keyboard tutorial: {e.stderr}'
    LOGGER.error(message)


def _is_valid_uuid(value: str) -> bool:
  """Returns whether `value` is a canonical hyphenated UUID string."""
  if not isinstance(value, str):
    return False
  try:
    return str(uuid.UUID(value)) == value.lower()
  except ValueError:
    return False


def _is_valid_uuid_dir(name: str, full_path: str) -> bool:
  """Returns whether `full_path` is a directory named by a valid UUID."""
  return os.path.isdir(full_path) and _is_valid_uuid(name)


def set_simulator_daemon_disabling_override(enabled: bool | None) -> None:
  """Sets an in-process override for simulator daemon disabling."""
  global _DISABLE_SIMULATOR_DAEMONS_OVERRIDE
  _DISABLE_SIMULATOR_DAEMONS_OVERRIDE = enabled


def _is_simulator_daemon_disabling_enabled() -> bool:
  """Returns whether background daemon disabling is enabled for this run."""
  if _DISABLE_SIMULATOR_DAEMONS_OVERRIDE is not None:
    return _DISABLE_SIMULATOR_DAEMONS_OVERRIDE
  env_val = (
    os.environ.get(IOS_DISABLE_SIMULATOR_DAEMONS_ENV, '').strip().lower()
  )
  if env_val in ('1', 'true', 'yes'):
    return True
  if env_val in ('0', 'false', 'no'):
    return False
  return DEFAULT_DISABLE_SIMULATOR_DAEMONS


def _is_simulator_booted(udid: str, simset_path: str | None = None) -> bool:
  """Returns whether the given simulator UDID is currently Booted."""
  if simset_path:
    device_plist = os.path.join(simset_path, udid, 'device.plist')
    try:
      if os.path.exists(device_plist):
        with open(device_plist, 'rb') as f:
          device_info = plistlib.load(f)
        if (
          isinstance(device_info, dict)
          and device_info.get('state') == SIMULATOR_PLIST_STATE_BOOTED
        ):
          return True
    except (
      OSError,
      plistlib.InvalidFileException,
      xml.parsers.expat.ExpatError,
    ):
      LOGGER.debug(
        'Failed to read device.plist for simulator %s.', udid, exc_info=True
      )

  if simset_path and not os.path.isdir(simset_path):
    return False

  try:
    raw = subprocess.check_output(
      _compose_simctl_cmd(
        ['list', 'devices', 'booted', '-j'], path=simset_path
      ),
      stderr=subprocess.DEVNULL,
    ).decode('utf-8')
    devices = json.loads(raw).get('devices', {})
    for runtime_devices in devices.values():
      for device in runtime_devices:
        if device.get('udid') == udid and device.get('state') == 'Booted':
          return True
  except (OSError, subprocess.SubprocessError, ValueError):
    LOGGER.debug(
      'Failed to query simctl booted state for simulator %s.',
      udid,
      exc_info=True,
    )
  return False


def cleanup_simulator_disabled_store(
  udid: str,
  store_root: str | None = None,
  simset_path: str | None = None,
) -> bool:
  """Removes an orphaned per-device host `disabled.plist` store directory.

  Skips removal if the target simulator is still `Booted` because `launchd_sim`
  holds live `syslogsock` and `launchd.log` handles inside that directory.

  Args:
    udid: Simulator UDID string.
    store_root: Optional root directory for per-device `disabled.plist` stores.
      Defaults to `SIMULATOR_DISABLED_STORE_ROOT` on macOS; no-ops elsewhere
      when `None`.
    simset_path: Optional CoreSimulator `--set` path to check for booted state.

  Returns:
    True if the store directory existed and was removed, False otherwise.
  """
  if not udid or not _is_valid_uuid(udid):
    return False

  if store_root is None:
    if sys.platform != 'darwin':
      return False
    store_root = SIMULATOR_DISABLED_STORE_ROOT

  store_dir = os.path.join(
    store_root, f'com.apple.CoreSimulator.SimDevice.{udid}'
  )
  if not os.path.exists(store_dir):
    return False

  if _is_simulator_booted(udid, simset_path=simset_path):
    LOGGER.debug(
      'Skipping cleanup of %s because simulator %s is still Booted.',
      store_dir,
      udid,
    )
    return False

  try:
    shutil.rmtree(store_dir)
    LOGGER.debug('Removed simulator disabled store %s.', store_dir)
    return True
  except OSError:
    LOGGER.warning(
      'Failed to remove simulator disabled store %s.', store_dir, exc_info=True
    )
    return False


def _write_plist_atomically(
  store_dir: str, store_path: str, payload: dict[str, object]
) -> None:
  """Writes `payload` as a world-readable XML plist to `store_path`."""
  os.makedirs(store_dir, exist_ok=True)
  temp_path = None
  try:
    with tempfile.NamedTemporaryFile(
      mode='wb',
      dir=store_dir,
      prefix='.disabled.',
      suffix='.plist.tmp',
      delete=False,
    ) as temp_file:
      temp_path = temp_file.name
      plistlib.dump(payload, temp_file, fmt=plistlib.FMT_XML, sort_keys=True)
    os.chmod(temp_path, 0o644)
    os.replace(temp_path, store_path)
    temp_path = None
  finally:
    if temp_path and os.path.exists(temp_path):
      os.unlink(temp_path)


def _configure_simulator_daemons(
  udid: str,
  store_root: str | None = None,
  simset_path: str | None = None,
  check_booted: bool = True,
) -> tuple[bool, bool, bool]:
  """Applies or strips `disabled.plist` daemon overrides for one simulator.

  Returns:
    A `(modified, skipped_booted, had_error)` tuple where `modified` is True
    only when `DISABLED_SIMULATOR_LAUNCHD_JOBS` entries were written.
  """
  if not udid or not _is_valid_uuid(udid):
    return (False, False, False)

  if store_root is None:
    if sys.platform != 'darwin':
      return (False, False, False)
    store_root = SIMULATOR_DISABLED_STORE_ROOT

  store_dir = os.path.join(
    store_root, f'com.apple.CoreSimulator.SimDevice.{udid}'
  )
  store_path = os.path.join(store_dir, 'disabled.plist')

  if not _is_simulator_daemon_disabling_enabled():
    if not os.path.exists(store_path):
      return (False, False, False)
    if check_booted and _is_simulator_booted(udid, simset_path=simset_path):
      LOGGER.debug(
        'Simulator %s is Booted; skipping disabled.plist cleanup.',
        udid,
      )
      return (False, True, False)
    try:
      with open(store_path, 'rb') as f:
        loaded = plistlib.load(f)
      if not isinstance(loaded, dict):
        return (False, False, False)
      managed_jobs = frozenset(DISABLED_SIMULATOR_LAUNCHD_JOBS)
      remaining = {
        job: value
        for job, value in loaded.items()
        if not (job in managed_jobs and value is True)
      }
      removed_count = len(loaded) - len(remaining)
      if removed_count == 0:
        return (False, False, False)
      if remaining:
        _write_plist_atomically(store_dir, store_path, remaining)
      else:
        os.unlink(store_path)
        try:
          os.rmdir(store_dir)
        except OSError:
          pass
      LOGGER.info(
        'Removed %d managed disabled launchd jobs for simulator %s '
        '(daemon disabling not enabled).',
        removed_count,
        udid,
      )
      return (False, False, False)
    except Exception:
      LOGGER.exception(
        'Failed to strip managed simulator daemons for %s.', udid
      )
      return (False, False, True)

  if check_booted and _is_simulator_booted(udid, simset_path=simset_path):
    LOGGER.debug(
      'Simulator %s is already Booted; skipping disabled.plist update.',
      udid,
    )
    return (False, True, False)

  try:
    existing: dict[str, object] = {}
    if os.path.exists(store_path):
      with open(store_path, 'rb') as f:
        loaded = plistlib.load(f)
      if isinstance(loaded, dict):
        existing = loaded

    if all(
      existing.get(job) is True for job in DISABLED_SIMULATOR_LAUNCHD_JOBS
    ):
      return (False, False, False)

    updated = dict(existing)
    for job in DISABLED_SIMULATOR_LAUNCHD_JOBS:
      updated[job] = True

    _write_plist_atomically(store_dir, store_path, updated)
    LOGGER.info(
      'Configured %d disabled launchd jobs for simulator %s.',
      len(DISABLED_SIMULATOR_LAUNCHD_JOBS),
      udid,
    )
    return (True, False, False)
  except Exception:
    LOGGER.exception('Failed to disable simulator daemons for %s.', udid)
    return (False, False, True)


def disable_simulator_daemons(
  udid: str,
  store_root: str | None = None,
  simset_path: str | None = None,
  check_booted: bool = True,
) -> bool:
  """Disables unneeded background launchd jobs for the given simulator.

  Writes `DISABLED_SIMULATOR_LAUNCHD_JOBS` into CoreSimulator's per-device
  host `disabled.plist` store before `launchd_sim` reads it on boot when
  daemon disabling is enabled (default; opt out via
  `--no-disable-simulator-daemons` or `IOS_DISABLE_SIMULATOR_DAEMONS=0`).
  Existing entries (including explicit `False` entries written by the runtime)
  are preserved. When daemon disabling is disabled, strips any pre-existing
  managed `True` entries instead and returns `False`. Exceptions are caught
  and logged so infra or filesystem failures never abort test execution.

  Args:
    udid: Simulator UDID string.
    store_root: Optional root directory for per-device `disabled.plist` files.
      Defaults to `SIMULATOR_DISABLED_STORE_ROOT` on macOS; no-ops on other
      platforms when `None`.
    simset_path: Optional CoreSimulator `--set` path used when checking whether
      `udid` is already `Booted`.
    check_booted: Whether to skip modifying `disabled.plist` if the simulator is
      already `Booted`.

  Returns:
    True if `disabled.plist` was written/updated to disable daemons, False if
    skipped, disabled, already up to date, or an error occurred.
  """
  modified, _, _ = _configure_simulator_daemons(
    udid,
    store_root=store_root,
    simset_path=simset_path,
    check_booted=check_booted,
  )
  return modified


@contextlib.contextmanager
def watch_simulator_clones_for_daemon_disabling(
  device_set_path: str,
  store_root: str | None = None,
  poll_interval: float = 0.05,
):
  """Watches a simulator set directory and disables daemons on new clones.

  CoreSimulator cloning (`xcodebuild` parallel workers under `XCTestDevices`)
  copies device data directories without copying host-side `disabled.plist`
  stores. Monitoring `device_set_path` lets new clones receive `disabled.plist`
  before `launchd_sim` boots them.

  Args:
    device_set_path: Directory containing simulator device subdirectories.
    store_root: Optional root directory for `disabled.plist` stores. If `None`,
      defaults to `SIMULATOR_DISABLED_STORE_ROOT` on macOS and no-ops on other
      platforms.
    poll_interval: Fallback sleep interval in seconds when `select.kqueue` is
      unavailable.
  """
  if (store_root is None and sys.platform != 'darwin') or not device_set_path:
    yield
    return

  if not _is_simulator_daemon_disabling_enabled():
    # Reused Shutdown clones in XCTestDevices may still carry disabled.plist
    # entries from an earlier opted-in run, so run one cleanup pass without
    # spawning a watcher thread.
    if os.path.isdir(device_set_path):
      try:
        for entry in os.listdir(device_set_path):
          entry_path = os.path.join(device_set_path, entry)
          if _is_valid_uuid_dir(entry, entry_path):
            _configure_simulator_daemons(
              entry,
              store_root=store_root,
              simset_path=device_set_path,
              check_booted=True,
            )
      except OSError:
        LOGGER.debug(
          'Failed to scan %s for disabled.plist cleanup.',
          device_set_path,
          exc_info=True,
        )
    yield
    return

  try:
    os.makedirs(device_set_path, exist_ok=True)
  except OSError:
    LOGGER.exception(
      'Failed to create simulator clone watch directory %s.', device_set_path
    )
    yield
    return
  stop_event = threading.Event()
  ready_event = threading.Event()
  seen_lock = threading.Lock()
  seen_udids: set[str] = set()
  ignored_booted_udids: set[str] = set()

  def _scan_once(initial_scan: bool = False) -> None:
    try:
      entries = os.listdir(device_set_path)
    except OSError:
      return
    for entry in entries:
      with seen_lock:
        already_handled = entry in seen_udids or entry in ignored_booted_udids
      if already_handled:
        continue
      entry_path = os.path.join(device_set_path, entry)
      if not _is_valid_uuid_dir(entry, entry_path):
        continue
      _, skipped_booted, had_error = _configure_simulator_daemons(
        entry,
        store_root=store_root,
        simset_path=device_set_path,
        check_booted=initial_scan,
      )
      if had_error:
        continue
      with seen_lock:
        if skipped_booted:
          ignored_booted_udids.add(entry)
        else:
          seen_udids.add(entry)

  def _watch_loop() -> None:
    kq = None
    dir_fd = None
    try:
      if hasattr(select, 'kqueue'):
        try:
          kq = select.kqueue()
          open_flags = getattr(os, 'O_EVTONLY', os.O_RDONLY)
          dir_fd = os.open(device_set_path, open_flags)
          kevent = select.kevent(
            dir_fd,
            filter=select.KQ_FILTER_VNODE,
            flags=select.KQ_EV_ADD | select.KQ_EV_CLEAR,
            fflags=select.KQ_NOTE_WRITE,
          )
          kq.control([kevent], 0, 0)
        except OSError:
          if dir_fd is not None:
            try:
              os.close(dir_fd)
            except OSError:
              pass
            dir_fd = None
          if kq is not None:
            try:
              kq.close()
            except OSError:
              pass
            kq = None

      # Configure pre-existing Shutdown clones right away because xcodebuild
      # reuses XCTestDevices worker clones across retry attempts, while
      # skipping pre-existing Booted clones so we neither modify nor clean up
      # their live stores.
      _scan_once(initial_scan=True)
      ready_event.set()

      while not stop_event.is_set():
        if kq is not None:
          try:
            kq.control(None, 1, 0.25)
          except OSError:
            LOGGER.warning(
              'kqueue watch failed on %s; falling back to polling.',
              device_set_path,
              exc_info=True,
            )
            try:
              kq.close()
            except OSError:
              pass
            kq = None
            if dir_fd is not None:
              try:
                os.close(dir_fd)
              except OSError:
                pass
              dir_fd = None
            _scan_once()
            continue
          if stop_event.is_set():
            break
          _scan_once()
        else:
          if stop_event.wait(poll_interval):
            break
          _scan_once()
    finally:
      ready_event.set()
      if dir_fd is not None:
        try:
          os.close(dir_fd)
        except OSError:
          pass
      if kq is not None:
        try:
          kq.close()
        except OSError:
          pass

  watcher = threading.Thread(target=_watch_loop, daemon=True)
  watcher.start()
  ready_event.wait(timeout=1.0)
  try:
    yield
  finally:
    stop_event.set()
    watcher.join(timeout=2.0)
    if watcher.is_alive():
      LOGGER.warning(
        'Simulator clone watcher for %s did not stop within timeout; '
        'skipping disabled store cleanup.',
        device_set_path,
      )
    else:
      with seen_lock:
        clones_to_clean = sorted(seen_udids)
      for clone_udid in clones_to_clean:
        cleanup_simulator_disabled_store(
          clone_udid, store_root=store_root, simset_path=device_set_path
        )
