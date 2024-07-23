// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SEARCH_ENGINE_SETTINGS_TEST_CASE_BASE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SEARCH_ENGINE_SETTINGS_TEST_CASE_BASE_H_

#import "ios/chrome/test/earl_grey/chrome_test_case.h"

@protocol GREYMatcher;
namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
}  // namespace TemplateURLPrepopulateData

// Name for the custom search engine.
extern NSString* const kCustomSearchEngineName;

// Base class to run test for the search engine settings.
@interface SearchEngineSettingsTestCaseBase : ChromeTestCase

// Returns the country used for the test. This is used with
// `switches::kSearchEngineChoiceCountry`.
// This method needs to be overridden by the subclass.
+ (const char*)countryForTestCase;

// Returns the second prepopulated search engine used for the test.
// This method needs to be overridden by the subclass.
+ (const TemplateURLPrepopulateData::PrepopulatedEngine*)
    secondPrepopulatedSearchEngine;

// Returns edit button matcher.
+ (id<GREYMatcher>)editButtonMatcherWithEnabled:(BOOL)enabled;

// Adds a custom search engine by navigating to a fake search engine page, then
// enters the search engine screen in Settings.
- (void)enterSettingsWithCustomSearchEngine;

// Starts HTTP server. It is needed to intercept Google search engine and
// the second search engine requests.
- (void)startHTTPServer;

// Adds url rewriter. `-[SearchEngineSettingsTestCaseBase startHTTPServer]`
// needs to be called first.
- (void)addURLRewriter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SEARCH_ENGINE_SETTINGS_TEST_CASE_BASE_H_
