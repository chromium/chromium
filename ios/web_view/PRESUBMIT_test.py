# Copyright 2020 The Chromium Authors. All rights reserved.
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
             SRC_PATH + '/' + path) for code, path in bads]
    goods = [((code + normal_code).split('\n'),
              SRC_PATH + '/' + path) for code, path in goods]

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


if __name__ == '__main__':
    unittest.main()
