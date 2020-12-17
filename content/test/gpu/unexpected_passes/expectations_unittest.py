# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

import validate_tag_consistency

from unexpected_passes import data_types
from unexpected_passes import expectations
from unexpected_passes import unittest_utils as uu

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


class SplitExpectationsByStalenessUnittest(unittest.TestCase):
  def testEmptyInput(self):
    """Tests that nothing blows up with empty input."""
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness({})
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, {})

  def testStaleExpectations(self):
    """Tests output when only stale expectations are provided."""
    expectation_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], ['Failure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 0),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(4, 0)
                },
            },
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(5, 0),
                    'step2': uu.CreateStatsWithPassFails(6, 0),
                },
            },
        },
        'bar': {
            data_types.Expectation('bar', ['win'], ['Failure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(7, 0),
                },
            },
        },
    }
    expected_stale_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, expected_stale_dict)
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, {})

  def testActiveExpectations(self):
    """Tests output when only active expectations are provided."""
    expectation_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], ['Failure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(0, 1),
                    'step2': uu.CreateStatsWithPassFails(0, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(0, 3),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(0, 5),
                    'step2': uu.CreateStatsWithPassFails(0, 6),
                },
            },
        },
        'bar': {
            data_types.Expectation('bar', ['win'], ['Failure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(0, 7),
                },
            },
        },
    }
    expected_active_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, expected_active_dict)

  def testSemiStaleExpectations(self):
    """Tests output when only semi-stale expectations are provided."""
    expectation_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], ['Failure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(5, 0),
                    'step2': uu.CreateStatsWithPassFails(6, 6),
                },
            },
        },
        'bar': {
            data_types.Expectation('bar', ['win'], ['Failure']): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(7, 0),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(0, 8),
                },
            },
        },
    }
    expected_semi_stale_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, expected_semi_stale_dict)
    self.assertEqual(active_dict, {})

  def testAllExpectations(self):
    """Tests output when all three types of expectations are provided."""
    expectation_map = {
        'foo': {
            data_types.Expectation('foo', ['stale'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 0),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(4, 0)
                },
            },
            data_types.Expectation('foo', ['semistale'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
            data_types.Expectation('foo', ['active'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 1),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 3),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
        },
    }
    expected_stale = {
        'foo': {
            data_types.Expectation('foo', ['stale'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 0),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(4, 0)
                },
            },
        },
    }
    expected_semi_stale = {
        'foo': {
            data_types.Expectation('foo', ['semistale'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
        },
    }
    expected_active = {
        'foo': {
            data_types.Expectation('foo', ['active'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 1),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 3),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
        },
    }

    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, expected_stale)
    self.assertEqual(semi_stale_dict, expected_semi_stale)
    self.assertEqual(active_dict, expected_active)


class RemoveExpectationsFromFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    with tempfile.NamedTemporaryFile(delete=False) as f:
      self.filename = f.name

  def testExpectationRemoval(self):
    """Tests that expectations are properly removed from a file."""
    contents = validate_tag_consistency.TAG_HEADER + """

# This is a test comment
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
[ linux ] bar/test [ RetryOnFailure ]
[ win ] bar/test [ RetryOnFailure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['win'], ['Failure']),
        data_types.Expectation('bar/test', ['linux'], ['RetryOnFailure'])
    ]

    expected_contents = validate_tag_consistency.TAG_HEADER + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
[ win ] bar/test [ RetryOnFailure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = expectations.RemoveExpectationsFromFile(
        stale_expectations, self.filename)
    self.assertEqual(removed_urls, set(['crbug.com/1234']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testNestedBlockComments(self):
    """Tests that nested disable block comments throw exceptions."""
    contents = validate_tag_consistency.TAG_HEADER + """
# finder:disable
# finder:disable
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable
# finder:enable
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      expectations.RemoveExpectationsFromFile([], self.filename)

    contents = validate_tag_consistency.TAG_HEADER + """
# finder:enable
crbug.com/1234 [ win ] foo/test [ Failure ]
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      expectations.RemoveExpectationsFromFile([], self.filename)

  def testBlockComments(self):
    """Tests that expectations in a disable block comment are not removed."""
    contents = validate_tag_consistency.TAG_HEADER + """
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:disable
crbug.com/2345 [ win ] foo/test [ Failure ]
crbug.com/3456 [ win ] foo/test [ Failure ]
# finder:enable
crbug.com/4567 [ win ] foo/test [ Failure ]
"""
    stale_expectations = [
        data_types.Expectation('foo/test', ['win'], ['Failure'])
    ]
    expected_contents = validate_tag_consistency.TAG_HEADER + """
# finder:disable
crbug.com/2345 [ win ] foo/test [ Failure ]
crbug.com/3456 [ win ] foo/test [ Failure ]
# finder:enable
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = expectations.RemoveExpectationsFromFile(
        stale_expectations, self.filename)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/4567']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testInlineComments(self):
    """Tests that expectations with inline disable comments are not removed."""
    contents = validate_tag_consistency.TAG_HEADER + """
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/2345 [ win ] foo/test [ Failure ]  # finder:disable
crbug.com/3456 [ win ] foo/test [ Failure ]
"""
    stale_expectations = [
        data_types.Expectation('foo/test', ['win'], ['Failure'])
    ]
    expected_contents = validate_tag_consistency.TAG_HEADER + """
crbug.com/2345 [ win ] foo/test [ Failure ]  # finder:disable
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = expectations.RemoveExpectationsFromFile(
        stale_expectations, self.filename)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/3456']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testGetDisableReasonFromComment(self):
    """Tests that the disable reason can be pulled from a line."""
    self.assertEqual(
        expectations._GetDisableReasonFromComment('# finder:disable foo'),
        'foo')
    self.assertEqual(
        expectations._GetDisableReasonFromComment(
            'crbug.com/1234 [ win ] bar/test [ Failure ]  # finder:disable foo'
        ), 'foo')


if __name__ == '__main__':
  unittest.main(verbosity=2)
