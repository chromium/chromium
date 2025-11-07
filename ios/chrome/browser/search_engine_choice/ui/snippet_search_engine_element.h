// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SNIPPET_SEARCH_ENGINE_ELEMENT_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SNIPPET_SEARCH_ENGINE_ELEMENT_H_

#import <UIKit/UIKit.h>

enum class SnippetState;

// This enum set the state of the current default search engine to highlight.
// The elements state should be either:
// - all elements are in `kNoCurrentDefault`.
// - all elements are in `kHasCurrentDefault` except the right element that is
//   in `kIsCurrentDefault`.
enum class CurrentDefaultState {
  // There is no current default search engine to highlight.
  kNoCurrentDefault,
  // This is not the current default search engine, but there is a current
  // default search engine.
  kHasCurrentDefault,
  // This search engine is the current default.
  kIsCurrentDefault,
};

// SnippetSearchEngineElement contains the model data for a
// SnippetSearchEngineButton.
@interface SnippetSearchEngineElement : NSObject

// The name of the search engine.
@property(nonatomic, copy) NSString* name;
// The text for the search engine snippet.
@property(nonatomic, copy) NSString* snippetDescription;
// Favicon attributes for the search engine.
@property(nonatomic, strong) UIImage* faviconImage;
// Search engine keyword. Unique per search engine.
@property(nonatomic, copy) NSString* keyword;
// See `CurrentDefaultState`. The default value is `kNoCurrentDefault`.
@property(nonatomic, assign) CurrentDefaultState currentDefaultState;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SNIPPET_SEARCH_ENGINE_ELEMENT_H_
