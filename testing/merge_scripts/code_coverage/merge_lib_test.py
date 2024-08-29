#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import unittest
from unittest import mock

import merge_lib as merger

# Protected access is allowed for unittests.
# pylint: disable=protected-access

class MergeLibTest(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self.maxDiff = None

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
      merger._validate_and_convert_profraw('mock.profraw',
                                           output_profdata_files,
                                           invalid_profraw_files,
                                           counter_overflows,
                                           '/usr/bin/llvm-cov',
                                           show_profdata=False)
      self.assertEqual(
          expected_results,
          [output_profdata_files, invalid_profraw_files, counter_overflows])


if __name__ == '__main__':
  unittest.main()
