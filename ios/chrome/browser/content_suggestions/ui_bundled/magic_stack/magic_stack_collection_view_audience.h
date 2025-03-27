// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_AUDIENCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_AUDIENCE_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;

// Audience for Magic Stack module events.
@protocol MagicStackCollectionViewControllerAudience

// Notifies the audience that the Magic Stack edit button was tapped.
- (void)didTapMagicStackEditButton;

// Notifies the audience that the displayed ephemeral `card` was shown to the
// user.
- (void)logEphemeralCardVisibility:(ContentSuggestionsModuleType)card;

// Notifies the audience that a module of type `moduleType` was shown as the top
// module.
- (void)logTopModuleImpressionForType:(ContentSuggestionsModuleType)moduleType;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_COLLECTION_VIEW_AUDIENCE_H_
