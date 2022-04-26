#!/usr/bin/env python3
# Copyright (c) 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import unittest

import PRESUBMIT


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
    input_api = self.get_input_api(1639641599)  # 2021/12/15 23:59:59 -0800
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(errors, [])

  def test_start_of_freeze(self):
    input_api = self.get_input_api(1639641600)  # 2021/12/16 00:00:00 -0800
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(len(errors), 1)
    self.assertTrue(
        errors[0].message.startswith('There is a prod freeze in effect'))

  def test_end_of_freeze(self):
    input_api = self.get_input_api(1641196799)  # 2022/01/02 23:59:59 -0800
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(len(errors), 1)
    self.assertTrue(
        errors[0].message.startswith('There is a prod freeze in effect'))

  def test_after_freeze(self):
    input_api = self.get_input_api(1641196800)  # 2022/01/03 00:00:00 -0800')
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(errors, [])

  def test_ignore_freeze(self):
    input_api = self.get_input_api(
        1639641600,  # 2021/12/16 00:00:00 -0800
        footers={'Ignore-Freeze': 'testing'})
    output_api = self.get_output_api()

    errors = PRESUBMIT.CheckFreeze(input_api, output_api)

    self.assertEqual(errors, [])


if __name__ == '__main__':
  unittest.main()
