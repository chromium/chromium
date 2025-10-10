# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for exceptions and exception checking."""

import logging

LOGGER = logging.getLogger(__name__)
DEVICE_SETUP_NOT_COMPLETE = 'Device setup is not yet complete'
FAILED_TO_INSTALL_EMBEDDED_PROFILE = 'Failed to install embedded profile'
DEVELOPER_MODE_DISABLED = 'Developer Mode disabled'

class DeviceSetupNotCompleteError(Exception):
  """Device setup isn't complete"""

  def __init__(self):
    super(DeviceSetupNotCompleteError,
          self).__init__(DEVICE_SETUP_NOT_COMPLETE)


class FailedToInstallEmbeddedProfileError(Exception):
  """Failed to install embedded profile when attempting to install the app
  on device. This could be because the device's UDID needs to be added to the
  provisioning profile, but can also happen when the device's network connection
  is flaky, since the process of verifying the provisioning profile relies on
  the network."""

  def __init__(self):
    super(FailedToInstallEmbeddedProfileError,
          self).__init__(FAILED_TO_INSTALL_EMBEDDED_PROFILE)


class DeveloperModeDisabledError(Exception):
  """Failed to launch the test on device because developer mode isn't enabled"""

  def __init__(self):
    super(DeveloperModeDisabledError, self).__init__(DEVELOPER_MODE_DISABLED)


DEVICE_EXCEPTIONS = {
    DEVICE_SETUP_NOT_COMPLETE: DeviceSetupNotCompleteError,
    FAILED_TO_INSTALL_EMBEDDED_PROFILE: FailedToInstallEmbeddedProfileError,
    DEVELOPER_MODE_DISABLED: DeveloperModeDisabledError,
}


class ExceptionChecker:
  """Base class containing log exception checking common to device & simulator"""
  exceptions: list[Exception]

  def __init__(self):
    """Initializes a new instance of this class."""
    self.exceptions = []

  def check_line(self, line: str):
    """Checks a log line for known messages and stores exceptions.

    Args:
      line: (str) a line of log output to check for exceptions.
    """
    #This method is a placeholder fo common functionality between sim and device
    pass

  def throw_first(self):
    """Throws the first exception the exception checker encountered and prints
        all other exceptions found.
    """
    if len(self.exceptions) > 0:

      LOGGER.info('Printing all exceptions encountered')
      for exception in self.exceptions:
        LOGGER.info(f"Exception: {exception}")

      LOGGER.info('Raising first exception encountered')
      raise self.exceptions[0]


class DeviceExceptionChecker(ExceptionChecker):
  """Contains device specific log exception checking."""

  def check_line(self, line: str):

    # Check line for exceptions from base class first.
    super().check_line(line)

    for text, exception in DEVICE_EXCEPTIONS.items():
      if text.upper() in line.upper():
        self.exceptions.append(exception())
