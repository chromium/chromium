// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/navigation_util.h"

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/navigation/proto_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Constants used by the tests.
const char kURL[] = "https://example.com";
const char16_t kTitle[] = u"Example domain";
const char kVirtualURL[] = "https://example.com/virtual-url";
const char kFakeHTTPHeaderName[] = "X-Fake-HTTP-Header";
const char kFakeHTTPHeaderValue[] = "Value";

}  // namespace

using NavigationUtilTest = PlatformTest;

// Tests that CreateWebStateStorage() returns an object with the expected
// content in a simple scenario (URL and title).
TEST_F(NavigationUtilTest, CreateWebStateStorage) {
  web::NavigationManager::WebLoadParams params(GURL{kURL});

  const bool created_with_opener = false;
  const base::Time creation_time = base::Time::Now();
  const web::UserAgentType user_agent = web::UserAgentType::MOBILE;
  const web::proto::WebStateStorage storage = web::CreateWebStateStorage(
      params, kTitle, created_with_opener, user_agent, creation_time);

  EXPECT_TRUE(storage.has_metadata());
  EXPECT_TRUE(storage.has_navigation());
  EXPECT_EQ(storage.has_opener(), created_with_opener);
  EXPECT_EQ(web::UserAgentTypeFromProto(storage.user_agent()), user_agent);

  const web::proto::WebStateMetadataStorage& metadata = storage.metadata();
  EXPECT_EQ(creation_time, web::TimeFromProto(metadata.creation_time()));
  EXPECT_EQ(creation_time, web::TimeFromProto(metadata.last_active_time()));
  EXPECT_EQ(GURL(metadata.active_page().page_url()), GURL(kURL));
  EXPECT_EQ(base::UTF8ToUTF16(metadata.active_page().page_title()), kTitle);
  EXPECT_EQ(metadata.navigation_item_count(), 1);

  ASSERT_EQ(storage.navigation().items_size(), 1);
  EXPECT_EQ(storage.navigation().last_committed_item_index(), 0);
  const web::proto::NavigationItemStorage& item = storage.navigation().items(0);

  EXPECT_EQ(GURL(item.url()), GURL(kURL));
  EXPECT_EQ(item.virtual_url(), "");
  EXPECT_EQ(base::UTF8ToUTF16(item.title()), kTitle);
  EXPECT_EQ(web::TimeFromProto(item.timestamp()), creation_time);
  EXPECT_EQ(web::UserAgentTypeFromProto(item.user_agent()), user_agent);

  EXPECT_FALSE(item.has_referrer());
  EXPECT_FALSE(item.has_http_request_headers());
}

// Tests that CreateWebStateStorage() returns an object with the expected
// content in a more complex scenario (URL, virtual URL, title, referrer,
// HTTP headers, ...).
TEST_F(NavigationUtilTest, CreateWebStateStorage_ComplexScenario) {
  web::NavigationManager::WebLoadParams params(GURL{kURL});
  params.virtual_url = GURL(kVirtualURL);
  params.referrer = web::Referrer(GURL(kURL), web::ReferrerPolicyOrigin);
  params.extra_headers = @{@(kFakeHTTPHeaderName) : @(kFakeHTTPHeaderValue)};

  const bool created_with_opener = true;
  const base::Time creation_time = base::Time::Now() - base::Hours(1);
  const web::UserAgentType user_agent = web::UserAgentType::DESKTOP;
  const web::proto::WebStateStorage storage = web::CreateWebStateStorage(
      params, kTitle, created_with_opener, user_agent, creation_time);

  EXPECT_TRUE(storage.has_metadata());
  EXPECT_TRUE(storage.has_navigation());
  EXPECT_EQ(storage.has_opener(), created_with_opener);
  EXPECT_EQ(web::UserAgentTypeFromProto(storage.user_agent()), user_agent);

  const web::proto::WebStateMetadataStorage& metadata = storage.metadata();
  EXPECT_EQ(creation_time, web::TimeFromProto(metadata.creation_time()));
  EXPECT_EQ(creation_time, web::TimeFromProto(metadata.last_active_time()));
  EXPECT_EQ(GURL(metadata.active_page().page_url()), GURL(kVirtualURL));
  EXPECT_EQ(base::UTF8ToUTF16(metadata.active_page().page_title()), kTitle);
  EXPECT_EQ(metadata.navigation_item_count(), 1);

  ASSERT_EQ(storage.navigation().items_size(), 1);
  EXPECT_EQ(storage.navigation().last_committed_item_index(), 0);
  const web::proto::NavigationItemStorage& item = storage.navigation().items(0);

  EXPECT_EQ(GURL(item.url()), GURL(kURL));
  EXPECT_EQ(GURL(item.virtual_url()), GURL(kVirtualURL));
  EXPECT_EQ(base::UTF8ToUTF16(item.title()), kTitle);
  EXPECT_EQ(web::TimeFromProto(item.timestamp()), creation_time);
  EXPECT_EQ(web::UserAgentTypeFromProto(item.user_agent()), user_agent);

  ASSERT_TRUE(item.has_referrer());
  const web::proto::ReferrerStorage& referrer = item.referrer();

  EXPECT_EQ(GURL(referrer.url()), params.referrer.url);
  EXPECT_EQ(web::ReferrerPolicyFromProto(referrer.policy()),
            params.referrer.policy);

  ASSERT_TRUE(item.has_http_request_headers());
  const web::proto::HttpHeaderListStorage& http = item.http_request_headers();

  EXPECT_EQ(http.headers_size(), static_cast<int>(params.extra_headers.count));
  for (const auto& header : http.headers()) {
    NSString* key = base::SysUTF8ToNSString(header.name());
    EXPECT_EQ(header.value(),
              base::SysNSStringToUTF8(params.extra_headers[key]));
  }
}
