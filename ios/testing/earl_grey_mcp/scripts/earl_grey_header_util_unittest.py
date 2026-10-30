#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for earl_grey_header_util.py."""

import unittest

from earl_grey_header_util import (
    clean_comment_text,
    extract_method_declaration,
    extract_preceding_comments,
    is_earlgrey_header,
    parse_method_declaration,
    strip_trailing_attributes,
    validate_method_docstrings,
    MethodInfo,
    MethodParam,
)


class EarlGreyHeaderUtilTest(unittest.TestCase):
    """Tests for shared EarlGrey header parsing and validation utilities."""

    def test_is_earlgrey_header(self):
        """Tests header filtering for EarlGrey and AppInterface headers."""
        # Standalone included headers.
        self.assertTrue(
            is_earlgrey_header('ios/testing/earl_grey/app_launch_manager.h')
        )
        self.assertTrue(
            is_earlgrey_header('/ios/testing/earl_grey/app_launch_manager.h')
        )

        # Standard app interface and earl grey headers.
        self.assertTrue(
            is_earlgrey_header(
                'ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h'
            )
        )
        self.assertTrue(
            is_earlgrey_header('ios/chrome/test/earl_grey/chrome_earl_grey.h')
        )
        self.assertTrue(
            is_earlgrey_header(
                'ios/web/shell/test/app/web_shell_test_app_interface.h'
            )
        )

        # Excluded directories.
        self.assertFalse(
            is_earlgrey_header('ios/chrome/test/perf/perf_test_app_interface.h')
        )
        self.assertFalse(
            is_earlgrey_header('ios/chrome/test/wpt/wpt_app_interface.h')
        )

        # Excluded specific headers.
        self.assertFalse(
            is_earlgrey_header(
                'ios/chrome/test/earl_grey/'
                'earl_grey_scoped_block_swizzler_app_interface.h'
            )
        )

        # Non-header files.
        self.assertFalse(
            is_earlgrey_header(
                'ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.mm'
            )
        )

        # Standard non-EG headers.
        self.assertFalse(
            is_earlgrey_header('ios/chrome/browser/ui/tab_grid/tab_grid_view.h')
        )
        self.assertFalse(is_earlgrey_header('chrome/browser/ui/browser.h'))

    def test_strip_trailing_attributes(self):
        """Tests stripping compiler attributes, macros, and semicolons."""
        decl = (
            '+ (BOOL)isTabOpen:(NSString*)title '
            'NS_SWIFT_NAME(isTabOpen(title:)) __attribute__((deprecated));'
        )
        self.assertEqual(
            strip_trailing_attributes(decl),
            '+ (BOOL)isTabOpen:(NSString*)title',
        )

        decl_bracket = '+ (void)doSomething [[nodiscard]];'
        self.assertEqual(
            strip_trailing_attributes(decl_bracket),
            '+ (void)doSomething',
        )

    def test_extract_method_declaration(self):
        """Tests reading multi-line method declarations."""
        lines = [
            '// Some comment',
            '+ (void)openURL:(NSURL*)url',
            '          timeout:(NSTimeInterval)timeout',
            '            error:(NSError**)error;',
            '@end',
        ]
        decl, end_idx = extract_method_declaration(lines, 1)
        self.assertEqual(end_idx, 3)
        self.assertIn('+ (void)openURL:(NSURL*)url', decl)
        self.assertIn('timeout:(NSTimeInterval)timeout', decl)
        self.assertTrue(decl.endswith(';'))

    def test_extract_preceding_comments(self):
        """Tests capturing contiguous preceding // comments."""
        lines = [
            'int unused;',
            '',
            '// First line.',
            '// Second line with `param`.',
            '+ (void)doAction;',
        ]
        comments, start_idx = extract_preceding_comments(lines, 4)
        self.assertEqual(start_idx, 2)
        self.assertEqual(
            comments,
            ['// First line.', '// Second line with `param`.'],
        )

    def test_clean_comment_text(self):
        """Tests stripping slashes and filtering standalone TODO comments."""
        raw_comments = [
            '/// Primary description line.',
            '// TODO: clean this up later',
            '// Secondary description mentioning `flag`.',
            '// FIXME:',
        ]
        cleaned = clean_comment_text(raw_comments)
        self.assertEqual(
            cleaned,
            'Primary description line. '
            'Secondary description mentioning `flag`.',
        )

    def test_parse_method_declaration_unary(self):
        """Tests parsing unary (parameterless) methods."""
        decl = '+ (BOOL)isSignedOut;'
        info = parse_method_declaration(decl)
        self.assertTrue(info.is_class_method)
        self.assertEqual(info.return_type, 'BOOL')
        self.assertEqual(info.selector, 'isSignedOut')
        self.assertEqual(info.params, [])

        instance_decl = '- (void)tearDown;'
        instance_info = parse_method_declaration(instance_decl)
        self.assertFalse(instance_info.is_class_method)
        self.assertEqual(instance_info.return_type, 'void')
        self.assertEqual(instance_info.selector, 'tearDown')
        self.assertEqual(instance_info.params, [])

    def test_parse_method_declaration_multi_part(self):
        """Tests parsing multi-part keyword selectors and parameter types."""
        decl = (
            '+ (BOOL)openURL:(NSURL*)url '
            'withTimeout:(NSTimeInterval)timeout '
            'completion:(void (^)(BOOL success))completion;'
        )
        info = parse_method_declaration(decl)
        self.assertTrue(info.is_class_method)
        self.assertEqual(info.return_type, 'BOOL')
        self.assertEqual(
            info.selector,
            'openURL:withTimeout:completion:',
        )
        self.assertEqual(len(info.params), 3)

        self.assertEqual(info.params[0].keyword, 'openURL')
        self.assertEqual(info.params[0].param_type, 'NSURL*')
        self.assertEqual(info.params[0].param_name, 'url')

        self.assertEqual(info.params[1].keyword, 'withTimeout')
        self.assertEqual(info.params[1].param_type, 'NSTimeInterval')
        self.assertEqual(info.params[1].param_name, 'timeout')

        self.assertEqual(info.params[2].keyword, 'completion')
        self.assertEqual(info.params[2].param_type, 'void (^)(BOOL success)')
        self.assertEqual(info.params[2].param_name, 'completion')

    def test_validate_method_docstrings(self):
        """Tests validation of method functionality, parameters, and returns."""
        method = MethodInfo(
            selector='openURL:withTimeout:error:',
            is_class_method=True,
            return_type='BOOL',
            params=[
                MethodParam('openURL', 'NSURL*', 'url'),
                MethodParam('withTimeout', 'NSTimeInterval', 'timeout'),
                MethodParam('error', 'NSError**', 'error'),
            ],
        )

        # Missing docstring.
        errors = validate_method_docstrings(method, '')
        self.assertTrue(any('Missing docstring' in e for e in errors))

        # Undocumented parameter.
        partial_doc = 'Opens the `url`.'
        errors = validate_method_docstrings(method, partial_doc)
        self.assertTrue(
            any('Undocumented parameter `timeout`' in e for e in errors)
        )
        # Note: `error` does not need to be documented.
        self.assertFalse(
            any('Undocumented parameter `error`' in e for e in errors)
        )
        # Return description is also missing.
        self.assertTrue(
            any('Missing return value description' in e for e in errors)
        )

        # Fully documented docstring.
        full_doc = (
            'Opens the `url` within `timeout` seconds. '
            'Returns YES if navigation succeeded, NO otherwise.'
        )
        errors = validate_method_docstrings(method, full_doc)
        self.assertEqual(errors, [])

    def test_validate_method_docstrings_void_and_instancetype(self):
        """Tests that void and instancetype methods need no return docs."""
        void_method = MethodInfo(
            selector='clearBrowsingData',
            is_class_method=True,
            return_type='void',
            params=[],
        )
        errors = validate_method_docstrings(
            void_method, 'Clears all browsing data.'
        )
        self.assertEqual(errors, [])

        init_method = MethodInfo(
            selector='init',
            is_class_method=False,
            return_type='instancetype',
            params=[],
        )
        errors = validate_method_docstrings(
            init_method, 'Initializes a new instance.'
        )
        self.assertEqual(errors, [])


if __name__ == '__main__':
    unittest.main()
