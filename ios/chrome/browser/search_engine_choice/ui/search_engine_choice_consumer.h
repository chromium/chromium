// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SEARCH_ENGINE_CHOICE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SEARCH_ENGINE_CHOICE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import <optional>

@class SnippetSearchEngineElement;

// Handles search engine choice UI updates.
@protocol SearchEngineChoiceConsumer

// The list of search engines to offer in the choice screen.
@property(nonatomic, strong)
    NSArray<SnippetSearchEngineElement*>* searchEngines;

// Title, subtitle 1, learn more for subtitle 1 with its accessibility, and the
// subtitle 2 (optional), for the search engine screen.
@property(nonatomic, assign) int titleStringID;
@property(nonatomic, assign) int subtitle1StringID;
@property(nonatomic, assign) int subtitle1LearnMoreSuffixStringID;
@property(nonatomic, assign) int subtitle1LearnMoreA11yStringID;
@property(nonatomic, assign) std::optional<int> subtitle2StringID;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SEARCH_ENGINE_CHOICE_CONSUMER_H_
