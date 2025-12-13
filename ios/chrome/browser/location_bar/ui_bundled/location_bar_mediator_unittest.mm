// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_consumer.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// The name of the default search provider established in SetUp.
const char16_t kTestProviderName[] = u"TestProvider";

}  // namespace

class LocationBarMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());

    // Initialize with a default search engine.
    AddAndSetDefaultSearchProvider(kTestProviderName);

    mediator_ = [[LocationBarMediator alloc] initWithIsIncognito:NO];
    mediator_.templateURLService = template_url_service_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    PlatformTest::TearDown();
  }

  // Helper to create, register, and select a default search provider.
  void AddAndSetDefaultSearchProvider(const std::u16string& name) {
    TemplateURLData data;
    data.SetShortName(name);
    data.SetURL("https://www.test.com/?q={searchTerms}");

    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TemplateURLService> template_url_service_;
  LocationBarMediator* mediator_;
};

// Tests that the consumer is updated with the placeholder text immediately
// upon being set.
TEST_F(LocationBarMediatorTest, SetConsumerUpdatesPlaceholderTextImmediately) {
  id mock_consumer = OCMProtocolMock(@protocol(LocationBarConsumer));

  OCMExpect([mock_consumer
      setPlaceholderText:base::SysUTF16ToNSString(kTestProviderName)]);

  [mediator_ setConsumer:mock_consumer];

  EXPECT_OCMOCK_VERIFY(mock_consumer);
}
