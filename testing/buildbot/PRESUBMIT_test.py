#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time
import unittest

# Add src/testing/ into sys.path for importing PRESUBMIT without pylint errors.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from buildbot import PRESUBMIT


class PresubmitError:
  def __init__(self, message):
    self.message = message

  def __eq__(self, other):
    return isinstance(other, PresubmitError) and self.message == other.message

  def __repr__(self):
    return 'PresubmitError({!r})'.format(self.message)


class TestCheckFreeze(unittest.TestCase):
  def get_input_api(self, current_time, footers=None):
    """Get an input API to use for tests.

    Args:
      current_time - Current time expressed as seconds since the epoch.
    """

    class FakeTime:

      localtime = time.localtime
      strftime = time.strftime

      def time(self):
        return float(current_time)

    class FakeChange:
      def GitFootersFromDescription(self):
        return footers or []

    class FakeInputApi:

      time = FakeTime()
      change = FakeChange()

    return FakeInputApi()

  def get_output_api(self):
    class FakeOutputApi:

      PresubmitError = PresubmitError

    return FakeOutputApi

  def test_before_freeze(self):
    input_api = self.get_input_api(PRESUBMIT._FREEZE_START - 1)
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(errors, [])

  def test_start_of_freeze(self):
    input_api = self.get_input_api(PRESUBMIT._FREEZE_START + 1)
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(len(errors), 1)
    self.assertTrue(
        errors[0].message.startswith('There is a prod freeze in effect'))

  def test_end_of_freeze(self):
    input_api = self.get_input_api(PRESUBMIT._FREEZE_END - 1)
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(len(errors), 1)
    self.assertTrue(
        errors[0].message.startswith('There is a prod freeze in effect'))

  def test_after_freeze(self):
    input_api = self.get_input_api(PRESUBMIT._FREEZE_END + 1)
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(errors, [])

  def test_ignore_freeze(self):
    input_api = self.get_input_api(PRESUBMIT._FREEZE_START + 1,
                                   footers={'Ignore-Freeze': 'testing'})
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(errors, [])


if __name__ == '__main__':
  unittest.main()
