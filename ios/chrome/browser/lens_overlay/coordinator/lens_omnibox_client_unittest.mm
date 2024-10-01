// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"

#import "base/test/task_environment.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feature_engagement/test/test_tracker.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "components/omnibox/browser/fake_autocomplete_provider.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_web_provider.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

/// Fake LensWebProvider.
@interface FakeLensWebProvider : NSObject <LensWebProvider>
@property(nonatomic, assign) web::WebState* webState;
@end
@implementation FakeLensWebProvider
@end

class LensOmniboxClientTest : public PlatformTest {
 public:
  LensOmniboxClientTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    tracker_ = feature_engagement::CreateTestTracker();

    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_provider_ = [[FakeLensWebProvider alloc] init];
    fake_web_provider_.webState = fake_web_state_.get();

    mock_delegate_ =
        [OCMockObject mockForProtocol:@protocol(LensOmniboxClientDelegate)];

    lens_omnibox_client_ = std::make_unique<LensOmniboxClient>(
        profile_.get(), tracker_.get(), fake_web_provider_, mock_delegate_);
  }

  void UseAutocompleteMatch(const std::u16string& input_text,
                            const AutocompleteMatch& match) {
    lens_omnibox_client_->OnAutocompleteAccept(
        match.destination_url, match.post_content.get(),
        WindowOpenDisposition::CURRENT_TAB, match.transition, match.type,
        base::TimeTicks(), false, false, input_text, match, match,
        IDNA2008DeviationCharacter::kNone);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<feature_engagement::Tracker> tracker_;

  std::unique_ptr<web::FakeWebState> fake_web_state_;
  FakeLensWebProvider* fake_web_provider_;
  OCMockObject<LensOmniboxClientDelegate>* mock_delegate_;

  std::unique_ptr<LensOmniboxClient> lens_omnibox_client_;
};

// Tests that the delegate is called on AutocompleteAccept.
TEST_F(LensOmniboxClientTest, AutocompleteAccept) {
  const std::u16string& input_text = u"search terms";
  AutocompleteMatch match{/*provider=*/nullptr, /*relevance=*/1000,
                          /*deletable=*/false,
                          /*type=*/AutocompleteMatchType::SEARCH_SUGGEST};
  match.fill_into_edit = input_text;
  match.destination_url = GURL("https://www.google.com/search?q=search+terms");

  OCMExpect([mock_delegate_ omniboxDidAcceptText:match.fill_into_edit
                                  destinationURL:match.destination_url
                                thumbnailRemoved:NO]);
  UseAutocompleteMatch(input_text, match);

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that GetFormattedFullURL returns the search terms when they are
// available.
TEST_F(LensOmniboxClientTest, GetFormattedFullURL) {
  // Returns search terms when they are available.
  fake_web_state_->SetVisibleURL(
      GURL("https://www.google.com/search?q=search+terms"));
  EXPECT_EQ(lens_omnibox_client_->GetFormattedFullURL(), u"search terms");

  // Returns empty string when they are not available.
  fake_web_state_->SetVisibleURL(GURL());
  EXPECT_EQ(lens_omnibox_client_->GetFormattedFullURL(), u"");
}
