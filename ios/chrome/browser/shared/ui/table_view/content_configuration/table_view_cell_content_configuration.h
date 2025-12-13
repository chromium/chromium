// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_main_content_configuration.h"

@protocol ChromeContentConfiguration;
@class TableViewCell;
@class LegacyTableViewCell;

// Configuration object for a TableView cell.
// It is using a TableViewCellContentView as content view.
// +---------------------------------------------------------------+
// |                     TableViewCellContentView                  |
// |                                                               |
// | +-----------+  Title                            +-----------+ |
// | | Leading   |                                   | Trailing  | |
// | | View      |  Subtitle          Trailing Label | View      | |
// | |(Optional) |                                   |(Optional) | |
// | +-----------+  Second Subtitle                  +-----------+ |
// |                                                               |
// +---------------------------------------------------------------+
@interface TableViewCellContentConfiguration
    : NSObject <ChromeMainContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The leading content configuration for the cell.
@property(nonatomic, copy)
    NSObject<ChromeContentConfiguration>* leadingConfiguration;
// The trailing content configuration for the cell.
@property(nonatomic, copy)
    NSObject<ChromeContentConfiguration>* trailingConfiguration;

// Whether the labels should be disabled (change text color). Default NO.
@property(nonatomic, assign, getter=isTextDisabled) BOOL textDisabled;

// The title of the cell. `attributedTitle` takes precedence over `title`.
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSAttributedString* attributedTitle;
@property(nonatomic, strong) UIColor* titleColor;
// Defaults to 0 (unlimited).
@property(nonatomic, assign) NSInteger titleNumberOfLines;
// Defaults to NSLineBreakByWordWrapping.
@property(nonatomic, assign) NSLineBreakMode titleLineBreakMode;

// The subtitle of the cell. `attributedSubtitle` takes precedence over
// `subtitle`.
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, copy) NSAttributedString* attributedSubtitle;
@property(nonatomic, strong) UIColor* subtitleColor;
// Defaults to 0 (unlimited).
@property(nonatomic, assign) NSInteger subtitleNumberOfLines;
// Defaults to NSLineBreakByWordWrapping.
@property(nonatomic, assign) NSLineBreakMode subtitleLineBreakMode;

// A row of text below the subtitle.
@property(nonatomic, copy) NSString* secondSubtitle;
// Defaults to 0 (unlimited).
@property(nonatomic, assign) NSInteger secondSubtitleNumberOfLines;

// The trailing details of the cell. `attributedTrailingText` takes precedence
// over `trailingText`.
@property(nonatomic, copy) NSString* trailingText;
@property(nonatomic, copy) NSAttributedString* attributedTrailingText;
@property(nonatomic, strong) UIColor* trailingTextColor;
// Defaults to 1.
@property(nonatomic, assign) NSInteger trailingTextNumberOfLines;

// Custom accessibility label, overriding the default one.
@property(nonatomic, copy) NSString* customAccessibilityLabel;

// Whether this Content Configuration is displayed besides an accessory view.
// Used to modifying the trailing constants. Its setter is part of the
// ChromeMainContentConfiguration protocol.
@property(nonatomic, assign) BOOL hasAccessoryView;

// LINT.ThenChange(table_view_cell_content_configuration.mm:Copy)

// Registers/Dequeues a TableViewCell for this content configuration. This
// ensures that the pool of cells that will be used for this content
// configuration can be reused for the same configurations.
+ (void)registerCellForTableView:(UITableView*)tableView;
+ (UITableViewCell*)dequeueTableViewCell:(UITableView*)tableView;
+ (UITableViewCell*)dequeueTableViewCell:(UITableView*)tableView
                            forIndexPath:(NSIndexPath*)indexPath;

// TODO(crbug.com/443034511): Remove this method.
// **DO NOT use both the legacy and non-legacy versions on the same
// table view**.
+ (void)legacyRegisterCellForTableView:(UITableView*)tableView;
+ (LegacyTableViewCell*)legacyDequeueTableViewCell:(UITableView*)tableView;

// Returns the same view as `makeContentView`, but with its accessibility
// configured.
- (UIView*)makeAccessibilityConfiguredContentView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_
