// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_FAVICON_LOADER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_data_source.h"

class FaviconLoader;

// Adapter bridging `FaviconLoader` to `ActorFormSuggestionFaviconDataSource`.
@interface ActorFormSuggestionFaviconLoader
    : NSObject <ActorFormSuggestionFaviconDataSource>

// Initializes the loader with `faviconLoader` (which may be null).
- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the loader and stops subsequent favicon requests from being
// processed.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_FORM_SUGGESTION_FAVICON_LOADER_H_
