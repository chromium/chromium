# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.version
import glob
import logging
import os
import re
import shutil
import subprocess
import sys
import time
import traceback

import constants
import iossim_util
import mac_util
import test_runner
import test_runner_errors

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.extend([
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto')),
    os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/'))
])
import measures

LOGGER = logging.getLogger(__name__)
XcodeIOSSimulatorRuntimeRelPath = ('Contents/Developer/Platforms/'
                                   'iPhoneOS.platform/Library/Developer/'
                                   'CoreSimulator/Profiles/Runtimes')
XcodeCipdFiles = ['.cipd', '.xcode_versions']
XcodeSimulatorRuntimeBuildTagRegx = r'(?:ios|tvos)_runtime_build:(.*)'
XcodeSimulatorRuntimeVersionTagRegx = r'(?:ios|tvos)_runtime_version:(.*)'
XcodeIOSSimulatorRuntimeDMGCipdPath = 'infra_internal/ios/xcode/ios_runtime_dmg'
XcodeTVOSSimulatorRuntimeDMGCipdPath = 'infra_internal/ios/xcode/tvos_runtime_dmg'

DMG_ADD_MAX_RETRIES = 2
DMG_ADD_RETRY_DELAY = 5  # seconds


def describe_cipd_ref(pkg_path, ref):
  cmd = ['cipd', 'describe', pkg_path, '-version', ref]
  output = ''
  try:
    output = subprocess.check_output(
        cmd, stderr=subprocess.STDOUT).decode('utf-8')
  except subprocess.CalledProcessError:
    LOGGER.debug('cipd describe cmd %s returned nothing' % cmd)
  return output


def convert_platform_version_to_cipd_ref(
    platform_type: constants.IOSPlatformType, platform_version: str):
  """Transforms an iOS/tvOS version to the mac_toolchain runtime version format.

  For example, "ios" and "14.4" become "ios-14-4".
  """
  if platform_type == constants.IOSPlatformType.IPHONEOS:
    prefix = 'ios'
  else:
    prefix = 'tvos'
  return '%s-%s' % (prefix, platform_version.replace('.', '-'))


def _is_legacy_xcode_package(xcode_app_path):
  """Checks and returns if the installed Xcode package is legacy version.

  Legacy Xcode package are uploaded with legacy version of mac_toolchain.
  Typically, multiple iOS runtimes are bundled into legacy Xcode packages. No
  runtime is bundled into new format Xcode packages.

  Args:
    xcode_app_path: (string) Path to install the contents of Xcode.app.

  Returns:
    (bool) True if the package is legacy(with runtime bundled). False otherwise.
  """
  # More than one iOS runtimes indicate the downloaded Xcode is a legacy one.
  # If no runtimes are found in the package, it's a new format package. If only
  # one runtime is found in package, it typically means it's an incorrectly
  # cached new format Xcode package. (The single runtime wasn't moved out from
  # Xcode in the end of last task, because last task was killed before moving.)
  runtimes_in_xcode = glob.glob(
      os.path.join(xcode_app_path, XcodeIOSSimulatorRuntimeRelPath,
                   '*.simruntime'))

  is_legacy = len(runtimes_in_xcode) >= 2
  if not is_legacy:
    for runtime in runtimes_in_xcode:
      LOGGER.warning('Removing %s from incorrectly cached Xcode.', runtime)
      shutil.rmtree(runtime)
  return is_legacy


def _install_runtime(mac_toolchain, install_path, xcode_build_version,
                     ios_version):
  """Invokes mac_toolchain to install the runtime.

  mac_toolchain will resolve & find the best suitable runtime and install to the
  path, with Xcode and ios version as input.

  This function is only expected to run on iOS, as tvOS runtimes are only
  installed via install_runtime_dmg().

  Args:
    install_path: (string) Path to install the runtime package into.
    xcode_build_version: (string) Xcode build version, e.g. 12d4e.
    ios_version: (string) Runtime version (number only), e.g. 13.4.
  """

  existing_runtimes = glob.glob(os.path.join(install_path, '*.simruntime'))
  # When no runtime file exists, remove any remaining .cipd or .xcode_versions
  # status folders, so mac_toolchain(underlying CIPD) will work to download a
  # new one.
  if len(existing_runtimes) == 0:
    for dir_name in XcodeCipdFiles:
      dir_path = os.path.join(install_path, dir_name)
      if os.path.exists(dir_path):
        LOGGER.warning('Removing %s in runtime cache folder.', dir_path)
        shutil.rmtree(dir_path)

  runtime_version = convert_platform_version_to_cipd_ref(
      constants.IOSPlatformType.IPHONEOS, ios_version)

  cmd = [
      mac_toolchain,
      'install-runtime',
      '-xcode-version',
      xcode_build_version.lower(),
      '-runtime-version',
      runtime_version,
      '-output-dir',
      install_path,
  ]

  LOGGER.debug('Installing runtime with command: %s' % cmd)
  output = subprocess.check_call(cmd, stderr=subprocess.STDOUT)
  return output


def construct_runtime_cache_folder(runtime_cache_prefix, platform_version):
  """Composes runtime cache folder from it's prefix and platform_version.

  Note: Please keep the pattern consistent between what's being passed into
  runner script in gn(build/config/ios/ios_test_runner_wrapper.gni), and what's
  being configured for swarming cache in test configs (testing/buildbot/*).
  """
  return runtime_cache_prefix + platform_version


def move_runtime(runtime_cache_folder, xcode_app_path):
  """Moves runtime from runtime cache into xcode or vice versa.

  The function is intended to only work with new Xcode packages.

  The function assumes that there's exactly one *.simruntime file in the source
  folder. It also removes existing runtimes in the destination folder. The above
  assumption & handling can ensure no incorrect Xcode package is cached from
  corner cases.

  Note: this function is iOS-specific; tvOS runtimes are always installed via
  install_runtime_dmg().

  Args:
    runtime_cache_folder: (string) Path to the runtime cache directory.
    xcode_app_path: (string) Path to install the contents of Xcode.app.

  Raises:
    IOSRuntimeHandlingError for issues moving runtime around.
    shutil.Error for exceptions from shutil when moving files around.
  """
  xcode_runtime_folder = os.path.join(xcode_app_path,
                                      XcodeIOSSimulatorRuntimeRelPath)
  src_folder = runtime_cache_folder
  dst_folder = xcode_runtime_folder

  runtimes_in_src = glob.glob(os.path.join(src_folder, '*.simruntime'))
  if len(runtimes_in_src) != 1:
    raise test_runner_errors.IOSRuntimeHandlingError(
        'Not exactly one runtime files (files: %s) to move from %s!' %
        (runtimes_in_src, src_folder))

  runtimes_in_dst = glob.glob(os.path.join(dst_folder, '*.simruntime'))
  for runtime in runtimes_in_dst:
    LOGGER.warning('Removing existing %s in destination folder.', runtime)
    shutil.rmtree(runtime)

  # Get the runtime package filename. It might not be the default name.
  runtime_name = os.path.basename(runtimes_in_src[0])
  dst_runtime = os.path.join(dst_folder, runtime_name)
  LOGGER.debug('Moving %s from %s to %s.' %
               (runtime_name, src_folder, dst_folder))
  shutil.move(os.path.join(src_folder, runtime_name), dst_runtime)
  return


def select(xcode_app_path):
  """Invokes sudo xcode-select -s {xcode_app_path}

  Raises:
    subprocess.CalledProcessError on exit codes non zero
  """
  cmd = [
      'sudo',
      'xcode-select',
      '-s',
      xcode_app_path,
  ]
  LOGGER.debug('Selecting Xcode, runFirstLaunch and "xcrun simctl list"')
  output = subprocess.check_output(
      cmd, stderr=subprocess.STDOUT).decode('utf-8')

  # After selecting xcode, ensure that the xcode is ready for launch
  run_first_launch_cmd = ['sudo', '/usr/bin/xcodebuild', '-runFirstLaunch']
  output += subprocess.check_output(
      run_first_launch_cmd, stderr=subprocess.STDOUT).decode('utf-8')

  # This is to avoid issues caused by mixed usage of different Xcode versions on
  # one machine.
  xcrun_simctl_cmd = ['xcrun', 'simctl', 'list']
  output += subprocess.check_output(
      xcrun_simctl_cmd, stderr=subprocess.STDOUT).decode('utf-8')

  return output


def _install_xcode(mac_toolchain, xcode_build_version, xcode_path):
  """Invokes mac_toolchain to install the given xcode version.

  Whether a runtime will be installed depends on the actual Xcode
  package in CIPD. e.g. An Xcode package uploaded with legacy mac_toolchain will
  include runtimes, even though it's installed with new mac_toolchain and
  "-with-runtime=False" switch.

  Args:
    xcode_build_version: (string) Xcode build version to install.
    mac_toolchain: (string) Path to mac_toolchain command to install Xcode
    See https://chromium.googlesource.com/infra/infra/+/main/go/src/infra/cmd/mac_toolchain/
    xcode_path: (string) Path to install the contents of Xcode.app.

  Raises:
    subprocess.CalledProcessError on exit codes non zero
  """

  # The -kind parameter's value may look misleading, but there is extra logic in
  # the mac_toolchain tool itself:
  #   * When running on Mac 13+, `mac_toolchain install` will install Xcode with
  #     the Mac runtime regardless of what is passed to -kind.
  #     In this case, both iOS and tvOS runtimes are installed separately via
  #     install_runtime_dmg() here (which invokes `mac_toolchain
  #     install-runtime-dmg`).
  #   * Otherwise, passing "-kind ios" here will either install Xcode with the
  #     iOS runtime, or Xcode without any runtimes, in which case the iOS
  #     runtime will be installed via _install_runtime(). This is irrelevant to
  #     tvOS runtimes because the tvOS bots are always running on Mac 13+.

  cmd = [
      mac_toolchain,
      'install',
      '-kind',
      'ios',
      '-xcode-version',
      xcode_build_version.lower(),
      '-output-dir',
      xcode_path,
      '-with-runtime=False',
  ]

  LOGGER.debug('Installing xcode with command: %s' % cmd)
  output = subprocess.check_call(cmd, stderr=subprocess.STDOUT)
  return output


def install(mac_toolchain, xcode_build_version, xcode_app_path, **runtime_args):
  """Installs the Xcode and returns if the installed one is a legacy package.

  Installs the Xcode of given version to path. Returns if the Xcode package
  of the version is a legacy package (with runtimes bundled in).

  Xcode package installation works as follows:
  * If installed Xcode is legacy one (with runtimes bundled), return.
  * If installed Xcode isn't legacy (without runtime bundled), install and copy
    the specified runtime version into Xcode.

  All MacOS13+ bots will install the whole legacy Xcode package due
  to the new codesign restrictions in crbug.com/1406204

  Args:
    xcode_build_version: (string) Xcode build version to install.
    mac_toolchain: (string) Path to mac_toolchain command to install Xcode
    See https://chromium.googlesource.com/infra/infra/+/main/go/src/infra/cmd/mac_toolchain/
    xcode_app_path: (string) Path to install the contents of Xcode.app.
    runtime_args: Keyword arguments related with runtime installation. Can be
    empty when installing an Xcode w/o runtime (for real device tasks). Namely:
      runtime_cache_folder: (string) Path to the folder where runtime package
          file (e.g. iOS.simruntime) is stored.
      ios_version: (string) iOS version requested to be in Xcode package.

  Raises:
    subprocess.CalledProcessError on exit codes non zero

  Returns:
    True, if the Xcode package in CIPD is legacy (bundled with runtimes).
    False, if the Xcode package in CIPD is new (not bundled with runtimes).
  """
  # (crbug/1406204): for MacOS13+, cipd files are automatically removed in
  # mac_toolchain prior to runFirstLaunch because they will cause codesign
  # check failures. If the cached Xcode still contains cipd files, it means
  # that something went wrong during the install process, and the Xcode should
  # be re-installed.
  if mac_util.is_macos_13_or_higher():
    LOGGER.debug('checking if the cached Xcode is corrupted...')
    for dir_name in XcodeCipdFiles:
      dir_path = os.path.join(xcode_app_path, dir_name)
      if os.path.exists(dir_path):
        LOGGER.debug('Xcode cache will be re-created because it contains %s' %
                     dir_path)
        shutil.rmtree(xcode_app_path)
        os.mkdir(xcode_app_path)
        break

  _install_xcode(mac_toolchain, xcode_build_version, xcode_app_path)

  # (crbug/1406204): for MacOS13+, we are using Xcode fat upload/download again,
  # so runtime should not be installed separately.
  is_legacy_xcode_package = mac_util.is_macos_13_or_higher(
  ) or _is_legacy_xcode_package(xcode_app_path)

  # Install & move the runtime to Xcode.
  # This is done only when working on a simulator (and therefore ios_version
  # is set).
  # This is iOS-specific; tvOS runtimes always require a legacy Xcode package
  # and are installed via install_runtime_dmg().
  if not is_legacy_xcode_package and runtime_args.get('ios_version'):
    runtime_cache_folder = runtime_args.get('runtime_cache_folder')
    ios_version = runtime_args.get('ios_version')
    if not runtime_cache_folder or not ios_version:
      raise test_runner_errors.IOSRuntimeHandlingError(
          'Insufficient runtime_args. runtime_cache_folder: %s, ios_version: %s'
          % (runtime_cache_folder, ios_version))

    # Try to install the runtime to it's cache folder. mac_toolchain will test
    # and install only when the runtime doesn't exist in cache.
    _install_runtime(mac_toolchain, runtime_cache_folder, xcode_build_version,
                     ios_version)
    move_runtime(runtime_cache_folder, xcode_app_path)

  return is_legacy_xcode_package


def _install_runtime_dmg(mac_toolchain, install_path,
                         platform_type: constants.IOSPlatformType,
                         platform_version, xcode_build_version):
  if platform_type == constants.IOSPlatformType.IPHONEOS:
    runtime_type = 'ios'
  else:
    runtime_type = 'tvos'

  runtime_version = convert_platform_version_to_cipd_ref(
      platform_type, platform_version)
  cmd = [
      mac_toolchain, 'install-runtime-dmg', '-runtime-version', runtime_version,
      '-runtime-type', runtime_type, '-xcode-version', xcode_build_version,
      '-output-dir', install_path
  ]

  LOGGER.debug('Installing runtime dmg with command: %s' % cmd)
  output = subprocess.check_call(cmd, stderr=subprocess.STDOUT)
  return output


def get_runtime_dmg_name(runtime_dmg_folder):
  runtime_dmg_name = glob.glob(os.path.join(runtime_dmg_folder, '*.dmg'))
  return runtime_dmg_name[0]


def get_latest_runtime_build_cipd(xcode_version,
                                  platform_type: constants.IOSPlatformType,
                                  platform_version: str):
  # Use Xcode version first to find the matching iOS/tvOS runtime,
  # if the runtime returned is not the desired version,
  # then use desired version to match as a fallback

  runtime_version = convert_platform_version_to_cipd_ref(
      platform_type, platform_version)
  output = describe_cipd_ref(XcodeIOSSimulatorRuntimeDMGCipdPath, xcode_version)
  runtime_build_match = re.search(XcodeSimulatorRuntimeBuildTagRegx, output,
                                  re.MULTILINE)
  runtime_version_match = re.search(XcodeSimulatorRuntimeVersionTagRegx, output,
                                    re.MULTILINE)
  if runtime_build_match and runtime_version_match:
    if runtime_version_match.group(1) == runtime_version:
      return runtime_build_match.group(1)

  if platform_type == constants.IOSPlatformType.IPHONEOS:
    runtime_dmg_cipd_path = XcodeIOSSimulatorRuntimeDMGCipdPath
  else:
    runtime_dmg_cipd_path = XcodeTVOSSimulatorRuntimeDMGCipdPath

  output = describe_cipd_ref(runtime_dmg_cipd_path, runtime_version)
  runtime_build_match = re.search(XcodeSimulatorRuntimeBuildTagRegx, output)
  if runtime_build_match:
    return runtime_build_match.group(1)
  return None


def is_runtime_builtin(platform_type: constants.IOSPlatformType,
                       platform_version: str):
  runtime = iossim_util.get_simulator_runtime_info(platform_type,
                                                   platform_version)
  return iossim_util.is_simulator_runtime_builtin(runtime)


def install_runtime_dmg(mac_toolchain, runtime_cache_folder,
                        platform_type: constants.IOSPlatformType,
                        platform_version: str, xcode_build_version):
  if is_runtime_builtin(platform_type, platform_version):
    LOGGER.debug(
        'Runtime is already built-in, no need to install from mac_toolchain')
    return

  runtime_build_to_install = get_latest_runtime_build_cipd(
      xcode_build_version, platform_type, platform_version)
  if runtime_build_to_install is None:
    raise test_runner_errors.RuntimeBuildNotFoundError(platform_version)

  # check if the desired runtime build already exists on disk
  runtime_info = iossim_util.get_simulator_runtime_info_by_build(
      runtime_build_to_install)
  if runtime_info is None:

    # clean up least used runtime first to free up disk space if possible.
    iossim_util.delete_least_recently_used_simulator_runtimes()
    iossim_util.delete_stale_simulator_runtimes()

    _install_runtime_dmg(mac_toolchain, runtime_cache_folder, platform_type,
                         platform_version, xcode_build_version)
    runtime_dmg_name = get_runtime_dmg_name(runtime_cache_folder)

    # crbug.com/370036129: sometimes the dmg add command fails with
    # exit status 5 for unknown reasons. Attempt to retry if it fails.
    attempt_count = measures.count('add_runtime_attempts')
    for attempt in range(DMG_ADD_MAX_RETRIES + 1):
      attempt_count.record()
      try:
        output = iossim_util.add_simulator_runtime(runtime_dmg_name)
        break
      except Exception as e:
        if attempt < DMG_ADD_MAX_RETRIES and e.returncode == 5:
          stderr_output = "Not available"
          stdout_output = "Not available"

          if isinstance(e, subprocess.CalledProcessError):
            if e.stderr:
              stderr_output = e.stderr.decode('utf-8', errors='replace')
            if e.output:
              stdout_output = e.output.decode('utf-8', errors='replace')

          logging.warning(
              f'Adding runtime failed (Attempt {attempt}).\n'
              f'Exit Code: {e.returncode}\n'
              f'STDERR: {stderr_output}\n'
              f'STDOUT: {stdout_output}',
              exc_info=True)

          # TODO(crbug.com/460133386): Sometimes iOS SDK could be in a bad state
          # which in term cause SDK to not show in the CLI output. In such case,
          # we should attempt to delete the original SDK and re-install.
          match = re.search(r"Duplicate of\s+([A-F0-9\-]+)", stdout_output)
          if match:
            duplicate_uuid = match.group(1)
            logging.warning(
                f"Conflict detected. Found duplicate runtime UUID: {duplicate_uuid}"
            )
            logging.info(
                f"Attempting to delete duplicate runtime: {duplicate_uuid}")
            iossim_util.delete_simulator_runtime(duplicate_uuid, True)
            iossim_util.delete_stale_simulator_runtimes()
          time.sleep(DMG_ADD_RETRY_DELAY)
        else:
          raise
    if platform_type == constants.IOSPlatformType.IPHONEOS:
      iossim_util.override_default_iphonesim_runtime(output, platform_version)
  else:
    LOGGER.debug(
        'Runtime %s already exists, no need to install from mac_toolchain',
        runtime_info)


def version():
  """Invokes xcodebuild -version

  Raises:
    subprocess.CalledProcessError on exit codes non zero

  Returns:
    version (12.0), build_version (12a6163b)
  """
  cmd = [
      'xcodebuild',
      '-version',
  ]
  LOGGER.debug('Checking Xcode version with command: %s' % cmd)

  output = subprocess.check_output(cmd).decode('utf-8')
  output = output.splitlines()
  # output sample:
  # Xcode 12.0
  # Build version 12A6159
  LOGGER.info(output)

  version = output[0].split(' ')[1]
  build_version = output[1].split(' ')[2].lower()

  return version, build_version


def using_xcode_11_or_higher():
  """Returns true if using Xcode version 11 or higher."""
  LOGGER.debug('Checking if Xcode version is 11 or higher')
  return distutils.version.LooseVersion(
      '11.0') <= distutils.version.LooseVersion(version()[0])


def using_xcode_13_or_higher():
  """Returns true if using Xcode version 13 or higher."""
  LOGGER.debug('Checking if Xcode version is 13 or higher')
  return distutils.version.LooseVersion(
      '13.0') <= distutils.version.LooseVersion(version()[0])


def using_xcode_15_or_higher():
  """Returns true if using Xcode version 15 or higher."""
  LOGGER.debug('Checking if Xcode version is 15 or higher')
  return distutils.version.LooseVersion(
      '15.0') <= distutils.version.LooseVersion(version()[0])


def using_xcode_16_or_higher():
  """Returns true if using Xcode version 16 or higher."""
  LOGGER.debug('Checking if Xcode version is 16 or higher')
  return distutils.version.LooseVersion(
      '16.0') <= distutils.version.LooseVersion(version()[0])


def is_local_run():
  """Use the existence of the LUCI_CONTEXT environment variable to determine
  whether we are running on a bot or running locally.

  Returns:
    (bool) True if running locally, false if on a bot."""
  return not os.environ.get('LUCI_CONTEXT')


def validate_local_xcode_install(xcode_build_version):
  """Confirm that the locally installed Xcode version matches the arguments
  passed to the test runner.

  Args:
    xcode_build_version: (str) Xcode version passed as an argument to the test
      runner, e.g. "16a242d"

  Raises:
    test_runner_errors.LocalRunXcodeError when the requested Xcode version is
      not installed locally
  """
  _, local_version = version()
  if xcode_build_version.lower() != local_version.lower():
    raise test_runner_errors.LocalRunXcodeError(xcode_build_version,
                                                local_version)


def validate_local_runtime(xcode_build_version,
                           platform_type: constants.IOSPlatformType,
                           platform_version: str):
  """Confirm that the locally installed iOS/tvOS simulator runtimes match the
  arguments passed to the test runner.

  Args:
    xcode_build_version: (str) Xcode version passed as an argument to the test
      runner, e.g. "16a242d"
    platform_type: (IOSPlatformType) iOS-based platform in use
    platform_version: (str) iOS version passed as an argument to the test
      runner, e.g. "18.0"

  Raises:
    test_runner_errors.LocalRunRuntimeError when the requested iOS version is
     not installed locally
  """
  runtime_build = get_latest_runtime_build_cipd(xcode_build_version,
                                                platform_type, platform_version)
  if runtime_build is None:
    raise test_runner_errors.RuntimeBuildNotFoundError(platform_version)
  local_runtime = iossim_util.get_simulator_runtime_info_by_build(runtime_build)
  if not local_runtime:
    raise test_runner_errors.LocalRunRuntimeError(platform_version,
                                                  runtime_build)


def check_xcode_exists_in_apps(xcode_version):
  """
    Checks if the specified Xcode version already exists in /Applications.
    This is mainly used when xcodes are already installed in VM images

    Args:
        xcode_version (str): The Xcode version string (e.g., "16f6").

    Returns:
        True if the Xcode app exists, otherwise False.
    """
  xcode_app_name = f"xcode_{xcode_version}.app"
  xcode_path = os.path.join("/Applications", xcode_app_name)
  return os.path.exists(xcode_path)


def ensure_xcode_ready_in_apps(xcode_build_version):
  """Ensures the specified Xcode version is ready for use.

  If the specified Xcode version is found in /Applications, this function
  selects it to ensure it has completed its initial setup. If the version
  is not found, it logs a warning.
  """
  LOGGER.info(
      'Checking for specified Xcode version in /Applications to ensure it is '
      'ready.')

  if check_xcode_exists_in_apps(xcode_build_version):
    xcode_app_name = f"xcode_{xcode_build_version}.app"
    app_path = os.path.join("/Applications", xcode_app_name)
    LOGGER.info(f"Found specified Xcode version at {app_path}. Selecting it.")
    select(app_path)
  else:
    LOGGER.warning(f"Specified Xcode version {xcode_build_version} not found "
                   "in /Applications.")


def install_xcode(mac_toolchain_cmd, xcode_build_version, xcode_path,
                  runtime_cache_prefix, device_type: str | None,
                  platform_version: str | None):
  """Installs the requested Xcode build version.

    Returns:
      True if installation was successful. False otherwise.
    """
  if is_local_run():
    validate_local_xcode_install(xcode_build_version)
    # Skip runtime validation if no platform_version is provided (indicating an
    # on-device test run).
    if platform_version:
      assert device_type is not None, "platform_version requires a device_type string"
      try:
        validate_local_runtime(
            xcode_build_version,
            iossim_util.get_platform_type_by_platform(device_type),
            platform_version)
      except test_runner_errors.RuntimeBuildNotFoundError as e:
        # If we hit this exception, a runtime was not found in CIPD. This can
        # happen when users do not have access to infra_internal, for example.
        LOGGER.warning(
            'Unable to find the iOS/tvOS runtime build version of Xcode %s '
            'and iOS/tvOS %s. CIPD is possibly not installed locally or the '
            'CIPD infra_internal repository cannot be accessed.',
            xcode_build_version, platform_version)
    return True

  # crbug.com/406819704: this is necessary when multiple versions of
  # xcodes exist in /Applications.
  ensure_xcode_ready_in_apps(xcode_build_version)

  try:
    if not mac_toolchain_cmd:
      raise test_runner_errors.MacToolchainNotFoundError(mac_toolchain_cmd)
    # Guard against incorrect install paths. On swarming, this path
    # should be a requested named cache, and it must exist.
    if not os.path.exists(xcode_path):
      raise test_runner_errors.XcodePathNotFoundError(xcode_path)

    runtime_cache_folder = None
    # Runner script only utilizes runtime cache when it's a simulator task.
    if platform_version:
      runtime_cache_folder = construct_runtime_cache_folder(
          runtime_cache_prefix, platform_version)
      if not os.path.exists(runtime_cache_folder):
        # Depending on infra project, runtime named cache might not be
        # deployed. Create the dir if it doesn't exist since xcode_util
        # assumes it exists.
        # TODO(crbug.com/40174473): Raise error instead of creating dirs after
        # runtime named cache is deployed everywhere.
        os.makedirs(runtime_cache_folder)
    # install() installs the Xcode & iOS runtime, and returns a bool
    # indicating if the Xcode version in CIPD is a legacy Xcode package (which
    # includes iOS runtimes).
    # Update as of 2023: for MacOS13+, iOS/tvOS runtime will not be installed in
    # install(). See install_runtime_dmg() below.
    install(
        mac_toolchain_cmd,
        xcode_build_version,
        xcode_path,
        runtime_cache_folder=runtime_cache_folder,
        ios_version=platform_version)
    select(xcode_path)

    # Starting MacOS13+, additional simulator runtime will be installed
    # in DMG format
    if platform_version and mac_util.is_macos_13_or_higher():
      install_runtime_dmg(
          mac_toolchain_cmd, runtime_cache_folder,
          iossim_util.get_platform_type_by_platform(device_type),
          platform_version, xcode_build_version)
  except subprocess.CalledProcessError as e:
    # Flush buffers to ensure correct output ordering.
    sys.stdout.flush()
    sys.stderr.write(traceback.format_exc())
    sys.stderr.write('Xcode build version %s failed to install: %s\n' %
                     (xcode_build_version, e))
    sys.stderr.flush()
    return False
  else:
    return True





def xctest_path(test_app_path: str) -> str:
  """Gets xctest-file from egtests/PlugIns folder.

  Returns:
      A path for xctest in the format of /PlugIns/file.xctest

  Raises:
      PlugInsNotFoundError: If no PlugIns folder found in egtests.app.
      XCTestPlugInNotFoundError: If no xctest-file found in PlugIns.
  """
  plugins_dir = os.path.join(test_app_path, 'PlugIns')
  if not os.path.exists(plugins_dir):
    raise test_runner.PlugInsNotFoundError(plugins_dir)
  plugin_xctest = None
  if os.path.exists(plugins_dir):
    for plugin in os.listdir(plugins_dir):
      if plugin.endswith('.xctest'):
        plugin_xctest = os.path.join(plugins_dir, plugin)
  if not plugin_xctest:
    raise test_runner.XCTestPlugInNotFoundError(plugin_xctest)

  return plugin_xctest.replace(test_app_path, '')
