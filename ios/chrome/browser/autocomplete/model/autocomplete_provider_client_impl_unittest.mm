// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"

#import <memory>
#import <string_view>

#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::ElementsAre;

class AutocompleteProviderClientImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    client_ = std::make_unique<AutocompleteProviderClientImpl>(profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<AutocompleteProviderClientImpl> client_;
};

// Tests that `GetBuiltinsToProvideAsUserTypes()` includes chrome://chrome-urls,
// chrome://flags, and chrome://version.
TEST_F(AutocompleteProviderClientImplTest, GetBuiltinsToProvideAsUserTypes) {
  EXPECT_THAT(
      client_->GetBuiltinsToProvideAsUserTypes(),
      ElementsAre(base::ASCIIToUTF16(std::string_view(kChromeUIChromeURLsURL)),
                  base::ASCIIToUTF16(std::string_view(kChromeUIFlagsURL)),
                  base::ASCIIToUTF16(std::string_view(kChromeUIVersionURL))));
}

}  // namespace
