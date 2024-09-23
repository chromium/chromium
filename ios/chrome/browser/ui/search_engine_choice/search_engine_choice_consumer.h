// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSUMER_H_

#import <UIKit/UIKit.h>

@class SnippetSearchEngineElement;

// Handles search engine choice UI updates.
@protocol SearchEngineChoiceConsumer

// The list of search engines to offer in the choice screen.
@property(nonatomic, strong)
    NSArray<SnippetSearchEngineElement*>* searchEngines;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSUMER_H_
