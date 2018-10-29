// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/table_view_modal_presenting.h"

namespace {

// Positions on the screen for the presented view controller.
typedef NS_ENUM(NSInteger, TablePresentationPosition) {
  // The position will be in the trailing side.
  TablePresentationPositionTrailing = 0,
  // The position will be in the leading side.
  TablePresentationPositionLeading
};

}  // namespace

@protocol TableViewPresentationControllerDelegate;

@interface TableViewPresentationController
    : UIPresentationController<TableViewModalPresenting>

// This controller's delegate.
@property(nonatomic, weak) id<TableViewPresentationControllerDelegate>
    modalDelegate;

// Position of the presented controller relative to the screen. Default is
// TablePresentationPositionTrailing.
@property(nonatomic, assign) TablePresentationPosition position;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_PRESENTATION_CONTROLLER_H_
