// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_FAVICON_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_FAVICON_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class ActorFormSuggestion;
@class UIImage;

// Completion handler invoked with the loaded favicon `icon` (or `nil`).
using ActorFormSuggestionFaviconCompletion = void (^)(UIImage* icon);

// Data source for loading a credential suggestion's website favicon.
@protocol ActorFormSuggestionFaviconDataSource <NSObject>

// Initiates loading the favicon image for `suggestion`, invoking `completion`
// with the loaded `icon` (or `nil` if loading finishes without an image).
// Returns `YES` if loading was initiated.
- (BOOL)actorFormSuggestion:(ActorFormSuggestion*)suggestion
    loadFaviconWithCompletion:(ActorFormSuggestionFaviconCompletion)completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_FAVICON_DATA_SOURCE_H_
