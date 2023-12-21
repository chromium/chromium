// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_CONSUMER_H_

@class SnippetSearchEngineItem;

// Handles search engine choice table UI updates.
@protocol SearchEngineChoiceTableConsumer

// The list of search engines to offer in the choice screen.
@property(nonatomic, strong) NSArray<SnippetSearchEngineItem*>* searchEngines;

- (void)reloadData;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_CONSUMER_H_
