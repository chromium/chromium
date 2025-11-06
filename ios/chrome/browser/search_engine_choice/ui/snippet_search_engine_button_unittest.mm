// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui/snippet_search_engine_button.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using SnippetSearchEngineButtonTest = PlatformTest;

// Tests that all string properties are configured as "copy".
TEST_F(SnippetSearchEngineButtonTest, TestHistograms) {
  SnippetSearchEngineButton* search_engine_button =
      [[SnippetSearchEngineButton alloc]
          initWithCurrentDefaultState:CurrentDefaultState::kNoCurrentDefault];
  NSString* expected_search_engine_name = @"name";
  NSString* expected_snippet_text = @"snippet";
  NSString* expected_search_engine_keyword = @"keyword";
  NSMutableString* mutable_search_engine_name =
      [expected_search_engine_name mutableCopy];
  NSMutableString* mutable_snippet_text = [expected_snippet_text mutableCopy];
  NSMutableString* mutable_search_engine_keyword =
      [expected_search_engine_keyword mutableCopy];
  search_engine_button.searchEngineName = mutable_search_engine_name;
  search_engine_button.snippetText = mutable_snippet_text;
  search_engine_button.searchEngineKeyword = mutable_search_engine_keyword;
  [mutable_search_engine_name appendString:@" added"];
  [mutable_snippet_text appendString:@" added"];
  [mutable_search_engine_keyword appendString:@" added"];
  EXPECT_NSEQ(search_engine_button.searchEngineName,
              expected_search_engine_name);
  EXPECT_NSEQ(search_engine_button.snippetText, expected_snippet_text);
  EXPECT_NSEQ(search_engine_button.searchEngineKeyword,
              expected_search_engine_keyword);
}
