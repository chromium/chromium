#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import copy
import os
import sys
import tempfile
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest

import validate_tag_consistency

from unexpected_passes_common import data_types
from unexpected_passes_common import expectations
from unexpected_passes_common import result_output
from unexpected_passes_common import unittest_utils as uu

FAKE_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win linux ]
# results: [ Failure RetryOnFailure Skip ]
crbug.com/1234 [ win ] foo/test [ Failure ]

[ linux ] foo/test [ Failure ]

crbug.com/2345 [ linux ] bar/* [ RetryOnFailure ]
crbug.com/3456 [ linux ] some/bad/test [ Skip ]
"""

FAKE_EXPECTATION_FILE_CONTENTS_WITH_TYPO = """\
# tags: [ win linux ]
# results: [ Failure RetryOnFailure Skip ]
crbug.com/1234 [ wine ] foo/test [ Failure ]

[ linux ] foo/test [ Failure ]

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
    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      filename = f.name
      f.write(FAKE_EXPECTATION_FILE_CONTENTS)
    expectation_map = expectations.CreateTestExpectationMap(filename, None)
    # Skip expectations should be omitted, but everything else should be
    # present.
    # yapf: disable
    expected_expectation_map = {
        'foo/test': {
            data_types.Expectation(
                'foo/test', ['win'], ['Failure'], 'crbug.com/1234'): {},
            data_types.Expectation('foo/test', ['linux'], ['Failure']): {},
        },
        'bar/*': {
            data_types.Expectation(
                'bar/*', ['linux'], ['RetryOnFailure'], 'crbug.com/2345'): {},
        },
    }
    # yapf: enable
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)

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
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)


class FilterOutUnusedExpectationsUnittest(unittest.TestCase):
  def testNoUnused(self):
    """Tests that filtering is a no-op if there are no unused expectations."""
    expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'SomeBuilder':
                data_types.StepBuildStatsMap(),
            }),
        })
    })
    expected_expectation_map = copy.deepcopy(expectation_map)
    unused_expectations = expectations.FilterOutUnusedExpectations(
        expectation_map)
    self.assertEqual(len(unused_expectations), 0)
    self.assertEqual(expectation_map, expected_expectation_map)

  def testUnusedButNotEmpty(self):
    """Tests filtering if there is an unused expectation but no empty tests."""
    expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'SomeBuilder':
                data_types.StepBuildStatsMap(),
            }),
            data_types.Expectation('foo/test', ['linux'], ['Failure']):
            data_types.BuilderStepMap(),
        })
    })
    expected_expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'SomeBuilder':
                data_types.StepBuildStatsMap(),
            }),
        }),
    })
    unused_expectations = expectations.FilterOutUnusedExpectations(
        expectation_map)
    self.assertEqual(
        unused_expectations,
        [data_types.Expectation('foo/test', ['linux'], ['Failure'])])
    self.assertEqual(expectation_map, expected_expectation_map)

  def testUnusedAndEmpty(self):
    """Tests filtering if there is an expectation that causes an empty test."""
    expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap(),
        }),
    })
    unused_expectations = expectations.FilterOutUnusedExpectations(
        expectation_map)
    self.assertEqual(unused_expectations,
                     [data_types.Expectation('foo/test', ['win'], ['Failure'])])
    self.assertEqual(expectation_map, {})


class SplitExpectationsByStalenessUnittest(unittest.TestCase):
  def testEmptyInput(self):
    """Tests that nothing blows up with empty input."""
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(
            data_types.TestExpectationMap())
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, {})
    self.assertIsInstance(stale_dict, data_types.TestExpectationMap)
    self.assertIsInstance(semi_stale_dict, data_types.TestExpectationMap)
    self.assertIsInstance(active_dict, data_types.TestExpectationMap)

  def testStaleExpectations(self):
    """Tests output when only stale expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(4, 0)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(5, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(6, 0),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(7, 0),
                }),
            }),
        }),
    })
    expected_stale_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, expected_stale_dict)
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, {})

  def testActiveExpectations(self):
    """Tests output when only active expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 1),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 3),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 5),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 6),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 7),
                }),
            }),
        }),
    })
    expected_active_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, expected_active_dict)

  def testSemiStaleExpectations(self):
    """Tests output when only semi-stale expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(5, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(6, 6),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(7, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 8),
                }),
            }),
        }),
    })
    expected_semi_stale_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectations.SplitExpectationsByStaleness(expectation_map)
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, expected_semi_stale_dict)
    self.assertEqual(active_dict, {})

  def testAllExpectations(self):
    """Tests output when all three types of expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['stale'], 'Failure'):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(4, 0)
                }),
            }),
            data_types.Expectation('foo', ['semistale'], 'Failure'):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['active'], 'Failure'):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 1),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 3),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
        }),
    })
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
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/1234'),
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
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/1234'),
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/2345'),
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/3456'),
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/4567'),
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
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/1234'),
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/2345'),
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/3456'),
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


class AddResultToMapUnittest(unittest.TestCase):
  def testResultMatchPassingNew(self):
    """Test adding a passing result when no results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'some/test/*':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
        }),
    })
    found_matching = expectations._AddResultToMap(r, 'builder', expectation_map)
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
    expectation_map = data_types.TestExpectationMap({
        'some/test/*':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
        }),
    })
    found_matching = expectations._AddResultToMap(r, 'builder', expectation_map)
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
    expectation_map = data_types.TestExpectationMap({
        'some/test/*':
        data_types.ExpectationBuilderMap({
            e:
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'pixel_tests': stats,
                }),
            }),
        }),
    })
    found_matching = expectations._AddResultToMap(r, 'builder', expectation_map)
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
    expectation_map = data_types.TestExpectationMap({
        'some/test/*':
        data_types.ExpectationBuilderMap({
            e:
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'pixel_tests': stats,
                }),
            }),
        }),
    })
    found_matching = expectations._AddResultToMap(r, 'builder', expectation_map)
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
    expectation_map = data_types.TestExpectationMap({
        'some/test/*':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
            e2: data_types.BuilderStepMap(),
        }),
    })
    found_matching = expectations._AddResultToMap(r, 'builder', expectation_map)
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
    expectation_map = data_types.TestExpectationMap({
        'some/test/*':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
        })
    })
    found_matching = expectations._AddResultToMap(r, 'builder', expectation_map)
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
    return data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            foo_expectation:
            data_types.BuilderStepMap(),
        }),
    })

  def GetEmptyMapForGenericFailureExpectation(self):
    foo_expectation = self.GetGenericFailureExpectation()
    return data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            foo_expectation:
            data_types.BuilderStepMap(),
        }),
    })

  def GetPassedMapForExpectation(self, expectation):
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    return self.GetMapForExpectationAndStats(expectation, stats)

  def GetFailedMapForExpectation(self, expectation):
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id')
    return self.GetMapForExpectationAndStats(expectation, stats)

  def GetMapForExpectationAndStats(self, expectation, stats):
    return data_types.TestExpectationMap({
        expectation.test:
        data_types.ExpectationBuilderMap({
            expectation:
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'pixel_tests': stats,
                }),
            }),
        }),
    })

  def testRetryOnlyPassMatching(self):
    """Tests when the only tests are retry expectations that pass and match."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = expectations.AddResultListToMap(expectation_map,
                                                        'builder', [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testRetryOnlyFailMatching(self):
    """Tests when the only tests are retry expectations that fail and match."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                   'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = expectations.AddResultListToMap(expectation_map,
                                                        'builder', [foo_result])
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
    unmatched_results = expectations.AddResultListToMap(
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
    unmatched_results = expectations.AddResultListToMap(expectation_map,
                                                        'builder', [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testFailureFailureMatching(self):
    """Tests when there are failure results for failure expectations."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                   'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = expectations.AddResultListToMap(expectation_map,
                                                        'builder', [foo_result])
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
    unmatched_results = expectations.AddResultListToMap(
        expectation_map, 'builder',
        [foo_match_result, foo_mismatch_result, bar_result])
    self.assertEqual(len(set(unmatched_results)), 2)
    self.assertEqual(set(unmatched_results),
                     set([foo_mismatch_result, bar_result]))

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)


class MergeExpectationMapsUnittest(unittest.TestCase):
  maxDiff = None

  def testEmptyBaseMap(self):
    """Tests that a merge with an empty base map copies the merge map."""
    base_map = data_types.TestExpectationMap()
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    original_merge_map = copy.deepcopy(merge_map)
    expectations.MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, merge_map)
    self.assertEqual(merge_map, original_merge_map)

  def testEmptyMergeMap(self):
    """Tests that a merge with an empty merge map is a no-op."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_map = data_types.TestExpectationMap()
    original_base_map = copy.deepcopy(base_map)
    expectations.MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, original_base_map)
    self.assertEqual(merge_map, {})

  def testMissingKeys(self):
    """Tests that missing keys are properly copied to the base map."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step2': data_types.BuildStats(),
                }),
                'builder2':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
            data_types.Expectation('foo', ['mac'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                })
            })
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
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
    expectations.MergeExpectationMaps(base_map, merge_map)
    self.assertEqual(base_map, expected_base_map)

  def testMergeBuildStats(self):
    """Tests that BuildStats for the same step are merged properly."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_stats = data_types.BuildStats()
    merge_stats.AddFailedBuild('1')
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': merge_stats,
                }),
            }),
        }),
    })
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
    expectations.MergeExpectationMaps(base_map, merge_map)
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
    expectations.MergeExpectationMaps(base_map, merge_map, original_base_map)
    with self.assertRaises(AssertionError):
      expectations.MergeExpectationMaps(base_map, merge_map, original_base_map)


class ConvertBuilderMapToPassOrderedStringDictUnittest(unittest.TestCase):
  def testEmptyInput(self):
    """Tests that an empty input doesn't cause breakage."""
    output = expectations._ConvertBuilderMapToPassOrderedStringDict(
        data_types.BuilderStepMap())
    expected_output = collections.OrderedDict()
    expected_output[result_output.FULL_PASS] = {}
    expected_output[result_output.NEVER_PASS] = {}
    expected_output[result_output.PARTIAL_PASS] = {}
    self.assertEqual(output, expected_output)

  def testBasic(self):
    """Tests that a map is properly converted."""
    builder_map = data_types.BuilderStepMap({
        'fully pass':
        data_types.StepBuildStatsMap({
            'step1': uu.CreateStatsWithPassFails(1, 0),
        }),
        'never pass':
        data_types.StepBuildStatsMap({
            'step3': uu.CreateStatsWithPassFails(0, 1),
        }),
        'partial pass':
        data_types.StepBuildStatsMap({
            'step5': uu.CreateStatsWithPassFails(1, 1),
        }),
        'mixed':
        data_types.StepBuildStatsMap({
            'step7': uu.CreateStatsWithPassFails(1, 0),
            'step8': uu.CreateStatsWithPassFails(0, 1),
            'step9': uu.CreateStatsWithPassFails(1, 1),
        }),
    })
    output = expectations._ConvertBuilderMapToPassOrderedStringDict(builder_map)

    expected_output = collections.OrderedDict()
    expected_output[result_output.FULL_PASS] = {
        'fully pass': [
            'step1 (1/1)',
        ],
        'mixed': [
            'step7 (1/1)',
        ],
    }
    expected_output[result_output.NEVER_PASS] = {
        'never pass': [
            'step3 (0/1)',
        ],
        'mixed': [
            'step8 (0/1)',
        ],
    }
    expected_output[result_output.PARTIAL_PASS] = {
        'partial pass': {
            'step5 (1/2)': [
                'http://ci.chromium.org/b/build_id0',
            ],
        },
        'mixed': {
            'step9 (1/2)': [
                'http://ci.chromium.org/b/build_id0',
            ],
        },
    }
    self.assertEqual(output, expected_output)


class GetExpectationLineUnittest(unittest.TestCase):
  def testNoMatchingExpectation(self):
    """Tests that the case of no matching expectation is handled."""
    expectation = data_types.Expectation('foo', ['win'], 'Failure')
    line, line_number = expectations._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS)
    self.assertIsNone(line)
    self.assertIsNone(line_number)

  def testMatchingExpectation(self):
    """Tests that matching expectations are found."""
    expectation = data_types.Expectation('foo/test', ['win'], 'Failure',
                                         'crbug.com/1234')
    line, line_number = expectations._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS)
    self.assertEqual(line, 'crbug.com/1234 [ win ] foo/test [ Failure ]')
    self.assertEqual(line_number, 3)


class ModifySemiStaleExpectationsUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()

    self._input_patcher = mock.patch.object(expectations,
                                            '_WaitForUserInputOnModification')
    self._input_mock = self._input_patcher.start()
    self.addCleanup(self._input_patcher.stop)

    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      f.write(FAKE_EXPECTATION_FILE_CONTENTS)
      self.filename = f.name

  def testEmptyExpectationMap(self):
    """Tests that an empty expectation map results in a no-op."""
    modified_urls = expectations.ModifySemiStaleExpectations(
        data_types.TestExpectationMap(), self.filename)
    self.assertEqual(modified_urls, set())
    self._input_mock.assert_not_called()
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)

  def testRemoveExpectation(self):
    """Tests that specifying to remove an expectation does so."""
    self._input_mock.return_value = 'r'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = expectations.ModifySemiStaleExpectations(
        test_expectation_map, self.filename)
    self.assertEqual(modified_urls, set(['crbug.com/1234']))
    expected_file_contents = """\
# tags: [ win linux ]
# results: [ Failure RetryOnFailure Skip ]

[ linux ] foo/test [ Failure ]

crbug.com/2345 [ linux ] bar/* [ RetryOnFailure ]
crbug.com/3456 [ linux ] some/bad/test [ Skip ]
"""
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_file_contents)

  def testModifyExpectation(self):
    """Tests that specifying to modify an expectation does not remove it."""
    self._input_mock.return_value = 'm'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = expectations.ModifySemiStaleExpectations(
        test_expectation_map, self.filename)
    self.assertEqual(modified_urls, set(['crbug.com/1234']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)

  def testIgnoreExpectation(self):
    """Tests that specifying to ignore an expectation does nothing."""
    self._input_mock.return_value = 'i'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        'foo/test':
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = expectations.ModifySemiStaleExpectations(
        test_expectation_map, self.filename)
    self.assertEqual(modified_urls, set())
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)

  def testParserErrorCorrection(self):
    """Tests that parser errors are caught and users can fix them."""

    def TypoSideEffect():
      with open(self.filename, 'w') as outfile:
        outfile.write(FAKE_EXPECTATION_FILE_CONTENTS_WITH_TYPO)
      return 'm'

    def CorrectionSideEffect():
      with open(self.filename, 'w') as outfile:
        outfile.write(FAKE_EXPECTATION_FILE_CONTENTS)

    self._input_mock.side_effect = TypoSideEffect
    with mock.patch.object(expectations,
                           '_WaitForAnyUserInput') as any_input_mock:
      any_input_mock.side_effect = CorrectionSideEffect
      # yapf: disable
      test_expectation_map = data_types.TestExpectationMap({
          'foo/test':
          data_types.ExpectationBuilderMap({
              data_types.Expectation(
                  'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
              data_types.BuilderStepMap(),
          }),
      })
      # yapf: enable
      expectations.ModifySemiStaleExpectations(test_expectation_map,
                                               self.filename)
      any_input_mock.assert_called_once()
      with open(self.filename) as infile:
        self.assertEqual(infile.read(), FAKE_EXPECTATION_FILE_CONTENTS)


class FindOrphanedBugsUnittest(fake_filesystem_unittest.TestCase):
  def CreateFile(self, *args, **kwargs):
    # TODO(crbug.com/1156806): Remove this and just use fs.create_file() when
    # Catapult is updated to a newer version of pyfakefs that is compatible with
    # Chromium's version.
    if hasattr(self.fs, 'create_file'):
      self.fs.create_file(*args, **kwargs)
    else:
      self.fs.CreateFile(*args, **kwargs)

  def setUp(self):
    expectations_dir = expectations.EXPECTATIONS_DIR
    # Make sure our fake expectations are where the real ones actually are.
    self.assertTrue(os.path.exists(expectations_dir))
    self.setUpPyfakefs()

    real_contents = 'crbug.com/1\ncrbug.com/2'
    skipped_contents = 'crbug.com/4'
    self.CreateFile(os.path.join(expectations_dir, 'real_expectations.txt'),
                    contents=real_contents)
    self.CreateFile(os.path.join(expectations_dir, 'fake.txt'),
                    contents=skipped_contents)

  def testNoOrphanedBugs(self):
    bugs = ['crbug.com/1', 'crbug.com/2']
    self.assertEqual(expectations.FindOrphanedBugs(bugs), set())

  def testOrphanedBugs(self):
    bugs = ['crbug.com/1', 'crbug.com/3', 'crbug.com/4']
    self.assertEqual(expectations.FindOrphanedBugs(bugs),
                     set(['crbug.com/3', 'crbug.com/4']))


if __name__ == '__main__':
  unittest.main(verbosity=2)
