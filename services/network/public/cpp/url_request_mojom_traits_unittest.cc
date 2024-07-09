// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_request_mojom_traits.h"

#include <optional>

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/filter/source_stream.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/http_request_headers_mojom_traits.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_request.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace network {
namespace {

TEST(URLRequestMojomTraitsTest, Roundtrips_URLRequestReferrerPolicy) {
  for (auto referrer_policy :
       {net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
        net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
        net::ReferrerPolicy::NEVER_CLEAR, net::ReferrerPolicy::ORIGIN,
        net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN,
        net::ReferrerPolicy::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::ReferrerPolicy::NO_REFERRER}) {
    int32_t serialized = -1;
    using URLRequestReferrerPolicySerializer =
        mojo::internal::Serializer<mojom::URLRequestReferrerPolicy,
                                   net::ReferrerPolicy>;
    URLRequestReferrerPolicySerializer::Serialize(referrer_policy, &serialized);
    EXPECT_EQ(static_cast<int32_t>(referrer_policy), serialized);
    net::ReferrerPolicy deserialized;
    URLRequestReferrerPolicySerializer::Deserialize(serialized, &deserialized);
    EXPECT_EQ(referrer_policy, deserialized);
  }
}

TEST(URLRequestMojomTraitsTest, Roundtrips_ResourceRequest) {
  network::ResourceRequest original;
  original.method = "POST";
  original.url = GURL("https://example.com/resources/dummy.xml");
  original.site_for_cookies =
      net::SiteForCookies::FromUrl(GURL("https://example.com/index.html"));
  original.update_first_party_url_on_redirect = false;
  original.request_initiator = url::Origin::Create(original.url);
  original.isolated_world_origin =
      url::Origin::Create(GURL("chrome-extension://blah"));
  original.referrer = GURL("https://referrer.com/");
  original.referrer_policy =
      net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  original.headers.SetHeader("Accept", "text/xml");
  original.cors_exempt_headers.SetHeader("X-Requested-With", "ForTesting");
  original.load_flags = net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                        net::LOAD_SHOULD_BYPASS_HSTS;
  original.resource_type = 2;
  original.priority = net::IDLE;
  original.priority_incremental = net::kDefaultPriorityIncremental;
  original.cors_preflight_policy =
      mojom::CorsPreflightPolicy::kConsiderPreflight;
  original.originated_from_service_worker = false;
  original.skip_service_worker = false;
  original.mode = mojom::RequestMode::kNoCors;
  original.credentials_mode = mojom::CredentialsMode::kInclude;
  original.redirect_mode = mojom::RedirectMode::kFollow;
  original.fetch_integrity = "dummy_fetch_integrity";
  original.keepalive = true;
  original.browsing_topics = true;
  original.ad_auction_headers = true;
  original.shared_storage_writable_eligible = true;
  original.has_user_gesture = false;
  original.enable_load_timing = true;
  original.enable_upload_progress = false;
  original.do_not_prompt_for_login = true;
  original.is_outermost_main_frame = true;
  original.transition_type = 0;
  original.previews_state = 0;
  original.upgrade_if_insecure = true;
  original.is_revalidating = false;
  original.throttling_profile_id = base::UnguessableToken::Create();
  original.fetch_window_id = base::UnguessableToken::Create();
  original.web_bundle_token_params =
      std::make_optional(ResourceRequest::WebBundleTokenParams(
          GURL("https://bundle.test/"), base::UnguessableToken::Create(),
          mojo::PendingRemote<network::mojom::WebBundleHandle>()));
  original.net_log_create_info = std::make_optional(net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID()));
  original.net_log_reference_info = std::make_optional(net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID()));
  original.devtools_accepted_stream_types =
      std::vector<net::SourceStream::SourceType>(
          {net::SourceStream::SourceType::TYPE_BROTLI,
           net::SourceStream::SourceType::TYPE_GZIP,
           net::SourceStream::SourceType::TYPE_DEFLATE});
  original.target_ip_address_space = mojom::IPAddressSpace::kPrivate;
  original.storage_access_api_status =
      net::StorageAccessApiStatus::kAccessViaAPI;

  original.trusted_params = ResourceRequest::TrustedParams();
  original.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(original.url), url::Origin::Create(original.url),
      original.site_for_cookies);
  original.trusted_params->disable_secure_dns = true;
  original.trusted_params->allow_cookies_from_browser = true;
  original.trusted_params->include_request_cookies_with_response = true;

  original.trust_token_params = network::mojom::TrustTokenParams();
  original.trust_token_params->issuers.push_back(
      url::Origin::Create(GURL("https://issuer.com")));
  original.trust_token_params->operation =
      mojom::TrustTokenOperationType::kRedemption;
  original.trust_token_params->include_timestamp_header = true;
  original.trust_token_params->sign_request_data =
      mojom::TrustTokenSignRequestData::kInclude;
  original.trust_token_params->additional_signed_headers.push_back(
      "some_header");

  network::ResourceRequest copied;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::URLRequest>(original, copied));
  EXPECT_TRUE(original.EqualsForTesting(copied));
}

class DataElementDeserializationTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DataElementDeserializationTest, DataPipe) {
  mojo::PendingRemote<mojom::DataPipeGetter> pending_remote;
  auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
  DataElement src{DataElementDataPipe(std::move(pending_remote))};

  DataElement dest;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DataElement>(src, dest));
  ASSERT_EQ(dest.type(), network::DataElement::Tag::kDataPipe);

  mojo::Remote<mojom::DataPipeGetter> remote(
      dest.As<DataElementDataPipe>().ReleaseDataPipeGetter());

  // Make sure that `remote` and `pending_receiver` is connected to each other.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(remote.is_bound());
  EXPECT_TRUE(remote.is_connected());

  pending_receiver.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(remote.is_bound());
  EXPECT_FALSE(remote.is_connected());
}

TEST_F(DataElementDeserializationTest, ChunkedDataPipe) {
  mojo::PendingRemote<mojom::ChunkedDataPipeGetter> pending_remote;
  auto pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
  DataElement src(DataElementChunkedDataPipe(
      std::move(pending_remote),
      DataElementChunkedDataPipe::ReadOnlyOnce(true)));
  DataElement dest;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DataElement>(src, dest));
  ASSERT_EQ(dest.type(), network::DataElement::Tag::kChunkedDataPipe);
  EXPECT_TRUE(dest.As<DataElementChunkedDataPipe>().read_only_once());
  mojo::Remote<mojom::ChunkedDataPipeGetter> remote(
      dest.As<DataElementChunkedDataPipe>().ReleaseChunkedDataPipeGetter());

  // Make sure that `remote` and `pending_receiver` is connected to each other.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(remote.is_bound());
  EXPECT_TRUE(remote.is_connected());

  pending_receiver.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(remote.is_bound());
  EXPECT_FALSE(remote.is_connected());
}

TEST_F(DataElementDeserializationTest, Bytes) {
  const std::vector<uint8_t> kData = {8, 1, 9};
  DataElement src{DataElementBytes(kData)};
  DataElement dest;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DataElement>(src, dest));
  ASSERT_EQ(mojom::DataElementDataView::Tag::kBytes, dest.type());
  EXPECT_EQ(kData, dest.As<DataElementBytes>().bytes());
}

TEST_F(DataElementDeserializationTest, File) {
  const base::FilePath kPath = base::FilePath::FromUTF8Unsafe("foobar");
  DataElement src(DataElementFile(kPath, /*offset=*/3, /*length=*/8,
                                  base::Time::UnixEpoch() + base::Minutes(2)));
  DataElement dest;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DataElement>(src, dest));
  ASSERT_EQ(mojom::DataElementDataView::Tag::kFile, dest.type());

  const auto& src_file = src.As<DataElementFile>();
  const auto& dest_file = dest.As<DataElementFile>();

  EXPECT_EQ(src_file.path(), dest_file.path());
  EXPECT_EQ(src_file.offset(), dest_file.offset());
  EXPECT_EQ(src_file.length(), dest_file.length());
  EXPECT_EQ(src_file.expected_modification_time(),
            dest_file.expected_modification_time());
}

}  // namespace
}  // namespace network
