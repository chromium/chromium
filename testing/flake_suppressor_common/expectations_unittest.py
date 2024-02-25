#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import datetime
import os
import sys
import tempfile
import unittest
import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest  # pylint:disable=import-error

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import expectations
from flake_suppressor_common import unittest_utils as uu


# Note for all tests in this class: We can safely check the contents of the file
# at the end despite potentially having multiple added lines because Python 3.7+
# guarantees that dictionaries remember insertion order, so there is no risk of
# the order of modification changing.
@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class IterateThroughResultsForUserUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self._new_stdout = open(os.devnull, 'w')
    self.setUpPyfakefs()
    self._expectations = uu.UnitTestExpectationProcessor()
    # Redirect stdout since the tested function prints a lot.
    self._old_stdout = sys.stdout
    sys.stdout = self._new_stdout

    self._input_patcher = mock.patch.object(expectations.ExpectationProcessor,
                                            'PromptUserForExpectationAction')
    self._input_mock = self._input_patcher.start()
    self.addCleanup(self._input_patcher.stop)

    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win']): ['a'],
                tuple(['mac']): ['b'],
            },
            'bar_test': {
                tuple(['win']): ['c'],
            },
        },
    }

    self.expectation_file = os.path.join(uu.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                         'pixel_expectations.txt')
    uu.CreateFile(self, self.expectation_file)
    expectation_file_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    self._expectation_file_patcher = mock.patch.object(
        uu.UnitTestExpectationProcessor, 'GetExpectationFileForSuite')
    self._expectation_file_mock = self._expectation_file_patcher.start()
    self._expectation_file_mock.return_value = self.expectation_file
    self.addCleanup(self._expectation_file_patcher.stop)

  def tearDown(self) -> None:
    sys.stdout = self._old_stdout
    self._new_stdout.close()

  def testIterateThroughResultsForUserIgnoreNoGroupByTags(self) -> None:
    """Tests that everything appears to function with ignore and no group."""
    self._input_mock.return_value = (None, None)
    self._expectations.IterateThroughResultsForUser(self.result_map, False,
                                                    True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserIgnoreGroupByTags(self) -> None:
    """Tests that everything appears to function with ignore and grouping."""
    self._input_mock.return_value = (None, None)
    self._expectations.IterateThroughResultsForUser(self.result_map, True, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserRetryNoGroupByTags(self) -> None:
    """Tests that everything appears to function with retry and no group."""
    self._input_mock.return_value = ('RetryOnFailure', '')
    self._expectations.IterateThroughResultsForUser(self.result_map, False,
                                                    True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
[ win ] foo_test [ RetryOnFailure ]
[ mac ] foo_test [ RetryOnFailure ]
[ win ] bar_test [ RetryOnFailure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserRetryGroupByTags(self) -> None:
    """Tests that everything appears to function with retry and grouping."""
    self._input_mock.return_value = ('RetryOnFailure', 'crbug.com/1')
    self._expectations.IterateThroughResultsForUser(self.result_map, True, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
crbug.com/1 [ win ] foo_test [ RetryOnFailure ]
crbug.com/1 [ win ] bar_test [ RetryOnFailure ]
[ mac ] some_test [ Failure ]
crbug.com/1 [ mac ] foo_test [ RetryOnFailure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserFailNoGroupByTags(self) -> None:
    """Tests that everything appears to function with failure and no group."""
    self._input_mock.return_value = ('Failure', 'crbug.com/1')
    self._expectations.IterateThroughResultsForUser(self.result_map, False,
                                                    True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
crbug.com/1 [ win ] foo_test [ Failure ]
crbug.com/1 [ mac ] foo_test [ Failure ]
crbug.com/1 [ win ] bar_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserFailGroupByTags(self) -> None:
    """Tests that everything appears to function with failure and grouping."""
    self._input_mock.return_value = ('Failure', '')
    self._expectations.IterateThroughResultsForUser(self.result_map, True, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ win ] foo_test [ Failure ]
[ win ] bar_test [ Failure ]
[ mac ] some_test [ Failure ]
[ mac ] foo_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserNoIncludeAllTags(self) -> None:
    """Tests that everything appears to function without including all tags"""
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win', 'win10']): ['a'],
                tuple(['mac']): ['b'],
            },
            'bar_test': {
                tuple(['win']): ['c'],
            },
        },
    }
    self._input_mock.return_value = ('RetryOnFailure', '')
    self._expectations.IterateThroughResultsForUser(self.result_map, False,
                                                    False)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
[ win10 ] foo_test [ RetryOnFailure ]
[ mac ] foo_test [ RetryOnFailure ]
[ win ] bar_test [ RetryOnFailure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class IterateThroughResultsWithThresholdsUnittest(
    fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._expectations = uu.UnitTestExpectationProcessor()
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win']): ['a'],
                tuple(['mac']): ['b'],
            },
            'bar_test': {
                tuple(['win']): ['c'],
            },
        },
    }

    self.expectation_file = os.path.join(uu.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                         'pixel_expectations.txt')
    uu.CreateFile(self, self.expectation_file)
    expectation_file_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    self._expectation_file_patcher = mock.patch.object(
        uu.UnitTestExpectationProcessor, 'GetExpectationFileForSuite')
    self._expectation_file_mock = self._expectation_file_patcher.start()
    self._expectation_file_mock.return_value = self.expectation_file
    self.addCleanup(self._expectation_file_patcher.stop)

  def testGroupByTags(self) -> None:
    """Tests that threshold-based expectations work when grouping by tags."""
    result_counts = {
        tuple(['win']): {
            # We expect this to be ignored since it has a 1% flake rate.
            'foo_test': 100,
            # We expect this to be RetryOnFailure since it has a 25% flake rate.
            'bar_test': 4,
        },
        tuple(['mac']): {
            # We expect this to be Failure since it has a 50% flake rate.
            'foo_test': 2
        }
    }
    self._expectations.IterateThroughResultsWithThresholds(
        self.result_map, True, result_counts, 0.02, 0.5, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ win ] bar_test [ RetryOnFailure ]
[ mac ] some_test [ Failure ]
[ mac ] foo_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testNoGroupByTags(self) -> None:
    """Tests that threshold-based expectations work when not grouping by tags"""
    result_counts = {
        tuple(['win']): {
            # We expect this to be ignored since it has a 1% flake rate.
            'foo_test': 100,
            # We expect this to be RetryOnFailure since it has a 25% flake rate.
            'bar_test': 4,
        },
        tuple(['mac']): {
            # We expect this to be Failure since it has a 50% flake rate.
            'foo_test': 2
        }
    }
    self._expectations.IterateThroughResultsWithThresholds(
        self.result_map, False, result_counts, 0.02, 0.5, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
[ mac ] foo_test [ Failure ]
[ win ] bar_test [ RetryOnFailure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testNoIncludeAllTags(self) -> None:
    """Tests that threshold-based expectations work when filtering tags."""
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win', 'win10']): ['a'],
                tuple(['mac']): ['b'],
            },
            'bar_test': {
                tuple(['win', 'win10']): ['c'],
            },
        },
    }

    result_counts = {
        tuple(['win', 'win10']): {
            # We expect this to be ignored since it has a 1% flake rate.
            'foo_test': 100,
            # We expect this to be RetryOnFailure since it has a 25% flake rate.
            'bar_test': 4,
        },
        tuple(['mac']): {
            # We expect this to be Failure since it has a 50% flake rate.
            'foo_test': 2
        }
    }
    self._expectations.IterateThroughResultsWithThresholds(
        self.result_map, False, result_counts, 0.02, 0.5, False)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
[ mac ] foo_test [ Failure ]
[ win10 ] bar_test [ RetryOnFailure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class CreateExpectationsForAllResultsUnittest(fake_filesystem_unittest.TestCase
                                              ):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._expectations = uu.UnitTestExpectationProcessor()
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win']): [
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/1111',
                        datetime.date.today() - datetime.timedelta(days=2),
                        False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/2222',
                        datetime.date.today() - datetime.timedelta(days=3),
                        False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/3333',
                                       datetime.date.today(), False, ['Pass']),
                ],
                tuple(['mac']): [
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/1111',
                        datetime.date.today() - datetime.timedelta(days=1),
                        False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/2222',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/3333',
                        datetime.date.today() - datetime.timedelta(days=3),
                        False, ['Pass']),
                ],
            },
            'bar_test': {
                tuple(['win']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/4444',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/5555',
                        datetime.date.today() - datetime.timedelta(days=1),
                        False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/6666',
                        datetime.date.today() - datetime.timedelta(days=2),
                        False, ['Pass']),
                ],
            },
            'baz_test': {
                # This test config causes build fail on less than 2 consecutive
                # days, and thus should not exist in the output.
                tuple(['win']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/7777',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/8888',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/9999',
                                       datetime.date.today(), False, ['Pass']),
                ],
                tuple(['mac']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/7777',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/8888',
                                       datetime.date.today(), False, ['Pass']),
                ],
            },
            'wpt_test': {
                # Test for same test in all builders over threshold.
                tuple(['win']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/1234',
                                       datetime.date.today(), False, ['Pass']),
                ],
                tuple(['mac']): [
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/2345',
                        datetime.date.today() - datetime.timedelta(days=1),
                        False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/3456',
                                       datetime.date.today(), False, ['Pass']),
                ],
            },
        },
    }
    self.build_fail_total_number_threshold = 3
    self.build_fail_consecutive_day_threshold = 2
    self.build_fail_recent_day_threshold = 1

    self.expectation_file = os.path.join(uu.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                         'pixel_expectations.txt')
    uu.CreateFile(self, self.expectation_file)
    expectation_file_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure Pass ]
[ mac ] some_test [ Failure Pass ]
[ android ] some_test [ Failure Pass ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    self._expectation_file_patcher = mock.patch.object(
        uu.UnitTestExpectationProcessor, 'GetExpectationFileForSuite')
    self._expectation_file_mock = self._expectation_file_patcher.start()
    self._expectation_file_mock.return_value = self.expectation_file
    self.addCleanup(self._expectation_file_patcher.stop)

  def testGroupByTags(self) -> None:
    """Tests that threshold-based expectations work when grouping by tags."""
    self._expectations.CreateExpectationsForAllResults(
        self.result_map, True, True, self.build_fail_total_number_threshold,
        self.build_fail_consecutive_day_threshold,
        self.build_fail_recent_day_threshold)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure Pass ]
[ win ] foo_test [ Failure Pass ]
[ win ] bar_test [ Failure Pass ]
[ win ] wpt_test [ Failure Pass ]
[ mac ] some_test [ Failure Pass ]
[ mac ] foo_test [ Failure Pass ]
[ mac ] wpt_test [ Failure Pass ]
[ android ] some_test [ Failure Pass ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testNoGroupByTags(self) -> None:
    """Tests that threshold-based expectations work when not grouping by tags"""
    self._expectations.CreateExpectationsForAllResults(
        self.result_map, False, True, self.build_fail_total_number_threshold,
        self.build_fail_consecutive_day_threshold,
        self.build_fail_recent_day_threshold)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure Pass ]
[ mac ] some_test [ Failure Pass ]
[ android ] some_test [ Failure Pass ]
[ win ] foo_test [ Failure Pass ]
[ mac ] foo_test [ Failure Pass ]
[ win ] bar_test [ Failure Pass ]
[ win ] wpt_test [ Failure Pass ]
[ mac ] wpt_test [ Failure Pass ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testNoIncludeAllTags(self) -> None:
    """Tests that threshold-based expectations work when filtering tags."""
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win', 'win10']): [
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/1111',
                        datetime.date.today() - datetime.timedelta(days=2),
                        False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/2222',
                        datetime.date.today() - datetime.timedelta(days=3),
                        False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/3333',
                                       datetime.date.today(), False, ['Pass']),
                ],
                tuple(['mac']): [
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/1111',
                        datetime.date.today() - datetime.timedelta(days=1),
                        False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/2222',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/3333',
                        datetime.date.today() - datetime.timedelta(days=3),
                        False, ['Pass']),
                ],
            },
            'bar_test': {
                tuple(['win', 'win10']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/4444',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/5555',
                        datetime.date.today() - datetime.timedelta(days=1),
                        False, ['Pass']),
                    ct.ResultTupleType(
                        ct.ResultStatus.FAIL, 'http://ci.chromium.org/b/6666',
                        datetime.date.today() - datetime.timedelta(days=2),
                        False, ['Pass']),
                ],
            },
            'baz_test': {
                # This test config causes build fail on less than 2 consecutive
                # days, and thus should not exist in the output.
                tuple(['win']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/7777',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/8888',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/9999',
                                       datetime.date.today(), False, ['Pass']),
                ],
                tuple(['mac']): [
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/7777',
                                       datetime.date.today(), False, ['Pass']),
                    ct.ResultTupleType(ct.ResultStatus.FAIL,
                                       'http://ci.chromium.org/b/8888',
                                       datetime.date.today(), False, ['Pass']),
                ],
            },
        },
    }
    self._expectations.CreateExpectationsForAllResults(
        self.result_map, False, False, self.build_fail_total_number_threshold,
        self.build_fail_consecutive_day_threshold,
        self.build_fail_recent_day_threshold)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure Pass ]
[ mac ] some_test [ Failure Pass ]
[ android ] some_test [ Failure Pass ]
[ win10 ] foo_test [ Failure Pass ]
[ mac ] foo_test [ Failure Pass ]
[ win10 ] bar_test [ Failure Pass ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class FindFailuresInSameConditionUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._expectations = uu.UnitTestExpectationProcessor()
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win']): ['a'],
                tuple(['mac']): ['a', 'b'],
            },
            'bar_test': {
                tuple(['win']): ['a', 'b', 'c'],
                tuple(['mac']): ['a', 'b', 'c', 'd'],
            },
        },
        'webgl_conformance_integration_test': {
            'foo_test': {
                tuple(['win']): ['a', 'b', 'c', 'd', 'e'],
                tuple(['mac']): ['a', 'b', 'c', 'd', 'e', 'f'],
            },
            'bar_test': {
                tuple(['win']): ['a', 'b', 'c', 'd', 'e', 'f', 'g'],
                tuple(['mac']): ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'],
            },
        },
    }

  def testFindFailuresInSameTest(self) -> None:
    other_failures = self._expectations.FindFailuresInSameTest(
        self.result_map, 'pixel_integration_test', 'foo_test', tuple(['win']))
    self.assertEqual(other_failures, [(tuple(['mac']), 2)])

  def testFindFailuresInSameConfig(self) -> None:
    typ_tag_ordered_result_map = self._expectations._ReorderMapByTypTags(
        self.result_map)
    other_failures = self._expectations.FindFailuresInSameConfig(
        typ_tag_ordered_result_map, 'pixel_integration_test', 'foo_test',
        tuple(['win']))
    expected_other_failures = [
        ('pixel_integration_test.bar_test', 3),
        ('webgl_conformance_integration_test.foo_test', 5),
        ('webgl_conformance_integration_test.bar_test', 7),
    ]
    self.assertEqual(len(other_failures), len(expected_other_failures))
    self.assertEqual(set(other_failures), set(expected_other_failures))


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class ModifyFileForResultUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._expectations = uu.UnitTestExpectationProcessor()
    self.expectation_file = os.path.join(uu.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                         'expectation.txt')
    uu.CreateFile(self, self.expectation_file)
    self._expectation_file_patcher = mock.patch.object(
        uu.UnitTestExpectationProcessor, 'GetExpectationFileForSuite')
    self._expectation_file_mock = self._expectation_file_patcher.start()
    self.addCleanup(self._expectation_file_patcher.stop)
    self._expectation_file_mock.return_value = self.expectation_file

  def testNoGroupByTags(self) -> None:
    """Tests that not grouping by tags appends to the end."""
    expectation_file_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)
    self._expectations.ModifyFileForResult('some_file', 'some_test',
                                           ('win', 'win10'), '', 'Failure',
                                           False, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]
[ win win10 ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testGroupByTagsNoMatch(self) -> None:
    """Tests that grouping by tags but finding no match appends to the end."""
    expectation_file_contents = uu.TAG_HEADER + """\
[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)
    self._expectations.ModifyFileForResult('some_file', 'some_test',
                                           ('win', 'win10'), '', 'Failure',
                                           True, True)
    expected_contents = uu.TAG_HEADER + """\
[ mac ] some_test [ Failure ]
[ win win10 ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testGroupByTagsMatch(self) -> None:
    """Tests that grouping by tags and finding a match adds mid-file."""
    expectation_file_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)
    self._expectations.ModifyFileForResult('some_file', 'foo_test',
                                           ('win', 'win10'), '', 'Failure',
                                           True, True)
    expected_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ win ] foo_test [ Failure ]

[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class FilterToMostSpecificTagTypeUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self._expectations = uu.UnitTestExpectationProcessor()
    self.setUpPyfakefs()
    with tempfile.NamedTemporaryFile(delete=False) as tf:
      self.expectation_file = tf.name

  def testBasic(self):
    """Tests that only the most specific tags are kept."""
    expectation_file_contents = """\
# tags: [ tag1_least_specific tag1_middle_specific tag1_most_specific ]
# tags: [ tag2_least_specific tag2_middle_specific tag2_most_specific ]"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = ('tag1_least_specific', 'tag1_most_specific', 'tag2_middle_specific',
            'tag2_least_specific')
    filtered_tags = self._expectations.FilterToMostSpecificTypTags(
        tags, self.expectation_file)
    self.assertEqual(filtered_tags,
                     ('tag1_most_specific', 'tag2_middle_specific'))

  def testSingleTags(self) -> None:
    """Tests that functionality works as expected with single tags."""
    expectation_file_contents = """\
# tags: [ tag1_most_specific ]
# tags: [ tag2_most_specific ]"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = ('tag1_most_specific', 'tag2_most_specific')
    filtered_tags = self._expectations.FilterToMostSpecificTypTags(
        tags, self.expectation_file)
    self.assertEqual(filtered_tags, tags)

  def testUnusedTags(self) -> None:
    """Tests that functionality works as expected with extra/unused tags."""
    expectation_file_contents = """\
# tags: [ tag1_least_specific tag1_middle_specific tag1_most_specific ]
# tags: [ tag2_least_specific tag2_middle_specific tag2_most_specific ]
# tags: [ some_unused_tag ]"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = ('tag1_least_specific', 'tag1_most_specific', 'tag2_middle_specific',
            'tag2_least_specific')
    filtered_tags = self._expectations.FilterToMostSpecificTypTags(
        tags, self.expectation_file)
    self.assertEqual(filtered_tags,
                     ('tag1_most_specific', 'tag2_middle_specific'))

  def testMultiline(self) -> None:
    """Tests that functionality works when tags cover multiple lines."""
    expectation_file_contents = """\
# tags: [ tag1_least_specific
#         tag1_middle_specific
#         tag1_most_specific ]
# tags: [ tag2_least_specific
#         tag2_middle_specific tag2_most_specific ]"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = ('tag1_least_specific', 'tag1_middle_specific', 'tag1_most_specific',
            'tag2_middle_specific', 'tag2_least_specific')
    filtered_tags = self._expectations.FilterToMostSpecificTypTags(
        tags, self.expectation_file)
    self.assertEqual(filtered_tags,
                     ('tag1_most_specific', 'tag2_middle_specific'))

  def testMissingTags(self) -> None:
    """Tests that a file not having all tags is an error."""
    expectation_file_contents = """\
# tags: [ tag1_least_specific tag1_middle_specific ]
# tags: [ tag2_least_specific tag2_middle_specific tag2_most_specific ]"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = ('tag1_least_specific', 'tag1_most_specific', 'tag2_middle_specific',
            'tag2_least_specific')
    with self.assertRaises(RuntimeError):
      self._expectations.FilterToMostSpecificTypTags(tags,
                                                     self.expectation_file)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class FindBestInsertionLineForExpectationUnittest(
    fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._expectations = uu.UnitTestExpectationProcessor()
    self.expectation_file = os.path.join(uu.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                         'expectation.txt')
    uu.CreateFile(self, self.expectation_file)
    expectation_file_contents = uu.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]

[ win release ] bar_test [ Failure ]
[ win ] foo_test [ Failure ]

[ chromeos ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

  def testNoMatchingTags(self) -> None:
    """Tests behavior when there are no expectations with matching tags."""
    insertion_line, tags = (
        self._expectations.FindBestInsertionLineForExpectation(
            tuple(['android']), self.expectation_file))
    self.assertEqual(insertion_line, -1)
    self.assertEqual(tags, set())

  def testMatchingTagsLastEntryChosen(self) -> None:
    """Tests that the last matching line is chosen."""
    insertion_line, tags = (
        self._expectations.FindBestInsertionLineForExpectation(
            tuple(['win']), self.expectation_file))
    # We expect "[ win ] foo_test [ Failure ]" to be chosen
    expected_line = len(uu.TAG_HEADER.splitlines()) + 6
    self.assertEqual(insertion_line, expected_line)
    self.assertEqual(tags, set(['win']))

  def testMatchingTagsClosestMatchChosen(self) -> None:
    """Tests that the closest tag match is chosen."""
    insertion_line, tags = (
        self._expectations.FindBestInsertionLineForExpectation(
            ('win', 'release'), self.expectation_file))
    # We expect "[ win release ] bar_test [ Failure ]" to be chosen
    expected_line = len(uu.TAG_HEADER.splitlines()) + 5
    self.assertEqual(insertion_line, expected_line)
    self.assertEqual(tags, set(['win', 'release']))


class AssertCheckoutIsUpToDateUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._expectations = uu.UnitTestExpectationProcessor()
    self._origin_patcher = mock.patch(
        'flake_suppressor_common.expectations.ExpectationProcessor.'
        'GetOriginExpectationFileContents')
    self._origin_mock = self._origin_patcher.start()
    self.addCleanup(self._origin_patcher.stop)
    self._local_patcher = mock.patch(
        'flake_suppressor_common.expectations.' +
        'ExpectationProcessor.GetLocalCheckoutExpectationFileContents')
    self._local_mock = self._local_patcher.start()
    self.addCleanup(self._local_patcher.stop)

  def testContentsMatch(self) -> None:
    """Tests the happy path where the contents match."""
    self._origin_mock.return_value = {
        'foo.txt': 'foo_content',
        'bar.txt': 'bar_content',
    }
    self._local_mock.return_value = {
        'bar.txt': 'bar_content',
        'foo.txt': 'foo_content',
    }
    self._expectations.AssertCheckoutIsUpToDate()

  def testContentsDoNotMatch(self) -> None:
    """Tests that mismatched contents results in a failure."""
    self._origin_mock.return_value = {
        'foo.txt': 'foo_content',
        'bar.txt': 'bar_content',
    }
    # Differing keys.
    self._local_mock.return_value = {
        'bar.txt': 'bar_content',
        'foo2.txt': 'foo_content',
    }
    with self.assertRaises(RuntimeError):
      self._expectations.AssertCheckoutIsUpToDate()

    # Differing values.
    self._local_mock.return_value = {
        'bar.txt': 'bar_content',
        'foo.txt': 'foo_content2',
    }
    with self.assertRaises(RuntimeError):
      self._expectations.AssertCheckoutIsUpToDate()


class OverFailedBuildThresholdUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.build_fail_total_number_threshold = 3

  def testOverThreshold(self) -> None:
    """Tests functionality when |result_tuple_list| passes
    |build_fail_total_number_threshold|.

    True is expected output on these inputs.
    """
    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2021, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/3333',
                           datetime.date(2023, 1, 1), False, ['Pass']),
    ]
    self.assertTrue(
        expectations.OverFailedBuildThreshold(
            result_tuple_list, self.build_fail_total_number_threshold))

  def testUnderThreshold(self) -> None:
    """Tests functionality when |result_tuple_list| cannot pass
       |build_fail_total_number_threshold|.

    False is expected output on these inputs.
    """
    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 2), False, ['Pass']),
    ]
    self.assertFalse(
        expectations.OverFailedBuildThreshold(
            result_tuple_list, self.build_fail_total_number_threshold))

    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 2), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 3), False, ['Pass']),
    ]
    self.assertFalse(
        expectations.OverFailedBuildThreshold(
            result_tuple_list, self.build_fail_total_number_threshold))


class OverFailedBuildByConsecutiveDayThresholdUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.build_fail_consecutive_day_threshold = 3

  def testOverThreshold(self) -> None:
    """Tests functionality when |result_tuple_list| passes
       |build_fail_consecutive_day_threshold|.

    True is expected output on these inputs.
    """
    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 2), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/3333',
                           datetime.date(2022, 1, 3), False, ['Pass']),
    ]
    self.assertTrue(
        expectations.OverFailedBuildByConsecutiveDayThreshold(
            result_tuple_list, self.build_fail_consecutive_day_threshold))

  def testUnderThreshold(self) -> None:
    """Tests functionality when |result_tuple_list| cannot pass
       |build_fail_consecutive_day_threshold|.

    False is expected output on these inputs.
    """
    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/3333',
                           datetime.date(2022, 1, 1), False, ['Pass']),
    ]
    self.assertFalse(
        expectations.OverFailedBuildByConsecutiveDayThreshold(
            result_tuple_list, self.build_fail_consecutive_day_threshold))

    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 2), False, ['Pass']),
    ]
    self.assertFalse(
        expectations.OverFailedBuildByConsecutiveDayThreshold(
            result_tuple_list, self.build_fail_consecutive_day_threshold))

    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 2), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/3333',
                           datetime.date(2022, 1, 4), False, ['Pass']),
    ]
    self.assertFalse(
        expectations.OverFailedBuildByConsecutiveDayThreshold(
            result_tuple_list, self.build_fail_consecutive_day_threshold))


class FailedBuildWithinRecentDayThresholdUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.build_fail_recent_day_threshold = 3

  def testWithinThreshold(self) -> None:
    """Tests functionality when |result_tuple_list| has build fail within
       |build_fail_recent_day_threshold|.

    True is expected output on these inputs.
    """
    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date.today(), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date.today(), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/3333',
                           datetime.date.today(), False, ['Pass']),
    ]
    self.assertTrue(
        expectations.FailedBuildWithinRecentDayThreshold(
            result_tuple_list, self.build_fail_recent_day_threshold))

  def testBeyondThreshold(self) -> None:
    """Tests functionality when |result_tuple_list| has no build fail within
       |build_fail_recent_day_threshold|.

    False is expected output on these inputs.
    """
    result_tuple_list = [
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/1111',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/2222',
                           datetime.date(2022, 1, 1), False, ['Pass']),
        ct.ResultTupleType(ct.ResultStatus.FAIL,
                           'http://ci.chromium.org/b/3333',
                           datetime.date(2022, 1, 1), False, ['Pass']),
    ]
    self.assertFalse(
        expectations.FailedBuildWithinRecentDayThreshold(
            result_tuple_list, self.build_fail_recent_day_threshold))


if __name__ == '__main__':
  unittest.main(verbosity=2)
