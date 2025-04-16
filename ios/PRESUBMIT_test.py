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

class CheckTODOFormatTest(unittest.TestCase):
    """Test the _CheckBugInToDo presubmit check."""

    def testTODOs(self):
        # All instances of the "TO DO" string in the following test cases are
        # broken by line breaks, because this file is run through the PRESUBIT
        # that it tests, so incorrectly formatted items in the test fixture
        # will trigger errors.
        bad_lines = [
            'TO'
            'DO(ldap): fix this', 'TO'
            'DO(ladp): see crbug.com/8675309', 'TO'
            'DO(8675309): fix this', 'TO'
            'DO(http://crbug.com/8675309): fix this', 'TO'
            'DO( crbug.com/8675309): fix this', 'TO'
            'DO(crbug/8675309): fix this', 'TO'
            'DO(crbug.com): fix this', 'TO'
            'DO(inccrbug.com): fix this',
        ]
        deprecated_lines = [
            'TO'
            'DO(b/12345): fix this'
        ]
        good_lines = [
            'TO'
            'DO(crbug.com/8675309): fix this', 'TO'
            'DO(crbug.com/8675309): fix this (please)'
        ]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        lines = bad_lines + deprecated_lines + good_lines
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_controller.mm', lines)
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        results = PRESUBMIT._CheckBugInToDo(mock_input, mock_output)
        # Expect one error result and one warning result.
        self.assertEqual(len(results), 2)
        self.assertEqual('error', results[0].type)
        self.assertEqual('warning', results[1].type)
        self.assertTrue('without bug numbers' in results[0].message)
        self.assertTrue('with a deprecated bug link' in results[1].message)
        error_lines = results[0].message.split('\n')
        self.assertEqual(len(error_lines), len(bad_lines) + 2)
        warning_lines = results[1].message.split('\n')
        self.assertEqual(len(warning_lines), len(deprecated_lines) + 2)

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
            '#if !defined(a) || !defined(b)',
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

class _CheckCanImproveTestUsingExpectNSEQ(unittest.TestCase):
    """Test the _CheckCanImproveTestUsingExpectNSEQ presubmit. """

    def testFindImprovableTestUsingExpectNSEQ(self):
        good_lines = [
          'EXPECT_TRUE(a == b);',
          'if (a isEqualToString:b) {',
          'if (a isEqualToData:b) {'
          ]
        bad_lines = [
          'EXPECT_TRUE(a ',
          ' isEqualToString:b);',
          'EXPECT_TRUE(@"example" isEqualToString:@"example");',
          'EXPECT_FALSE(@"example" isEqualToData:@"example");',
          'EXPECT_TRUE(@"example" isEqualToArray:@"example");'
        ]
        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/foo_unittest.mm',
                                          good_lines + bad_lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckCanImproveTestUsingExpectNSEQ(
            mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('warning', errors[0].type)
        self.assertFalse('ios/path/foo_unittest.mm:1' in errors[0].message)
        self.assertFalse('ios/path/foo_unittest.mm:2' in errors[0].message)
        self.assertFalse('ios/path/foo_unittest.mm:3' in errors[0].message)
        self.assertTrue('ios/path/foo_unittest.mm:4' in errors[0].message)
        self.assertFalse('ios/path/foo_unittest.mm:5' in errors[0].message)
        self.assertTrue('ios/path/foo_unittest.mm:6' in errors[0].message)
        self.assertTrue('ios/path/foo_unittest.mm:7' in errors[0].message)
        self.assertTrue('ios/path/foo_unittest.mm:8' in errors[0].message)

class _CheckNotUsingNSUserDefaults(unittest.TestCase):
    """Test the _CheckNotUsingNSUserDefaults presubmit. """

    def testFindImprovableTestUsingExpectNSEQ(self):
        good_lines = [
          '[defaults dictionaryForKey:key_name];',
          ]
        bad_lines = [
          '[[NSUserDefaults standardUserDefaults]',
          'NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];',
          '[[NSUserDefaults standardUserDefaults] setObject:object_name',
        ]

        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/defaults_unittest.mm',
                                          good_lines + bad_lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckNotUsingNSUserDefaults(
            mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('warning', errors[0].type)
        self.assertFalse('ios/path/defaults_unittest.mm:1' in errors[0].message)
        self.assertTrue('ios/path/defaults_unittest.mm:2' in errors[0].message)
        self.assertTrue('ios/path/defaults_unittest.mm:3' in errors[0].message)
        self.assertTrue('ios/path/defaults_unittest.mm:4' in errors[0].message)

class _CheckUIGraphicsBeginImageContextWithOptions(unittest.TestCase):
    """Test the _CheckUIGraphicsBeginImageContextWithOptions presubmit."""

    def testFindUsesOfDeprecatedAPIs(self):
        good_lines = [
          '// Update UIGraphicsBeginImageContextWithOptions',
          ]
        bad_lines = [
          'UIGraphicsBeginImageContextWithOptions(',
        ]

        mock_input = PRESUBMIT_test_mocks.MockInputApi()
        mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/deprecated.mm',
                                          good_lines + bad_lines),
        ]
        mock_output = PRESUBMIT_test_mocks.MockOutputApi()
        errors = PRESUBMIT._CheckUIGraphicsBeginImageContextWithOptions(
            mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertEqual('error', errors[0].type)
        self.assertFalse('ios/path/deprecated.mm:1' in errors[0].message)
        self.assertTrue('ios/path/deprecated.mm:2' in errors[0].message)


class CheckNewColorIntroductionTest(unittest.TestCase):
    """Test the _CheckNewColorIntroduction presubmit check."""

    def setUp(self):
        self.mock_input = PRESUBMIT_test_mocks.MockInputApi()
        self.mock_output = PRESUBMIT_test_mocks.MockOutputApi()

    def testNoColorChanges(self):
        """Test when there are no color file changes."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile('ios/path/some_controller.mm', [])
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 0)

    def testNewColorInSharedDirectory(self):
        """Test adding a new color in the shared directory."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/common/ui/colors/resources/Assets.xcassets/'
                'test_color.colorset/Contents.json', [],
                action='A')
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('warning', results[0].type)
        self.assertTrue('New color(s) added in' in results[0].message)
        self.assertTrue(
            'ensure the color does not already exist' in results[0].message)

    def testNewColorOutsideSharedDirectory(self):
        """Test adding a new color outside the shared directory."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/browser/safety_check/resources/Assets.xcassets/'
                'test_color.colorset/Contents.json', [],
                action='A')
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('error', results[0].type)
        self.assertTrue(
            'New color(s) must be added to the' in results[0].message)
        self.assertTrue('ios/chrome/common/ui/colors' in results[0].message)

    def testModifiedColorInSharedDirectory(self):
        """Test modifying an existing color in the shared directory."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/common/ui/colors/resources/Assets.xcassets/'
                'test_color.colorset/Contents.json', [],
                action='M')
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('warning', results[0].type)
        self.assertTrue('Color(s) modified in' in results[0].message)
        self.assertTrue(
            'ensure the color does not already exist' in results[0].message)

    def testModifiedColorOutsideSharedDirectory(self):
        """Test modifying an existing color outside the shared directory."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/browser/safety_check/resources/Assets.xcassets/'
                'test_color.colorset/Contents.json', [],
                action='M')
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 1)
        self.assertEqual('warning', results[0].type)
        self.assertTrue('Color(s) modified' in results[0].message)
        self.assertTrue(
            'ensure the color does not already exist' in results[0].message)

    def testMultipleColorChanges(self):
        """Test multiple color changes in different locations."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/common/ui/colors/resources/Assets.xcassets/'
                'color1.colorset/Contents.json', [],
                action='A'),
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/browser/safety_check/resources/Assets.xcassets/'
                'color2.colorset/Contents.json', [],
                action='A'),
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/common/ui/colors/resources/Assets.xcassets/'
                'color3.colorset/Contents.json', [],
                action='M'),
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/browser/safety_check/resources/Assets.xcassets/'
                'color4.colorset/Contents.json', [],
                action='M')
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 4)
        # Check for error about new color outside shared directory
        self.assertEqual('error', results[0].type)
        self.assertTrue(
            'New color(s) must be added to the' in results[0].message)
        # Check for warning about new color in shared directory
        self.assertEqual('warning', results[1].type)
        self.assertTrue('New color(s) added in' in results[1].message)
        # Check for warning about modified color in shared directory
        self.assertEqual('warning', results[2].type)
        self.assertTrue('Color(s) modified in' in results[2].message)
        # Check for warning about modified color outside shared directory
        self.assertEqual('warning', results[3].type)
        self.assertTrue('Color(s) modified' in results[3].message)

    def testNonColorsetFiles(self):
        """Test that non-colorset files are ignored."""
        self.mock_input.files = [
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/common/ui/colors/resources/Assets.xcassets/'
                'test_file.json', [],
                action='A'),
            PRESUBMIT_test_mocks.MockFile(
                'ios/chrome/browser/colors/ui_bundled/test_file.mm', [],
                action='M')
        ]
        results = PRESUBMIT._CheckNewColorIntroduction(self.mock_input,
                                                       self.mock_output)
        self.assertEqual(len(results), 0)

if __name__ == '__main__':
    unittest.main()
