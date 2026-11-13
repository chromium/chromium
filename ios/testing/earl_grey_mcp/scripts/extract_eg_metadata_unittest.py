#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for extract_eg_metadata.py."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import extract_eg_metadata


class ExtractEgMetadataTest(unittest.TestCase):
    """Tests for EarlGrey build-time metadata extraction and validation."""

    def _write_temp_header(
        self, tmp_dir: Path, rel_path: str, content: str
    ) -> Path:
        """Writes `content` to `tmp_dir / rel_path` and returns the path."""
        full_path = tmp_dir / rel_path
        full_path.parent.mkdir(parents=True, exist_ok=True)
        full_path.write_text(content, encoding='utf-8')
        return full_path

    def test_extracts_methods_categories_enums_and_schemas(self) -> None:
        """Tests that valid headers produce complete metadata and schemas."""
        header_content = """
// Copyright 2026 The Chromium Authors

typedef NS_ENUM(NSInteger, SampleMode) {
  SampleModeDefault = 0,
  SampleModeFast = 1,
};

// App interface for testing sample bookmarks.
@interface SampleBookmarkAppInterface : NSObject

#pragma mark - Setup and Teardown

// Clears all bookmarks with `mode` and populates `error` on failure.
+ (void)clearBookmarksWithMode:(SampleMode)mode error:(NSError**)error;

#pragma mark - Queries

// Returns the matcher for a bookmark row with `title` at `index`.
+ (id<GREYMatcher>)matcherForBookmarkWithTitle:(NSString*)title
                                       atIndex:(NSInteger)index;

// Waits up to `timeout` seconds and returns YES if `url` is loaded.
- (BOOL)waitForURL:(const GURL&)url timeout:(base::TimeDelta)timeout;

@end
"""
        with tempfile.TemporaryDirectory() as tmp:
            src_root = Path(tmp)
            rel = (
                'ios/chrome/browser/bookmarks/test/'
                'sample_bookmark_app_interface.h'
            )
            header = self._write_temp_header(src_root, rel, header_content)
            metadata, errors = extract_eg_metadata.extract_metadata(
                src_root=src_root,
                header_paths=[header],
            )

            self.assertEqual(errors, [])
            self.assertIn('SampleMode', metadata['enums'])
            self.assertIn('SampleBookmarkAppInterface', metadata['classes'])

            cls_info = metadata['classes']['SampleBookmarkAppInterface']
            self.assertEqual(cls_info['domain'], 'bookmarks_and_reading')
            self.assertEqual(
                cls_info['docstring'],
                'App interface for testing sample bookmarks.',
            )
            self.assertIn('Setup and Teardown', cls_info['categories'])
            self.assertIn('Queries', cls_info['categories'])

            # Verify `clearBookmarksWithMode:error:` omits `error` from required
            # args and maps `SampleMode` to its enum values.
            clear_method = cls_info['methods']['clearBookmarksWithMode:error:']
            self.assertTrue(clear_method['is_class_method'])
            clear_schema = clear_method['json_schema']
            self.assertFalse(clear_schema['additionalProperties'])
            self.assertEqual(clear_schema['required'], ['mode'])
            self.assertNotIn('error', clear_schema['properties'])
            self.assertEqual(
                clear_schema['properties']['mode']['x_enum_name'],
                'SampleMode',
            )

            # Verify `matcherForBookmarkWithTitle:atIndex:` schema.
            matcher_method = cls_info['methods'][
                'matcherForBookmarkWithTitle:atIndex:'
            ]
            matcher_schema = matcher_method['json_schema']
            self.assertEqual(matcher_schema['required'], ['title', 'index'])
            self.assertEqual(
                matcher_schema['properties']['title']['type'],
                'string',
            )
            self.assertEqual(
                matcher_schema['properties']['index']['type'],
                'integer',
            )

            # Verify instance method includes `target_handle` and maps C++.
            wait_method = cls_info['methods']['waitForURL:timeout:']
            self.assertFalse(wait_method['is_class_method'])
            wait_schema = wait_method['json_schema']
            self.assertEqual(
                wait_schema['required'],
                ['target_handle', 'url', 'timeout'],
            )
            self.assertEqual(
                wait_schema['properties']['url']['type'],
                'string',
            )
            self.assertEqual(
                wait_schema['properties']['timeout']['type'],
                'number',
            )

    def test_detects_missing_docstrings_params_and_return_docs(self) -> None:
        """Tests that missing docstrings or parameter backticks fail."""
        header_content = """
@interface BrokenAppInterface : NSObject

+ (void)undocumentedMethod;

// Does something without mentioning the parameter in backticks.
+ (void)doActionWithValue:(NSInteger)value;

// Processes `input` in the app process.
+ (NSString*)processInput:(NSString*)input;

@end
"""
        with tempfile.TemporaryDirectory() as tmp:
            src_root = Path(tmp)
            header = self._write_temp_header(
                src_root,
                'ios/chrome/browser/autofill/broken_app_interface.h',
                header_content,
            )
            _, errors = extract_eg_metadata.extract_metadata(
                src_root=src_root,
                header_paths=[header],
            )

            self.assertEqual(len(errors), 3)
            self.assertTrue(any('undocumentedMethod' in e for e in errors))
            self.assertTrue(
                any('does not document parameter `value`' in e for e in errors)
            )
            self.assertTrue(
                any('does not document its return value' in e for e in errors)
            )

    def test_detects_unclassified_header_domain(self) -> None:
        """Tests that a header without a recognized domain raises an error."""
        header_content = """
@interface UnknownAppInterface : NSObject
// Does an action in `mode`.
+ (void)doActionInMode:(NSInteger)mode;
@end
"""
        with tempfile.TemporaryDirectory() as tmp:
            src_root = Path(tmp)
            header = self._write_temp_header(
                src_root,
                'ios/chrome/browser/unknown_feature/unknown_app_interface.h',
                header_content,
            )
            _, errors = extract_eg_metadata.extract_metadata(
                src_root=src_root,
                header_paths=[header],
            )
            self.assertEqual(len(errors), 1)
            self.assertIn('cannot be mapped to any known domain', errors[0])

    def test_enforces_hierarchy_breadth_caps_with_rebalancing_guidance(
        self,
    ) -> None:
        """Tests that exceeding breadth caps fails with rebalancing help."""
        header_content = """
@interface OversizedAppInterface : NSObject

#pragma mark - Category One

// Performs first action with `arg1`.
+ (void)firstAction:(NSInteger)arg1;

// Performs second action with `arg2`.
+ (void)secondAction:(NSInteger)arg2;

// Performs third action with `arg3`.
+ (void)thirdAction:(NSInteger)arg3;

@end
"""
        with tempfile.TemporaryDirectory() as tmp:
            src_root = Path(tmp)
            header = self._write_temp_header(
                src_root,
                'ios/chrome/browser/autofill/oversized_app_interface.h',
                header_content,
            )
            _, errors = extract_eg_metadata.extract_metadata(
                src_root=src_root,
                header_paths=[header],
                max_methods_per_category=2,
            )

            self.assertEqual(len(errors), 1)
            self.assertIn(
                'Hierarchy Breadth Cap Exceeded at Level 4',
                errors[0],
            )
            self.assertIn('How to rebalance: Add `#pragma mark', errors[0])

    def test_filters_excluded_headers_and_non_bridgeable_methods(self) -> None:
        """Tests skipping plumbing headers and non-bridgeable methods."""
        self.assertTrue(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/chrome/test/earl_grey/chrome_earl_grey.h'
            )
        )
        self.assertTrue(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/chrome/browser/metrics/model/metrics_app_interface.h'
            )
        )
        self.assertTrue(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/web/shell/test/earl_grey/shell_earl_grey.h'
            )
        )
        self.assertTrue(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/chrome/test/earl_grey/chrome_actions_app_interface.h'
            )
        )
        self.assertFalse(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h'
            )
        )
        self.assertFalse(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/web/shell/test/earl_grey/shell_earl_grey_app_interface.h'
            )
        )
        self.assertFalse(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/chrome/test/earl_grey/'
                'earl_grey_scoped_block_swizzler_app_interface.h'
            )
        )
        self.assertFalse(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/chrome/test/earl_grey/chrome_test_case.h'
            )
        )
        self.assertFalse(
            extract_eg_metadata.is_bridged_earlgrey_header(
                'ios/web/shell/test/earl_grey/web_shell_test_case.h'
            )
        )

        header_content = """
// Helper implementation for sample EarlGrey tests.
@interface SampleEarlGreyImpl : BaseEGTestHelperImpl

// Invokes `name` on the sample helper.
- (void)performSampleActionWithName:(NSString*)name;

// Unavailable initializer.
- (instancetype)init NS_UNAVAILABLE;

// Internal test setup helper.
+ (void)setUpHelper;

// Internal observer registration.
- (void)addObserver:(id)observer;

// Unsupported C++ FeatureRef parameter.
- (void)enableFeature:(base::test::FeatureRef)feature;

@end
"""
        with tempfile.TemporaryDirectory() as tmp:
            src_root = Path(tmp)
            header = self._write_temp_header(
                src_root,
                'ios/chrome/test/earl_grey/sample_earl_grey.h',
                header_content,
            )
            metadata, errors = extract_eg_metadata.extract_metadata(
                src_root=src_root,
                header_paths=[header],
            )
            self.assertEqual(errors, [])
            cls_info = metadata['classes']['SampleEarlGreyImpl']
            self.assertEqual(
                list(cls_info['methods'].keys()),
                ['performSampleActionWithName:'],
            )
            schema = cls_info['methods']['performSampleActionWithName:'][
                'json_schema'
            ]
            self.assertEqual(schema['required'], ['name'])
            self.assertNotIn('target_handle', schema['properties'])


if __name__ == '__main__':
    unittest.main()
