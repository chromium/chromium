// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_consumer.h"

@protocol MagicStackHalfSheetModelDelegate;
class PrefService;

// Delegate for MagicStackHalfSheetTableViewController actions.
@protocol MagicStackHalfSheetTableViewControllerDelegate

// Indicates that the half sheet should be dismissed.
- (void)dismissMagicStackHalfSheet;

@end

// Presents a module disable customization page for the Magic Stack.
@interface MagicStackHalfSheetTableViewController
    : LegacyChromeTableViewController<MagicStackHalfSheetConsumer>

// Initializer for this class.
- (instancetype)init;

// Delegate for this ViewController.
@property(nonatomic, weak) id<MagicStackHalfSheetTableViewControllerDelegate>
    delegate;

// Model delegate for this ViewController.
@property(nonatomic, weak) id<MagicStackHalfSheetModelDelegate> modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_TABLE_VIEW_CONTROLLER_H_
