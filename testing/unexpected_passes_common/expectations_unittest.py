#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import tempfile
import unittest
from unittest import mock

from pyfakefs import fake_filesystem_unittest

from unexpected_passes_common import data_types
from unexpected_passes_common import expectations
from unexpected_passes_common import unittest_utils as uu

# Protected access is allowed for unittests.
# pylint: disable=protected-access

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

FAKE_EXPECTATION_FILE_CONTENTS_WITH_COMPLEX_TAGS = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""


class CreateTestExpectationMapUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = expectations.Expectations()

    self._expectation_content = {}
    self._content_patcher = mock.patch.object(
        self.instance, '_GetNonRecentExpectationContent')
    self._content_mock = self._content_patcher.start()
    self.addCleanup(self._content_patcher.stop)

    def SideEffect(filepath, _):
      return self._expectation_content[filepath]

    self._content_mock.side_effect = SideEffect

  def testExclusiveOr(self) -> None:
    """Tests that only one input can be specified."""
    with self.assertRaises(AssertionError):
      self.instance.CreateTestExpectationMap(None, None,
                                             datetime.timedelta(days=0))
    with self.assertRaises(AssertionError):
      self.instance.CreateTestExpectationMap('foo', ['bar'],
                                             datetime.timedelta(days=0))

  def testExpectationFile(self) -> None:
    """Tests reading expectations from an expectation file."""
    filename = '/tmp/foo'
    self._expectation_content[filename] = FAKE_EXPECTATION_FILE_CONTENTS
    expectation_map = self.instance.CreateTestExpectationMap(
        filename, None, datetime.timedelta(days=0))
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

  def testMultipleExpectationFiles(self) -> None:
    """Tests reading expectations from multiple files."""
    filename1 = '/tmp/foo'
    filename2 = '/tmp/bar'
    expectation_files = [filename1, filename2]
    self._expectation_content[filename1] = FAKE_EXPECTATION_FILE_CONTENTS
    self._expectation_content[
        filename2] = SECONDARY_FAKE_EXPECTATION_FILE_CONTENTS

    expectation_map = self.instance.CreateTestExpectationMap(
        expectation_files, None, datetime.timedelta(days=0))
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

  def testIndividualTests(self) -> None:
    """Tests reading expectations from a list of tests."""
    expectation_map = self.instance.CreateTestExpectationMap(
        None, ['foo/test', 'bar/*'], datetime.timedelta(days=0))
    expected_expectation_map = {
        '': {
            data_types.Expectation('foo/test', [], ['RetryOnFailure']): {},
            data_types.Expectation('bar/*', [], ['RetryOnFailure']): {},
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)
    self.assertIsInstance(expectation_map, data_types.TestExpectationMap)


class GetNonRecentExpectationContentUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = uu.CreateGenericExpectations()
    self._output_patcher = mock.patch(
        'unexpected_passes_common.expectations.subprocess.check_output')
    self._output_mock = self._output_patcher.start()
    self.addCleanup(self._output_patcher.stop)

  def testBasic(self) -> None:
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
3fcadac9d861d0 (Some R. Author    {older_date} 00:00:00 +0000  7)[ tag1 ] othertest [ Failure ]
5f03bc04975c04 (Some R. Author    {today_date} 00:00:00 +0000  8)crbug.com/2345 testname [ Failure ]
3fcadac9d861d0 (Some R. Author    {older_date} 00:00:00 +0000  9)crbug.com/3456 othertest [ Failure ]"""
    # pylint: enable=line-too-long
    blame_output = blame_output.format(today_date=today_str,
                                       yesterday_date=yesterday_str,
                                       older_date=older_str)
    self._output_mock.return_value = blame_output.encode('utf-8')

    expected_content = """\
# tags: [ tag1 ]
# tags: [ tag2 ]
# results: [ Failure ]

[ tag1 ] othertest [ Failure ]
crbug.com/3456 othertest [ Failure ]"""
    self.assertEqual(
        self.instance._GetNonRecentExpectationContent(
            '', datetime.timedelta(days=1)), expected_content)

  def testNegativeGracePeriod(self) -> None:
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
    self.assertEqual(
        self.instance._GetNonRecentExpectationContent(
            '', datetime.timedelta(days=-1)), expected_content)


class RemoveExpectationsFromFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.instance = uu.CreateGenericExpectations()
    self.header = self.instance._GetExpectationFileTagHeader(None)
    self.setUpPyfakefs()
    with tempfile.NamedTemporaryFile(delete=False) as f:
      self.filename = f.name

  def testExpectationRemoval(self) -> None:
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

  def testRemovalWithMultipleBugs(self) -> None:
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

  def testGeneralBlockComments(self) -> None:
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

  def testStaleBlockComments(self) -> None:
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

  def testUnusedBlockComments(self) -> None:
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

  def testMismatchedBlockComments(self) -> None:
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

  def testInlineGeneralComments(self) -> None:
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

  def testInlineStaleComments(self) -> None:
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

  def testInlineUnusedComments(self) -> None:
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

  def testGroupBlockAllRemovable(self):
    """Tests that a group with all members removable is removed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment

# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:group-end
[ win ] bar/test [ RetryOnFailure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/1234'),
        data_types.Expectation('bar/test', ['linux'], ['RetryOnFailure']),
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

  def testLargeGroupBlockAllRemovable(self):
    """Tests that a large group with all members removable is removed."""
    # This test exists because we've had issues that passed tests with
    # relatively small groups, but failed on larger ones.
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment

# finder:group-start some group name
[ linux ] a [ RetryOnFailure ]
[ linux ] b [ RetryOnFailure ]
[ linux ] c [ RetryOnFailure ]
[ linux ] d [ RetryOnFailure ]
[ linux ] e [ RetryOnFailure ]
[ linux ] f [ RetryOnFailure ]
[ linux ] g [ RetryOnFailure ]
[ linux ] h [ RetryOnFailure ]
[ linux ] i [ RetryOnFailure ]
[ linux ] j [ RetryOnFailure ]
[ linux ] k [ RetryOnFailure ]
# finder:group-end
[ win ] bar/test [ RetryOnFailure ]
"""

    stale_expectations = [
        data_types.Expectation('a', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('b', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('c', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('d', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('e', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('f', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('g', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('h', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('i', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('j', ['linux'], ['RetryOnFailure']),
        data_types.Expectation('k', ['linux'], ['RetryOnFailure']),
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
    self.assertEqual(removed_urls, set([]))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testNestedGroupAndNarrowingAllRemovable(self):
    """Tests that a disable block within a group can be properly removed."""
    contents = self.header + """
crbug.com/2345 [ win ] baz/test [ Failure ]

# Description
# finder:group-start name
# finder:disable-narrowing
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/1234 [ win ] bar/test [ Failure ]
# finder:enable-narrowing
# finder:group-end

crbug.com/3456 [ linux ] foo/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['win'], ['Failure'],
                               'crbug.com/1234'),
        data_types.Expectation('bar/test', ['win'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """
crbug.com/2345 [ win ] baz/test [ Failure ]


crbug.com/3456 [ linux ] foo/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set(['crbug.com/1234']))
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testGroupBlockNotAllRemovable(self):
    """Tests that a group with not all members removable is not removed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:group-end
[ win ] bar/test [ RetryOnFailure ]
"""

    stale_expectations = [
        data_types.Expectation('bar/test', ['linux'], ['RetryOnFailure'])
    ]

    expected_contents = contents

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set())
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testGroupSplitAllRemovable(self):
    """Tests that a split group with all members removable is removed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment

# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
# finder:group-end

# finder:group-start some group name
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:group-end
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

  def testGroupSplitNotAllRemovable(self):
    """Tests that a split group without all members removable is not removed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
# finder:group-end

# finder:group-start some group name
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:group-end
[ win ] bar/test [ RetryOnFailure ]
"""

    stale_expectations = [
        data_types.Expectation('bar/test', ['linux'], ['RetryOnFailure'])
    ]

    expected_contents = contents

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set())
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testGroupMultipleGroupsAllRemovable(self):
    """Tests that multiple groups with all members removable are removed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment

# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
# finder:group-end

# finder:group-start another group name
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:group-end
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

  def testGroupMultipleGroupsSomeRemovable(self):
    """Tests that multiple groups are handled separately."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment

# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
# finder:group-end

# finder:group-start another group name
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/1234 [ linux ] foo/test [ Failure ]
# finder:group-end
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


# finder:group-start another group name
crbug.com/1234 [ win ] foo/test [ Failure ]
crbug.com/1234 [ linux ] foo/test [ Failure ]
# finder:group-end
[ win ] bar/test [ RetryOnFailure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set())
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testNestedGroupStart(self):
    """Tests that nested groups are disallowed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
# finder:group-start some group name
[ linux ] bar/test [ RetryOnFailure ]
# finder:group-start another group name
# finder:group-end
# finder:group-end
[ win ] bar/test [ RetryOnFailure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    with self.assertRaisesRegex(RuntimeError,
                                'that is inside another group block'):
      self.instance.RemoveExpectationsFromFile([], self.filename,
                                               expectations.RemovalType.STALE)

  def testOrphanedGroupEnd(self):
    """Tests that orphaned group ends are disallowed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
# finder:group-end
[ linux ] bar/test [ RetryOnFailure ]
[ win ] bar/test [ RetryOnFailure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    with self.assertRaisesRegex(RuntimeError, 'without a group start comment'):
      self.instance.RemoveExpectationsFromFile([], self.filename,
                                               expectations.RemovalType.STALE)

  def testNoGroupName(self):
    """Tests that unnamed groups are disallowed."""
    contents = self.header + """

# This is a test comment
crbug.com/2345 [ win ] foo/test [ RetryOnFailure ]

# Another comment
# finder:group-start
# finder:group-end
[ linux ] bar/test [ RetryOnFailure ]
[ win ] bar/test [ RetryOnFailure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    with self.assertRaisesRegex(RuntimeError, 'did not have a group name'):
      self.instance.RemoveExpectationsFromFile([], self.filename,
                                               expectations.RemovalType.STALE)

  def testRemoveCommentBlockSimpleTrailingWhitespace(self):
    """Tests stale comment removal in a simple case with trailing whitespace."""
    contents = self.header + """
# Comment line 1
# Comment line 2
crbug.com/1234 [ linux ] foo/test [ Failure ]

crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """

crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockSimpleTrailingComment(self):
    """Tests stale comment removal in a simple case with trailing comment."""
    contents = self.header + """
# Comment line 1
# Comment line 2
crbug.com/1234 [ linux ] foo/test [ Failure ]
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockSimpleEndOfFile(self):
    """Tests stale comment removal in a simple case at file end."""
    contents = self.header + """
crbug.com/2345 [ win ] bar/test [ Failure ]

# Comment line 1
# Comment line 2
crbug.com/1234 [ linux ] foo/test [ Failure ]"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """
crbug.com/2345 [ win ] bar/test [ Failure ]

"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockWithAnnotations(self):
    """Tests stale comment removal with annotations on both ends."""
    contents = self.header + """
# Comment line 1
# Comment line 2
# finder:disable-unused
crbug.com/1234 [ linux ] foo/test [ Failure ]
# finder:enable-unused
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockWithMissingTrailingAnnotation(self):
    """Tests stale comment removal with a missing trailing annotation."""
    contents = self.header + """
# Comment line 1
# Comment line 2
# finder:disable-unused
crbug.com/1234 [ linux ] foo/test [ Failure ]

crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-unused

# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """
# Comment line 1
# Comment line 2
# finder:disable-unused

crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-unused

# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockWithMissingStartAnnotation(self):
    """Tests stale comment removal with a missing start annotation."""
    contents = self.header + """
# finder:disable-unused
crbug.com/1234 [ win ] foo/test [ Failure ]
# Comment line 1
# Comment line 2
crbug.com/1234 [ linux ] foo/test [ Failure ]
# finder:enable-unused
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
    ]

    expected_contents = self.header + """
# finder:disable-unused
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-unused
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockMultipleExpectations(self):
    """Tests stale comment removal with multiple expectations in a block."""
    contents = self.header + """
# Comment line 1
# Comment line 2
# finder:disable-unused
crbug.com/1234 [ linux ] foo/test [ Failure ]
crbug.com/3456 [ mac ] foo/test [ Failure ]
# finder:enable-unused

# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
        data_types.Expectation('foo/test', ['mac'], ['Failure'],
                               'crbug.com/3456'),
    ]

    expected_contents = self.header + """

# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234', 'crbug.com/3456'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveCommentBlockMultipleBlocks(self):
    """Tests stale comment removal with expectations in multiple blocks."""
    contents = self.header + """
# Comment line 1
# Comment line 2
# finder:disable-unused
crbug.com/1234 [ linux ] foo/test [ Failure ]
# finder:enable-unused

# Comment line 4
# finder:disable-unused
crbug.com/3456 [ mac ] foo/test [ Failure ]
# finder:enable-unused
# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    stale_expectations = [
        data_types.Expectation('foo/test', ['linux'], ['Failure'],
                               'crbug.com/1234'),
        data_types.Expectation('foo/test', ['mac'], ['Failure'],
                               'crbug.com/3456'),
    ]

    expected_contents = self.header + """

# Comment line 3
crbug.com/2345 [ win ] bar/test [ Failure ]
"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, {'crbug.com/1234', 'crbug.com/3456'})
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testRemoveStaleAnnotationBlocks(self):
    """Tests removal of annotation blocks not associated with removals."""
    contents = self.header + """
# finder:disable-general
# finder:enable-general

# finder:disable-stale

# finder:enable-stale

# finder:disable-unused
# comment
# finder:enable-unused

# finder:disable-narrowing description
# comment
# finder:enable-narrowing

# finder:group-start name
# finder:group-end
"""

    stale_expectations = []

    expected_contents = self.header + """




"""

    with open(self.filename, 'w') as f:
      f.write(contents)

    removed_urls = self.instance.RemoveExpectationsFromFile(
        stale_expectations, self.filename, expectations.RemovalType.STALE)
    self.assertEqual(removed_urls, set())
    with open(self.filename) as f:
      self.assertEqual(f.read(), expected_contents)

  def testGroupNameExtraction(self):
    """Tests that group names are properly extracted."""
    group_name = expectations._GetGroupNameFromCommentLine(
        '# finder:group-start group name')
    self.assertEqual(group_name, 'group name')


class GetDisableAnnotatedExpectationsFromFileUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = uu.CreateGenericExpectations()

  def testNestedBlockComments(self) -> None:
    """Tests that nested disable block comments throw exceptions."""
    contents = """
# finder:disable-general
# finder:disable-general
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-general
# finder:enable-general
"""
    with self.assertRaises(RuntimeError):
      self.instance._GetDisableAnnotatedExpectationsFromFile(
          'expectation_file', contents)

    contents = """
# finder:disable-general
# finder:disable-stale
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-stale
# finder:enable-general
"""
    with self.assertRaises(RuntimeError):
      self.instance._GetDisableAnnotatedExpectationsFromFile(
          'expectation_file', contents)

    contents = """
# finder:enable-general
crbug.com/1234 [ win ] foo/test [ Failure ]
"""
    with self.assertRaises(RuntimeError):
      self.instance._GetDisableAnnotatedExpectationsFromFile(
          'expectation_file', contents)

  def testBlockComments(self) -> None:
    """Tests that disable block comments are properly parsed."""
    contents = """
# finder:disable-general general-reason
crbug.com/1234 [ win ] foo/test [ Failure ]
# finder:enable-general

# finder:disable-stale
crbug.com/1234 [ mac ] foo/test [ Failure ]
# finder:enable-stale

# finder:disable-unused unused reason
crbug.com/1234 [ linux ] foo/test [ Failure ]
# finder:enable-unused

# finder:disable-narrowing
crbug.com/1234 [ win ] bar/test [ Failure ]
# finder:enable-narrowing

crbug.com/1234 [ mac ] bar/test [ Failure ]
"""
    annotated_expectations = (
        self.instance._GetDisableAnnotatedExpectationsFromFile(
            'expectation_file', contents))
    self.assertEqual(len(annotated_expectations), 4)
    self.assertEqual(
        annotated_expectations[data_types.Expectation('foo/test', ['win'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-general', 'general-reason'))
    self.assertEqual(
        annotated_expectations[data_types.Expectation('foo/test', ['mac'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-stale', ''))
    self.assertEqual(
        annotated_expectations[data_types.Expectation('foo/test', ['linux'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-unused', 'unused reason'))
    self.assertEqual(
        annotated_expectations[data_types.Expectation('bar/test', ['win'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-narrowing', ''))

  def testInlineComments(self) -> None:
    """Tests that inline disable comments are properly parsed."""
    # pylint: disable=line-too-long
    contents = """
crbug.com/1234 [ win ] foo/test [ Failure ]  # finder:disable-general general-reason

crbug.com/1234 [ mac ] foo/test [ Failure ]  # finder:disable-stale

crbug.com/1234 [ linux ] foo/test [ Failure ]  # finder:disable-unused unused reason

crbug.com/1234 [ win ] bar/test [ Failure ]  # finder:disable-narrowing

crbug.com/1234 [ mac ] bar/test [ Failure ]
"""
    # pylint: enable=line-too-long
    annotated_expectations = (
        self.instance._GetDisableAnnotatedExpectationsFromFile(
            'expectation_file', contents))
    self.assertEqual(len(annotated_expectations), 4)
    self.assertEqual(
        annotated_expectations[data_types.Expectation('foo/test', ['win'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-general', 'general-reason'))
    self.assertEqual(
        annotated_expectations[data_types.Expectation('foo/test', ['mac'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-stale', ''))
    self.assertEqual(
        annotated_expectations[data_types.Expectation('foo/test', ['linux'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-unused', 'unused reason'))
    self.assertEqual(
        annotated_expectations[data_types.Expectation('bar/test', ['win'],
                                                      'Failure',
                                                      'crbug.com/1234')],
        ('-narrowing', ''))


class GetExpectationLineUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = uu.CreateGenericExpectations()

  def testNoMatchingExpectation(self) -> None:
    """Tests that the case of no matching expectation is handled."""
    expectation = data_types.Expectation('foo', ['win'], 'Failure')
    line, line_number = self.instance._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS, 'expectation_file')
    self.assertIsNone(line)
    self.assertIsNone(line_number)

  def testMatchingExpectation(self) -> None:
    """Tests that matching expectations are found."""
    expectation = data_types.Expectation('foo/test', ['win'], 'Failure',
                                         'crbug.com/1234')
    line, line_number = self.instance._GetExpectationLine(
        expectation, FAKE_EXPECTATION_FILE_CONTENTS, 'expectation_file')
    self.assertEqual(line, 'crbug.com/1234 [ win ] foo/test [ Failure ]')
    self.assertEqual(line_number, 3)


class FilterToMostSpecificTypTagsUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self._expectations = uu.CreateGenericExpectations()
    self.setUpPyfakefs()
    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      self.filename = f.name

  def testBasic(self) -> None:
    """Tests that only the most specific tags are kept."""
    expectation_file_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
"""
    with open(self.filename, 'w') as outfile:
      outfile.write(expectation_file_contents)
    tags = frozenset(['win', 'nvidia', 'nvidia-0x1111', 'release'])
    filtered_tags = self._expectations._FilterToMostSpecificTypTags(
        tags, self.filename)
    self.assertEqual(filtered_tags, set(['win', 'nvidia-0x1111', 'release']))

  def testSingleTags(self) -> None:
    """Tests that functionality works with single tags."""
    expectation_file_contents = """\
# tags: [ tag1_most_specific ]
# tags: [ tag2_most_specific ]"""
    with open(self.filename, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = frozenset(['tag1_most_specific', 'tag2_most_specific'])
    filtered_tags = self._expectations._FilterToMostSpecificTypTags(
        tags, self.filename)
    self.assertEqual(filtered_tags, tags)

  def testUnusedTags(self) -> None:
    """Tests that functionality works as expected with extra/unused tags."""
    expectation_file_contents = """\
# tags: [ tag1_least_specific tag1_middle_specific tag1_most_specific ]
# tags: [ tag2_least_specific tag2_middle_specific tag2_most_specific ]
# tags: [ some_unused_tag ]"""
    with open(self.filename, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = frozenset([
        'tag1_least_specific', 'tag1_most_specific', 'tag2_middle_specific',
        'tag2_least_specific'
    ])
    filtered_tags = self._expectations._FilterToMostSpecificTypTags(
        tags, self.filename)
    self.assertEqual(filtered_tags,
                     set(['tag1_most_specific', 'tag2_middle_specific']))

  def testMissingTags(self) -> None:
    """Tests that a file not having all tags is an error."""
    expectation_file_contents = """\
# tags: [ tag1_least_specific tag1_middle_specific ]
# tags: [ tag2_least_specific tag2_middle_specific tag2_most_specific ]"""
    with open(self.filename, 'w') as outfile:
      outfile.write(expectation_file_contents)

    tags = frozenset([
        'tag1_least_specific', 'tag1_most_specific', 'tag2_middle_specific',
        'tag2_least_specific'
    ])
    with self.assertRaisesRegex(RuntimeError, r'.*tag1_most_specific.*'):
      self._expectations._FilterToMostSpecificTypTags(tags, self.filename)

  def testTagsLowerCased(self) -> None:
    """Tests that found tags are lower cased to match internal tags."""
    expectation_file_contents = """\
# tags: [ Win Win10
#         Linux
#         Mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
"""
    with open(self.filename, 'w') as outfile:
      outfile.write(expectation_file_contents)
    tags = frozenset(['win', 'win10', 'nvidia', 'release'])
    filtered_tags = self._expectations._FilterToMostSpecificTypTags(
        tags, self.filename)
    self.assertEqual(filtered_tags, set(['win10', 'nvidia', 'release']))


class NarrowSemiStaleExpectationScopeUnittest(fake_filesystem_unittest.TestCase
                                              ):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self.instance = uu.CreateGenericExpectations()

    with tempfile.NamedTemporaryFile(delete=False, mode='w') as f:
      f.write(FAKE_EXPECTATION_FILE_CONTENTS_WITH_COMPLEX_TAGS)
      self.filename = f.name

  def testEmptyExpectationMap(self) -> None:
    """Tests that scope narrowing with an empty map is a no-op."""
    urls = self.instance.NarrowSemiStaleExpectationScope(
        data_types.TestExpectationMap({}))
    self.assertEqual(urls, set())
    with open(self.filename) as infile:
      self.assertEqual(infile.read(),
                       FAKE_EXPECTATION_FILE_CONTENTS_WITH_COMPLEX_TAGS)

  def testWildcard(self) -> None:
    """Regression test to ensure that wildcards are modified correctly."""
    file_contents = """\
# tags: [ win ]
# tags: [ amd intel ]
# results: [ Failure ]

crbug.com/1234 [ win ] foo/bar* [ Failure ]
"""
    with open(self.filename, 'w') as f:
      f.write(file_contents)

    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/bar*', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yap: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win ]
# tags: [ amd intel ]
# results: [ Failure ]

crbug.com/1234 [ intel win ] foo/bar* [ Failure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, {'crbug.com/1234'})

  def testMultipleSteps(self) -> None:
    """Tests that scope narrowing works across multiple steps."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ intel win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testMultipleBuilders(self) -> None:
    """Tests that scope narrowing works across multiple builders."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_amd_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                }),
                'win_intel_builder':
                data_types.StepBuildStatsMap({
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ intel win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testMultipleExpectations(self) -> None:
    """Tests that scope narrowing works across multiple expectations."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    failed_amd_stats = data_types.BuildStats()
    failed_amd_stats.AddFailedBuild('1', frozenset(['win', 'amd']))
    multi_amd_stats = data_types.BuildStats()
    multi_amd_stats.AddFailedBuild('1', frozenset(['win', 'amd', 'debug']))
    multi_amd_stats.AddFailedBuild('1', frozenset(['win', 'amd', 'release']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    debug_stats = data_types.BuildStats()
    debug_stats.AddFailedBuild('1', frozenset(['linux', 'debug']))
    release_stats = data_types.BuildStats()
    release_stats.AddPassedBuild(frozenset(['linux', 'release']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
            # These two expectations are here to ensure that our continue logic
            # works as expected when we hit cases we can't handle, i.e. that
            # later expectations are still handled properly.
            data_types.Expectation('bar/test', ['win'], 'Failure', ''):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'win1': amd_stats,
                    'win2': failed_amd_stats,
                }),
            }),
            data_types.Expectation('baz/test', ['win'], 'Failure', ''):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'win1': amd_stats,
                    'win2': multi_amd_stats,
                }),
            }),
            data_types.Expectation(
                'foo/test', ['linux'], 'RetryOnFailure', 'crbug.com/2345'):
            data_types.BuilderStepMap({
                'linux_builder':
                data_types.StepBuildStatsMap({
                    'debug': debug_stats,
                    'release': release_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ intel win ] foo/test [ Failure ]
crbug.com/2345 [ debug linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234', 'crbug.com/2345']))

  def testMultipleOutputLines(self) -> None:
    """Tests that scope narrowing works with multiple output lines."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    nvidia_stats = data_types.BuildStats()
    nvidia_stats.AddFailedBuild('1', frozenset(['win', 'nvidia']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_amd_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                }),
                'win_intel_builder':
                data_types.StepBuildStatsMap({
                    'intel': intel_stats,
                }),
                'win_nvidia_builder':
                data_types.StepBuildStatsMap({
                    'nvidia': nvidia_stats,
                })
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ intel win ] foo/test [ Failure ]
crbug.com/1234 [ nvidia win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testMultipleTagSets(self) -> None:
    """Tests that multiple tag sets result in a scope narrowing no-op."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd', 'release']))
    amd_stats.AddPassedBuild(frozenset(['win', 'amd', 'debug']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    with open(self.filename) as infile:
      self.assertEqual(infile.read(),
                       FAKE_EXPECTATION_FILE_CONTENTS_WITH_COMPLEX_TAGS)
    self.assertEqual(urls, set())

  def testAmbiguousTags(self):
    """Tests that ambiguous tag sets result in a scope narrowing no-op."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    bad_amd_stats = data_types.BuildStats()
    bad_amd_stats.AddFailedBuild('1', frozenset(['win', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                    'bad_amd': bad_amd_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    with open(self.filename) as infile:
      self.assertEqual(infile.read(),
                       FAKE_EXPECTATION_FILE_CONTENTS_WITH_COMPLEX_TAGS)
    self.assertEqual(urls, set())

  def testRemoveCommonTags(self) -> None:
    """Tests that scope narrowing removes common/redundant tags."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd', 'desktop']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel', 'desktop']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ intel win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, {'crbug.com/1234'})

  def testConsolidateKnownOverlappingTags(self) -> None:
    """Tests that scope narrowing consolidates known overlapping tags."""

    # This specific example emulates a dual GPU machine where we remove the
    # integrated GPU tag.
    def SideEffect(tags):
      return tags - {'intel'}

    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd']))
    nvidia_dgpu_intel_igpu_stats = data_types.BuildStats()
    nvidia_dgpu_intel_igpu_stats.AddFailedBuild(
        '1', frozenset(['win', 'nvidia', 'intel']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'dual_gpu': nvidia_dgpu_intel_igpu_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    with mock.patch.object(self.instance,
                           '_ConsolidateKnownOverlappingTags',
                           side_effect=SideEffect):
      urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ nvidia win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testFilterToSpecificTags(self) -> None:
    """Tests that scope narrowing filters to the most specific tags."""
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd', 'amd-0x1111']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['win', 'intel', 'intel-0x2222']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ intel-0x2222 win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testSupersetsRemoved(self) -> None:
    """Tests that superset tags (i.e. conflicts) are omitted."""
    # These stats are set up so that the raw new tag sets are:
    # [{win, amd}, {win, amd, debug}] since the passed Intel build also has
    # "release". Thus, if we aren't correctly filtering out supersets, we'll
    # end up with [ amd win ] and [ amd debug win ] in the expectation file
    # instead of just [ amd win ].
    amd_release_stats = data_types.BuildStats()
    amd_release_stats.AddFailedBuild('1', frozenset(['win', 'amd', 'release']))
    amd_debug_stats = data_types.BuildStats()
    amd_debug_stats.AddFailedBuild('1', frozenset(['win', 'amd', 'debug']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddPassedBuild(frozenset(['win', 'intel', 'release']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd_release': amd_release_stats,
                    'amd_debug': amd_debug_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ amd win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testNoPassingOverlap(self):
    """Tests that scope narrowing works with no overlap between passing tags."""
    # There is no commonality between [ amd debug ] and [ intel release ], so
    # the resulting expectation we generate should just be the tags from the
    # failed build.
    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['win', 'amd', 'debug']))
    intel_stats_debug = data_types.BuildStats()
    intel_stats_debug.AddFailedBuild('1', frozenset(['win', 'intel', 'debug']))
    intel_stats_release = data_types.BuildStats()
    intel_stats_release.AddPassedBuild(frozenset(['win', 'intel', 'release']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel_debug': intel_stats_debug,
                    'intel_release': intel_stats_release,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ debug intel win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testMultipleOverlap(self):
    """Tests that scope narrowing works with multiple potential overlaps."""
    # [ win intel debug ], [ win intel release ], and [ win amd debug ] each
    # have 2/3 tags overlapping with each other, so we expect one pair to be
    # simplified and the other to remain the same.
    intel_debug_stats = data_types.BuildStats()
    intel_debug_stats.AddFailedBuild('1', frozenset(['win', 'intel', 'debug']))
    intel_release_stats = data_types.BuildStats()
    intel_release_stats.AddFailedBuild('1',
                                       frozenset(['win', 'intel', 'release']))
    amd_debug_stats = data_types.BuildStats()
    amd_debug_stats.AddFailedBuild('1', frozenset(['win', 'amd', 'debug']))
    amd_release_stats = data_types.BuildStats()
    amd_release_stats.AddPassedBuild(frozenset(['win', 'amd', 'release']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['win'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'win_builder':
                data_types.StepBuildStatsMap({
                    'amd_debug': amd_debug_stats,
                    'amd_release': amd_release_stats,
                    'intel_debug': intel_debug_stats,
                    'intel_release': intel_release_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    # Python sets are not stable between different processes due to random hash
    # seeds that are on by default. Since there are two valid ways to simplify
    # the tags we provided, this means that the test is flaky if we only check
    # for one due to the non-deterministic order the tags are processed, so
    # instead, accept either valid output.
    #
    # Random hash seeds can be disabled by setting PYTHONHASHSEED, but that
    # requires that we either ensure that this test is always run with that set
    # (difficult/error-prone), or we manually set the seed and recreate the
    # process (hacky). Simply accepting either valid value instead of trying to
    # force a certain order seems like the better approach.
    expected_contents1 = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ amd debug win ] foo/test [ Failure ]
crbug.com/1234 [ intel win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    expected_contents2 = """\
# tags: [ win win10
#         linux
#         mac ]
# tags: [ nvidia nvidia-0x1111
#         intel intel-0x2222
#         amd amd-0x3333]
# tags: [ release debug ]
# results: [ Failure RetryOnFailure ]

crbug.com/1234 [ debug win ] foo/test [ Failure ]
crbug.com/1234 [ intel release win ] foo/test [ Failure ]
crbug.com/2345 [ linux ] foo/test [ RetryOnFailure ]
"""
    with open(self.filename) as infile:
      self.assertIn(infile.read(), (expected_contents1, expected_contents2))
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testMultipleOverlapRepeatedIntersection(self):
    """Edge case where intersection checks need to be repeated to work."""
    original_contents = """\
# tags: [ mac
#         win ]
# tags: [ amd amd-0x3333
#         intel intel-0x2222 intel-0x4444
#         nvidia nvidia-0x1111 ]
# results: [ Failure ]

crbug.com/1234 foo/test [ Failure ]
"""
    with open(self.filename, 'w') as outfile:
      outfile.write(original_contents)
    amd_stats = data_types.BuildStats()
    amd_stats.AddFailedBuild('1', frozenset(['mac', 'amd', 'amd-0x3333']))
    intel_stats_1 = data_types.BuildStats()
    intel_stats_1.AddFailedBuild('1',
                                 frozenset(['mac', 'intel', 'intel-0x2222']))
    intel_stats_2 = data_types.BuildStats()
    intel_stats_2.AddFailedBuild('1',
                                 frozenset(['mac', 'intel', 'intel-0x4444']))
    nvidia_stats = data_types.BuildStats()
    nvidia_stats.AddPassedBuild(frozenset(['win', 'nvidia', 'nvidia-0x1111']))

    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', [], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'mixed_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel_1': intel_stats_1,
                    'intel_2': intel_stats_2,
                    'nvidia': nvidia_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ mac
#         win ]
# tags: [ amd amd-0x3333
#         intel intel-0x2222 intel-0x4444
#         nvidia nvidia-0x1111 ]
# results: [ Failure ]

crbug.com/1234 [ mac ] foo/test [ Failure ]
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testBlockDisableAnnotation(self) -> None:
    """Tests that narrowing is skipped if block annotations are present."""
    original_contents = """\
# tags: [ mac ]
# tags: [ amd intel ]
# results: [ Failure ]

crbug.com/1234 [ mac ] foo/test [ Failure ]
# finder:disable-narrowing
crbug.com/2345 [ mac ] bar/test [ Failure ]
# finder:enable-narrowing
"""
    with open(self.filename, 'w') as outfile:
      outfile.write(original_contents)

    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['mac', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['mac', 'intel']))

    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['mac'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'mac_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
            data_types.Expectation(
                'bar/test', ['mac'], 'Failure', 'crbug.com/2345'):
            data_types.BuilderStepMap({
                'mac_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ mac ]
# tags: [ amd intel ]
# results: [ Failure ]

crbug.com/1234 [ intel mac ] foo/test [ Failure ]
# finder:disable-narrowing
crbug.com/2345 [ mac ] bar/test [ Failure ]
# finder:enable-narrowing
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))

  def testNoOverlapsInNarrowedExpectations(self):
    """Tests that scope narrowing does not produce overlapping tag sets."""
    original_contents = """\
# tags: [ Linux
#         Mac Mac10.15 Mac11 Mac11-arm64 Mac12 Mac12-arm64
#         Win Win10.20h2 Win11 ]
# tags: [ Release Debug ]
# results: [ Failure ]

crbug.com/874695 foo/test [ Failure ]
"""
    with open(self.filename, 'w') as outfile:
      outfile.write(original_contents)

    linux_debug_stats = data_types.BuildStats()
    linux_debug_stats.AddPassedBuild(frozenset(['debug', 'linux']))
    linux_release_stats = data_types.BuildStats()
    linux_release_stats.AddFailedBuild('1', frozenset(['linux', 'release']))
    mac10_release_stats = data_types.BuildStats()
    mac10_release_stats.AddFailedBuild(
        '1', frozenset(['mac', 'mac10.15', 'release']))
    mac11_arm_release_stats = data_types.BuildStats()
    mac11_arm_release_stats.AddFailedBuild(
        '1', frozenset(['mac', 'mac11-arm64', 'release']))
    mac11_release_stats = data_types.BuildStats()
    mac11_release_stats.AddFailedBuild('1',
                                       frozenset(['mac', 'mac11', 'release']))
    mac12_arm_release_stats = data_types.BuildStats()
    mac12_arm_release_stats.AddFailedBuild(
        '1', frozenset(['mac', 'mac12-arm64', 'release']))
    mac12_debug_stats = data_types.BuildStats()
    mac12_debug_stats.AddFailedBuild('1', frozenset(['debug', 'mac', 'mac12']))
    mac12_release_stats = data_types.BuildStats()
    mac12_release_stats.AddFailedBuild('1',
                                       frozenset(['mac', 'mac12', 'release']))
    win10_release_stats = data_types.BuildStats()
    win10_release_stats.AddFailedBuild(
        '1', frozenset(['release', 'win', 'win10.20h2']))
    win11_release_stats = data_types.BuildStats()
    win11_release_stats.AddFailedBuild('1',
                                       frozenset(['release', 'win', 'win11']))
    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test',
                [], 'Failure', 'crbug.com/874695'):
            data_types.BuilderStepMap({
                'Linux Tests (dbg)(1)':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': linux_debug_stats,
                }),
                'mac11-arm64-rel-tests':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': mac11_arm_release_stats,
                }),
                'Mac11 Tests':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': mac11_release_stats,
                }),
                'mac12-arm64-rel-tests':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': mac12_arm_release_stats,
                }),
                'Mac12 Tests (dbg)':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': mac12_debug_stats,
                }),
                'Mac12 Tests':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': mac12_release_stats,
                }),
                'Linux Tests':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': linux_release_stats,
                }),
                'WebKit Win10':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': win10_release_stats,
                }),
                'Win11 Tests x64':
                data_types.StepBuildStatsMap({
                    'blink_web_tests': win11_release_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    # Python sets are not stable between different processes due to random hash
    # seeds that are on by default. Since there are two valid ways to simplify
    # the tags we provided, this means that the test is flaky if we only check
    # for one due to the non-deterministic order the tags are processed, so
    # instead, accept either valid output.
    #
    # Random hash seeds can be disabled by setting PYTHONHASHSEED, but that
    # requires that we either ensure that this test is always run with that set
    # (difficult/error-prone), or we manually set the seed and recreate the
    # process (hacky). Simply accepting either valid value instead of trying to
    # force a certain order seems like the better approach.
    expected_contents1 = """\
# tags: [ Linux
#         Mac Mac10.15 Mac11 Mac11-arm64 Mac12 Mac12-arm64
#         Win Win10.20h2 Win11 ]
# tags: [ Release Debug ]
# results: [ Failure ]

crbug.com/874695 [ debug mac12 ] foo/test [ Failure ]
crbug.com/874695 [ release ] foo/test [ Failure ]
"""
    expected_contents2 = """\
# tags: [ Linux
#         Mac Mac10.15 Mac11 Mac11-arm64 Mac12 Mac12-arm64
#         Win Win10.20h2 Win11 ]
# tags: [ Release Debug ]
# results: [ Failure ]

crbug.com/874695 [ linux release ] foo/test [ Failure ]
crbug.com/874695 [ mac ] foo/test [ Failure ]
crbug.com/874695 [ release win ] foo/test [ Failure ]
"""
    with open(self.filename) as infile:
      self.assertIn(infile.read(), (expected_contents1, expected_contents2))
    self.assertEqual(urls, set(['crbug.com/874695']))

  def testInlineDisableAnnotation(self) -> None:
    """Tests that narrowing is skipped if inline annotations are present."""
    original_contents = """\
# tags: [ mac ]
# tags: [ amd intel ]
# results: [ Failure ]

crbug.com/1234 [ mac ] foo/test [ Failure ]
crbug.com/2345 [ mac ] bar/test [ Failure ]  # finder:disable-narrowing
"""
    with open(self.filename, 'w') as outfile:
      outfile.write(original_contents)

    amd_stats = data_types.BuildStats()
    amd_stats.AddPassedBuild(frozenset(['mac', 'amd']))
    intel_stats = data_types.BuildStats()
    intel_stats.AddFailedBuild('1', frozenset(['mac', 'intel']))

    # yapf: disable
    test_expectation_map = data_types.TestExpectationMap({
        self.filename:
        data_types.ExpectationBuilderMap({
            data_types.Expectation(
                'foo/test', ['mac'], 'Failure', 'crbug.com/1234'):
            data_types.BuilderStepMap({
                'mac_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
            data_types.Expectation(
                'bar/test', ['mac'], 'Failure', 'crbug.com/2345'):
            data_types.BuilderStepMap({
                'mac_builder':
                data_types.StepBuildStatsMap({
                    'amd': amd_stats,
                    'intel': intel_stats,
                }),
            }),
        }),
    })
    # yapf: enable
    urls = self.instance.NarrowSemiStaleExpectationScope(test_expectation_map)
    expected_contents = """\
# tags: [ mac ]
# tags: [ amd intel ]
# results: [ Failure ]

crbug.com/1234 [ intel mac ] foo/test [ Failure ]
crbug.com/2345 [ mac ] bar/test [ Failure ]  # finder:disable-narrowing
"""
    with open(self.filename) as infile:
      self.assertEqual(infile.read(), expected_contents)
    self.assertEqual(urls, set(['crbug.com/1234']))


class FindOrphanedBugsUnittest(fake_filesystem_unittest.TestCase):
  def CreateFile(self, *args, **kwargs) -> None:
    # TODO(crbug.com/40160566): Remove this and just use fs.create_file() when
    # Catapult is updated to a newer version of pyfakefs that is compatible with
    # Chromium's version.
    if hasattr(self.fs, 'create_file'):
      self.fs.create_file(*args, **kwargs)
    else:
      self.fs.CreateFile(*args, **kwargs)

  def setUp(self) -> None:
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

  def testNoOrphanedBugs(self) -> None:
    bugs = ['crbug.com/1', 'crbug.com/2']
    self.assertEqual(self.instance.FindOrphanedBugs(bugs), set())

  def testOrphanedBugs(self) -> None:
    bugs = ['crbug.com/1', 'crbug.com/3', 'crbug.com/4']
    self.assertEqual(self.instance.FindOrphanedBugs(bugs),
                     set(['crbug.com/3', 'crbug.com/4']))


if __name__ == '__main__':
  unittest.main(verbosity=2)
