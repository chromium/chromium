# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

from unexpected_passes import data_types
from unexpected_passes import expectations

FAKE_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win linux ]
# results: [ Failure RetryOnFailure Skip ]
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/1233 [ linux ] foo/test [ Failure ]
crbug.com/2345 [ linux ] bar/* [ RetryOnFailure ]
crbug.com/3456 [ linux ] some/bad/test [ Skip ]
"""


class CreateTestExpectationMapUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()

  def testExclusiveOr(self):
    """Tests that only one input can be specified."""
    with self.assertRaises(AssertionError):
      expectations.CreateTestExpectationMap(None, None)
    with self.assertRaises(AssertionError):
      expectations.CreateTestExpectationMap('foo', ['bar'])

  def testExpectationFile(self):
    """Tests reading expectations from an expectation file."""
    with tempfile.NamedTemporaryFile(delete=False) as f:
      filename = f.name
      f.write(FAKE_EXPECTATION_FILE_CONTENTS)
    expectation_map = expectations.CreateTestExpectationMap(filename, None)
    # Skip expectations should be omitted, but everything else should be
    # present.
    expected_expectation_map = {
        'foo/test': {
            data_types.Expectation('foo/test', ['win'], ['Failure']): {},
            data_types.Expectation('foo/test', ['linux'], ['Failure']): {},
        },
        'bar/*': {
            data_types.Expectation('bar/*', ['linux'], ['RetryOnFailure']): {},
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testIndividualTests(self):
    """Tests reading expectations from a list of tests."""
    expectation_map = expectations.CreateTestExpectationMap(
        None, ['foo/test', 'bar/*'])
    expected_expectation_map = {
        'foo/test': {
            data_types.Expectation('foo/test', [], ['RetryOnFailure']): {},
        },
        'bar/*': {
            data_types.Expectation('bar/*', [], ['RetryOnFailure']): {},
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)


class FilterOutUnusedExpectationsUnittest(unittest.TestCase):
  def testNoUnused(self):
    """Tests that filtering is a no-op if there are no unused expectations."""
    expectation_map = {
        'foo/test': {
            data_types.Expectation('foo/test', ['win'], ['Failure']): {
                'SomeBuilder': [],
            },
        },
    }
    expected_expectation_map = copy.deepcopy(expectation_map)
    unused_expectations = expectations.FilterOutUnusedExpectations(
        expectation_map)
    self.assertEqual(len(unused_expectations), 0)
    self.assertEqual(expectation_map, expected_expectation_map)

  def testUnusedButNotEmpty(self):
    """Tests filtering if there is an unused expectation but no empty tests."""
    expectation_map = {
        'foo/test': {
            data_types.Expectation('foo/test', ['win'], ['Failure']): {
                'SomeBuilder': [],
            },
            data_types.Expectation('foo/test', ['linux'], ['Failure']): {},
        },
    }
    expected_expectation_map = {
        'foo/test': {
            data_types.Expectation('foo/test', ['win'], ['Failure']): {
                'SomeBuilder': [],
            },
        },
    }
    unused_expectations = expectations.FilterOutUnusedExpectations(
        expectation_map)
    self.assertEqual(
        unused_expectations,
        [data_types.Expectation('foo/test', ['linux'], ['Failure'])])
    self.assertEqual(expectation_map, expected_expectation_map)

  def testUnusedAndEmpty(self):
    """Tests filtering if there is an expectation that causes an empty test."""
    expectation_map = {
        'foo/test': {
            data_types.Expectation('foo/test', ['win'], ['Failure']): {},
        },
    }
    unused_expectations = expectations.FilterOutUnusedExpectations(
        expectation_map)
    self.assertEqual(unused_expectations,
                     [data_types.Expectation('foo/test', ['win'], ['Failure'])])
    self.assertEqual(expectation_map, {})


if __name__ == '__main__':
  unittest.main(verbosity=2)
