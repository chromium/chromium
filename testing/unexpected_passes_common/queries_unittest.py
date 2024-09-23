#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Iterable, Optional
import unittest
from unittest import mock

from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types
from unexpected_passes_common import expectations
from unexpected_passes_common import queries
from unexpected_passes_common import unittest_utils as uu

# Protected access is allowed for unittests.
# pylint: disable=protected-access

class HelperMethodUnittest(unittest.TestCase):
  def testStripPrefixFromBuildIdValidId(self) -> None:
    self.assertEqual(queries._StripPrefixFromBuildId('build-1'), '1')

  def testStripPrefixFromBuildIdInvalidId(self) -> None:
    with self.assertRaises(AssertionError):
      queries._StripPrefixFromBuildId('build1')
    with self.assertRaises(AssertionError):
      queries._StripPrefixFromBuildId('build-1-2')

  def testConvertActualResultToExpectationFileFormatAbort(self) -> None:
    self.assertEqual(
        queries._ConvertActualResultToExpectationFileFormat('ABORT'), 'Timeout')


class BigQueryQuerierInitUnittest(unittest.TestCase):

  def testInvalidNumSamples(self):
    """Tests that the number of samples is validated."""
    with self.assertRaises(AssertionError):
      uu.CreateGenericQuerier(num_samples=-1)

  def testDefaultSamples(self):
    """Tests that the number of samples is set to a default if not provided."""
    querier = uu.CreateGenericQuerier(num_samples=0)
    self.assertGreater(querier._num_samples, 0)


class GetBuilderGroupedQueryResultsUnittest(unittest.TestCase):

  def setUp(self):
    builders.ClearInstance()
    expectations.ClearInstance()
    uu.RegisterGenericBuildersImplementation()
    uu.RegisterGenericExpectationsImplementation()
    self._querier = uu.CreateGenericQuerier()

  def testUnknownBuilderType(self):
    """Tests behavior when an unknown builder type is provided."""
    with self.assertRaisesRegex(RuntimeError, 'Unknown builder type unknown'):
      for _ in self._querier.GetBuilderGroupedQueryResults('unknown', False):
        pass

  def testQueryRouting(self):
    """Tests that the correct query is used based on inputs."""
    with mock.patch.object(self._querier,
                           '_GetPublicCiQuery',
                           return_value='public_ci') as public_ci_mock:
      with mock.patch.object(self._querier,
                             '_GetInternalCiQuery',
                             return_value='internal_ci') as internal_ci_mock:
        with mock.patch.object(self._querier,
                               '_GetPublicTryQuery',
                               return_value='public_try') as public_try_mock:
          with mock.patch.object(
              self._querier,
              '_GetInternalTryQuery',
              return_value='internal_try') as internal_try_mock:
            all_mocks = [
                public_ci_mock,
                internal_ci_mock,
                public_try_mock,
                internal_try_mock,
            ]
            inputs = [
                (constants.BuilderTypes.CI, False, public_ci_mock),
                (constants.BuilderTypes.CI, True, internal_ci_mock),
                (constants.BuilderTypes.TRY, False, public_try_mock),
                (constants.BuilderTypes.TRY, True, internal_try_mock),
            ]
            for builder_type, internal_status, called_mock in inputs:
              for _ in self._querier.GetBuilderGroupedQueryResults(
                  builder_type, internal_status):
                pass
              for m in all_mocks:
                if m == called_mock:
                  m.assert_called_once()
                else:
                  m.assert_not_called()
              for m in all_mocks:
                m.reset_mock()

  def testNoResults(self):
    """Tests functionality if the query returns no results."""
    returned_builders = []
    with self.assertLogs(level='WARNING') as log_manager:
      with mock.patch.object(self._querier,
                             '_GetPublicCiQuery',
                             return_value=''):
        for builder_name, _, _ in self._querier.GetBuilderGroupedQueryResults(
            constants.BuilderTypes.CI, False):
          returned_builders.append(builder_name)
      for message in log_manager.output:
        if ('Did not get any results for builder type ci and internal status '
            'False. Depending on where tests are run and how frequently '
            'trybots are used for submission, this may be benign') in message:
          break
      else:
        self.fail('Did not find expected log message: %s' % log_manager.output)
      self.assertEqual(len(returned_builders), 0)

  def testHappyPath(self):
    """Tests functionality in the happy path."""
    self._querier.query_results = [
        uu.FakeQueryResult(builder_name='builder_a',
                           id_='build-a',
                           test_id='test_a',
                           status='PASS',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a'),
        uu.FakeQueryResult(builder_name='builder_b',
                           id_='build-b',
                           test_id='test_b',
                           status='FAIL',
                           typ_tags=['win'],
                           step_name='step_b'),
    ]

    expected_results = [
        ('builder_a',
         [data_types.BaseResult('test_a', ('linux', ), 'Pass', 'step_a',
                                'a')], None),
        ('builder_b',
         [data_types.BaseResult('test_b', ('win', ), 'Failure', 'step_b',
                                'b')], None),
    ]

    results = []
    with mock.patch.object(self._querier, '_GetPublicCiQuery', return_value=''):
      for builder_name, result_list, expectation_files in (
          self._querier.GetBuilderGroupedQueryResults(constants.BuilderTypes.CI,
                                                      False)):
        results.append((builder_name, result_list, expectation_files))

    self.assertEqual(results, expected_results)

  def testHappyPathWithExpectationFiles(self):
    """Tests functionality in the happy path with expectation files provided."""
    self._querier.query_results = [
        uu.FakeQueryResult(builder_name='builder_a',
                           id_='build-a',
                           test_id='test_a',
                           status='PASS',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a'),
        uu.FakeQueryResult(builder_name='builder_b',
                           id_='build-b',
                           test_id='test_b',
                           status='FAIL',
                           typ_tags=['win'],
                           step_name='step_b'),
    ]

    expected_results = [
        ('builder_a',
         [data_types.BaseResult('test_a', ('linux', ), 'Pass', 'step_a',
                                'a')], list(set(['ef_a']))),
        ('builder_b',
         [data_types.BaseResult('test_b', ('win', ), 'Failure', 'step_b',
                                'b')], list(set(['ef_b', 'ef_c']))),
    ]

    results = []
    with mock.patch.object(self._querier,
                           '_GetRelevantExpectationFilesForQueryResult',
                           side_effect=(['ef_a'], ['ef_b', 'ef_c'])):
      with mock.patch.object(self._querier,
                             '_GetPublicCiQuery',
                             return_value=''):
        for builder_name, result_list, expectation_files in (
            self._querier.GetBuilderGroupedQueryResults(
                constants.BuilderTypes.CI, False)):
          results.append((builder_name, result_list, expectation_files))

    self.assertEqual(results, expected_results)


class FillExpectationMapForBuildersUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._querier = uu.CreateGenericQuerier()

    expectations.ClearInstance()
    uu.RegisterGenericExpectationsImplementation()

  def testErrorOnMixedBuilders(self) -> None:
    """Tests that providing builders of mixed type is an error."""
    builders_to_fill = [
        data_types.BuilderEntry('ci_builder', constants.BuilderTypes.CI, False),
        data_types.BuilderEntry('try_builder', constants.BuilderTypes.TRY,
                                False)
    ]
    with self.assertRaises(AssertionError):
      self._querier.FillExpectationMapForBuilders(
          data_types.TestExpectationMap({}), builders_to_fill)

  def _runValidResultsTest(self, keep_unmatched_results: bool) -> None:
    self._querier = uu.CreateGenericQuerier(
        keep_unmatched_results=keep_unmatched_results)

    public_results = [
        uu.FakeQueryResult(builder_name='matched_builder',
                           id_='build-build_id',
                           test_id='foo',
                           status='PASS',
                           typ_tags=['win'],
                           step_name='step_name'),
        uu.FakeQueryResult(builder_name='unmatched_builder',
                           id_='build-build_id',
                           test_id='bar',
                           status='PASS',
                           typ_tags=[],
                           step_name='step_name'),
        uu.FakeQueryResult(builder_name='extra_builder',
                           id_='build-build_id',
                           test_id='foo',
                           status='PASS',
                           typ_tags=['win'],
                           step_name='step_name'),
    ]

    internal_results = [
        uu.FakeQueryResult(builder_name='matched_internal',
                           id_='build-build_id',
                           test_id='foo',
                           status='PASS',
                           typ_tags=['win'],
                           step_name='step_name_internal'),
        uu.FakeQueryResult(builder_name='unmatched_internal',
                           id_='build-build_id',
                           test_id='bar',
                           status='PASS',
                           typ_tags=[],
                           step_name='step_name_internal'),
    ]

    builders_to_fill = [
        data_types.BuilderEntry('matched_builder', constants.BuilderTypes.CI,
                                False),
        data_types.BuilderEntry('unmatched_builder', constants.BuilderTypes.CI,
                                False),
        data_types.BuilderEntry('matched_internal', constants.BuilderTypes.CI,
                                True),
        data_types.BuilderEntry('unmatched_internal', constants.BuilderTypes.CI,
                                True),
    ]

    expectation = data_types.Expectation('foo', ['win'], 'RetryOnFailure')
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            expectation:
            data_types.BuilderStepMap(),
        }),
    })

    def PublicSideEffect():
      self._querier.query_results = public_results
      return ''

    def InternalSideEffect():
      self._querier.query_results = internal_results
      return ''

    with self.assertLogs(level='WARNING') as log_manager:
      with mock.patch.object(self._querier,
                             '_GetPublicCiQuery',
                             side_effect=PublicSideEffect) as public_mock:
        with mock.patch.object(self._querier,
                               '_GetInternalCiQuery',
                               side_effect=InternalSideEffect) as internal_mock:
          unmatched_results = self._querier.FillExpectationMapForBuilders(
              expectation_map, builders_to_fill)
          public_mock.assert_called_once()
          internal_mock.assert_called_once()

      for message in log_manager.output:
        if ('Did not find a matching builder for name extra_builder and '
            'internal status False. This is normal if the builder is no longer '
            'running tests (e.g. it was experimental).') in message:
          break
      else:
        self.fail('Did not find expected log message')

    stats = data_types.BuildStats()
    stats.AddPassedBuild(frozenset(['win']))
    expected_expectation_map = {
        'foo': {
            expectation: {
                'chromium/ci:matched_builder': {
                    'step_name': stats,
                },
                'chrome/ci:matched_internal': {
                    'step_name_internal': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)
    if keep_unmatched_results:
      self.assertEqual(
          unmatched_results, {
              'chromium/ci:unmatched_builder': [
                  data_types.Result('bar', [], 'Pass', 'step_name', 'build_id'),
              ],
              'chrome/ci:unmatched_internal': [
                  data_types.Result('bar', [], 'Pass', 'step_name_internal',
                                    'build_id'),
              ],
          })
    else:
      self.assertEqual(unmatched_results, {})

  def testValidResultsKeepUnmatched(self) -> None:
    """Tests behavior w/ valid results and keeping unmatched results."""
    self._runValidResultsTest(True)

  def testValidResultsDoNotKeepUnmatched(self) -> None:
    """Tests behavior w/ valid results and not keeping unmatched results."""
    self._runValidResultsTest(False)


class ProcessRowsForBuilderUnittest(unittest.TestCase):

  def setUp(self):
    self._querier = uu.CreateGenericQuerier()

  def testHappyPathWithExpectationFiles(self):
    """Tests functionality along the happy path with expectation files."""

    def SideEffect(row: queries.QueryResult) -> Optional[Iterable[str]]:
      if row.step_name == 'step_a1':
        return ['ef_a1']
      if row.step_name == 'step_a2':
        return ['ef_a2']
      if row.step_name == 'step_b':
        return ['ef_b1', 'ef_b2']
      raise RuntimeError('Unexpected row')

    rows = [
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-a',
                           test_id='test_a',
                           status='PASS',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a1'),
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-a',
                           test_id='test_a',
                           status='FAIL',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a2'),
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-b',
                           test_id='test_b',
                           status='FAIL',
                           typ_tags=['win'],
                           step_name='step_b'),
    ]

    # Reversed order is expected since results are popped.
    expected_results = [
        data_types.BaseResult(test='test_b',
                              tags=['win'],
                              actual_result='Failure',
                              step='step_b',
                              build_id='b'),
        data_types.BaseResult(test='test_a',
                              tags=['linux'],
                              actual_result='Failure',
                              step='step_a2',
                              build_id='a'),
        data_types.BaseResult(test='test_a',
                              tags=['linux'],
                              actual_result='Pass',
                              step='step_a1',
                              build_id='a'),
    ]

    with mock.patch.object(self._querier,
                           '_GetRelevantExpectationFilesForQueryResult',
                           side_effect=SideEffect):
      results, expectation_files = self._querier._ProcessRowsForBuilder(rows)
    self.assertEqual(results, expected_results)
    self.assertEqual(len(expectation_files), len(set(expectation_files)))
    self.assertEqual(set(expectation_files),
                     set(['ef_a1', 'ef_a2', 'ef_b1', 'ef_b2']))

  def testHappyPathNoneExpectation(self):
    """Tests functionality along the happy path with a None expectation file."""

    # A single None expectation file should cause the resulting return value to
    # become None.
    def SideEffect(row: queries.QueryResult) -> Optional[Iterable[str]]:
      if row.step_name == 'step_a1':
        return ['ef_a1']
      if row.step_name == 'step_a2':
        return ['ef_a2']
      return None

    rows = [
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-a',
                           test_id='test_a',
                           status='PASS',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a1'),
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-a',
                           test_id='test_a',
                           status='FAIL',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a2'),
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-b',
                           test_id='test_b',
                           status='FAIL',
                           typ_tags=['win'],
                           step_name='step_b'),
    ]

    # Reversed order is expected since results are popped.
    expected_results = [
        data_types.BaseResult(test='test_b',
                              tags=['win'],
                              actual_result='Failure',
                              step='step_b',
                              build_id='b'),
        data_types.BaseResult(test='test_a',
                              tags=['linux'],
                              actual_result='Failure',
                              step='step_a2',
                              build_id='a'),
        data_types.BaseResult(test='test_a',
                              tags=['linux'],
                              actual_result='Pass',
                              step='step_a1',
                              build_id='a'),
    ]

    with mock.patch.object(self._querier,
                           '_GetRelevantExpectationFilesForQueryResult',
                           side_effect=SideEffect):
      results, expectation_files = self._querier._ProcessRowsForBuilder(rows)
    self.assertEqual(results, expected_results)
    self.assertEqual(expectation_files, None)

  def testHappyPathSkippedResult(self):
    """Tests functionality along the happy path with a skipped result."""

    def SideEffect(row: queries.QueryResult) -> bool:
      if row.step_name == 'step_b':
        return True
      return False

    rows = [
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-a',
                           test_id='test_a',
                           status='PASS',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a1'),
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-a',
                           test_id='test_a',
                           status='FAIL',
                           typ_tags=['linux', 'unknown_tag'],
                           step_name='step_a2'),
        uu.FakeQueryResult(builder_name='unused',
                           id_='build-b',
                           test_id='test_b',
                           status='FAIL',
                           typ_tags=['win'],
                           step_name='step_b'),
    ]

    # Reversed order is expected since results are popped.
    expected_results = [
        data_types.BaseResult(test='test_a',
                              tags=['linux'],
                              actual_result='Failure',
                              step='step_a2',
                              build_id='a'),
        data_types.BaseResult(test='test_a',
                              tags=['linux'],
                              actual_result='Pass',
                              step='step_a1',
                              build_id='a'),
    ]

    with mock.patch.object(self._querier,
                           '_ShouldSkipOverResult',
                           side_effect=SideEffect):
      results, expectation_files = self._querier._ProcessRowsForBuilder(rows)
    self.assertEqual(results, expected_results)
    self.assertEqual(expectation_files, None)


if __name__ == '__main__':
  unittest.main(verbosity=2)
