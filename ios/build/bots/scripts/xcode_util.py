# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.version
import logging
import subprocess

LOGGER = logging.getLogger(__name__)


def select(xcode_app_path):
  """Invoke sudo xcode-select -s {xcode_app_path}

  Raises:
    subprocess.CalledProcessError on exit codes non zero
  """
  cmd = [
      'sudo',
      'xcode-select',
      '-s',
      xcode_app_path,
  ]
  LOGGER.debug('Selecting XCode with command: %s and "xcrun simctl list".' % cmd)
  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)

  # This is to avoid issues caused by mixed usage of different Xcode versions on
  # one machine.
  xcrun_simctl_cmd = ['xcrun', 'simctl', 'list']
  output += subprocess.check_output(xcrun_simctl_cmd, stderr=subprocess.STDOUT)

  return output


def install(mac_toolchain, xcode_build_version, xcode_path):
  """Invoke mactoolchain to install the given xcode version.

  Args:
    xcode_build_version: (string) Xcode build version to install.
    mac_toolchain: (string) Path to mac_toolchain command to install Xcode
    See https://chromium.googlesource.com/infra/infra/+/master/go/src/infra/cmd/mac_toolchain/
    xcode_path: (string) Path to install the contents of Xcode.app.

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
  LOGGER.debug("Installing xcode with command: %s" % cmd)
  output = subprocess.check_call(cmd, stderr=subprocess.STDOUT)
  return output


def version():
  """Invoke xcodebuild -version

  Raises:
    subprocess.CalledProcessError on exit codes non zero

  Returns:
    version (12.0), build_version (12a6163b)
  """
  cmd = [
      'xcodebuild',
      '-version',
  ]
  LOGGER.debug("Checking XCode version with command: %s" % cmd)

  output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  output = output.splitlines()
  # output sample:
  # Xcode 12.0
  # Build version 12A6159
  logging.info(output)

  version = output[0].decode('UTF-8').split(' ')[1]
  build_version = output[1].decode('UTF-8').split(' ')[2].lower()

  return version, build_version

def using_xcode_11_or_higher():
  """Returns true if using Xcode version 11 or higher."""
  LOGGER.debug("Checking if Xcode version is 11 or higher")
  return distutils.version.LooseVersion(
      '11.0') <= distutils.version.LooseVersion(version()[0])
