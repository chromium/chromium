// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/extension_search_engine_data_updater.h"

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for ExtensionSearchEngineDataUpdater class.
class ExtensionSearchEngineDataUpdaterTest : public PlatformTest {
 protected:
  ExtensionSearchEngineDataUpdaterTest()
      : search_by_image_key_(base::SysUTF8ToNSString(
            app_group::kChromeAppGroupSupportsSearchByImage)),
        is_google_key_(base::SysUTF8ToNSString(
            app_group::kChromeAppGroupIsGoogleDefaultSearchEngine)) {}

  void SetUp() override {
    PlatformTest::SetUp();

    template_url_service_.reset(new TemplateURLService(nullptr, 0));
    template_url_service_->Load();
    observer_.reset(
        new ExtensionSearchEngineDataUpdater(template_url_service_.get()));

    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    [shared_defaults setBool:NO forKey:search_by_image_key_];
  }

  bool StoredSupportsSearchByImage() {
    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    return [shared_defaults boolForKey:search_by_image_key_];
  }

  bool StoredIsGoogleDefaultSearchEngine() {
    NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
    return [shared_defaults boolForKey:is_google_key_];
  }

  std::unique_ptr<TemplateURLService> template_url_service_;

 private:
  std::unique_ptr<ExtensionSearchEngineDataUpdater> observer_;
  NSString* search_by_image_key_;
  NSString* is_google_key_;
};

TEST_F(ExtensionSearchEngineDataUpdaterTest, AddSupportedSearchEngine) {
  ASSERT_FALSE(StoredSupportsSearchByImage());

  const char kImageSearchURL[] = "http://foo.com/sbi";
  const char kPostParamsString[] = "image_content={google:imageThumbnail}";

  TemplateURLData supported_template_url_data{};
  supported_template_url_data.image_url = kImageSearchURL;
  supported_template_url_data.image_url_post_params = kPostParamsString;
  TemplateURL supported_template_url(supported_template_url_data);

  template_url_service_->SetUserSelectedDefaultSearchProvider(
      &supported_template_url);

  ASSERT_TRUE(StoredSupportsSearchByImage());
}

TEST_F(ExtensionSearchEngineDataUpdaterTest, AddUnsupportedSearchEngine) {
  ASSERT_FALSE(StoredSupportsSearchByImage());

  TemplateURLData unsupported_template_url_data{};
  TemplateURL unsupported_template_url(unsupported_template_url_data);

  template_url_service_->SetUserSelectedDefaultSearchProvider(
      &unsupported_template_url);

  ASSERT_FALSE(StoredSupportsSearchByImage());
}

TEST_F(ExtensionSearchEngineDataUpdaterTest, AddGoogleSearchEngine) {
  ASSERT_FALSE(StoredSupportsSearchByImage());

  TemplateURLData google_template_url_data(
      u" shortname ", u" keyword ", "https://google.com", base::StringPiece(),
      base::StringPiece(), base::StringPiece(), base::StringPiece(),
      base::StringPiece(), base::StringPiece(), base::StringPiece(),
      base::StringPiece(), base::StringPiece(), base::StringPiece(),
      base::StringPiece(), base::StringPiece(), base::StringPiece(),
      base::StringPiece(), {}, base::StringPiece(), base::StringPiece(),
      base::StringPiece16(), base::Value::List(), false, false, 0);
  TemplateURL google_template_url(google_template_url_data);

  template_url_service_->SetUserSelectedDefaultSearchProvider(
      &google_template_url);

  ASSERT_TRUE(StoredIsGoogleDefaultSearchEngine());
}
