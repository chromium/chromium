#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import PRESUBMIT_test_mocks

class CheckARCCompilationGuardTest(unittest.TestCase):
    """Test the _CheckARCCompilationGuard presubmit check."""

    def testGoodImplementationFiles(self):
        """Test that .m and .mm files with a guard don't raise any errors."""
        lines = ["foobar"] + PRESUBMIT.ARC_COMPILE_GUARD
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.mm', lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.m', lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckARCCompilationGuard(mock_input, mock_output)
        self.assertEqual(len(errors), 0)

    def testBadImplementationFiles(self):
        """Test that .m and .mm files without a guard raise an error."""
        lines = ["foobar"]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.mm', lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.m', lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckARCCompilationGuard(mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('error', errors[0].type)
        self.assertTrue('ios/path/foo_controller.m' in errors[0].message)
        self.assertTrue('ios/path/foo_controller.mm' in errors[0].message)

    def testOtherFiles(self):
        """Test that other files without a guard don't raise errors."""
        lines = ["foobar"]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.h', lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.cc', lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/BUILD.gn', lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckARCCompilationGuard(mock_input, mock_output)
        self.assertEqual(len(errors), 0)


class CheckTODOFormatTest(unittest.TestCase):
    """Test the _CheckBugInToDo presubmit check."""

    def testTODOs(self):
        bad_lines = [
            'TO'
            'DO(ldap): fix this', 'TO'
            'DO(ladp): see crbug.com/8675309', 'TO'
            'DO(8675309): fix this', 'TO'
            'DO(http://crbug.com/8675309): fix this', 'TO'
            'DO( crbug.com/8675309): fix this', 'TO'
            'DO(crbug/8675309): fix this', 'TO'
            'DO(crbug.com): fix this'
        ]
        good_lines = [
            'TO'
            'DO(crbug.com/8675309): fix this', 'TO'
            'DO(crbug.com/8675309): fix this (please)'
        ]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.mm',
                                          bad_lines + good_lines)
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckBugInToDo(mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('error', errors[0].type)
        self.assertTrue('without bug numbers' in errors[0].message)
        error_lines = errors[0].message.split('\n')
        self.assertEqual(len(error_lines), len(bad_lines) + 2)


class CheckHasNoIncludeDirectivesTest(unittest.TestCase):
    """Test the _CheckHasNoIncludeDirectives presubmit check."""

    def testFindsIncludeDirectives(self):
        good_lines = [
            '#import <system>', '#import "my/path/my/header.h"',
            '#import "my/path/my/source.mm"', '#import "my/path/my/source.m"'
        ]
        bad_lines = [
            '#include <system>', '#import <system>',
            '#include "my/path/my/header.h"',
            '#include "my/path/my/source.mm"', '#import "my/path/my/header.h"'
            '#include "my/path/my/source.m"'
        ]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.mm',
                                          bad_lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller_2.mm',
                                          good_lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/bar_controller.h',
                                          bad_lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/bar_controller.m',
                                          bad_lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/bar_controller.cc',
                                          bad_lines),
            PRESUBMIT_test_mocks.MockFile('chrome/path/foo_controller.mm',
                                          bad_lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckHasNoIncludeDirectives(
            mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('error', errors[0].type)
        self.assertTrue('ios/path/foo_controller.mm:1' in errors[0].message)
        self.assertTrue('ios/path/foo_controller.mm:3' in errors[0].message)
        self.assertTrue('ios/path/foo_controller.mm:4' in errors[0].message)


class CheckHasNoPipeInCommentTest(unittest.TestCase):
    """Test the _CheckHasNoPipeInComment presubmit check."""

    def testFindsIncludeDirectives(self):
        good_lines = [
            '#if !defined(__has_feature) || !__has_feature(objc_arc)',
            '// This does A || B', '// `MySymbol` is correct',
            'bitVariable1 | bitVariable2'
        ]
        bad_lines = [
            '// |MySymbol| is wrong', '// What is wrong is: |MySymbol|'
        ]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.mm',
                                          good_lines + bad_lines),
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.h',
                                          bad_lines + good_lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckHasNoPipeInComment(mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('warning', errors[0].type)
        self.assertTrue('ios/path/foo_controller.mm:5' in errors[0].message)
        self.assertTrue('ios/path/foo_controller.mm:6' in errors[0].message)
        self.assertTrue('ios/path/foo_controller.h:1' in errors[0].message)
        self.assertTrue('ios/path/foo_controller.h:2' in errors[0].message)
        error_lines = errors[0].message.split('\n')
        self.assertEqual(len(error_lines), len(bad_lines) * 2 + 3)


if __name__ == '__main__':
    unittest.main()
