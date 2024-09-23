# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Stores Error classes to be accessed in multiple files within package."""


class Error(Exception):
  """Base class for errors."""
  pass


class IOSRuntimeHandlingError(Error):
  """Base class for iOS runtime package related errors."""
  pass


class XcodeEnumerateTestsError(Error):
  """Xcodebuild failed to enumerate test methods present in the EG test app"""

  def __init__(self, error=''):
    super(
        XcodeEnumerateTestsError,
        self).__init__(f'Xcodebuild failed to enumerate test methods.\n{error}')


class XcodeInstallError(Error):
  """Base class for xcode install related errors."""
  pass


class XcodeInstallFailedError(Error):
  """The requested version of Xcode was not successfully installed."""

  def __init__(self, xcode_version):
    super(XcodeInstallFailedError, self).__init__(
        'Xcode version %s failed to install. File a ticket to go/ios-ops-ticket'
        % xcode_version)


class XcodeUnsupportedFeatureError(Error):
  """Base class for unsupported features related with Xcode."""
  pass


class XcodeMacToolchainMismatchError(XcodeInstallError):
  """The mac_toolchain version can't work with the Xcode package."""

  def __init__(self, xcode_build_version):
    super(XcodeMacToolchainMismatchError, self).__init__(
        'Legacy mac_toolchain cannot work with Xcode: %s' % xcode_build_version)


class MacToolchainNotFoundError(XcodeInstallError):
  """The mac_toolchain is not specified."""

  def __init__(self, mac_toolchain):
    super(MacToolchainNotFoundError, self).__init__(
        'mac_toolchain is not specified or not found: "%s"' % mac_toolchain)


class XcodePathNotFoundError(XcodeInstallError):
  """The path to Xcode.app is not specified."""

  def __init__(self, xcode_path):
    super(XcodePathNotFoundError, self).__init__(
        'xcode_path is not specified or does not exist: "%s"' % xcode_path)


class RuntimeBuildNotFoundError(Error):
  """The desired runtime build is not found on cipd."""

  def __init__(self, ios_version):
    super(RuntimeBuildNotFoundError, self).__init__(
        'the desired runtime build for iOS %s is not found on cipd' %
        ios_version)


class SimRuntimeDeleteTimeoutError(Error):
  """When deleting a simulator runtime exceeds timeout."""

  def __init__(self, ios_version):
    super(SimRuntimeDeleteTimeoutError, self).__init__(
        'Unable to delete runtime %s after timeout' % ios_version)
