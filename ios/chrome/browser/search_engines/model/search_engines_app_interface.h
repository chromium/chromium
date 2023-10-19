// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// App interface for the search engines.
@interface SearchEnginesAppInterface : NSObject

// Returns the short name of the default search engine.
+ (NSString*)defaultSearchEngine;

// Resets the default search engine to `defaultSearchEngine`.
// `defaultSearchEngine` should be its short name.
+ (void)setSearchEngineTo:(NSString*)defaultSearchEngine;

// Adds a new Search engine an optionally sets it as default.
+ (void)addSearchEngineWithName:(NSString*)name
                            URL:(NSString*)URL
                     setDefault:(BOOL)setDefault;

// Removes the search engine.
+ (void)removeSearchEngineWithName:(NSString*)name;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_APP_INTERFACE_H_
