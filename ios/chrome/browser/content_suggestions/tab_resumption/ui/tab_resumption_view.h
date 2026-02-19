// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_UI_TAB_RESUMPTION_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_UI_TAB_RESUMPTION_VIEW_H_

#import <UIKit/UIKit.h>

@protocol TabResumptionCommands;
@class TabResumptionConfig;

/// TODO(crbug.com/482377120): Consider refactoring to use an `IconDetailView`
/// as the main content view for this Magic Stack card.

// A view that displays a tab resumption item in the Magic Stack.
@interface TabResumptionView : UIView

// Initialize a TabResumptionView with the given `config`.
- (instancetype)initWithConfig:(TabResumptionConfig*)config;

// The handler that receives TabResumptionView's events.
@property(nonatomic, weak) id<TabResumptionCommands> tabResumptionHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TAB_RESUMPTION_UI_TAB_RESUMPTION_VIEW_H_
