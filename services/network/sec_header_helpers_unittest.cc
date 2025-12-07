// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sec_header_helpers.h"

#include <optional>
#include <string_view>

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/storage_access_status_cache.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kSecureSite[] = "https://site.tld";
constexpr char kInsecureSite[] = "http://othersite.tld";
constexpr char kPrivilegedInitiator[] = "https://chrome-extension.example.com";
constexpr char kSecureSameSite[] = "https://same.site.tld";
constexpr char kSecureCrossSite[] = "https://cross-site.tld";
constexpr char kFile[] = "file://test";
constexpr char kOtherFile[] = "file://other";

constexpr char kKnownSecChHeader[] = "Sec-CH-UA";
constexpr char kKnownSecFetchSiteHeader[] = "Sec-Fetch-Site";
constexpr char kKnownSecFetchModeHeader[] = "Sec-Fetch-Mode";
constexpr char kKnownSecFetchUserHeader[] = "Sec-Fetch-User";
constexpr char kKnownSecFetchDestHeader[] = "Sec-Fetch-Dest";
constexpr char kKnownSecFetchStorageAccessHeader[] = "Sec-Fetch-Storage-Access";
constexpr char kKnownSecFetchFrameTopHeader[] = "Sec-Fetch-Frame-Top";
constexpr char kKnownSecFetchFrameAncestorsHeader[] =
    "Sec-Fetch-Frame-Ancestors";
constexpr char kOtherSecHeader[] = "sec-other-info-header";
constexpr char kOtherHeader[] = "Other-Header";

constexpr char kHeaderValue[] = "testdata";

struct FrameAncestorTestData {
  GURL url_request_dest;
  url::Origin frame_origin;
  std::optional<std::string> expected_value;
};

url::Origin secure_site_origin() {
  return url::Origin::Create(GURL(kSecureSite));
}

}  // namespace

namespace network {

using testing::UnorderedElementsAreArray;

class SecHeaderHelpersTestBase : public PlatformTest {
 public:
  SecHeaderHelpersTestBase()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(GURL(kSecureSite),
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {
    url_request_->set_initiator(
        url::Origin::Create(GURL(kPrivilegedInitiator)));

    scoped_feature_list_.InitWithFeatures(
        {features::kFrameTopHeader, features::kFrameAncestorsHeader}, {});
  }

  net::URLRequest& url_request() const { return *url_request_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

class SecHeaderHelpersTest : public SecHeaderHelpersTestBase {
 public:
  SecHeaderHelpersTest() = default;

  void SetUp() override {
    SecHeaderHelpersTestBase::SetUp();
    url_request_->set_storage_access_status(net::StorageAccessStatusCache(
        net::cookie_util::StorageAccessStatus::kNone));
  }
};

// Validate that Sec- prefixed headers are all removed when a request is
// downgraded from trustworthy to not such as when an https => http redirect
// occurs. We should only remove sec-ch- and sec-fetch- prefixed headers. Others
// should remain as they may be valid in an insecure context.
TEST_F(SecHeaderHelpersTest, SecHeadersRemovedOnDowngrade) {
  net::URLRequest& current_url_request = url_request();

  current_url_request.SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                  kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                  kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kOtherSecHeader, kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                  /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request.extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));

  EXPECT_THAT(current_url_request.extra_request_headers().GetHeaderVector(),
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
  net::URLRequest& current_url_request = url_request();

  current_url_request.SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                  kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                  kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kOtherSecHeader, kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                  /*overwrite=*/false);
  ASSERT_EQ(4, static_cast<int>(current_url_request.extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kSecureSite));

  EXPECT_THAT(current_url_request.extra_request_headers().GetHeaderVector(),
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
  net::URLRequest& current_url_request = url_request();

  current_url_request.SetExtraRequestHeaderByName(kKnownSecFetchSiteHeader,
                                                  kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kOtherHeader, kHeaderValue,
                                                  /*overwrite=*/false);
  current_url_request.SetExtraRequestHeaderByName(kKnownSecChHeader,
                                                  kHeaderValue,
                                                  /*overwrite=*/false);
  ASSERT_EQ(3, static_cast<int>(current_url_request.extra_request_headers()
                                    .GetHeaderVector()
                                    .size()));

  MaybeRemoveSecHeaders(current_url_request, GURL(kInsecureSite));

  EXPECT_THAT(current_url_request.extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{kOtherHeader,
                                                              kHeaderValue},
              }));
}

// Validate Sec-Fetch-Site and Sec-Fetch-Mode are set correctly with
// unprivileged requests from chrome extension background page.
TEST_F(SecHeaderHelpersTest, UnprivilegedRequestOnExtension) {
  net::URLRequest& current_url_request = url_request();
  GURL url = GURL(kSecureSite);

  // Set the request's net::IsolationInfo for Sec-Fetch-Frame-Top.
  current_url_request.set_isolation_info(net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, url::Origin::Create(url),
      url::Origin::Create(url), net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      /*network_isolation_partition=*/net::NetworkIsolationPartition::kGeneral,
      /*frame_ancestor_relation=*/
      net::IsolationInfo::FrameAncestorRelation::kSameOrigin));

  network::mojom::URLLoaderFactoryParams params;
  params.unsafe_non_webby_initiator = true;

  cors::OriginAccessList origin_access_list;  // empty in this test

  SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors, false,
      network::mojom::RequestDestination::kIframe, url, params,
      origin_access_list, mojom::CredentialsMode::kInclude);

  EXPECT_THAT(current_url_request.extra_request_headers().GetHeaderVector(),
              UnorderedElementsAreArray({
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchSiteHeader, "cross-site"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchModeHeader, "cors"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchDestHeader, "iframe"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchStorageAccessHeader, "none"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchFrameTopHeader, "same-origin"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchFrameAncestorsHeader, "same-origin"},
              }));
}

// Validate Sec-Fetch-Site and Sec-Fetch-Mode are set correctly with privileged
// requests from chrome extension background page.
TEST_F(SecHeaderHelpersTest, PrivilegedRequestOnExtension) {
  net::URLRequest& current_url_request = url_request();
  GURL url = GURL(kSecureSite);

  // Set the request's net::IsolationInfo for Sec-Fetch-Frame-Top.
  current_url_request.set_isolation_info(net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, url::Origin::Create(url),
      url::Origin::Create(url), net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      /*network_isolation_partition=*/net::NetworkIsolationPartition::kGeneral,
      /*frame_ancestor_relation=*/
      net::IsolationInfo::FrameAncestorRelation::kSameOrigin));

  network::mojom::URLLoaderFactoryParams params;
  params.unsafe_non_webby_initiator = true;

  cors::OriginAccessList origin_access_list;
  origin_access_list.AddAllowListEntryForOrigin(
      url::Origin::Create(GURL(kPrivilegedInitiator)),  // source_origin
      url.GetScheme(),                                  // protocol
      url.GetHost(),                                    // domain
      0,                                                // port
      mojom::CorsDomainMatchMode::kDisallowSubdomains,
      mojom::CorsPortMatchMode::kAllowAnyPort,
      mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors, true,
      network::mojom::RequestDestination::kEmbed, url, params,
      origin_access_list, mojom::CredentialsMode::kInclude);

  EXPECT_THAT(current_url_request.extra_request_headers().GetHeaderVector(),
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
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchFrameTopHeader, "same-origin"},
                  net::HttpRequestHeaders::HeaderKeyValuePair{
                      kKnownSecFetchFrameAncestorsHeader, "same-origin"},
              }));
}

struct FileSchemeTestData {
  const url::Origin test_origin;
  const std::string_view expected_header_value;
};

// Parameterized test suite checking that headers are set correctly on requests
// with the `file` scheme.
class SecHeaderHelpersFileSchemeTest
    : public PlatformTest,
      public testing::WithParamInterface<FileSchemeTestData> {
 public:
  SecHeaderHelpersFileSchemeTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(GURL(kFile),
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  net::URLRequest* url_request() const { return url_request_.get(); }

  const url::Origin test_origin() { return GetParam().test_origin; }

  const std::string_view expected_header_value() {
    return GetParam().expected_header_value;
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kFrameTopHeader);
    url_request_->set_storage_access_status(net::StorageAccessStatusCache(
        net::cookie_util::StorageAccessStatus::kNone));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

// Validate that the Sec-Fetch-Frame-Top header is set correctly,
// for a request whose top frame's origin is kSecureSite.
TEST_P(SecHeaderHelpersFileSchemeTest, SecFetchFrameTop) {
  net::URLRequest* current_url_request = url_request();
  current_url_request->set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/test_origin(),
      /*frame_origin=*/url::Origin::Create(GURL(kFile)),
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt));

  current_url_request->set_initiator(url::Origin::Create(GURL(kFile)));

  SetFetchMetadataHeaders(
      *current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(current_url_request->extra_request_headers().GetHeader(
                kKnownSecFetchFrameTopHeader),
            expected_header_value());
}

TEST_P(SecHeaderHelpersFileSchemeTest, SecFetchSite) {
  net::URLRequest* current_url_request = url_request();
  current_url_request->set_initiator(test_origin());

  SetFetchMetadataHeaders(
      *current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(current_url_request->extra_request_headers().GetHeader(
                kKnownSecFetchSiteHeader),
            expected_header_value());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SecHeaderHelpersFileSchemeTest,
    testing::Values(FileSchemeTestData{url::Origin::Create(GURL(kFile)),
                                       std::string_view("same-origin")},
                    FileSchemeTestData{url::Origin::Create(GURL(kOtherFile)),
                                       std::string_view("cross-site")}));

struct StorageAccessTestData {
  net::StorageAccessStatusCache status;
  mojom::CredentialsMode credentials_mode;
  std::optional<std::string> expected_value;
  net::cookie_util::SecFetchStorageAccessOutcome expected_sample;
};

class StorageAccessSecHeaderHelpersTest
    : public SecHeaderHelpersTestBase,
      public testing::WithParamInterface<StorageAccessTestData> {};

TEST_P(StorageAccessSecHeaderHelpersTest, Serialization) {
  const StorageAccessTestData& test_data = GetParam();
  net::URLRequest& current_url_request = url_request();
  current_url_request.set_storage_access_status(test_data.status);
  GURL url = GURL(kSecureSite);

  base::HistogramTester histogram_tester;
  SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe, url, {},
      /*origin_access_list=*/{}, test_data.credentials_mode);

  EXPECT_EQ(current_url_request.extra_request_headers().GetHeader(
                kKnownSecFetchStorageAccessHeader),
            test_data.expected_value);
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccessHeader.SecFetchStorageAccessOutcome",
      /*sample=*/test_data.expected_sample,
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    StorageAccessSecHeaderHelpersTest,
    testing::Values(
        StorageAccessTestData{
            net::StorageAccessStatusCache(),
            mojom::CredentialsMode::kOmit,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(),
            mojom::CredentialsMode::kSameOrigin,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(std::nullopt),
            mojom::CredentialsMode::kOmit,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedStatusMissing,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(std::nullopt),
            mojom::CredentialsMode::kSameOrigin,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedStatusMissing,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(std::nullopt),
            mojom::CredentialsMode::kInclude,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedStatusMissing,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kNone),
            mojom::CredentialsMode::kOmit,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kNone),
            mojom::CredentialsMode::kSameOrigin,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kNone),
            mojom::CredentialsMode::kInclude,
            "none",
            net::cookie_util::SecFetchStorageAccessOutcome::kValueNone,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kInactive),
            mojom::CredentialsMode::kOmit,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kInactive),
            mojom::CredentialsMode::kSameOrigin,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kInactive),
            mojom::CredentialsMode::kInclude,
            "inactive",
            net::cookie_util::SecFetchStorageAccessOutcome::kValueInactive,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kActive),
            mojom::CredentialsMode::kOmit,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kActive),
            mojom::CredentialsMode::kSameOrigin,
            std::nullopt,
            net::cookie_util::SecFetchStorageAccessOutcome::
                kOmittedRequestOmitsCredentials,
        },
        StorageAccessTestData{
            net::StorageAccessStatusCache(
                net::cookie_util::StorageAccessStatus::kActive),
            mojom::CredentialsMode::kInclude,
            "active",
            net::cookie_util::SecFetchStorageAccessOutcome::kValueActive,
        }));

#ifdef GTEST_HAS_DEATH_TEST
TEST_F(
    SecHeaderHelpersTest,
    StorageAccessSecHeaderHelpersCrashWithCredentialsModeIncludeWithoutStorageAccessStatus) {
  net::URLRequest& current_url_request = url_request();
  current_url_request.set_storage_access_status(
      net::StorageAccessStatusCache());
  GURL url = GURL(kSecureSite);

  EXPECT_CHECK_DEATH(SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe, url, {},
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude));
}
#endif  // GTEST_HAS_DEATH_TEST

// Parameterized test Suite for the Sec-Fetch-Frame-Top header. The
// params of this test are GURLs, which are used to set the destination of
// the test's url_request_ when it is constructed.
class FrameTopSecHeaderHelpersTest : public PlatformTest,
                                     public testing::WithParamInterface<GURL> {
 public:
  FrameTopSecHeaderHelpersTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(GetParam(),
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {
    url_request_->set_initiator(
        url::Origin::Create(GURL(kPrivilegedInitiator)));
    url_request_->set_storage_access_status(net::StorageAccessStatusCache(
        net::cookie_util::StorageAccessStatus::kNone));
  }

  net::URLRequest* url_request() const { return url_request_.get(); }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kFrameTopHeader);
  }

  // Describes a site's relationship to kSecureSite. kNone represents an
  // inapplicable case.
  enum class SecureSiteRelation { kSameOrigin, kSameSite, kCrossSite, kNone };

  SecureSiteRelation GetSecureSiteRelationForURL(GURL url) {
    if (url == GURL(kSecureSite)) {
      return SecureSiteRelation::kSameOrigin;
    }
    if (url == GURL(kSecureSameSite)) {
      return SecureSiteRelation::kSameSite;
    }
    if (url == GURL(kSecureCrossSite)) {
      return SecureSiteRelation::kCrossSite;
    }
    return SecureSiteRelation::kNone;
  }

  std::optional<std::string_view> SecureSiteRelationToString(
      SecureSiteRelation relation) {
    switch (relation) {
      case SecureSiteRelation::kSameOrigin:
        return "same-origin";
      case SecureSiteRelation::kSameSite:
        return "same-site";
      case SecureSiteRelation::kCrossSite:
        return "cross-site";
      case SecureSiteRelation::kNone:
        return std::nullopt;
    }
  }

  const url::Origin secure_site_origin() {
    return url::Origin::Create(GURL(kSecureSite));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

// Validate that the Sec-Fetch-Frame-Top header is set correctly,
// for a request whose top frame's origin is kSecureSite.
TEST_P(FrameTopSecHeaderHelpersTest, HeaderValuesMatchRelation) {
  net::URLRequest* current_url_request = url_request();
  current_url_request->set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/secure_site_origin(),
      /*frame_origin=*/url::Origin::Create(GetParam()),
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt));

  SetFetchMetadataHeaders(
      *current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(
      current_url_request->extra_request_headers().GetHeader(
          kKnownSecFetchFrameTopHeader),
      SecureSiteRelationToString(GetSecureSiteRelationForURL(GetParam())));
}

// Validate that the Sec-Fetch-Frame-Top header is not attached to
// main frame requests.
TEST_P(FrameTopSecHeaderHelpersTest, NoHeaderOnMainFrameRequests) {
  net::URLRequest* current_url_request = url_request();
  current_url_request->set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kMainFrame,
      /*top_frame_origin=*/secure_site_origin(),
      /*frame_origin=*/url::Origin::Create(GetParam()),
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt));

  SetFetchMetadataHeaders(
      *current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kDocument,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(current_url_request->extra_request_headers().GetHeader(
                kKnownSecFetchFrameTopHeader),
            std::nullopt);
}

// Validate that the calculation of the Sec-Fetch-Frame-Top header
// incorporates any pending redirect urls.
TEST_P(FrameTopSecHeaderHelpersTest, UpdatesOnRedirects) {
  net::URLRequest* current_url_request = url_request();
  current_url_request->set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/secure_site_origin(),
      /*frame_origin=*/url::Origin::Create(GetParam()),
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt));

  for (GURL redirect_url :
       {GURL(kSecureSite), GURL(kSecureSameSite), GURL(kSecureCrossSite)}) {
    SetFetchMetadataHeaders(
        *current_url_request, network::mojom::RequestMode::kCors,
        /*has_user_activation=*/false,
        network::mojom::RequestDestination::kIframe, redirect_url,
        network::mojom::URLLoaderFactoryParams(),
        /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

    std::optional<std::string_view> expected_header =
        SecureSiteRelationToString(GetSecureSiteRelationForURL(redirect_url));
    EXPECT_EQ(current_url_request->extra_request_headers().GetHeader(
                  kKnownSecFetchFrameTopHeader),
              expected_header);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         FrameTopSecHeaderHelpersTest,
                         testing::Values(GURL(kSecureSite),
                                         GURL(kSecureSameSite),
                                         GURL(kSecureCrossSite)));

// Parameterized test Suite for the Sec-Fetch-Frame-Ancestors header.
class FrameAncestorsSecHeaderHelpersTest
    : public PlatformTest,
      public testing::WithParamInterface<FrameAncestorTestData> {
 public:
  FrameAncestorsSecHeaderHelpersTest()
      : context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(GetParam().url_request_dest,
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {
    url_request_->set_initiator(
        url::Origin::Create(GURL(kPrivilegedInitiator)));
    url_request_->set_storage_access_status(net::StorageAccessStatusCache(
        net::cookie_util::StorageAccessStatus::kNone));
    scoped_feature_list_.InitAndEnableFeature(features::kFrameAncestorsHeader);
  }

  net::URLRequest& url_request() const { return *url_request_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

TEST_P(FrameAncestorsSecHeaderHelpersTest, HeaderValuesMatchRelation) {
  net::URLRequest& current_url_request = url_request();
  current_url_request.set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/secure_site_origin(),
      /*frame_origin=*/GetParam().frame_origin,
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      /*network_isolation_partition=*/net::NetworkIsolationPartition::kGeneral,
      /*frame_ancestor_relation=*/
      net::IsolationInfo::OriginRelationToFrameAncestorRelation(
          net::GetOriginRelation(GetParam().frame_origin,
                                 secure_site_origin()))));

  SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(current_url_request.extra_request_headers().GetHeader(
                kKnownSecFetchFrameAncestorsHeader),
            GetParam().expected_value);
}

// Validate that the Sec-Fetch-Frame-Ancestors header is always same-origin for
// main frame requests.
TEST_P(FrameAncestorsSecHeaderHelpersTest, SameOriginOnMainFrameRequests) {
  net::URLRequest& current_url_request = url_request();
  current_url_request.set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kMainFrame,
      /*top_frame_origin=*/secure_site_origin(),
      /*frame_origin=*/GetParam().frame_origin,
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      /*network_isolation_partition=*/net::NetworkIsolationPartition::kGeneral,
      /*frame_ancestor_relation=*/
      net::IsolationInfo::FrameAncestorRelation::kSameOrigin));

  SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kDocument,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(current_url_request.extra_request_headers()
                .GetHeader(kKnownSecFetchFrameAncestorsHeader)
                .value(),
            net::IsolationInfo::FrameAncestorRelationString(
                net::IsolationInfo::FrameAncestorRelation::kSameOrigin));
}

TEST_P(FrameAncestorsSecHeaderHelpersTest, NoHeaderWhenFeatureDisabled) {
  base::test::ScopedFeatureList local_scoped_feature_list;
  local_scoped_feature_list.InitAndDisableFeature(
      features::kFrameAncestorsHeader);

  net::URLRequest& current_url_request = url_request();
  current_url_request.set_isolation_info(net::IsolationInfo::Create(
      /*request_type=*/net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/secure_site_origin(),
      /*frame_origin=*/GetParam().frame_origin,
      /*site_for_cookies=*/net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      /*network_isolation_partition=*/net::NetworkIsolationPartition::kGeneral,
      /*frame_ancestor_relation=*/
      net::IsolationInfo::OriginRelationToFrameAncestorRelation(
          net::GetOriginRelation(GetParam().frame_origin,
                                 secure_site_origin()))));

  SetFetchMetadataHeaders(
      current_url_request, network::mojom::RequestMode::kCors,
      /*has_user_activation=*/false,
      network::mojom::RequestDestination::kIframe,
      /*pending_redirect_url=*/std::nullopt,
      network::mojom::URLLoaderFactoryParams(),
      /*origin_access_list=*/{}, mojom::CredentialsMode::kInclude);

  EXPECT_EQ(current_url_request.extra_request_headers().GetHeader(
                kKnownSecFetchFrameAncestorsHeader),
            std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FrameAncestorsSecHeaderHelpersTest,
    testing::Values(
        FrameAncestorTestData{GURL(kSecureSite),
                              url::Origin::Create(GURL(kSecureSite)),
                              "same-origin"},
        FrameAncestorTestData{GURL(kSecureSite),
                              url::Origin::Create(GURL(kSecureSameSite)),
                              "same-site"},
        FrameAncestorTestData{GURL(kSecureSite),
                              url::Origin::Create(GURL(kSecureCrossSite)),
                              "cross-site"},
        FrameAncestorTestData{GURL(kSecureSameSite),
                              url::Origin::Create(GURL(kSecureSite)),
                              "same-site"},
        FrameAncestorTestData{GURL(kSecureSameSite),
                              url::Origin::Create(GURL(kSecureSameSite)),
                              "same-site"},
        FrameAncestorTestData{GURL(kSecureSameSite),
                              url::Origin::Create(GURL(kSecureCrossSite)),
                              "cross-site"},
        FrameAncestorTestData{GURL(kSecureCrossSite),
                              url::Origin::Create(GURL(kSecureSite)),
                              "cross-site"},
        FrameAncestorTestData{GURL(kSecureCrossSite),
                              url::Origin::Create(GURL(kSecureSameSite)),
                              "cross-site"},
        FrameAncestorTestData{GURL(kSecureCrossSite),
                              url::Origin::Create(GURL(kSecureCrossSite)),
                              "cross-site"}));
}  // namespace network
