// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_VIEW_H_

#import <UIKit/UIKit.h>

@protocol StandaloneModuleDelegate;
@class StandaloneModuleItem;

// View for standalone-type Magic Stack modules.
@interface StandaloneModuleView : UIView

// Delegate to handle user interaction.
@property(nonatomic, weak) id<StandaloneModuleDelegate> delegate;

// Configures this view with `config`.
- (void)configureView:(StandaloneModuleItem*)config;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_STANDALONE_MODULE_VIEW_H_
