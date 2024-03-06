// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SNIPPET_SEARCH_ENGINE_ELEMENT_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SNIPPET_SEARCH_ENGINE_ELEMENT_H_

#import <UIKit/UIKit.h>

enum class SnippetState;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SNIPPET_SEARCH_ENGINE_ELEMENT_H_
