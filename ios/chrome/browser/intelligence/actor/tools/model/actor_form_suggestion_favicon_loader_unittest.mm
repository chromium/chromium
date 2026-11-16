// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_loader.h"

#import <UIKit/UIKit.h>

#import <vector>

#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/mock_favicon_loader.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

class ActorFormSuggestionFaviconLoaderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    loader_ = [[ActorFormSuggestionFaviconLoader alloc]
        initWithFaviconLoader:&mock_favicon_loader_];
  }

  void TearDown() override {
    [loader_ disconnect];
    loader_ = nil;
    PlatformTest::TearDown();
  }

  testing::StrictMock<MockFaviconLoader> mock_favicon_loader_;
  ActorFormSuggestionFaviconLoader* loader_ = nil;
};

// Tests that `source_site_or_app` is preferred over `request_origin` when it is
// a valid HTTP/HTTPS URL, and that two-phase `FaviconLoader` callbacks forward
// `attributes.faviconImage`.
TEST_F(ActorFormSuggestionFaviconLoaderTest, LoadsFaviconFromSourceSiteOrApp) {
  const GURL source_url("https://accounts.example.com/");
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.source_site_or_app = u"https://accounts.example.com/";
  cred.request_origin = url::Origin::Create(GURL("https://shop.example.com"));
  ActorFormSuggestion* suggestion =
      [[ActorFormSuggestion alloc] initWithCredential:cred];

  FaviconLoader::FaviconAttributesCompletionBlock favicon_completion = nil;
  EXPECT_CALL(mock_favicon_loader_,
              FaviconForPageUrlOrHost(source_url, kDesiredMediumFaviconSizePt,
                                      testing::_))
      .WillOnce(
          [&favicon_completion](
              const GURL&, float,
              FaviconLoader::FaviconAttributesCompletionBlock completion) {
            FaviconAttributes* monogram =
                [FaviconAttributes attributesWithMonogram:@"A"
                                                textColor:UIColor.whiteColor
                                          backgroundColor:UIColor.blackColor
                                   defaultBackgroundColor:YES];
            completion(monogram, /*cached=*/true);
            favicon_completion = [completion copy];
          });

  __block std::vector<UIImage*> received_icons;
  EXPECT_TRUE([loader_ actorFormSuggestion:suggestion
                 loadFaviconWithCompletion:^(UIImage* icon) {
                   received_icons.push_back(icon);
                 }]);
  ASSERT_EQ(received_icons.size(), 1u);
  EXPECT_EQ(received_icons[0], nil);
  ASSERT_TRUE(favicon_completion);

  UIImage* favicon = [[UIImage alloc] init];
  favicon_completion([FaviconAttributes attributesWithImage:favicon],
                     /*cached=*/false);
  ASSERT_EQ(received_icons.size(), 2u);
  EXPECT_EQ(received_icons[1], favicon);
}

// Tests that `request_origin` is used as a fallback when `source_site_or_app`
// is empty or not an HTTP/HTTPS URL.
TEST_F(ActorFormSuggestionFaviconLoaderTest,
       FallsBackToRequestOriginWhenSourceSiteIsNotHttpOrHttps) {
  const GURL origin_url("https://example.com/");
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.source_site_or_app = u"android://hash@com.example.app/";
  cred.request_origin = url::Origin::Create(origin_url);
  ActorFormSuggestion* suggestion =
      [[ActorFormSuggestion alloc] initWithCredential:cred];

  UIImage* favicon = [[UIImage alloc] init];
  EXPECT_CALL(mock_favicon_loader_,
              FaviconForPageUrlOrHost(origin_url, kDesiredMediumFaviconSizePt,
                                      testing::_))
      .WillOnce(
          [favicon](
              const GURL&, float,
              FaviconLoader::FaviconAttributesCompletionBlock completion) {
            completion([FaviconAttributes attributesWithImage:favicon],
                       /*cached=*/true);
          });

  __block UIImage* loaded_icon = nil;
  EXPECT_TRUE([loader_ actorFormSuggestion:suggestion
                 loadFaviconWithCompletion:^(UIImage* icon) {
                   loaded_icon = icon;
                 }]);
  EXPECT_EQ(loaded_icon, favicon);
}

// Tests that loading returns `NO` when neither `source_site_or_app` nor
// `request_origin` produces a valid URL.
TEST_F(ActorFormSuggestionFaviconLoaderTest, ReturnsNoWhenUrlsAreInvalid) {
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  ActorFormSuggestion* suggestion =
      [[ActorFormSuggestion alloc] initWithCredential:cred];

  __block bool completion_called = false;
  EXPECT_FALSE([loader_ actorFormSuggestion:suggestion
                  loadFaviconWithCompletion:^(UIImage* icon) {
                    completion_called = true;
                  }]);
  EXPECT_FALSE(completion_called);
}

// Tests that loading returns `NO` for non-credential autofill suggestions.
TEST_F(ActorFormSuggestionFaviconLoaderTest,
       ReturnsNoForNonCredentialSuggestion) {
  autofill::ActorSuggestion autofill_suggestion;
  autofill_suggestion.id = autofill::ActorSuggestionId(1);
  autofill_suggestion.title = "123 Main St";
  ActorFormSuggestion* suggestion = [[ActorFormSuggestion alloc]
      initWithActorSuggestion:autofill_suggestion
                     dataType:autofill::ActorFormFillingRequestedData::
                                  kAddress];

  __block bool completion_called = false;
  EXPECT_FALSE([loader_ actorFormSuggestion:suggestion
                  loadFaviconWithCompletion:^(UIImage* icon) {
                    completion_called = true;
                  }]);
  EXPECT_FALSE(completion_called);
}

// Tests that loading returns `NO` when initialized with a null `FaviconLoader`
// or after `-disconnect` is called.
TEST_F(ActorFormSuggestionFaviconLoaderTest,
       ReturnsNoWhenFaviconLoaderIsNullOrDisconnected) {
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.source_site_or_app = u"https://example.com/";
  ActorFormSuggestion* suggestion =
      [[ActorFormSuggestion alloc] initWithCredential:cred];

  ActorFormSuggestionFaviconLoader* null_loader =
      [[ActorFormSuggestionFaviconLoader alloc] initWithFaviconLoader:nullptr];
  EXPECT_FALSE([null_loader actorFormSuggestion:suggestion
                      loadFaviconWithCompletion:^(UIImage*){
                      }]);

  [loader_ disconnect];
  EXPECT_FALSE([loader_ actorFormSuggestion:suggestion
                  loadFaviconWithCompletion:^(UIImage*){
                  }]);
}

}  // namespace
