# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT


# append the path of src/ to sys.path to import PRESUBMIT_test_mocks
SRC_IOS_WEB_VIEW_PATH = os.path.dirname(os.path.abspath(__file__))
SRC_PATH = os.path.dirname(os.path.dirname(SRC_IOS_WEB_VIEW_PATH))
sys.path.append(SRC_PATH)
import PRESUBMIT_test_mocks


class InclusionPathCheckerTest(unittest.TestCase):
  """Test the _CheckAbsolutePathInclusionInPublicHeaders presubmit check."""

  def testInclusionPathChecker(self):
    bads = [
        ('#import "ios/web_view/aaa_imported.h"', 'ios/web_view/public/aaa.h'),
        ('#include "ios/web_view/eee_imported.h"', 'ios/web_view/public/eee.h'),
        ('#include "base/logging.h"', 'ios/web_view/public/fff.h'),
        ('#import "ios/web_view/public/ggg_imported.h"',
         'ios/web_view/public/ggg.h'),
        ('#import "subdirectory/hhh_imported.h"', 'ios/web_view/public/hhh.h'),
    ]
    goods = [
        ('#import "ios/web_view/bbb_imported.h"', 'ios/web_view/shell/bbb.h'),
        ('#import "ccc_imported.h"', 'ios/web_view/public/ccc.h'),
        ('#import <UIKit/UIKit.h>', 'ios/web_view/public/ddd.h'),
    ]
    normal_code = '''

        /**
         *  Some random comments here.
         *  Write #include "base/logging.h" to use logging functions.
         */

        int main() {
            double a = 1.0 / 2.0;
            const char* str = "Hello, World!"; // a string to print
            printf(str);
        }'''
    bads = [((code + normal_code).split('\n'),
             os.path.join(SRC_PATH, path)) for code, path in bads]
    goods = [((code + normal_code).split('\n'),
              os.path.join(SRC_PATH, path)) for code, path in goods]

    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    mock_input.change = PRESUBMIT_test_mocks.MockChange([
                            PRESUBMIT_test_mocks.MockFile(file_path, code)
                            for code, file_path in (bads + goods)])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()

    errors = PRESUBMIT._CheckAbsolutePathInclusionInPublicHeaders(mock_input,
                                                                  mock_output)

    self.assertEqual(len(errors), 1)
    self.assertEqual('error', errors[0].type)
    self.assertTrue('with absolute path inclusion' in errors[0].message)

    for _, file_path in bads:
        self.assertTrue(file_path in errors[0].message)
    for _, file_path in goods:
        self.assertFalse(file_path in errors[0].message)


class NotFatalUntilAdoptionTest(unittest.TestCase):
  """Test the _CheckNotFatalUntilAdoption presubmit check."""

  def testNewCheckWithoutNFU(self):
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  CHECK(condition);'],
        action='A')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckNotFatalUntilAdoption(mock_input, mock_output)
    self.assertEqual(len(errors), 1)
    self.assertEqual('warning', errors[0].type)
    self.assertTrue('Consider using base::NotFatalUntil' in errors[0].message)
    self.assertTrue(
        file.LocalPath() + ':1: CHECK(condition);' in errors[0].message)

  def testNewCheckWithNFU(self):
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  CHECK(condition) << base::NotFatalUntil(2024, 10);'],
        action='A')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckNotFatalUntilAdoption(mock_input, mock_output)
    self.assertEqual(len(errors), 0)

  def testCheckPromotedFromNFU(self):
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  CHECK(condition);'],
        old_contents=['  CHECK(condition) << base::NotFatalUntil(2024, 10);'],
        action='M')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckNotFatalUntilAdoption(mock_input, mock_output)
    self.assertEqual(len(errors), 0)

  def testCheckModifiedWithoutPriorNFU(self):
    # If a CHECK was already fatal (no NFU) and is modified, it should warn.
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  CHECK(new_condition);'],
        old_contents=['  CHECK(old_condition);'],
        action='M')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckNotFatalUntilAdoption(mock_input, mock_output)
    self.assertEqual(len(errors), 1)

  def testMultilineNFU(self):
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  CHECK(condition)',
         '      << base::NotFatalUntil(2024, 10);'],
        action='A')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckNotFatalUntilAdoption(mock_input, mock_output)
    self.assertEqual(len(errors), 0)

  def testCheckInComment(self):
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  // CHECK(condition);'],
        action='A')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckNotFatalUntilAdoption(mock_input, mock_output)
    self.assertEqual(len(errors), 0)


class DiscourageCheckDerefTest(unittest.TestCase):
  """Test the _CheckDiscourageCheckDeref presubmit check."""

  def testCheckDerefUsed(self):
    mock_input = PRESUBMIT_test_mocks.MockInputApi()
    mock_input.presubmit_local_path = SRC_IOS_WEB_VIEW_PATH
    file = PRESUBMIT_test_mocks.MockFile(
        'ios/web_view/test.cc',
        ['  CHECK_DEREF(ptr);'],
        action='A')
    mock_input.InitFiles([file])
    mock_output = PRESUBMIT_test_mocks.MockOutputApi()
    errors = PRESUBMIT._CheckDiscourageCheckDeref(mock_input, mock_output)
    self.assertEqual(len(errors), 1)
    self.assertEqual('warning', errors[0].type)
    self.assertTrue('Avoid using CHECK_DEREF' in errors[0].message)
    self.assertTrue(
        file.LocalPath() + ':1: CHECK_DEREF(ptr);' in errors[0].message)


if __name__ == '__main__':
    unittest.main()
