// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSUMER_H_

@class FaviconAttributes;

// Handles updates from the mediator to the UI.
@protocol SearchEngineChoiceConsumer <NSObject>

// Switches the fake omnibox at the top of the screen to one with the correct
// favicon and search engine name
- (void)updateFakeOmniboxWithFaviconImage:(UIImage*)icon
                         searchEngineName:(NSString*)name;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSUMER_H_
