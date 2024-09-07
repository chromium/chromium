// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;
@class MagicStackModule;

// A protocol for the MagicStackModule delegate.
@protocol MagicStackModuleDelegate

// Called when the module is added to the displayed list.
- (void)magicStackModule:(MagicStackModule*)magicStackModule
     wasDisplayedAtIndex:(NSUInteger)index;

@end

// Base object for all Magic Stack modules configs. Subclass this class when
// creating a new module config.
@interface MagicStackModule : NSObject

// The type of the module config.
@property(nonatomic, assign, readonly) ContentSuggestionsModuleType type;

// YES to display the "See More" button for this module.
// This button takes precedence over the notifications opt-in button.
@property(nonatomic, assign) BOOL shouldShowSeeMore;

// YES to display the notifications opt-in button for this module.
// This button is hidden if the "See More" button is shown.
@property(nonatomic, assign) BOOL showNotificationsOptIn;

// The delegate of the module.
@property(nonatomic, weak) id<MagicStackModuleDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_H_
