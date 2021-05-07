# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.version
import glob
import logging
import os
import shutil
import subprocess

import test_runner_errors

LOGGER = logging.getLogger(__name__)
XcodeIOSSimulatorDefaultRuntimeFilename = 'iOS.simruntime'
XcodeIOSSimulatorRuntimeRelPath = ('Contents/Developer/Platforms/'
                                   'iPhoneOS.platform/Library/Developer/'
                                   'CoreSimulator/Profiles/Runtimes')


def _using_new_mac_toolchain(mac_toolchain):
  """Returns if the mac_toolchain command passed in is new version.

  New mac_toolchain can download an Xcode without bundled runtime, and can
  download single runtimes. Legacy mac_toolchain can only download Xcode package
  as a whole package. The function tells the difference by checking the
  existence of a new command line switch in new version.
  TODO(crbug.com/1191260): Remove this util function when the new mac_toolchain
  version is rolled to everywhere using this script.
  """
  cmd = [
      mac_toolchain,
      'help',
  ]
  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)

  # "install-runtime" presents as a command line switch in help output in the
  # new mac_toolchain.
  using_new_mac_toolchain = 'install-runtime' in output
  return using_new_mac_toolchain


def _is_legacy_xcode_package(xcode_app_path):
  """Checks and returns if the installed Xcode package is legacy version.

  Legacy Xcode package are uploaded with legacy version of mac_toolchain. iOS
  runtimes are packaged into legacy Xcode packages but not into new packages.

  Args:
    xcode_app_path: (string) Path to install the contents of Xcode.app.

  Returns:
    (bool) True if the package is legacy(with runtime bundled). False otherwise.
  """
  # Existence of default iOS runtime indicates the downloaded Xcode is a legacy
  # one (with runtime bundled).
  return os.path.exists(
      os.path.join(xcode_app_path, XcodeIOSSimulatorRuntimeRelPath,
                   XcodeIOSSimulatorDefaultRuntimeFilename))


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
  # Transform iOS version to the runtime version format required my the tool.
  # e.g. "14.4" -> "ios-14-4"
  runtime_version = 'ios-' + ios_version.replace('.', '-')

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


def move_runtime(runtime_cache_folder, xcode_app_path, into_xcode):
  """Moves runtime from runtime cache into xcode or vice versa.

  The function assumes that there's exactly one *.simruntime file in the source
  folder. It's intended to only work with new Xcode packages.

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

  # Get the runtime package filename. It might not be the default name.
  runtime_name = os.path.basename(runtimes_in_src[0])
  dst_runtime = os.path.join(dst_folder, runtime_name)

  # Remove if the runtime package already exists in dst.
  if os.path.exists(dst_runtime):
    shutil.rmtree(dst_runtime)

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
  LOGGER.debug('Selecting XCode with command %s and "xcrun simctl list".' % cmd)
  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)

  # This is to avoid issues caused by mixed usage of different Xcode versions on
  # one machine.
  xcrun_simctl_cmd = ['xcrun', 'simctl', 'list']
  output += subprocess.check_output(xcrun_simctl_cmd, stderr=subprocess.STDOUT)

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

  TODO(crbug.com/1191260): Remove the last argument when the new mac_toolchain
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

  If using new mac_toolchain, first install the Xcode package:
  * If installed Xcode is legacy one (with runtimes bundled), return.
  * If installed Xcode isn't legacy (without runtime bundled), install and copy
  * the specified runtime version into Xcode.

  Args:
    xcode_build_version: (string) Xcode build version to install.
    mac_toolchain: (string) Path to mac_toolchain command to install Xcode
    See https://chromium.googlesource.com/infra/infra/+/main/go/src/infra/cmd/mac_toolchain/
    xcode_app_path: (string) Path to install the contents of Xcode.app.
    runtime_args: Keyword arguments related with runtime installation, namely:
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

  _install_xcode(mac_toolchain, xcode_build_version, xcode_app_path,
                 using_new_mac_toolchain)
  is_legacy_xcode_package = _is_legacy_xcode_package(xcode_app_path)

  if not using_new_mac_toolchain and not is_legacy_xcode_package:
    # Legacy mac_toolchain can't handle the situation when no runtime is in
    # Xcode package.
    raise test_runner_errors.XcodeMacToolchainMismatchError(xcode_build_version)

  # Install & move the runtime to Xcode. Can only work with new mac_toolchain.
  if not is_legacy_xcode_package:
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
  LOGGER.debug('Checking XCode version with command: %s' % cmd)

  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  output = output.splitlines()
  # output sample:
  # Xcode 12.0
  # Build version 12A6159
  LOGGER.info(output)

  version = output[0].decode('UTF-8').split(' ')[1]
  build_version = output[1].decode('UTF-8').split(' ')[2].lower()

  return version, build_version

def using_xcode_11_or_higher():
  """Returns true if using Xcode version 11 or higher."""
  LOGGER.debug('Checking if Xcode version is 11 or higher')
  return distutils.version.LooseVersion(
      '11.0') <= distutils.version.LooseVersion(version()[0])
