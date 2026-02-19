// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_STANDALONE_MODULE_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_STANDALONE_MODULE_VIEW_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;
@class StandaloneModuleViewConfig;

// Delegate protocol to relay information from the Magic Stack standalone
// modules.
@protocol StandaloneModuleViewTapDelegate

// Called when the button is tapped on the module of the given type.
- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType;

@end

// View for standalone-type Magic Stack modules.
@interface StandaloneModuleView : UIView

// Delegate to handle user interaction.
@property(nonatomic, weak) id<StandaloneModuleViewTapDelegate> tapDelegate;

// Configures this view with `config`.
- (void)configureView:(StandaloneModuleViewConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CELLS_STANDALONE_MODULE_VIEW_H_
