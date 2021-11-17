#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys
import unittest

# This script is not Python 2-compatible, but some presubmit scripts end up
# trying to parse this to find tests.
# TODO(crbug.com/1198237): Remove this once all the GPU tests, and by
# extension the presubmit scripts, are Python 3-compatible.
if sys.version_info[0] == 3:
  import unittest.mock as mock

from flake_suppressor import queries
from flake_suppressor import unittest_utils as uu


class GetResultCountsUnittest(unittest.TestCase):
  def setUp(self):
    self._subprocess_patcher = mock.patch(
        'flake_suppressor.queries.subprocess.run')
    self._subprocess_mock = self._subprocess_patcher.start()
    self.addCleanup(self._subprocess_patcher.stop)

  def testBasic(self):
    """Tests that queried data is properly returned."""
    query_result = [
        {
            'typ_tags': ['a', 'b', 'c'],
            'test_name': 'garbage.suite.garbage.alphabet',
            'result_count': '100',
        },
        {
            'typ_tags': ['1', '2', '3'],
            'test_name': 'garbage.suite.garbage.numbers',
            'result_count': '50',
        },
    ]
    fake_process = uu.FakeProcess(stdout=json.dumps(query_result))
    self._subprocess_mock.return_value = fake_process
    result_counts = queries.GetResultCounts(1, 'project')
    expected_result_counts = {
        ('a', 'b', 'c'): {
            'alphabet': 100,
        },
        ('1', '2', '3'): {
            'numbers': 50,
        },
    }
    self.assertEqual(result_counts, expected_result_counts)
    self._subprocess_mock.assert_called_once()

  def testIgnoredTags(self):
    """Tests that ignored tags are removed and their counts merged."""
    query_result = [
        {
            'typ_tags': ['win', 'win-laptop'],
            'test_name': 'garbage.suite.garbage.windows',
            'result_count': '100',
        },
        {
            'typ_tags': ['win'],
            'test_name': 'garbage.suite.garbage.windows',
            'result_count': '50',
        },
        {
            'typ_tags': ['mac', 'exact'],
            'test_name': 'garbage.suite.garbage.mac',
            'result_count': '200',
        },
    ]
    fake_process = uu.FakeProcess(stdout=json.dumps(query_result))
    self._subprocess_mock.return_value = fake_process
    result_counts = queries.GetResultCounts(1, 'project')
    expected_result_counts = {
        tuple(['win']): {
            'windows': 150,
        },
        tuple(['mac']): {
            'mac': 200,
        },
    }
    self.assertEqual(result_counts, expected_result_counts)
    self._subprocess_mock.assert_called_once()


if __name__ == '__main__':
  unittest.main(verbosity=2)
