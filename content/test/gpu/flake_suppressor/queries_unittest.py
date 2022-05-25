#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import json
import unittest
import unittest.mock as mock

from flake_suppressor import queries
from flake_suppressor import unittest_utils as uu


class GetResultCountsUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._querier_instance = queries.BigQueryQuerier(1, 'project')
    self._querier_instance._submitted_builds = set(['build-1234', 'build-2345'])
    self._subprocess_patcher = mock.patch(
        'flake_suppressor.queries.subprocess.run')
    self._subprocess_mock = self._subprocess_patcher.start()
    self.addCleanup(self._subprocess_patcher.stop)

  def testBasic(self) -> None:
    """Tests that queried data is properly returned."""

    def SideEffect(*_, **kwargs) -> uu.FakeProcess:
      query = kwargs['input']
      if 'submitted_builds' in query:
        # Try results.
        query_result = [{
            'typ_tags': ['a1', 'a2', 'a3'],
            'test_name': 'garbage.suite.garbage.alphanumeric',
            'result_count': '200',
        }, {
            'typ_tags': ['a', 'b', 'c'],
            'test_name': 'garbage.suite.garbage.alphabet',
            'result_count': '50',
        }]
      else:
        # CI Results.
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
      return uu.FakeProcess(stdout=json.dumps(query_result))

    self._subprocess_mock.side_effect = SideEffect
    result_counts = self._querier_instance.GetResultCounts()
    expected_result_counts = {
        ('a', 'b', 'c'): {
            'alphabet': 150,
        },
        ('1', '2', '3'): {
            'numbers': 50,
        },
        ('a1', 'a2', 'a3'): {
            'alphanumeric': 200,
        }
    }
    self.assertEqual(result_counts, expected_result_counts)
    self.assertEqual(self._subprocess_mock.call_count, 2)

  def testIgnoredTags(self) -> None:
    """Tests that ignored tags are removed and their counts merged."""

    def SideEffect(*_, **kwargs) -> uu.FakeProcess:
      query = kwargs['input']
      if 'submitted_builds' in query:
        # Try results.
        query_result = [
            {
                'typ_tags': ['linux', 'nvidia'],
                'test_name': 'garbage.suite.garbage.linux',
                'result_count': '25',
            },
            {
                'typ_tags': ['linux', 'win-laptop'],
                'test_name': 'garbage.suite.garbage.linux',
                'result_count': '50',
            },
        ]
      else:
        # CI results.
        query_result = [{
            'typ_tags': ['win', 'win-laptop'],
            'test_name': 'garbage.suite.garbage.windows',
            'result_count': '100',
        }, {
            'typ_tags': ['win'],
            'test_name': 'garbage.suite.garbage.windows',
            'result_count': '50',
        }, {
            'typ_tags': ['mac', 'exact'],
            'test_name': 'garbage.suite.garbage.mac',
            'result_count': '200',
        }, {
            'typ_tags': ['linux'],
            'test_name': 'garbage.suite.garbage.linux',
            'result_count': '300',
        }]
      return uu.FakeProcess(stdout=json.dumps(query_result))

    self._subprocess_mock.side_effect = SideEffect
    result_counts = self._querier_instance.GetResultCounts()
    expected_result_counts = {
        tuple(['win']): {
            'windows': 150,
        },
        tuple(['mac']): {
            'mac': 200,
        },
        tuple(['linux']): {
            'linux': 350,
        },
        tuple(['linux', 'nvidia']): {
            'linux': 25,
        },
    }
    self.assertEqual(result_counts, expected_result_counts)
    self.assertEqual(self._subprocess_mock.call_count, 2)


if __name__ == '__main__':
  unittest.main(verbosity=2)
