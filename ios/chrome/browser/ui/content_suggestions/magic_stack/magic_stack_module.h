// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;

// Base object for all Magic Stack modules configs. Subclass this class when
// creating a new module config.
@interface MagicStackModule : NSObject

// The type of the module config.
@property(nonatomic, assign, readonly) ContentSuggestionsModuleType type;

// YES if the "See More" button should be shown for this module.
@property(nonatomic, assign) BOOL shouldShowSeeMore;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_H_
