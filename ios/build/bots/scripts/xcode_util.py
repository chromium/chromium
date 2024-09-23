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

import iossim_util
import mac_util
import test_runner
import test_runner_errors

LOGGER = logging.getLogger(__name__)
XcodeIOSSimulatorDefaultRuntimeFilename = 'iOS.simruntime'
XcodeIOSSimulatorRuntimeRelPath = ('Contents/Developer/Platforms/'
                                   'iPhoneOS.platform/Library/Developer/'
                                   'CoreSimulator/Profiles/Runtimes')
XcodeCipdFiles = ['.cipd', '.xcode_versions']
XcodeIOSSimulatorRuntimeBuildTagRegx = r'ios_runtime_build:(.*)'
XcodeIOSSimulatorRuntimeVersionTagRegx = r'ios_runtime_version:(.*)'
XcodeIOSSimulatorRuntimeDMGCipdPath = 'infra_internal/ios/xcode/ios_runtime_dmg'

# TODO(crbug.com/40910268): remove Legacy Download once iOS 15.5 is deprecated
IOS_SIM_RUNTIME_BUILTIN_STATE = ['Legacy Download', 'Bundled with Xcode']


def describe_cipd_ref(pkg_path, ref):
  cmd = ['cipd', 'describe', pkg_path, '-version', ref]
  output = ''
  try:
    output = subprocess.check_output(
        cmd, stderr=subprocess.STDOUT).decode('utf-8')
  except subprocess.CalledProcessError:
    LOGGER.debug('cipd describe cmd %s returned nothing' % cmd)
  return output


def convert_ios_version_to_cipd_ref(ios_version):
  # Transform iOS version to the runtime version format required by
  # mac_toolchain. e.g. "14.4" -> "ios-14-4"
  return 'ios-' + ios_version.replace('.', '-')


def _using_new_mac_toolchain(mac_toolchain):
  """Returns if the mac_toolchain command passed in is new version.

  New mac_toolchain can download an Xcode without bundled runtime, and can
  download single runtimes. Legacy mac_toolchain can only download Xcode package
  as a whole package. The function tells the difference by checking the
  existence of a new command line switch in new version.
  TODO(crbug.com/40174473): Remove this util function when the new mac_toolchain
  version is rolled to everywhere using this script.
  """
  cmd = [
      mac_toolchain,
      'help',
  ]
  output = subprocess.check_output(
      cmd, stderr=subprocess.STDOUT).decode('utf-8')

  # "install-runtime" presents as a command line switch in help output in the
  # new mac_toolchain.
  using_new_mac_toolchain = 'install-runtime' in output
  return using_new_mac_toolchain


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

  runtime_version = convert_ios_version_to_cipd_ref(ios_version)

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


def construct_runtime_cache_folder(runtime_cache_prefix, ios_version):
  """Composes runtime cache folder from it's prefix and ios_version.

  Note: Please keep the pattern consistent between what's being passed into
  runner script in gn(build/config/ios/ios_test_runner_wrapper.gni), and what's
  being configured for swarming cache in test configs (testing/buildbot/*).
  """
  return runtime_cache_prefix + ios_version


def move_runtime(runtime_cache_folder, xcode_app_path, into_xcode):
  """Moves runtime from runtime cache into xcode or vice versa.

  The function is intended to only work with new Xcode packages.

  The function assumes that there's exactly one *.simruntime file in the source
  folder. It also removes existing runtimes in the destination folder. The above
  assumption & handling can ensure no incorrect Xcode package is cached from
  corner cases.

  Args:
    runtime_cache_folder: (string) Path to the runtime cache directory.
    xcode_app_path: (string) Path to install the contents of Xcode.app.
    into_xcode: (bool) Whether the function moves from cache dir into Xcode or
      from Xcode to cache dir.

  Raises:
    IOSRuntimeHandlingError for issues moving runtime around.
    shutil.Error for exceptions from shutil when moving files around.
  """
  xcode_runtime_folder = os.path.join(xcode_app_path,
                                      XcodeIOSSimulatorRuntimeRelPath)
  src_folder = runtime_cache_folder if into_xcode else xcode_runtime_folder
  dst_folder = xcode_runtime_folder if into_xcode else runtime_cache_folder

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


def remove_runtimes(xcode_app_path):
  """Removes all runtimes in given xcode path."""
  runtimes = glob.glob(
      os.path.join(xcode_app_path, XcodeIOSSimulatorRuntimeRelPath,
                   '*.simruntime'))
  for runtime in runtimes:
    LOGGER.warning('Removing existing %s in xcode.', runtime)
    shutil.rmtree(runtime)


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
  LOGGER.debug('Selecting Xcode with command %s and "xcrun simctl list".' % cmd)
  output = subprocess.check_output(
      cmd, stderr=subprocess.STDOUT).decode('utf-8')

  # This is to avoid issues caused by mixed usage of different Xcode versions on
  # one machine.
  xcrun_simctl_cmd = ['xcrun', 'simctl', 'list']
  output += subprocess.check_output(
      xcrun_simctl_cmd, stderr=subprocess.STDOUT).decode('utf-8')

  return output


def _install_xcode(mac_toolchain, xcode_build_version, xcode_path,
                   using_new_mac_toolchain):
  """Invokes mac_toolchain to install the given xcode version.

  If using legacy mac_toolchain, install the whole Xcode package. If using the
  new mac_toolchain, add a command line switch to try to install an Xcode
  without runtime. However, the existence of runtime depends on the actual Xcode
  package in CIPD. e.g. An Xcode package uploaded with legacy mac_toolchain will
  include runtimes, even though it's installed with new mac_toolchain and
  "-with-runtime=False" switch.

  TODO(crbug.com/40174473): Remove the last argument when the new mac_toolchain
  version is rolled to everywhere using this script.

  Args:
    xcode_build_version: (string) Xcode build version to install.
    mac_toolchain: (string) Path to mac_toolchain command to install Xcode
    See https://chromium.googlesource.com/infra/infra/+/main/go/src/infra/cmd/mac_toolchain/
    xcode_path: (string) Path to install the contents of Xcode.app.
    using_new_mac_toolchain: (bool) Using new mac_toolchain.

  Raises:
    subprocess.CalledProcessError on exit codes non zero
  """
  cmd = [
      mac_toolchain,
      'install',
      '-kind',
      'ios',
      '-xcode-version',
      xcode_build_version.lower(),
      '-output-dir',
      xcode_path,
  ]

  if using_new_mac_toolchain:
    cmd.append('-with-runtime=False')

  LOGGER.debug('Installing xcode with command: %s' % cmd)
  output = subprocess.check_call(cmd, stderr=subprocess.STDOUT)
  return output


def install(mac_toolchain, xcode_build_version, xcode_app_path, **runtime_args):
  """Installs the Xcode and returns if the installed one is a legacy package.

  Installs the Xcode of given version to path. Returns if the Xcode package
  of the version is a legacy package (with runtimes bundled in). Runtime related
  arguments will only work when |mac_toolchain| is a new version (with runtime
  features), and the |xcode_build_version| in CIPD is a new package (uploaded
  by new mac_toolchain).

  If using legacy mac_toolchain, install the whole legacy Xcode package. (Will
  raise if the Xcode package isn't legacy.)
  UPDATE: all MacOS13+ bots will also install the whole legacy Xcode package due
  to the new codesign restrictions in crbug/1406204

  If using new mac_toolchain, first install the Xcode package:
  * If installed Xcode is legacy one (with runtimes bundled), return.
  * If installed Xcode isn't legacy (without runtime bundled), install and copy
  * the specified runtime version into Xcode.

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
    XcodeMacToolchainMismatchError if an Xcode without runtime is installed with
      a legacy mac_toolchain.

  Returns:
    True, if the Xcode package in CIPD is legacy (bundled with runtimes).
    False, if the Xcode package in CIPD is new (not bundled with runtimes).
  """
  using_new_mac_toolchain = _using_new_mac_toolchain(mac_toolchain)

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

  _install_xcode(mac_toolchain, xcode_build_version, xcode_app_path,
                 using_new_mac_toolchain)

  # (crbug/1406204): for MacOS13+, we are using Xcode fat upload/download again,
  # so runtime should not be installed separately.
  is_legacy_xcode_package = mac_util.is_macos_13_or_higher(
  ) or _is_legacy_xcode_package(xcode_app_path)

  if not using_new_mac_toolchain and not is_legacy_xcode_package:
    # Legacy mac_toolchain can't handle the situation when no runtime is in
    # Xcode package.
    raise test_runner_errors.XcodeMacToolchainMismatchError(xcode_build_version)

  # Install & move the runtime to Xcode. Can only work with new mac_toolchain.
  # Only install runtime when it's working for a simulator task.
  if not is_legacy_xcode_package and runtime_args.get('ios_version'):
    runtime_cache_folder = runtime_args.get('runtime_cache_folder')
    ios_version = runtime_args.get('ios_version')
    if not runtime_cache_folder or not ios_version:
      raise test_runner_errors.IOSRuntimeHandlingError(
          'Insufficient runtime_args. runtime_cache_folder: %s, ios_version: %s'
          % s(runtime_cache_folder, ios_version))

    # Try to install the runtime to it's cache folder. mac_toolchain will test
    # and install only when the runtime doesn't exist in cache.
    _install_runtime(mac_toolchain, runtime_cache_folder, xcode_build_version,
                     ios_version)
    move_runtime(runtime_cache_folder, xcode_app_path, into_xcode=True)

  return is_legacy_xcode_package


def _install_runtime_dmg(mac_toolchain, install_path, ios_version,
                         xcode_build_version):
  runtime_version = convert_ios_version_to_cipd_ref(ios_version)
  cmd = [
      mac_toolchain, 'install-runtime-dmg', '-runtime-version', runtime_version,
      '-xcode-version', xcode_build_version, '-output-dir', install_path
  ]

  LOGGER.debug('Installing runtime dmg with command: %s' % cmd)
  output = subprocess.check_call(cmd, stderr=subprocess.STDOUT)
  return output


def get_runtime_dmg_name(runtime_dmg_folder):
  runtime_dmg_name = glob.glob(os.path.join(runtime_dmg_folder, '*.dmg'))
  return runtime_dmg_name[0]


def get_latest_runtime_build_cipd(xcode_version, ios_version):
  # Use Xcode version first to find the matching iOS runtime,
  # if the runtime returned is not the desired version,
  # then use desired version to match as a fallback
  runtime_version = convert_ios_version_to_cipd_ref(ios_version)
  output = describe_cipd_ref(XcodeIOSSimulatorRuntimeDMGCipdPath, xcode_version)
  runtime_build_match = re.search(XcodeIOSSimulatorRuntimeBuildTagRegx, output,
                                  re.MULTILINE)
  runtime_version_match = re.search(XcodeIOSSimulatorRuntimeVersionTagRegx,
                                    output, re.MULTILINE)
  if runtime_build_match and runtime_version_match:
    if runtime_version_match.group(1) == runtime_version:
      return runtime_build_match.group(1)

  output = describe_cipd_ref(XcodeIOSSimulatorRuntimeDMGCipdPath,
                             runtime_version)
  runtime_build_match = re.search(XcodeIOSSimulatorRuntimeBuildTagRegx, output)
  if runtime_build_match:
    return runtime_build_match.group(1)
  return None


def is_runtime_builtin(ios_version):
  runtime = iossim_util.get_simulator_runtime_info(ios_version)
  return iossim_util.is_simulator_runtime_builtin(runtime)


def install_runtime_dmg(mac_toolchain, runtime_cache_folder, ios_version,
                        xcode_build_version):
  if is_runtime_builtin(ios_version):
    LOGGER.debug(
        'Runtime is already built-in, no need to install from mac_toolchain')
    return

  # try to delete some simulator runtimes first, to free some disk space,
  # if needed.
  if not os.environ.get('LUCI_CONTEXT'):
    logging.warning('Sim runtimes will not be cleaned up running locally')
  else:
    iossim_util.delete_least_recently_used_simulator_runtimes()

  runtime_build_to_install = get_latest_runtime_build_cipd(
      xcode_build_version, ios_version)
  if runtime_build_to_install is None:
    raise test_runner_errors.RuntimeBuildNotFoundError(ios_version)

  # check if the desired runtime build already exists on disk
  if iossim_util.get_simulator_runtime_info_by_build(
      runtime_build_to_install) is None:
    _install_runtime_dmg(mac_toolchain, runtime_cache_folder, ios_version,
                         xcode_build_version)
    output = iossim_util.add_simulator_runtime(
        get_runtime_dmg_name(runtime_cache_folder))
    iossim_util.override_default_iphonesim_runtime(output, ios_version)
  else:
    LOGGER.debug(
        'Runtime %s already exists, no need to install from mac_toolchain',
        runtime_build_to_install)
  # TODO(crbug.com/349660173): See if this can be removed after the release of
  # subsequent Xcode16 betas
  if using_xcode_16_or_higher():
    iossim_util.delete_other_ios18_runtimes(runtime_build_to_install)


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


def install_xcode(mac_toolchain_cmd, xcode_build_version, xcode_path,
                  runtime_cache_prefix, ios_version):
  """Installs the requested Xcode build version.

    Returns:
      (bool, bool)
        First bool: True if installation was successful. False otherwise.
        Second bool: True if Xcode is legacy package. False if it's new.
    """
  try:
    if not mac_toolchain_cmd:
      raise test_runner_errors.MacToolchainNotFoundError(mac_toolchain_cmd)
    # Guard against incorrect install paths. On swarming, this path
    # should be a requested named cache, and it must exist.
    if not os.path.exists(xcode_path):
      raise test_runner_errors.XcodePathNotFoundError(xcode_path)

    runtime_cache_folder = None
    # Runner script only utilizes runtime cache when it's a simulator task.
    if ios_version:
      runtime_cache_folder = construct_runtime_cache_folder(
          runtime_cache_prefix, ios_version)
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
    # Update as of 2023: for MacOS13+, iOS runtime will not be installed in
    # install(). See install_runtime_dmg below().
    is_legacy_xcode = install(
        mac_toolchain_cmd,
        xcode_build_version,
        xcode_path,
        runtime_cache_folder=runtime_cache_folder,
        ios_version=ios_version)
    select(xcode_path)

    # Starting MacOS13+, additional simulator runtime will be installed
    # in DMG format
    if ios_version and mac_util.is_macos_13_or_higher():
      install_runtime_dmg(mac_toolchain_cmd, runtime_cache_folder, ios_version,
                          xcode_build_version)
  except subprocess.CalledProcessError as e:
    # Flush buffers to ensure correct output ordering.
    sys.stdout.flush()
    sys.stderr.write('Xcode build version %s failed to install: %s\n' %
                     (xcode_build_version, e))
    sys.stderr.flush()
    return False, False
  else:
    return True, is_legacy_xcode


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
