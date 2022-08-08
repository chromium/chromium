#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest

import mock

import merge_lib as merger


class MergeLibTest(unittest.TestCase):

  # pylint: disable=super-with-arguments
  def __init__(self, *args, **kwargs):
    super(MergeLibTest, self).__init__(*args, **kwargs)
    self.maxDiff = None
  # pylint: enable=super-with-arguments

  @mock.patch.object(subprocess, 'check_output')
  def test_validate_and_convert_profraw(self, mock_cmd):
    test_cases = [
        ([''], [['mock.profdata'], [], []]),
        (['Counter overflow'], [[], ['mock.profraw'], ['mock.profraw']]),
        (subprocess.CalledProcessError(
            255,
            'llvm-cov merge -o mock.profdata -sparse=true mock.profraw',
            output='Malformed profile'), [[], ['mock.profraw'], []]),
    ]
    for side_effect, expected_results in test_cases:
      mock_cmd.side_effect = side_effect
      output_profdata_files = []
      invalid_profraw_files = []
      counter_overflows = []
      merger._validate_and_convert_profraw(
          'mock.profraw', output_profdata_files, invalid_profraw_files,
          counter_overflows, '/usr/bin/llvm-cov')
      self.assertEqual(
          expected_results,
          [output_profdata_files, invalid_profraw_files, counter_overflows])


if __name__ == '__main__':
  unittest.main()
