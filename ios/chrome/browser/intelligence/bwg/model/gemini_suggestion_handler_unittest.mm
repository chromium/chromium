// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Fake implementation of `GeminiTabHelper` for testing suggestion handling
// without relying on private state.
class FakeGeminiTabHelper : public GeminiTabHelper {
 public:
  static FakeGeminiTabHelper* CreateForWebState(web::WebState* web_state) {
    CHECK(web_state);
    auto tab_helper = std::make_unique<FakeGeminiTabHelper>();
    FakeGeminiTabHelper* raw_tab_helper = tab_helper.get();
    web_state->SetUserData(GeminiTabHelper::UserDataKey(),
                           std::move(tab_helper));
    return raw_tab_helper;
  }

  FakeGeminiTabHelper() = default;
  ~FakeGeminiTabHelper() override = default;

  void SetSuggestions(NSArray<NSString*>* suggestions) {
    suggestions_ = [suggestions copy];
  }

  int fetch_count() const { return fetch_count_; }

  // GeminiTabHelper:
  void FetchZeroStateSuggestionsAsStrings(
      base::OnceCallback<void(NSArray<NSString*>* suggestions)> callback)
      override {
    ++fetch_count_;
    std::move(callback).Run(suggestions_);
  }

 private:
  __strong NSArray<NSString*>* suggestions_ = nil;
  int fetch_count_ = 0;
};

}  // namespace

class GeminiSuggestionHandlerTest : public PlatformTest {
 protected:
  GeminiSuggestionHandlerTest()
      : web_state_list_(&web_state_list_delegate_),
        handler_([[GeminiSuggestionHandler alloc]
            initWithWebStateList:&web_state_list_]) {}

  // Creates a fake `WebState`, optionally attaches `FakeGeminiTabHelper`, and
  // inserts it as the active `WebState`.
  FakeGeminiTabHelper* CreateAndInsertWebState(bool attach_tab_helper = true) {
    auto web_state = std::make_unique<web::FakeWebState>();

    FakeGeminiTabHelper* tab_helper = nullptr;
    if (attach_tab_helper) {
      tab_helper = FakeGeminiTabHelper::CreateForWebState(web_state.get());
    }

    web_state_list_.InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));

    return tab_helper;
  }

  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  GeminiSuggestionHandler* handler_;
};

// Tests that `-fetchZeroStateSuggestions:` safely returns without querying the
// tab helper when `completion` is `nil`.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_NilCompletion) {
  FakeGeminiTabHelper* tab_helper = CreateAndInsertWebState();
  ASSERT_TRUE(tab_helper);

  [handler_ fetchZeroStateSuggestions:nil];

  EXPECT_EQ(0, tab_helper->fetch_count());
}

// Tests that `-fetchZeroStateSuggestions:` calls `completion` with `nil` when
// initialized with a null `WebStateList`.
TEST_F(GeminiSuggestionHandlerTest,
       FetchZeroStateSuggestions_NullWebStateList) {
  GeminiSuggestionHandler* handler =
      [[GeminiSuggestionHandler alloc] initWithWebStateList:nullptr];

  base::test::TestFuture<NSArray<NSString*>*> future;
  [handler
      fetchZeroStateSuggestions:base::CallbackToBlock(future.GetCallback())];

  EXPECT_EQ(nil, future.Get());
}

// Tests that `-fetchZeroStateSuggestions:` calls `completion` with `nil` when
// there is no active `WebState`.
TEST_F(GeminiSuggestionHandlerTest,
       FetchZeroStateSuggestions_NoActiveWebState) {
  base::test::TestFuture<NSArray<NSString*>*> future;
  [handler_
      fetchZeroStateSuggestions:base::CallbackToBlock(future.GetCallback())];

  EXPECT_EQ(nil, future.Get());
}

// Tests that `-fetchZeroStateSuggestions:` calls `completion` with `nil` when
// the active `WebState` has no `GeminiTabHelper`.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_NoTabHelper) {
  CreateAndInsertWebState(/*attach_tab_helper=*/false);

  base::test::TestFuture<NSArray<NSString*>*> future;
  [handler_
      fetchZeroStateSuggestions:base::CallbackToBlock(future.GetCallback())];

  EXPECT_EQ(nil, future.Get());
}

// Tests that `-fetchZeroStateSuggestions:` calls `completion` with suggestions
// when available.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_Success) {
  FakeGeminiTabHelper* tab_helper = CreateAndInsertWebState();
  ASSERT_TRUE(tab_helper);
  tab_helper->SetSuggestions(@[ @"suggestion1", @"suggestion2" ]);

  base::test::TestFuture<NSArray<NSString*>*> future;
  [handler_
      fetchZeroStateSuggestions:base::CallbackToBlock(future.GetCallback())];

  NSArray<NSString*>* returned_suggestions = future.Get();
  ASSERT_NE(nil, returned_suggestions);
  EXPECT_EQ(2u, returned_suggestions.count);
  EXPECT_NSEQ(@"suggestion1", returned_suggestions[0]);
  EXPECT_NSEQ(@"suggestion2", returned_suggestions[1]);
}

// Tests that `-fetchZeroStateSuggestions:` calls `completion` with an empty
// array when suggestions are empty.
TEST_F(GeminiSuggestionHandlerTest, FetchZeroStateSuggestions_Empty) {
  FakeGeminiTabHelper* tab_helper = CreateAndInsertWebState();
  ASSERT_TRUE(tab_helper);
  tab_helper->SetSuggestions(@[]);

  base::test::TestFuture<NSArray<NSString*>*> future;
  [handler_
      fetchZeroStateSuggestions:base::CallbackToBlock(future.GetCallback())];

  NSArray<NSString*>* returned_suggestions = future.Get();
  ASSERT_NE(nil, returned_suggestions);
  EXPECT_EQ(0u, returned_suggestions.count);
}
