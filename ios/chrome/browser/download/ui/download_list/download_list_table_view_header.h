// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_HEADER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_HEADER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol DownloadListMutator;

/// View for displaying download filter options as table view header.
/// Contains filter controls at the top and attributed text with clickable links
/// at the bottom.
@interface DownloadListTableViewHeader : UIView

/// Mutator for handling filter changes.
@property(nonatomic, weak) id<DownloadListMutator> mutator;

/// Returns the preferred height for the header view based on its current
/// content.
/// @param width The width to use for height calculation.
- (CGFloat)preferredHeightForWidth:(CGFloat)width;

/// Controls the visibility of the attribution text at the bottom of the header.
/// @param shown Whether to show the attribution text.
- (void)setAttributionTextShown:(BOOL)shown;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_HEADER_H_
