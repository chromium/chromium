#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys
import tempfile
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest

from unexpected_passes_common import data_types
from unexpected_passes_common import expectations
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
    self.instance = expectations.Expectations()

  def testExclusiveOr(self):
    """Tests that only one input can be specified."""
    with self.assertRaises(AssertionError):
      self.instance.CreateTestExpectationMap(None, None)
    with self.assertRaises(AssertionError):
      self.instance.CreateTestExpectationMap('foo', ['bar'])

  def testExpectationFile(self):
    """Tests reading expectations from an expectation file."""
    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      filename = f.name
      f.write(FAKE_EXPECTATION_FILE_CONTENTS)
    basename = os.path.basename(filename)
    expectation_map = self.instance.CreateTestExpectationMap(filename, None)
    # Skip expectations should be omitted, but everything else should be
    # present.
    # yapf: disable
    expected_expectation_map = {
        basename: {
            data_types.Expectation(
                'foo/test', ['win'], ['Failure'], 'crbug.com/1234'): {},
            data_types.Expectation('foo/test', ['linux'], ['Failure']): {},
            data_types.Expectation(
                'bar/*', ['linux'], ['RetryOnFailure'], 'crbug.com/2345'): {},
        },
    }
    # yapf: enable
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)

  def testIndividualTests(self):
    """Tests reading expectations from a list of tests."""
    expectation_map = self.instance.CreateTestExpectationMap(
        None, ['foo/test', 'bar/*'])
    expected_expectation_map = {
        '': {
            data_types.Expectation('foo/test', [], ['RetryOnFailure']): {},
            data_types.Expectation('bar/*', [], ['RetryOnFailure']): {},
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)


class RemoveExpectationsFromFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.instance = uu.CreateGenericExpectations()
    self.header = self.instance._GetExpectationFileTagHeader()
    self.setUpPyfakefs()
    with tempfile.NamedTemporaryFile(delete=False) as f:
      self.filename = f.name

  def testExpectationRemoval(self):
    """Tests that expectations are properly removed from a file."""
    contents = self.header + """

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

    expected_contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
[ win ] bar/test [ RetryOnFailure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename)
    self.assertEqual(removed_urls, set(['crbug.com/1234']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testNestedBlockComments(self):
    """Tests that nested disable block comments throw exceptions."""
    contents = self.header + """
# finder:disable
# finder:disable
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable
# finder:enable
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      self.instance.RemoveExpectationsFromFile([], self.filename)

    contents = self.header + """
# finder:enable
crbug.com/1234 [ win ] foo/test [ Failure ]
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      self.instance.RemoveExpectationsFromFile([], self.filename)

  def testBlockComments(self):
    """Tests that expectations in a disable block comment are not removed."""
    contents = self.header + """
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
    expected_contents = self.header + """
# finder:disable
crbug.com/2345 [ win ] foo/test [ Failure ]
crbug.com/3456 [ win ] foo/test [ Failure ]
# finder:enable
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/4567']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testInlineComments(self):
    """Tests that expectations with inline disable comments are not removed."""
    contents = self.header + """
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
    expected_contents = self.header + """
crbug.com/2345 [ win ] foo/test [ Failure ]  # finder:disable
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
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


class GetExpectationLineUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = uu.CreateGenericExpectations()

  def testNoMatchingExpectation(self):
    """Tests that the case of no matching expectation is handled."""
    expectation = data_types.Expectation('foo', ['win'], 'Failure')
    line, line_number = self.instance._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS)
    self.assertIsNone(line)
    self.assertIsNone(line_number)

  def testMatchingExpectation(self):
    """Tests that matching expectations are found."""
    expectation = data_types.Expectation('foo/test', ['win'], 'Failure',
                                         'crbug.com/1234')
    line, line_number = self.instance._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS)
    self.assertEqual(line, 'crbug.com/1234 [ win ] foo/test [ Failure ]')
    self.assertEqual(line_number, 3)


class ModifySemiStaleExpectationsUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self.instance = uu.CreateGenericExpectations()

    self._input_patcher = mock.patch.object(expectations,
                                            '_WaitForUserInputOnModification')
    self._input_mock = self._input_patcher.start()
    self.addCleanup(self._input_patcher.stop)

    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      f.write(FAKE_EXPECTATION_FILE_CONTENTS)
      self.filename = f.name

  def testEmptyExpectationMap(self):
    """Tests that an empty expectation map results in a no-op."""
    modified_urls = self.instance.ModifySemiStaleExpectations(
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
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
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
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
        test_expectation_map, self.filename)
    self.assertEqual(modified_urls, set(['crbug.com/1234']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)

  def testIgnoreExpectation(self):
    """Tests that specifying to ignore an expectation does nothing."""
    self._input_mock.return_value = 'i'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
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
          'expectation_file':
          data_types.ExpectationBuilderMap({
              data_types.Expectation(
                  'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
              data_types.BuilderStepMap(),
          }),
      })
      # yapf: enable
      self.instance.ModifySemiStaleExpectations(test_expectation_map,
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
    expectations_dir = os.path.join(os.path.dirname(__file__), 'expectations')
    self.setUpPyfakefs()
    self.instance = expectations.Expectations()
    self.filepath_patcher = mock.patch.object(
        self.instance,
        '_GetExpectationFilepaths',
        return_value=[os.path.join(expectations_dir, 'real_expectations.txt')])
    self.filepath_mock = self.filepath_patcher.start()
    self.addCleanup(self.filepath_patcher.stop)

    real_contents = 'crbug.com/1\ncrbug.com/2'
    skipped_contents = 'crbug.com/4'
    self.CreateFile(os.path.join(expectations_dir, 'real_expectations.txt'),
                    contents=real_contents)
    self.CreateFile(os.path.join(expectations_dir, 'fake.txt'),
                    contents=skipped_contents)

  def testNoOrphanedBugs(self):
    bugs = ['crbug.com/1', 'crbug.com/2']
    self.assertEqual(self.instance.FindOrphanedBugs(bugs), set())

  def testOrphanedBugs(self):
    bugs = ['crbug.com/1', 'crbug.com/3', 'crbug.com/4']
    self.assertEqual(self.instance.FindOrphanedBugs(bugs),
                     set(['crbug.com/3', 'crbug.com/4']))


if __name__ == '__main__':
  unittest.main(verbosity=2)
