#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import datetime
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
# results: [ Failure RetryOnFailure Skip Pass ]
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/5678 crbug.com/6789 [ win ] foo/another/test [ RetryOnFailure ]

[ linux ] foo/test [ Failure ]

crbug.com/2345 [ linux ] bar/* [ RetryOnFailure ]
crbug.com/3456 [ linux ] some/bad/test [ Skip ]
crbug.com/4567 [ linux ] some/good/test [ Pass ]
"""

SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS = """\
# tags: [ mac ]
# results: [ Failure ]

crbug.com/4567 [ mac ] foo/test [ Failure ]
"""

FAKE_EXPECTATION_FILE_CONTENTS_WITH_TYPO = """\
# tags: [ win linux ]
# results: [ Failure RetryOnFailure Skip ]
crbug.com/1234 [ wine ] foo/test [ Failure ]

[ linux ] foo/test [ Failure ]

crbug.com/2345 [ linux ] bar/* [ RetryOnFailure ]
crbug.com/3456 [ linux ] some/bad/test [ Skip ]
"""


class CreateTestExpectationMapUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = expectations.Expectations()

    self._expectation_content = {}
    self._content_patcher = mock.patch.object(
        self.instance, '_GetNonRecentExpectationContent')
    self._content_mock = self._content_patcher.start()
    self.addCleanup(self._content_patcher.stop)

    def SideEffect(filepath, _):
      return self._expectation_content[filepath]

    self._content_mock.side_effect = SideEffect

  def testExclusiveOr(self):
    """Tests that only one input can be specified."""
    with self.assertRaises(AssertionError):
      self.instance.CreateTestExpectationMap(None, None, 0)
    with self.assertRaises(AssertionError):
      self.instance.CreateTestExpectationMap('foo', ['bar'], 0)

  def testExpectationFile(self):
    """Tests reading expectations from an expectation file."""
    filename = '/tmp/foo'
    self._expectation_content[filename] = FAKE_EXPECTATION_FILE_CONTENTS
    expectation_map = self.instance.CreateTestExpectationMap(filename, None, 0)
    # Skip expectations should be omitted, but everything else should be
    # present.
    # yapf: disable
    expected_expectation_map = {
        filename: {
            data_types.Expectation(
                'foo/test', ['win'], ['Failure'], 'crbug.com/1234'): {},
            data_types.Expectation(
                'foo/another/test', ['win'], ['RetryOnFailure'],
                'crbug.com/5678 crbug.com/6789'): {},
            data_types.Expectation('foo/test', ['linux'], ['Failure']): {},
            data_types.Expectation(
                'bar/*', ['linux'], ['RetryOnFailure'], 'crbug.com/2345'): {},
        },
    }
    # yapf: enable
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)

  def testMultipleExpectationFiles(self):
    """Tests reading expectations from multiple files."""
    filename1 = '/tmp/foo'
    filename2 = '/tmp/bar'
    expectation_files = [filename1, filename2]
    self._expectation_content[filename1] = FAKE_EXPECTATION_FILE_CONTENTS
    self._expectation_content[
        filename2] = SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS

    expectation_map = self.instance.CreateTestExpectationMap(
        expectation_files, None, 0)
    # yapf: disable
    expected_expectation_map = {
      expectation_files[0]: {
        data_types.Expectation(
            'foo/test', ['win'], ['Failure'], 'crbug.com/1234'): {},
        data_types.Expectation(
             'foo/another/test', ['win'], ['RetryOnFailure'],
             'crbug.com/5678 crbug.com/6789'): {},
        data_types.Expectation('foo/test', ['linux'], ['Failure']): {},
        data_types.Expectation(
            'bar/*', ['linux'], ['RetryOnFailure'], 'crbug.com/2345'): {},
      },
      expectation_files[1]: {
        data_types.Expectation(
            'foo/test', ['mac'], ['Failure'], 'crbug.com/4567'): {},
      }
    }
    # yapf: enable
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)

  def testIndividualTests(self):
    """Tests reading expectations from a list of tests."""
    expectation_map = self.instance.CreateTestExpectationMap(
        None, ['foo/test', 'bar/*'], 0)
    expected_expectation_map = {
        '': {
            data_types.Expectation('foo/test', [], ['RetryOnFailure']): {},
            data_types.Expectation('bar/*', [], ['RetryOnFailure']): {},
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)


class GetNonRecentExpectationContentUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = uu.CreateGenericExpectations()
    self._output_patcher = mock.patch(
        'unexpected_passes_common.expectations.subprocess.check_output')
    self._output_mock = self._output_patcher.start()
    self.addCleanup(self._output_patcher.stop)

  def testBasic(self):
    """Tests that only expectations that are old enough are kept."""
    today_date = datetime.date.today()
    yesterday_date = today_date - datetime.timedelta(days=1)
    older_date = today_date - datetime.timedelta(days=2)
    today_str = today_date.isoformat()
    yesterday_str = yesterday_date.isoformat()
    older_str = older_date.isoformat()
    # pylint: disable=line-too-long
    blame_output = """\
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  1)# tags: [ tag1 ]
98637cd80f8c15 (Some R. Author    {yesterday_date} 00:00:00 +0000  2)# tags: [ tag2 ]
3fcadac9d861d0 (Some R. Author    {older_date} 00:00:00 +0000  3)# results: [ Failure ]
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  4)
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  5)crbug.com/1234 [ tag1 ] testname [ Failure ]
98637cd80f8c15 (Some R. Author    {yesterday_date} 00:00:00 +0000  6)[ tag2 ] testname [ Failure ] # Comment
3fcadac9d861d0 (Some R. Author    {older_date} 00:00:00 +0000  7)[ tag1 ] othertest [ Failure ]"""
    # pylint: enable=line-too-long
    blame_output = blame_output.format(today_date=today_str,
                                       yesterday_date=yesterday_str,
                                       older_date=older_str)
    self._output_mock.return_value = blame_output.encode('utf-8')

    expected_content = """\
# tags: [ tag1 ]
# tags: [ tag2 ]
# results: [ Failure ]

[ tag1 ] othertest [ Failure ]"""
    self.assertEqual(self.instance._GetNonRecentExpectationContent('', 1),
                     expected_content)

  def testNegativeGracePeriod(self):
    """Tests that setting a negative grace period disables filtering."""
    today_date = datetime.date.today()
    yesterday_date = today_date - datetime.timedelta(days=1)
    older_date = today_date - datetime.timedelta(days=2)
    today_str = today_date.isoformat()
    yesterday_str = yesterday_date.isoformat()
    older_str = older_date.isoformat()
    # pylint: disable=line-too-long
    blame_output = """\
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  1)# tags: [ tag1 ]
98637cd80f8c15 (Some R. Author    {yesterday_date} 00:00:00 +0000  2)# tags: [ tag2 ]
3fcadac9d861d0 (Some R. Author    {older_date} 00:00:00 +0000  3)# results: [ Failure ]
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  4)
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  5)crbug.com/1234 [ tag1 ] testname [ Failure ]
98637cd80f8c15 (Some R. Author    {yesterday_date} 00:00:00 +0000  6)[ tag2 ] testname [ Failure ] # Comment
3fcadac9d861d0 (Some R. Author    {older_date} 00:00:00 +0000  7)[ tag1 ] othertest [ Failure ]"""
    # pylint: enable=line-too-long
    blame_output = blame_output.format(today_date=today_str,
                                       yesterday_date=yesterday_str,
                                       older_date=older_str)
    self._output_mock.return_value = blame_output.encode('utf-8')

    expected_content = """\
# tags: [ tag1 ]
# tags: [ tag2 ]
# results: [ Failure ]

crbug.com/1234 [ tag1 ] testname [ Failure ]
[ tag2 ] testname [ Failure ] # Comment
[ tag1 ] othertest [ Failure ]"""
    self.assertEqual(self.instance._GetNonRecentExpectationContent('', -1),
                     expected_content)


class RemoveExpectationsFromFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.instance = uu.CreateGenericExpectations()
    self.header = self.instance._GetExpectationFileTagHeader(None)
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
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set(['crbug.com/1234']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemovalWithMultipleBugs(self):
    """Tests removal of expectations with multiple associated bugs."""
    contents = self.header + """

# This is a test comment
crbug.com/1234 crbug.com/3456 crbug.com/4567 [ win ] foo/test [ Failure ]
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
[ linux ] bar/test [ RetryOnFailure ]
[ win ] bar/test [ RetryOnFailure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/1234 crbug.com/3456 crbug.com/4567'),
    ]
    expected_contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
[ linux ] bar/test [ RetryOnFailure ]
[ win ] bar/test [ RetryOnFailure ]
"""
    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(
        removed_urls,
        set(['crbug.com/1234', 'crbug.com/3456', 'crbug.com/4567']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testNestedBlockComments(self):
    """Tests that nested disable block comments throw exceptions."""
    contents = self.header + """
# finder:disable-general
# finder:disable-general
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-general
# finder:enable-general
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      self.instance.RemoveExpectationsFromFile([], self.filename,
                                               expectations.RemovalType.STALE)

    contents = self.header + """
# finder:disable-general
# finder:disable-stale
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-stale
# finder:enable-genearl
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      self.instance.RemoveExpectationsFromFile([], self.filename,
                                               expectations.RemovalType.STALE)

    contents = self.header + """
# finder:enable-general
crbug.com/1234 [ win ] foo/test [ Failure ]
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    with self.assertRaises(RuntimeError):
      self.instance.RemoveExpectationsFromFile([], self.filename,
                                               expectations.RemovalType.STALE)

  def testGeneralBlockComments(self):
    """Tests that expectations in a disable block comment are not removed."""
    contents = self.header + """
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:disable-general
crbug.com/2345 [ win ] foo/test [ Failure ]
crbug.com/3456 [ win ] foo/test [ Failure ]
# finder:enable-general
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
# finder:disable-general
crbug.com/2345 [ win ] foo/test [ Failure ]
crbug.com/3456 [ win ] foo/test [ Failure ]
# finder:enable-general
"""
    for removal_type in (expectations.RemovalType.STALE,
                         expectations.RemovalType.UNUSED):
      with open(self.filename, 'w') as f:
        f.write(contents)
      removed_urls = self.instance.RemoveExpectationsFromFile(
          stale_expectations, self.filename, removal_type)
      self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/4567']))
      with open(self.filename) as f:
        self.assertEqual(f.read(), expected_contents)

  def testStaleBlockComments(self):
    """Tests that stale expectations in a stale disable block are not removed"""
    contents = self.header + """
crbug.com/1234 [ win ] not_stale [ Failure ]
crbug.com/1234 [ win ] before_block [ Failure ]
# finder:disable-stale
crbug.com/2345 [ win ] in_block [ Failure ]
# finder:enable-stale
crbug.com/3456 [ win ] after_block [ Failure ]
"""
    stale_expectations = [
        data_types.Expectation('before_block', ['win'], 'Failure',
                               'crbug.com/1234'),
        data_types.Expectation('in_block', ['win'], 'Failure',
                               'crbug.com/2345'),
        data_types.Expectation('after_block', ['win'], 'Failure',
                               'crbug.com/3456'),
    ]
    expected_contents = self.header + """
crbug.com/1234 [ win ] not_stale [ Failure ]
# finder:disable-stale
crbug.com/2345 [ win ] in_block [ Failure ]
# finder:enable-stale
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/3456']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testUnusedBlockComments(self):
    """Tests that stale expectations in unused disable blocks are not removed"""
    contents = self.header + """
crbug.com/1234 [ win ] not_unused [ Failure ]
crbug.com/1234 [ win ] before_block [ Failure ]
# finder:disable-unused
crbug.com/2345 [ win ] in_block [ Failure ]
# finder:enable-unused
crbug.com/3456 [ win ] after_block [ Failure ]
"""
    unused_expectations = [
        data_types.Expectation('before_block', ['win'], 'Failure',
                               'crbug.com/1234'),
        data_types.Expectation('in_block', ['win'], 'Failure',
                               'crbug.com/2345'),
        data_types.Expectation('after_block', ['win'], 'Failure',
                               'crbug.com/3456'),
    ]
    expected_contents = self.header + """
crbug.com/1234 [ win ] not_unused [ Failure ]
# finder:disable-unused
crbug.com/2345 [ win ] in_block [ Failure ]
# finder:enable-unused
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        unused_expectations, self.filename, expectations.RemovalType.UNUSED)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/3456']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testMismatchedBlockComments(self):
    """Tests that block comments for the wrong removal type do nothing."""
    contents = self.header + """
crbug.com/1234 [ win ] do_not_remove [ Failure ]
# finder:disable-stale
crbug.com/2345 [ win ] disabled_stale [ Failure ]
# finder:enable-stale
# finder:disable-unused
crbug.com/3456 [ win ] disabled_unused [ Failure ]
# finder:enable-unused
crbug.com/4567 [ win ] also_do_not_remove [ Failure ]
"""
    expectations_to_remove = [
        data_types.Expectation('disabled_stale', ['win'], 'Failure',
                               'crbug.com/2345'),
        data_types.Expectation('disabled_unused', ['win'], 'Failure',
                               'crbug.com/3456'),
    ]

    expected_contents = self.header + """
crbug.com/1234 [ win ] do_not_remove [ Failure ]
# finder:disable-stale
crbug.com/2345 [ win ] disabled_stale [ Failure ]
# finder:enable-stale
# finder:disable-unused
# finder:enable-unused
crbug.com/4567 [ win ] also_do_not_remove [ Failure ]
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        expectations_to_remove, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set(['crbug.com/3456']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

    expected_contents = self.header + """
crbug.com/1234 [ win ] do_not_remove [ Failure ]
# finder:disable-stale
# finder:enable-stale
# finder:disable-unused
crbug.com/3456 [ win ] disabled_unused [ Failure ]
# finder:enable-unused
crbug.com/4567 [ win ] also_do_not_remove [ Failure ]
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        expectations_to_remove, self.filename, expectations.RemovalType.UNUSED)
    self.assertEqual(removed_urls, set(['crbug.com/2345']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testInlineGeneralComments(self):
    """Tests that expectations with inline disable comments are not removed."""
    contents = self.header + """
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/2345 [ win ] foo/test [ Failure ]  # finder:disable-general
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
crbug.com/2345 [ win ] foo/test [ Failure ]  # finder:disable-general
"""
    for removal_type in (expectations.RemovalType.STALE,
                         expectations.RemovalType.UNUSED):
      with open(self.filename, 'w') as f:
        f.write(contents)
      removed_urls = self.instance.RemoveExpectationsFromFile(
          stale_expectations, self.filename, removal_type)
      self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/3456']))
      with open(self.filename) as f:
        self.assertEqual(f.read(), expected_contents)

  def testInlineStaleComments(self):
    """Tests that expectations with inline stale disable comments not removed"""
    contents = self.header + """
crbug.com/1234 [ win ] not_disabled [ Failure ]
crbug.com/2345 [ win ] stale_disabled [ Failure ]  # finder:disable-stale
crbug.com/3456 [ win ] unused_disabled [ Failure ]  # finder:disable-unused
"""
    stale_expectations = [
        data_types.Expectation('not_disabled', ['win'], 'Failure',
                               'crbug.com/1234'),
        data_types.Expectation('stale_disabled', ['win'], 'Failure',
                               'crbug.com/2345'),
        data_types.Expectation('unused_disabled', ['win'], 'Failure',
                               'crbug.com/3456')
    ]
    expected_contents = self.header + """
crbug.com/2345 [ win ] stale_disabled [ Failure ]  # finder:disable-stale
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/3456']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testInlineUnusedComments(self):
    """Tests that expectations with inline unused comments not removed"""
    contents = self.header + """
crbug.com/1234 [ win ] not_disabled [ Failure ]
crbug.com/2345 [ win ] stale_disabled [ Failure ]  # finder:disable-stale
crbug.com/3456 [ win ] unused_disabled [ Failure ]  # finder:disable-unused
"""
    stale_expectations = [
        data_types.Expectation('not_disabled', ['win'], 'Failure',
                               'crbug.com/1234'),
        data_types.Expectation('stale_disabled', ['win'], 'Failure',
                               'crbug.com/2345'),
        data_types.Expectation('unused_disabled', ['win'], 'Failure',
                               'crbug.com/3456')
    ]
    expected_contents = self.header + """
crbug.com/3456 [ win ] unused_disabled [ Failure ]  # finder:disable-unused
"""
    with open(self.filename, 'w') as f:
      f.write(contents)
    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.UNUSED)
    self.assertEqual(removed_urls, set(['crbug.com/1234', 'crbug.com/2345']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testGetDisableReasonFromComment(self):
    """Tests that the disable reason can be pulled from a line."""
    self.assertEqual(
        expectations._GetDisableReasonFromComment(
            '# finder:disable-general foo'), 'foo')
    self.assertEqual(
        expectations._GetDisableReasonFromComment(
            'crbug.com/1234 [ win ] bar/test [ Failure ]  '
            '# finder:disable-general foo'), 'foo')


class GetExpectationLineUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = uu.CreateGenericExpectations()

  def testNoMatchingExpectation(self):
    """Tests that the case of no matching expectation is handled."""
    expectation = data_types.Expectation('foo', ['win'], 'Failure')
    line, line_number = self.instance._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS, None)
    self.assertIsNone(line)
    self.assertIsNone(line_number)

  def testMatchingExpectation(self):
    """Tests that matching expectations are found."""
    expectation = data_types.Expectation('foo/test', ['win'], 'Failure',
                                         'crbug.com/1234')
    line, line_number = self.instance._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS, None)
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
    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      f.write(SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS)
      self.secondary_filename = f.name

  def testEmptyExpectationMap(self):
    """Tests that an empty expectation map results in a no-op."""
    modified_urls = self.instance.ModifySemiStaleExpectations(
        data_types.TestExpectationMap())
    self.assertEqual(modified_urls, set())
    self._input_mock.assert_not_called()
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)

  def testRemoveExpectation(self):
    """Tests that specifying to remove an expectation does so."""
    self._input_mock.return_value = 'r'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
        self.secondary_filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['mac'], 'Failure', 'crbug.com/4567'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
        test_expectation_map)
    self.assertEqual(modified_urls, set(['crbug.com/1234', 'crbug.com/4567']))
    expected_file_contents = """\
# tags: [ win linux ]
# results: [ Failure RetryOnFailure Skip Pass ]
crbug.com/5678 crbug.com/6789 [ win ] foo/another/test [ RetryOnFailure ]

[ linux ] foo/test [ Failure ]

crbug.com/2345 [ linux ] bar/* [ RetryOnFailure ]
crbug.com/3456 [ linux ] some/bad/test [ Skip ]
crbug.com/4567 [ linux ] some/good/test [ Pass ]
"""
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_file_contents)
    expected_file_contents = """\
# tags: [ mac ]
# results: [ Failure ]

"""
    with open(self.secondary_filename) as f:
      self.assertEqual(f.read(), expected_file_contents)

  def testModifyExpectation(self):
    """Tests that specifying to modify an expectation does not remove it."""
    self._input_mock.return_value = 'm'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
        self.secondary_filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['mac'], 'Failure', 'crbug.com/4567',
            ): data_types.BuilderStepMap()
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
        test_expectation_map)
    self.assertEqual(modified_urls, set(['crbug.com/1234', 'crbug.com/4567']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)
    with open(self.secondary_filename) as f:
      self.assertEqual(f.read(), SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS)

  def testModifyExpectationMultipleBugs(self):
    """Tests that modifying an expectation with multiple bugs works properly."""
    self._input_mock.return_value = 'm'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/another/test', ['win'], 'RetryOnFailure',
                'crbug.com/5678 crbug.com/6789'):
            data_types.BuilderStepMap(),
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
        test_expectation_map)
    self.assertEqual(modified_urls, set(['crbug.com/5678', 'crbug.com/6789']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)
    with open(self.secondary_filename) as f:
      self.assertEqual(f.read(), SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS)

  def testIgnoreExpectation(self):
    """Tests that specifying to ignore an expectation does nothing."""
    self._input_mock.return_value = 'i'
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap(),
        }),
        self.secondary_filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['mac'], 'Failure', 'crbug.com/4567',
            ): data_types.BuilderStepMap()
        }),
    })
    # yapf: enable
    modified_urls = self.instance.ModifySemiStaleExpectations(
        test_expectation_map)
    self.assertEqual(modified_urls, set())
    with open(self.filename) as f:
      self.assertEqual(f.read(), FAKE_EXPECTATION_FILE_CONTENTS)
    with open(self.secondary_filename) as f:
      self.assertEqual(f.read(), SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS)

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
          self.filename:
          data_types.ExpectationBuilderMap({
              data_types.Expectation(
                  'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
              data_types.BuilderStepMap(),
          }),
      })
      # yapf: enable
      self.instance.ModifySemiStaleExpectations(test_expectation_map)
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
        'GetExpectationFilepaths',
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
