// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_FILTER_VIEW_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_FILTER_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol DownloadListMutator;

/// View for displaying download filter options.
@interface DownloadListFilterView : UIView

/// Mutator for handling filter changes.
@property(nonatomic, weak) id<DownloadListMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_FILTER_VIEW_H_
