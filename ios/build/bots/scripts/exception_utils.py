# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utils for exceptions and exception checking."""

import logging

LOGGER = logging.getLogger(__name__)


class DeviceSetupNotCompleteError(Exception):
  """Device setup isn't complete"""

  def __init__(self):
    super(DeviceSetupNotCompleteError,
          self).__init__('Device setup isn\'t complete')


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

    if 'Device setup is not yet complete'.upper() in line.upper():
      self.exceptions.append(DeviceSetupNotCompleteError())
