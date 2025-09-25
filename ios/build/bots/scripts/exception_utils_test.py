#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest

import exception_utils


class DeviceExceptionCheckerTest(unittest.TestCase):

  def setUp(self):
    self.exception_checker = exception_utils.DeviceExceptionChecker()

  def test_device_setup_not_complete_error_raised(self):
    log_message = "(Underlying Error: The operation couldn’t be completed. Device setup is not yet complete.)"

    self.exception_checker.check_line(log_message)

    self.assertRaises(exception_utils.DeviceSetupNotCompleteError,
                      self.exception_checker.throw_first)


if __name__ == '__main__':
  logging.basicConfig(
      format='[%(asctime)s:%(levelname)s] %(message)s',
      level=logging.DEBUG,
      datefmt='%I:%M:%S')
  unittest.main()
