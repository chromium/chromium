#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import json
import unittest
import unittest.mock as mock

from flake_suppressor_common import queries
from flake_suppressor_common import unittest_utils as uu


class GetResultCountsUnittest(unittest.TestCase):
  def setUp(self) -> None:
    expectations_proceccor = uu.UnitTestExpectationProcessor()
    results_processor = uu.UnitTestResultProcessor(expectations_proceccor)
    self._querier_instance = uu.UnitTest_BigQueryQuerier(
        1, 'project', results_processor)

    self._querier_instance._submitted_builds = set(['build-1234', 'build-2345'])
    self._subprocess_patcher = mock.patch(
        'flake_suppressor_common.queries.subprocess.run')
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


class GenerateBigQueryCommandUnittest(unittest.TestCase):

  def testNoParametersSpecified(self) -> None:
    """Tests that no parameters are added if none are specified."""
    cmd = queries.GenerateBigQueryCommand('project', {})
    for element in cmd:
      self.assertFalse(element.startswith('--parameter'))

  def testParameterAddition(self) -> None:
    """Tests that specified parameters are added appropriately."""
    cmd = queries.GenerateBigQueryCommand('project', {
        '': {
            'string': 'string_value'
        },
        'INT64': {
            'int': 1
        }
    })
    self.assertIn('--parameter=string::string_value', cmd)
    self.assertIn('--parameter=int:INT64:1', cmd)

  def testBatchMode(self) -> None:
    """Tests that batch mode adds the necessary arg."""
    cmd = queries.GenerateBigQueryCommand('project', {}, batch=True)
    self.assertIn('--batch', cmd)


if __name__ == '__main__':
  unittest.main(verbosity=2)
