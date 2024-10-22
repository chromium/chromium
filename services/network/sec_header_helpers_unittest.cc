// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sec_header_helpers.h"

#include "base/test/task_environment.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace {

constexpr char kSecureSite[] = "https://site.tld";
constexpr char kInsecureSite[] = "http://othersite.tld";
constexpr char kPrivilegedInitiator[] = "https://chrome-extension.example.com";

constexpr char kKnownSecChHeader[] = "Sec-CH-UA";
constexpr char kKnownSecFetchSiteHeader[] = "Sec-Fetch-Site";
constexpr char kKnownSecFetchModeHeader[] = "Sec-Fetch-Mode";
constexpr char kKnownSecFetchUserHeader[] = "Sec-Fetch-User";
constexpr char kKnownSecFetchDestHeader[] = "Sec-Fetch-Dest";
constexpr char kKnownSecFetchStorageAccessHeader[] = "Sec-Fetch-Storage-Access";
constexpr char kOtherSecHeader[] = "sec-other-info-header";
constexpr char kOtherHeader[] = "Other-Header";

constexpr char kHeaderValue[] = "testdata";

}  // namespace

namespace network {

using testing::UnorderedElementsAreArray;

class SecHeaderHelpersTest : public PlatformTest {
 public:
  SecHeaderHelpersTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(GURL(kSecureSite),
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {
    url_request_->set_initiator(
        url::Origin::Create(GURL(kPrivilegedInitiator)));
  }

  net::URLRequest* url_request() const { return url_request_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

// Validate that Sec- prefixed headers are all removed when a request is
// downgraded from trustworthy to not such as when an https => http redirect
// occurs. We should only remove sec-ch- and sec-fetch- prefixed headers. Others
// should remain as they may be valid in an insecure context.
TEST_F(SecHeaderHelpersTest, SecHeadersRemovedOnDowngrade) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherSecHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));

  EXPECT_THAT(current_url_request->extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{kOtherSecHeader,
                                                              kHeaderValue},
                  net::HttpRequestHeaders::HeaderKeyValuePair{kOtherHeader,
                                                              kHeaderValue},
              }));
}

// Validate that if no downgrade occurs any Sec- prefixed headers remain on the
// provided request.
TEST_F(SecHeaderHelpersTest, SecHeadersRemainOnSecureRedirect) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherSecHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kSecureSite));

  EXPECT_THAT(current_url_request->extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{kKnownSecChHeader,
                                                              kHeaderValue},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchSiteHeader, kHeaderValue},
                  net::HttpRequestHeaders::HeaderKeyValuePair{kOtherSecHeader,
                                                              kHeaderValue},
                  net::HttpRequestHeaders::HeaderKeyValuePair{kOtherHeader,
                                                              kHeaderValue},
              }));
}

// Validate that if Sec- headers exist as the first or last entries we properly
// remove them also.
TEST_F(SecHeaderHelpersTest, SecHeadersRemoveFirstLast) {
  net::URLRequest* current_url_request = url_request();

  current_url_request->SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                   /*overwrite=*/false);
  current_url_request->SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                   kHeaderValue,
                                                   /*overwrite=*/false);
  ASSERT_EQ(3, static_cast<int>(current_url_request->extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));

  EXPECT_THAT(current_url_request->extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{kOtherHeader,
                                                              kHeaderValue},
              }));
}

// Validate Sec-Fetch-Site and Sec-Fetch-Mode are set correctly with
// unprivileged requests from chrome extension background page.
TEST_F(SecHeaderHelpersTest, UnprivilegedRequestOnExtension) {
  net::URLRequest* current_url_request = url_request();
  url_request()->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kNone);
  GURL url = GURL(kSecureSite);

  network::mojom::URLLoaderFactoryParams params;
  params.unsafe_non_webby_initiator = true;

  cors::OriginAccessList origin_access_list;  // empty in this test

  SetFetchMetadataHeaders(current_url_request,
                          network::mojom::RequestMode::kCors, false,
                          network::mojom::RequestDestination::kIframe, &url,
                          params, origin_access_list);

  EXPECT_THAT(current_url_request->extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchSiteHeader, "cross-site"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchModeHeader, "cors"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchDestHeader, "iframe"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchStorageAccessHeader, "none"},
              }));
}

// Validate Sec-Fetch-Site and Sec-Fetch-Mode are set correctly with privileged
// requests from chrome extension background page.
TEST_F(SecHeaderHelpersTest, PrivilegedRequestOnExtension) {
  net::URLRequest* current_url_request = url_request();
  current_url_request->set_storage_access_status(
      net::cookie_util::StorageAccessStatus::kNone);
  GURL url = GURL(kSecureSite);

  network::mojom::URLLoaderFactoryParams params;
  params.unsafe_non_webby_initiator = true;

  cors::OriginAccessList origin_access_list;
  origin_access_list.AddAllowListEntryForOrigin(
      url::Origin::Create(GURL(kPrivilegedInitiator)),  // source_origin
      url.scheme(),                                     // protocol
      url.host(),                                       // domain
      0,                                                // port
      mojom::CorsDomainMatchMode::kDisallowSubdomains,
      mojom::CorsPortMatchMode::kAllowAnyPort,
      mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  SetFetchMetadataHeaders(current_url_request,
                          network::mojom::RequestMode::kCors, true,
                          network::mojom::RequestDestination::kEmbed, &url,
                          params, origin_access_list);

  EXPECT_THAT(current_url_request->extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchSiteHeader, "none"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchModeHeader, "cors"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchUserHeader, "?1"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchDestHeader, "embed"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchStorageAccessHeader, "none"},
              }));
}

struct StorageAccessTestData {
  std::optional<net::cookie_util::StorageAccessStatus> status;
  std::optional<std::string> expected_value;
};

class StorageAccessSecHeaderHelpersTest
    : public SecHeaderHelpersTest,
      public testing::WithParamInterface<StorageAccessTestData> {};

TEST_P(StorageAccessSecHeaderHelpersTest, Serialization) {
  const StorageAccessTestData& test_data = GetParam();
  net::URLRequest* current_url_request = url_request();
  url_request()->set_storage_access_status(test_data.status);
  GURL url = GURL(kSecureSite);

  SetFetchMetadataHeaders(current_url_request,
                          network::mojom::RequestMode::kCors,
                          /*has_user_activation=*/false,
                          network::mojom::RequestDestination::kIframe, &url, {},
                          /*origin_access_list=*/{});

  EXPECT_EQ(current_url_request->extra_request_headers().GetHeader(
                kKnownSecFetchStorageAccessHeader),
            test_data.expected_value);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    StorageAccessSecHeaderHelpersTest,
    testing::Values(
        StorageAccessTestData{std::nullopt, std::nullopt},
        StorageAccessTestData{net::cookie_util::StorageAccessStatus::kNone,
                              "none"},
        StorageAccessTestData{net::cookie_util::StorageAccessStatus::kInactive,
                              "inactive"},
        StorageAccessTestData{net::cookie_util::StorageAccessStatus::kActive,
                              "active"}));

}  // namespace network
