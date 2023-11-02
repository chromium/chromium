#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from mock import patch
import os
import unittest

import shard_util

DEBUG_APP_OTOOL_OUTPUT = '\n'.join([
    'Meta Class', 'name 0x1064b8438 CacheTestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'imp 0x1075e6887 -[CacheTestCase testA]', 'types 0x1064cc3e1',
    'imp 0x1075e6887 -[CacheTestCase testB]',
    'imp 0x1075e6887 -[CacheTestCase testc]', 'name 0x1064b8438 TabUITestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'imp 0x1075e6887 -[TabUITestCase testD]', 'types 0x1064cc3e1 v16@0:8',
    'imp 0x1075e6887 -[TabUITestCase testE]',
    'name 0x1064b8438 KeyboardTestCase',
    'imp 0x1075e6887 -[KeyboardTestCase testF]',
    'name 0x1064b8438 PasswordsTestCase',
    'imp 0x1075e6887 -[PasswordsTestCase testG]',
    'name 0x1064b8438 ToolBarTestCase',
    'imp 0x1075e6887 -[ToolBarTestCase testH]',
    'imp 0x1075e6887 -[ToolBarTestCase DISABLED_testI]',
    'imp 0x1075e6887 -[ToolBarTestCase FLAKY_testJ]', 'version 0'
]).encode('utf-8')

# Debug app otool output format in Xcode 11.4 toolchain.
DEBUG_APP_OTOOL_OUTPUT_114 = '\n'.join([
    'Meta Class', 'name        0x1064b8438 CacheTestCase',
    'baseMethods    0x1068586d8 (struct method_list_t *)',
    '    imp     0x1075e6887 -[CacheTestCase testA]', '    types   0x1064cc3e1',
    '    imp     0x1075e6887 -[CacheTestCase testB]',
    '    imp     0x1075e6887 -[CacheTestCase testc]',
    '    name    0x1064b8438 TabUITestCase',
    'baseMethods    0x1068586d8 (struct method_list_t *)',
    '    imp     0x1075e6887 -[TabUITestCase testD]',
    '    types   0x1064cc3e1 v16@0:8',
    '    imp     0x1075e6887 -[TabUITestCase testE]',
    '    name    0x1064b8438 KeyboardTestCase',
    '    imp     0x1075e6887 -[KeyboardTestCase testF]',
    '    name    0x1064b8438 PasswordsTestCase',
    '    imp     0x1075e6887 -[PasswordsTestCase testG]',
    '    name    0x1064b8438 ToolBarTestCase',
    '    imp     0x1075e6887 -[ToolBarTestCase testH]',
    '    imp     0x1075e6887 -[ToolBarTestCase DISABLED_testI]',
    '    imp     0x1075e6887 -[ToolBarTestCase FLAKY_testJ]', 'version 0'
]).encode('utf-8')

RELEASE_APP_OTOOL_OUTPUT = '\n'.join([
    'Meta Class', 'name 0x1064b8438 CacheTestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'name 0x1075e6887 testA', 'types 0x1064cc3e1', 'name 0x1075e6887 testB',
    'name 0x1075e6887 testc', 'baseProtocols 0x0',
    'name 0x1064b8438 CacheTestCase', 'unrelated line', 'Meta Class',
    'name 0x1064b8438 TabUITestCase', 'no test methods in this case',
    'name 0x1064b8438 TabUITestCase', 'unrelated line',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'name 0x1064b8438 KeyboardTest', 'name 0x1075e6887 testD',
    'types 0x1064cc3e1 v16@0:8', 'name 0x1075e6887 testE',
    'name 0x1075e6887 testF', 'baseProtocols 0x0',
    'name 0x1064b8438 KeyboardTest', 'name 0x1075e6887 testUnrelatedG',
    'unrelated line', 'name 0x1064b8438 ChromeTestCase',
    'name 0x1064b8438 setUp', 'name 0x1064b8438 testPort',
    'name 0x5345ac561 testSomeUnrelatedUtil', 'baseProtocols 0x0',
    'name 0x1064b8438 ChromeTestCase', 'unrelated line',
    'name 0x1064b8438 invalidTestCase', 'name 0x1075e6887 testG',
    'baseProtocols 0x0', 'name 0x1064b8438 ToolBarTestCase',
    'name 0x1075e6887 testG', 'name 0x1075e6887 testH',
    'name 0x1075e6887 DISABLED_testI', 'name 0x1075e6887 FLAKY_testJ',
    'name 0x1064b8438 ToolBarTestCase', 'baseProtocols 0x0', 'version 0'
]).encode('utf-8')

RELEASE_APP_OTOOL_OUTPUT_CLASS_NOT_IN_PAIRS = '\n'.join([
    'Meta Class', 'name 0x1064b8438 CacheTestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'name 0x1075e6887 testA', 'types 0x1064cc3e1', 'name 0x1075e6887 testB',
    'name 0x1075e6887 testc', 'baseProtocols 0x0',
    'name 0x1064b8438 CacheTestCase', 'unrelated line', 'Meta Class',
    'name 0x1064b8438 TabUITestCase', 'no test methods in this case',
    'name 0x1064b8438 TabUITestCase', 'unrelated line',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    'name 0x1064b8438 KeyboardTest', 'name 0x1075e6887 testD',
    'types 0x1064cc3e1 v16@0:8', 'name 0x1075e6887 testE',
    'name 0x1075e6887 testF', 'baseProtocols 0x0',
    'name 0x1075e6887 testUnrelatedG', 'unrelated line',
    'name 0x1064b8438 ChromeTestCase', 'name 0x1064b8438 setUp',
    'name 0x1064b8438 testPort', 'name 0x5345ac561 testSomeUnrelatedUtil',
    'baseProtocols 0x0', 'name 0x1064b8438 ChromeTestCase', 'unrelated line',
    'name 0x1064b8438 invalidTestCase', 'name 0x1075e6887 testG',
    'baseProtocols 0x0', 'name 0x1064b8438 ToolBarTestCase',
    'name 0x1075e6887 testG', 'name 0x1075e6887 testH',
    'name 0x1075e6887 DISABLED_testI', 'name 0x1075e6887 FLAKY_testJ',
    'name 0x1064b8438 ToolBarTestCase', 'baseProtocols 0x0', 'version 0'
]).encode('utf-8')

# Release app otool output format in Xcode 11.4 toolchain.
RELEASE_APP_OTOOL_OUTPUT_114 = '\n'.join([
    'Meta Class', '    name    0x1064b8438 CacheTestCase',
    'baseMethods 0x1068586d8 (struct method_list_t *)',
    '    name    0x1075e6887 testA', '    types    0x1064cc3e1',
    '    name    0x1075e6887 testB', '    name    0x1075e6887 testc',
    'baseProtocols 0x0', '    name    0x1064b8438 CacheTestCase',
    'unrelated line', 'Meta Class', '    name    0x1064b8438 TabUITestCase',
    'no test methods in this case', '    name    0x1064b8438 TabUITestCase',
    'unrelated line', 'baseMethods 0x1068586d8 (struct method_list_t *)',
    '    name    0x1064b8438 KeyboardTest', '    name    0x1075e6887 testD',
    '    types    0x1064cc3e1 v16@0:8', '    name    0x1075e6887 testE',
    '    name    0x1075e6887 testF', 'baseProtocols 0x0',
    '    name    0x1064b8438 KeyboardTest',
    '    name    0x1075e6887 testUnrelatedG', 'unrelated line',
    '    name    0x1064b8438 ChromeTestCase', '    name    0x1064b8438 setUp',
    '    name    0x1064b8438 testPort',
    '    name    0x5345ac561 testSomeUnrelatedUtil', 'baseProtocols 0x0',
    '    name    0x1064b8438 ChromeTestCase', 'unrelated line',
    '    name    0x1064b8438 invalidTestCase', '    name    0x1075e6887 testG',
    'baseProtocols 0x0', '    name    0x1064b8438 ToolBarTestCase',
    '    name    0x1075e6887 testG', '    name    0x1075e6887 testH',
    '    name    0x1075e6887 DISABLED_testI',
    '    name    0x1075e6887 FLAKY_testJ',
    '    name    0x1064b8438 ToolBarTestCase', 'baseProtocols 0x0', 'version 0'
]).encode('utf-8')


class TestShardUtil(unittest.TestCase):
  """Test cases for shard_util.py"""

  @patch('shard_util.os.path.abspath')
  def test_determine_path_non_eg2(self, mock_abspath):
    mock_abspath.return_value = '/b/s/w/ir/ios/build/bots/scripts/share_util.py'
    app = 'some_ios_test.app'

    actual_path = shard_util.determine_app_path(app)
    expected_path = os.path.join('/b/s/w/ir', 'out/Debug', app, 'some_ios_test')
    self.assertEqual(actual_path, expected_path)

  @patch('shard_util.os.path.abspath')
  def test_determine_path_eg2(self, mock_abspath):
    mock_abspath.return_value = '/b/s/w/ir/ios/build/bots/scripts/share_util.py'
    app = 'some_ios_test-Runner.app'
    host = 'some_host.app'

    actual_path = shard_util.determine_app_path(app, host)
    expected_path = os.path.join('/b/s/w/ir', 'out/Debug', app, 'PlugIns',
                                 'some_ios_test.xctest', 'some_ios_test')
    self.assertEqual(actual_path, expected_path)

  @patch('shard_util.os.path.abspath')
  def test_determine_path_eg2_release(self, mock_abspath):
    mock_abspath.return_value = '/b/s/w/ir/ios/build/bots/scripts/share_util.py'
    app = 'some_ios_test-Runner.app'
    host = 'some_host.app'

    actual_path = shard_util.determine_app_path(app, host, True)
    expected_path = os.path.join('/b/s/w/ir', 'out/Release', app, 'PlugIns',
                                 'some_ios_test.xctest', 'some_ios_test')
    self.assertEqual(actual_path, expected_path)

  def test_fetch_test_names_debug(self):
    """Ensures that the debug output is formatted correctly"""
    resp = shard_util.fetch_test_names_for_debug(DEBUG_APP_OTOOL_OUTPUT)
    self.assertEqual(len(resp), 10)
    expected_test_names = [
        ('CacheTestCase', 'testA'),
        ('CacheTestCase', 'testB'),
        ('CacheTestCase', 'testc'),
        ('TabUITestCase', 'testD'),
        ('TabUITestCase', 'testE'),
        ('KeyboardTestCase', 'testF'),
        ('PasswordsTestCase', 'testG'),
        ('ToolBarTestCase', 'testH'),
        ('ToolBarTestCase', 'DISABLED_testI'),
        ('ToolBarTestCase', 'FLAKY_testJ'),
    ]
    for test_name in expected_test_names:
      self.assertTrue(test_name in resp)

    test_cases = [test_case for (test_case, _) in resp]
    # ({'CacheTestCase': 3, 'TabUITestCase': 2, 'PasswordsTestCase': 1,
    # 'KeyboardTestCase': 1, 'ToolBarTestCase': 3})
    counts = collections.Counter(test_cases).most_common()
    name, _ = counts[0]
    # CacheTestCase and ToolBarTestCase each have 3 entries.
    # In case of ties, most_common() returns the first encountered at index 0.
    self.assertEqual(name, 'CacheTestCase')

  def test_fetch_test_counts_release(self):
    """Ensures that the release output is formatted correctly"""
    resp = shard_util.fetch_test_names_for_release(RELEASE_APP_OTOOL_OUTPUT)
    self.assertEqual(len(resp), 10)

    expected_test_names = [
        ('CacheTestCase', 'testA'),
        ('CacheTestCase', 'testB'),
        ('CacheTestCase', 'testc'),
        ('KeyboardTest', 'testD'),
        ('KeyboardTest', 'testE'),
        ('KeyboardTest', 'testF'),
        ('ToolBarTestCase', 'testG'),
        ('ToolBarTestCase', 'testH'),
        ('ToolBarTestCase', 'DISABLED_testI'),
        ('ToolBarTestCase', 'FLAKY_testJ'),
    ]
    for test_name in expected_test_names:
      self.assertTrue(test_name in resp)

    test_cases = [test_case for (test_case, _) in resp]
    # ({'KeyboardTest': 3, 'CacheTestCase': 3,
    # 'ToolBarTestCase': 4})
    counts = collections.Counter(test_cases).most_common()
    name, _ = counts[0]
    self.assertEqual(name, 'ToolBarTestCase')

  def test_fetch_test_error_release(self):
    """Ensures that unexpected release output raises error."""
    with self.assertRaises(shard_util.ShardingError) as context:
      shard_util.fetch_test_names_for_release(
          RELEASE_APP_OTOOL_OUTPUT_CLASS_NOT_IN_PAIRS)
    expected_message = (
        'Incorrect otool output in which a test class name doesn\'t appear in '
        'group of 2. Test class: KeyboardTest')
    self.assertTrue(expected_message in str(context.exception))

  def test_fetch_test_names_debug_114(self):
    """Test the debug output from otool in Xcode 11.4"""
    resp = shard_util.fetch_test_names_for_debug(DEBUG_APP_OTOOL_OUTPUT_114)
    self.assertEqual(len(resp), 10)
    expected_test_names = [
        ('CacheTestCase', 'testA'),
        ('CacheTestCase', 'testB'),
        ('CacheTestCase', 'testc'),
        ('TabUITestCase', 'testD'),
        ('TabUITestCase', 'testE'),
        ('KeyboardTestCase', 'testF'),
        ('PasswordsTestCase', 'testG'),
        ('ToolBarTestCase', 'testH'),
        ('ToolBarTestCase', 'DISABLED_testI'),
        ('ToolBarTestCase', 'FLAKY_testJ'),
    ]
    for test_name in expected_test_names:
      self.assertTrue(test_name in resp)

    test_cases = [test_case for (test_case, _) in resp]

    # ({'CacheTestCase': 3, 'TabUITestCase': 2, 'PasswordsTestCase': 1,
    # 'KeyboardTestCase': 1, 'ToolBarTestCase': 3})
    counts = collections.Counter(test_cases).most_common()
    name, _ = counts[0]
    # CacheTestCase and ToolBarTestCase each have 3 entries.
    # In case of ties, most_common() returns the first encountered at index 0.
    self.assertEqual(name, 'CacheTestCase')

  def test_fetch_test_counts_release_114(self):
    """Test the release output from otool in Xcode 11.4"""
    resp = shard_util.fetch_test_names_for_release(RELEASE_APP_OTOOL_OUTPUT_114)
    self.assertEqual(len(resp), 10)

    expected_test_names = [
        ('CacheTestCase', 'testA'),
        ('CacheTestCase', 'testB'),
        ('CacheTestCase', 'testc'),
        ('KeyboardTest', 'testD'),
        ('KeyboardTest', 'testE'),
        ('KeyboardTest', 'testF'),
        ('ToolBarTestCase', 'testG'),
        ('ToolBarTestCase', 'testH'),
        ('ToolBarTestCase', 'DISABLED_testI'),
        ('ToolBarTestCase', 'FLAKY_testJ'),
    ]
    for test_name in expected_test_names:
      self.assertTrue(test_name in resp)

    test_cases = [test_case for (test_case, _) in resp]
    # ({'KeyboardTest': 3, 'CacheTestCase': 3,
    # 'ToolBarTestCase': 4})
    counts = collections.Counter(test_cases).most_common()
    name, _ = counts[0]
    self.assertEqual(name, 'ToolBarTestCase')

  def test_balance_into_sublists_debug(self):
    """Ensure the balancing algorithm works"""
    resp = shard_util.fetch_test_names_for_debug(DEBUG_APP_OTOOL_OUTPUT)
    test_cases = [test_case for (test_case, _) in resp]
    test_counts = collections.Counter(test_cases)

    sublists_1 = shard_util.balance_into_sublists(test_counts, 1)
    self.assertEqual(len(sublists_1), 1)
    self.assertEqual(len(sublists_1[0]), 5)

    sublists_3 = shard_util.balance_into_sublists(test_counts, 3)
    self.assertEqual(len(sublists_3), 3)
    # CacheTestCase has 3,
    # TabUITestCase has 2, ToolBarTestCase has 4
    # PasswordsTestCase has 1, KeyboardTestCase has 1
    # They will be balanced into:
    # [[ToolBarTestCase], [CacheTestCase, PasswordsTestCase],
    # [TabUITestCase, KeyboardTestCase]]
    self.assertEqual(
        sorted([len(sublists_3[0]),
                len(sublists_3[1]),
                len(sublists_3[2])]), [1, 2, 2])

  def test_balance_into_sublists_release(self):
    """Ensure the balancing algorithm works"""
    resp = shard_util.fetch_test_names_for_release(RELEASE_APP_OTOOL_OUTPUT)
    test_cases = [test_case for (test_case, _) in resp]
    test_counts = collections.Counter(test_cases)

    sublists_3 = shard_util.balance_into_sublists(test_counts, 3)
    self.assertEqual(len(sublists_3), 3)
    # KeyboardTest has 3
    # CacheTestCase has 3
    # ToolbarTest Case has 4
    # They will be balanced as one in each shard.
    self.assertEqual(len(sublists_3[0]), 1)
    self.assertEqual(len(sublists_3[1]), 1)
    self.assertEqual(len(sublists_3[2]), 1)


if __name__ == '__main__':
  unittest.main()
