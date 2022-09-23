#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import json
import unittest
import unittest.mock as mock

from flake_suppressor import gpu_expectations
from flake_suppressor import gpu_queries as queries
from flake_suppressor import gpu_tag_utils as tag_utils
from flake_suppressor import gpu_results as results_module
from flake_suppressor_common import unittest_utils as uu
from flake_suppressor_common import tag_utils as common_tag_utils


class GpuQueriesUnittest(unittest.TestCase):
  def setUp(self) -> None:
    common_tag_utils.SetTagUtilsImplementation(tag_utils.GpuTagUtils)
    expectations_processor = gpu_expectations.GpuExpectationProcessor()
    result_processor = results_module.GpuResultProcessor(expectations_processor)
    self._querier_instance = queries.GpuBigQueryQuerier(1, 'project',
                                                        result_processor)
    self._querier_instance._submitted_builds = set(['build-1234', 'build-2345'])
    self._subprocess_patcher = mock.patch(
        'flake_suppressor_common.queries.subprocess.run')
    self._subprocess_mock = self._subprocess_patcher.start()
    self.addCleanup(self._subprocess_patcher.stop)

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
