#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import subprocess
import sys
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from unexpected_passes import gpu_queries
from unexpected_passes import gpu_unittest_utils as gpu_uu
from unexpected_passes_common import builders
from unexpected_passes_common import data_types
from unexpected_passes_common import unittest_utils as uu


class QueryBuilderUnittest(unittest.TestCase):
  def setUp(self):
    self._patcher = mock.patch.object(subprocess, 'Popen')
    self._popen_mock = self._patcher.start()
    self.addCleanup(self._patcher.stop)

    builders.ClearInstance()
    uu.RegisterGenericBuildersImplementation()

  def testWebGlVersion(self):
    """Tests that only results for the correct WebGL version are returned."""
    query_results = [
        {
            'id':
            'build-1234',
            'test_id': ('ninja://chrome/test:telemetry_gpu_integration_test/'
                        'gpu_tests.webgl_conformance_integration_test.'
                        'WebGLConformanceIntegrationTest.test_name'),
            'status':
            'FAIL',
            'typ_expectations': [
                'RetryOnFailure',
            ],
            'typ_tags': [
                'webgl-version-1',
            ],
            'step_name':
            'step_name',
        },
        {
            'id':
            'build-2345',
            'test_id': ('ninja://chrome/test:telemetry_gpu_integration_test/'
                        'gpu_tests.webgl_conformance_integration_test.'
                        'WebGLConformanceIntegrationTest.test_name'),
            'status':
            'FAIL',
            'typ_expectations': [
                'RetryOnFailure',
            ],
            'typ_tags': [
                'webgl-version-2',
            ],
            'step_name':
            'step_name',
        },
    ]
    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance1')
    self._popen_mock.return_value = uu.FakeProcess(
        stdout=json.dumps(query_results))
    results, expectation_files = querier.QueryBuilder('builder', 'ci')
    self.assertEqual(len(results), 1)
    self.assertIsNone(expectation_files)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['webgl-version-1'], 'Failure',
                          'step_name', '1234'))

    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance2')
    results, expectation_files = querier.QueryBuilder('builder', 'ci')
    self.assertEqual(len(results), 1)
    self.assertIsNone(expectation_files)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['webgl-version-2'], 'Failure',
                          'step_name', '2345'))

  def testSuiteExceptionMap(self):
    """Tests that the suite passed to the query changes for some suites."""

    def assertSuiteInQuery(suite, call_args):
      query = call_args[0][0][0]
      s = 'r"gpu_tests\\.%s\\."' % suite
      self.assertIn(s, query)

    # Non-special cased suite.
    querier = gpu_uu.CreateGenericGpuQuerier()
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder('builder', 'ci')
      assertSuiteInQuery('pixel_integration_test', query_mock.call_args)

    # Special-cased suites.
    querier = gpu_uu.CreateGenericGpuQuerier(suite='info_collection')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder('builder', 'ci')
      assertSuiteInQuery('info_collection_test', query_mock.call_args)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='power')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder('builder', 'ci')
      assertSuiteInQuery('power_measurement_integration_test',
                         query_mock.call_args)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='trace_test')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder('builder', 'ci')
      assertSuiteInQuery('trace_integration_test', query_mock.call_args)


class GetQueryGeneratorForBuilderUnittest(unittest.TestCase):
  def setUp(self):
    self._querier = gpu_uu.CreateGenericGpuQuerier()
    self._query_patcher = mock.patch.object(
        self._querier, '_RunBigQueryCommandsForJsonOutput')
    self._query_mock = self._query_patcher.start()
    self.addCleanup(self._query_patcher.stop)

  def testNoLargeQueryMode(self):
    """Tests that the expected clause is returned in normal mode."""
    test_filter = self._querier._GetQueryGeneratorForBuilder('', '')
    self.assertEqual(len(test_filter.GetClauses()), 1)
    self.assertEqual(
        test_filter.GetClauses()[0], """\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.pixel_integration_test\.")""")
    self.assertIsInstance(test_filter, gpu_queries.GpuFixedQueryGenerator)
    self._query_mock.assert_not_called()

  def testLargeQueryModeNoTests(self):
    """Tests that a special value is returned if no tests are found."""
    querier = gpu_uu.CreateGenericGpuQuerier(large_query_mode=True)
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput',
                           return_value=[]) as query_mock:
      test_filter = querier._GetQueryGeneratorForBuilder('', '')
      self.assertIsNone(test_filter)
      query_mock.assert_called_once()

  def testLargeQueryModeFoundTests(self):
    """Tests that a clause containing found tests is returned."""
    querier = gpu_uu.CreateGenericGpuQuerier(large_query_mode=True)
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      query_mock.return_value = [{
          'test_id': 'foo_test'
      }, {
          'test_id': 'bar_test'
      }]
      test_filter = querier._GetQueryGeneratorForBuilder('', '')
      self.assertEqual(test_filter.GetClauses(),
                       ['AND test_id IN UNNEST(["foo_test", "bar_test"])'])
      self.assertIsInstance(test_filter, gpu_queries.GpuSplitQueryGenerator)


class GetSuiteFilterClauseUnittest(unittest.TestCase):
  def testNonWebGl(self):
    """Tests that no filter is returned for non-WebGL suites."""
    for suite in [
        'context_lost',
        'depth_capture',
        'hardware_accelerated_feature',
        'gpu_process',
        'info_collection',
        'maps',
        'pixel',
        'power',
        'screenshot_sync',
        'trace_test',
    ]:
      querier = gpu_uu.CreateGenericGpuQuerier(suite=suite)
      self.assertEqual(querier._GetSuiteFilterClause(), '')

  def testWebGl(self):
    """Tests that filters are returned for WebGL suites."""
    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance1')
    expected_filter = 'AND "webgl-version-1" IN UNNEST(typ_tags)'
    self.assertEqual(querier._GetSuiteFilterClause(), expected_filter)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl_conformance2')
    expected_filter = 'AND "webgl-version-2" IN UNNEST(typ_tags)'
    self.assertEqual(querier._GetSuiteFilterClause(), expected_filter)


class HelperMethodUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = gpu_uu.CreateGenericGpuQuerier()

  def testStripPrefixFromTestIdValidId(self):
    test_name = 'conformance/programs/program-handling.html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_tests.webgl_conformance_integration_test.'
              'WebGLConformanceIntegrationTest.')
    test_id = prefix + test_name
    self.assertEqual(self.instance._StripPrefixFromTestId(test_id), test_name)

  def testStripPrefixFromTestIdInvalidId(self):
    test_name = 'conformance/programs/program-handling_html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_testse.webgl_conformance_integration_test.')
    test_id = prefix + test_name
    with self.assertRaises(AssertionError):
      self.instance._StripPrefixFromTestId(test_id)


if __name__ == '__main__':
  unittest.main(verbosity=2)
