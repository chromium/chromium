// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_cobrowse_omnibox_client.h"

#import "base/test/task_environment.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/search_engines/template_url.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeComposeboxOmniboxClientDelegate
    : NSObject <ComposeboxOmniboxClientDelegate>
@property(nonatomic, assign) std::u16string acceptedText;
@end

@implementation FakeComposeboxOmniboxClientDelegate
- (web::WebState*)webState {
  return nullptr;
}
- (contextual_search::InputState)inputState {
  return {};
}
- (std::optional<lens::proto::LensOverlaySuggestInputs>)suggestInputs {
  return std::nullopt;
}
- (ComposeboxMode)composeboxMode {
  return ComposeboxMode::kRegularSearch;
}

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
               URLLoadParams:(const UrlLoadParams&)URLLoadParams
                isSearchType:(BOOL)isSearchType {
  self.acceptedText = text;
}

- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress {
}
@end

class ComposeboxCobrowseOmniboxClientTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    tracker_ = std::make_unique<feature_engagement::test::MockTracker>();
    delegate_ = [[FakeComposeboxOmniboxClientDelegate alloc] init];

    client_ = std::make_unique<ComposeboxCobrowseOmniboxClient>(
        browser_.get(), tracker_.get(), delegate_);
  }

  void TearDown() override {
    client_.reset();
    tracker_.reset();
    browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<feature_engagement::test::MockTracker> tracker_;
  FakeComposeboxOmniboxClientDelegate* delegate_;
  std::unique_ptr<ComposeboxCobrowseOmniboxClient> client_;
};

// Tests that OnAutocompleteAccept correctly combines the raw typed text
// with the inline autocompletion from the match, ensuring the full visible
// text is sent to the delegate.
TEST_F(ComposeboxCobrowseOmniboxClientTest, OnAutocompleteAccept) {
  std::u16string text = u"typed";
  AutocompleteMatch match;
  match.inline_autocompletion = u" completed";
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;

  client_->OnAutocompleteAccept(
      GURL("http://example.com"), nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      base::TimeTicks::Now(), false, false, text, match, match);

  EXPECT_EQ(delegate_.acceptedText, u"typed completed");
}

// Tests that OnAutocompleteAccept ignores fill_into_edit and instead uses
// the combined typed text and inline autocompletion, ensuring we send what
// is visible.
TEST_F(ComposeboxCobrowseOmniboxClientTest,
       OnAutocompleteAccept_IgnoresFillIntoEdit) {
  std::u16string text = u"typed";
  AutocompleteMatch match;
  match.inline_autocompletion = u" completed";
  match.fill_into_edit = u"different fill into edit";
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;

  client_->OnAutocompleteAccept(
      GURL("http://example.com"), nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      base::TimeTicks::Now(), false, false, text, match, match);

  EXPECT_EQ(delegate_.acceptedText, u"typed completed");
}
