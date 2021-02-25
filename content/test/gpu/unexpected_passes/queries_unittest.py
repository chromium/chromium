# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import subprocess
import unittest

import mock

from unexpected_passes import data_types
from unexpected_passes import multiprocessing_utils
from unexpected_passes import queries
from unexpected_passes import unittest_utils


class AddResultToMapUnittest(unittest.TestCase):
  def testResultMatchPassingNew(self):
    """Test adding a passing result when no results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    expectation_map = {
        'some/test/*': {
            e: {},
        },
    }
    found_matching = queries._AddResultToMap(r, 'builder', expectation_map)
    self.assertTrue(found_matching)
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    expected_expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchFailingNew(self):
    """Test adding a failing result when no results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Failure',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    expectation_map = {
        'some/test/*': {
            e: {},
        },
    }
    found_matching = queries._AddResultToMap(r, 'builder', expectation_map)
    self.assertTrue(found_matching)
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id')
    expected_expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            }
        }
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchPassingExisting(self):
    """Test adding a passing result when results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id')
    expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    found_matching = queries._AddResultToMap(r, 'builder', expectation_map)
    self.assertTrue(found_matching)
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id')
    stats.AddPassedBuild()
    expected_expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchFailingExisting(self):
    """Test adding a failing result when results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Failure',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    found_matching = queries._AddResultToMap(r, 'builder', expectation_map)
    self.assertTrue(found_matching)
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id')
    stats.AddPassedBuild()
    expected_expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchMultiMatch(self):
    """Test adding a passing result when multiple expectations match."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    e2 = data_types.Expectation('some/test/case', ['win10'], 'Failure')
    expectation_map = {
        'some/test/*': {
            e: {},
            e2: {},
        },
    }
    found_matching = queries._AddResultToMap(r, 'builder', expectation_map)
    self.assertTrue(found_matching)
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    expected_expectation_map = {
        'some/test/*': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
            e2: {
                'builder': {
                    'pixel_tests': stats,
                },
            }
        }
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultNoMatch(self):
    """Tests that a result is not added if no match is found."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Failure',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10', 'foo'], 'Failure')
    expectation_map = {'some/test/*': {e: {}}}
    found_matching = queries._AddResultToMap(r, 'builder', expectation_map)
    self.assertFalse(found_matching)
    expected_expectation_map = {'some/test/*': {e: {}}}
    self.assertEqual(expectation_map, expected_expectation_map)


class AddResultListToMapUnittest(unittest.TestCase):
  def GetGenericRetryExpectation(self):
    return data_types.Expectation('foo/test', ['win10'], 'RetryOnFailure')

  def GetGenericFailureExpectation(self):
    return data_types.Expectation('foo/test', ['win10'], 'Failure')

  def GetEmptyMapForGenericRetryExpectation(self):
    foo_expectation = self.GetGenericRetryExpectation()
    return {
        'foo/test': {
            foo_expectation: {},
        },
    }

  def GetEmptyMapForGenericFailureExpectation(self):
    foo_expectation = self.GetGenericFailureExpectation()
    return {
        'foo/test': {
            foo_expectation: {},
        },
    }

  def GetPassedMapForExpectation(self, expectation):
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    return self.GetMapForExpectationAndStats(expectation, stats)

  def GetFailedMapForExpectation(self, expectation):
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id')
    return self.GetMapForExpectationAndStats(expectation, stats)

  def GetMapForExpectationAndStats(self, expectation, stats):
    return {
        expectation.test: {
            expectation: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }

  def testRetryOnlyPassMatching(self):
    """Tests when the only tests are retry expectations that pass and match."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = queries._AddResultListToMap(expectation_map, 'builder',
                                                    [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testRetryOnlyFailMatching(self):
    """Tests when the only tests are retry expectations that fail and match."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                   'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = queries._AddResultListToMap(expectation_map, 'builder',
                                                    [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetFailedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testRetryFailThenPassMatching(self):
    """Tests when there are pass and fail results for retry expectations."""
    foo_fail_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                        'pixel_tests', 'build_id')
    foo_pass_result = data_types.Result('foo/test', ['win10'], 'Pass',
                                        'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = queries._AddResultListToMap(
        expectation_map, 'builder', [foo_fail_result, foo_pass_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetFailedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testFailurePassMatching(self):
    """Tests when there are pass results for failure expectations."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = queries._AddResultListToMap(expectation_map, 'builder',
                                                    [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testFailureFailureMatching(self):
    """Tests when there are failure results for failure expectations."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                   'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = queries._AddResultListToMap(expectation_map, 'builder',
                                                    [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetFailedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testMismatches(self):
    """Tests that unmatched results get returned."""
    foo_match_result = data_types.Result('foo/test', ['win10'], 'Pass',
                                         'pixel_tests', 'build_id')
    foo_mismatch_result = data_types.Result('foo/not_a_test', ['win10'],
                                            'Failure', 'pixel_tests',
                                            'build_id')
    bar_result = data_types.Result('bar/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = queries._AddResultListToMap(
        expectation_map, 'builder',
        [foo_match_result, foo_mismatch_result, bar_result])
    self.assertEqual(len(set(unmatched_results)), 2)
    self.assertEqual(set(unmatched_results),
                     set([foo_mismatch_result, bar_result]))

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)


class HelperMethodUnittest(unittest.TestCase):
  def testStripPrefixFromBuildIdValidId(self):
    self.assertEqual(queries._StripPrefixFromBuildId('build-1'), '1')

  def testStripPrefixFromBuildIdInvalidId(self):
    with self.assertRaises(AssertionError):
      queries._StripPrefixFromBuildId('build1')
    with self.assertRaises(AssertionError):
      queries._StripPrefixFromBuildId('build-1-2')

  def testStripPrefixFromTestIdValidId(self):
    test_name = 'conformance/programs/program-handling.html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_tests.webgl_conformance_integration_test.'
              'WebGLConformanceIntegrationTest.')
    test_id = prefix + test_name
    self.assertEqual(queries._StripPrefixFromTestId(test_id), test_name)

  def testStripPrefixFromTestIdInvalidId(self):
    test_name = 'conformance/programs/program-handling_html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_testse.webgl_conformance_integration_test.')
    test_id = prefix + test_name
    with self.assertRaises(AssertionError):
      queries._StripPrefixFromTestId(test_id)


class QueryBuilderUnittest(unittest.TestCase):
  def setUp(self):
    self._patcher = mock.patch.object(subprocess, 'Popen')
    self._popen_mock = self._patcher.start()
    self.addCleanup(self._patcher.stop)

  def testQueryFailureRaised(self):
    """Tests that a query failure is properly surfaced."""
    self._popen_mock.return_value = unittest_utils.FakeProcess(returncode=1)
    with self.assertRaises(RuntimeError):
      queries.QueryBuilder('builder', 'ci', 'pixel', 'project', 5, False)

  def testInvalidNumSamples(self):
    """Tests that the number of samples is validated."""
    with self.assertRaises(AssertionError):
      queries.QueryBuilder('builder', 'ci', 'pixel', 'project', -1, False)
    self._popen_mock.assert_not_called()

  def testNoResults(self):
    """Tests functionality if the query returns no results."""
    self._popen_mock.return_value = unittest_utils.FakeProcess(stdout='[]')
    builder, results = queries.QueryBuilder('builder', 'ci', 'pixel', 'project',
                                            5, False)
    self.assertEqual(builder, 'builder')
    self.assertEqual(results, [])

  def testValidResults(self):
    """Tests functionality when valid results are returned."""
    query_results = [
        {
            'id':
            'build-1234',
            'test_id': ('ninja://chrome/test:telemetry_gpu_integration_test/'
                        'gpu_tests.pixel_integration_test.'
                        'PixelIntegrationTest.test_name'),
            'status':
            'FAIL',
            'typ_expectations': [
                'RetryOnFailure',
            ],
            'typ_tags': [
                'win',
                'intel',
            ],
            'step_name':
            'step_name',
        },
    ]
    self._popen_mock.return_value = unittest_utils.FakeProcess(
        stdout=json.dumps(query_results))
    builder, results = queries.QueryBuilder('builder', 'ci', 'pixel', 'project',
                                            5, False)
    self.assertEqual(builder, 'builder')
    self.assertEqual(len(results), 1)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['win', 'intel'], 'Failure', 'step_name',
                          '1234'))

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
    self._popen_mock.return_value = unittest_utils.FakeProcess(
        stdout=json.dumps(query_results))
    _, results = queries.QueryBuilder('builder', 'ci', 'webgl_conformance1',
                                      'project', 5, False)
    self.assertEqual(len(results), 1)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['webgl-version-1'], 'Failure',
                          'step_name', '1234'))

    _, results = queries.QueryBuilder('builder', 'ci', 'webgl_conformance2',
                                      'project', 5, False)
    self.assertEqual(len(results), 1)
    self.assertEqual(
        results[0],
        data_types.Result('test_name', ['webgl-version-2'], 'Failure',
                          'step_name', '2345'))

  def testSuiteExceptionMap(self):
    """Tests that the suite passed to the query changes for some suites."""
    def assertSuiteInQuery(suite, call_args):
      query = call_args[0][0]
      s = 'r"gpu_tests\\.%s\\."' % suite
      self.assertIn(s, query)

    with mock.patch.object(queries,
                           '_RunBigQueryCommandForJsonOutput') as query_mock:
      # Non-special cased suite.
      _, _ = queries.QueryBuilder('builder', 'ci', 'pixel', 'project', 5, False)
      assertSuiteInQuery('pixel_integration_test', query_mock.call_args)

      # Special-cased suites.
      _, _ = queries.QueryBuilder('builder', 'ci', 'info_collection', 'project',
                                  5, False)
      assertSuiteInQuery('info_collection_test', query_mock.call_args)
      _, _ = queries.QueryBuilder('builder', 'ci', 'power', 'project', 5, False)
      assertSuiteInQuery('power_measurement_integration_test',
                         query_mock.call_args)

      _, _ = queries.QueryBuilder('builder', 'ci', 'trace_test', 'project', 5,
                                  False)
      assertSuiteInQuery('trace_integration_test', query_mock.call_args)

  def testFilterInsertion(self):
    """Tests that test filters are properly inserted into the query."""
    with mock.patch.object(
        queries, '_GetTestFilterClauseForBuilder',
        return_value='a real filter'), mock.patch.object(
            queries, '_RunBigQueryCommandForJsonOutput') as query_mock:
      queries.QueryBuilder('builder', 'ci', 'info_collection', 'project', 5,
                           False)
      query_mock.assert_called_once()
      query = query_mock.call_args[0][0]
      self.assertIn(
          """\
        AND STRUCT("builder", @builder_name) IN UNNEST(variant)
        a real filter
      GROUP BY exported.id""", query)

  def testEarlyReturnOnNoFilter(self):
    """Tests that the absence of a test filter results in an early return."""
    with mock.patch.object(
        queries, '_GetTestFilterClauseForBuilder',
        return_value=None), mock.patch.object(
            queries, '_RunBigQueryCommandForJsonOutput') as query_mock:
      builder, results = queries.QueryBuilder('builder', 'ci',
                                              'info_collection', 'project', 5,
                                              False)
      query_mock.assert_not_called()
      self.assertEqual(builder, 'builder')
      self.assertEqual(results, [])


class FillExpectationMapForBuildersUnittest(unittest.TestCase):
  def setUp(self):
    self._patcher = mock.patch.object(queries, 'QueryBuilder')
    self._query_mock = self._patcher.start()
    self.addCleanup(self._patcher.stop)
    self._pool_patcher = mock.patch.object(multiprocessing_utils,
                                           'GetProcessPool')
    self._pool_mock = self._pool_patcher.start()
    self._pool_mock.return_value = unittest_utils.FakePool()
    self.addCleanup(self._pool_patcher.stop)
    self._sleep_patcher = mock.patch.object(queries,
                                            'ASYNC_RESULT_SLEEP_DURATION',
                                            return_value=0.1)
    self._sleep_mock = self._sleep_patcher.start()
    self.addCleanup(self._sleep_patcher.stop)

  def testValidResults(self):
    """Tests functionality when valid results are returned by the query."""

    def SideEffect(builder, *args):
      del args
      if builder == 'matched_builder':
        return builder, [
            data_types.Result('foo', ['win'], 'Pass', 'step_name', 'build_id')
        ]
      else:
        return builder, [
            data_types.Result('bar', [], 'Pass', 'step_name', 'build_id')
        ]

    self._query_mock.side_effect = SideEffect

    expectation = data_types.Expectation('foo', ['win'], 'RetryOnFailure')
    expectation_map = {
        'foo': {
            expectation: {},
        },
    }
    unmatched_results = queries._FillExpectationMapForBuilders(
        expectation_map, ['matched_builder', 'unmatched_builder'], 'ci',
        'pixel', 'project', 5, False)
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    expected_expectation_map = {
        'foo': {
            expectation: {
                'ci:matched_builder': {
                    'step_name': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertEqual(
        unmatched_results, {
            'ci:unmatched_builder': [
                data_types.Result('bar', [], 'Pass', 'step_name', 'build_id'),
            ],
        })

  def testQueryFailureIsSurfaced(self):
    """Tests that a query failure is properly surfaced despite being async."""
    self._query_mock.side_effect = IndexError('failure')
    with self.assertRaises(IndexError):
      queries._FillExpectationMapForBuilders({}, ['matched_builder'], 'ci',
                                             'pixel', 'project', 5, False)


class MergeExpectationMapsUnittest(unittest.TestCase):
  maxDiff = None

  def testEmptyBaseMap(self):
    """Tests that a merge with an empty base map copies the merge map."""
    base_map = {}
    merge_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    original_merge_map = copy.deepcopy(merge_map)
    queries._MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, merge_map)
    self.assertEqual(merge_map, original_merge_map)

  def testEmptyMergeMap(self):
    """Tests that a merge with an empty merge map is a no-op."""
    base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    merge_map = {}
    original_base_map = copy.deepcopy(base_map)
    queries._MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, original_base_map)
    self.assertEqual(merge_map, {})

  def testMissingKeys(self):
    """Tests that missing keys are properly copied to the base map."""
    base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    merge_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step2': data_types.BuildStats(),
                },
                'builder2': {
                    'step': data_types.BuildStats(),
                },
            },
            data_types.Expectation('foo', ['mac'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                }
            }
        },
        'bar': {
            data_types.Expectation('bar', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    expected_base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                    'step2': data_types.BuildStats(),
                },
                'builder2': {
                    'step': data_types.BuildStats(),
                },
            },
            data_types.Expectation('foo', ['mac'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                }
            }
        },
        'bar': {
            data_types.Expectation('bar', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    queries._MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, expected_base_map)

  def testMergeBuildStats(self):
    """Tests that BuildStats for the same step are merged properly."""
    base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    merge_stats = data_types.BuildStats()
    merge_stats.AddFailedBuild('1')
    merge_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': merge_stats,
                },
            },
        },
    }
    expected_stats = data_types.BuildStats()
    expected_stats.AddFailedBuild('1')
    expected_base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': expected_stats,
                },
            },
        },
    }
    queries._MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, expected_base_map)

  def testInvalidMerge(self):
    """Tests that updating a BuildStats instance twice is an error."""
    base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    merge_stats = data_types.BuildStats()
    merge_stats.AddFailedBuild('1')
    merge_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': merge_stats,
                },
            },
        },
    }
    original_base_map = copy.deepcopy(base_map)
    queries._MergeExpectationMaps(base_map, merge_map, original_base_map)
    with self.assertRaises(AssertionError):
      queries._MergeExpectationMaps(base_map, merge_map, original_base_map)


class GetTestFilterClauseForBuilderUnittest(unittest.TestCase):
  def setUp(self):
    self._query_patcher = mock.patch.object(queries,
                                            '_RunBigQueryCommandForJsonOutput')
    self._query_mock = self._query_patcher.start()
    self.addCleanup(self._query_patcher.stop)

  def testNoLargeQueryMode(self):
    """Tests that the expected clause is returned in normal mode."""
    clause = queries._GetTestFilterClauseForBuilder('', '', 'foo_suite', '',
                                                    False)
    self.assertEqual(
        clause, """\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.foo_suite\.")""")
    self._query_mock.assert_not_called()

  def testLargeQueryModeNoTests(self):
    """Tests that a special value is returned if no tests are found."""
    self._query_mock.return_value = []
    clause = queries._GetTestFilterClauseForBuilder('', '', '', '', True)
    self.assertIsNone(clause)
    self._query_mock.assert_called_once()

  def testLargeQueryModeFoundTests(self):
    """Tests that a clause containing found tests is returned."""
    self._query_mock.return_value = [{
        'test_id': 'foo_test'
    }, {
        'test_id': 'bar_test'
    }]
    clause = queries._GetTestFilterClauseForBuilder('', '', '', '', True)
    self.assertEqual(clause, 'AND test_id IN UNNEST(["foo_test", "bar_test"])')


class RunBigQueryCommandForJsonOutputUnittest(unittest.TestCase):
  def setUp(self):
    self._popen_patcher = mock.patch.object(subprocess, 'Popen')
    self._popen_mock = self._popen_patcher.start()
    self.addCleanup(self._popen_patcher.stop)

  def testJsonReturned(self):
    """Tests that valid JSON parsed from stdout is returned."""
    query_output = {'foo': 'bar'}
    self._popen_mock.return_value = unittest_utils.FakeProcess(
        stdout=json.dumps(query_output))
    result = queries._RunBigQueryCommandForJsonOutput('', '', {})
    self.assertEqual(result, query_output)
    self._popen_mock.assert_called_once()

  def testExceptionRaisedOnFailure(self):
    """Tests that an exception is raised if the query fails."""
    self._popen_mock.return_value = unittest_utils.FakeProcess(returncode=1)
    with self.assertRaises(RuntimeError):
      queries._RunBigQueryCommandForJsonOutput('', '', {})


class GenerateBigQueryCommandUnittest(unittest.TestCase):
  def testNoParametersSpecified(self):
    """Tests that no parameters are added if none are specified."""
    cmd = queries._GenerateBigQueryCommand('project', {})
    for element in cmd:
      self.assertFalse(element.startswith('--parameter'))

  def testParameterAddition(self):
    """Tests that specified parameters are added appropriately."""
    cmd = queries._GenerateBigQueryCommand('project', {
        '': {
            'string': 'string_value'
        },
        'INT64': {
            'int': 1
        }
    })
    self.assertIn('--parameter=string::string_value', cmd)
    self.assertIn('--parameter=int:INT64:1', cmd)


if __name__ == '__main__':
  unittest.main(verbosity=2)
