// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_server_properties.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/privacy_mode.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_network_session.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

const base::TimeDelta BROKEN_ALT_SVC_EXPIRE_DELAYS[10] = {
    base::Seconds(300),    base::Seconds(600),   base::Seconds(1200),
    base::Seconds(2400),   base::Seconds(4800),  base::Seconds(9600),
    base::Seconds(19200),  base::Seconds(38400), base::Seconds(76800),
    base::Seconds(153600),
};

class HttpServerPropertiesPeer {
 public:
  static void AddBrokenAlternativeServiceWithExpirationTime(
      HttpServerProperties* impl,
      const AlternativeService& alternative_service,
      base::TimeTicks when,
      const NetworkAnonymizationKey network_anonymization_key =
          NetworkAnonymizationKey()) {
    BrokenAlternativeService broken_alternative_service(
        alternative_service, network_anonymization_key,
        true /* use_network_anonymization_key */);
    BrokenAlternativeServiceList::iterator unused_it;
    impl->broken_alternative_services_.AddToBrokenListAndMap(
        broken_alternative_service, when, &unused_it);
    auto it =
        impl->broken_alternative_services_.recently_broken_alternative_services_
            .Get(broken_alternative_service);
    if (it == impl->broken_alternative_services_
                  .recently_broken_alternative_services_.end()) {
      impl->broken_alternative_services_.recently_broken_alternative_services_
          .Put(broken_alternative_service, 1);
    } else {
      it->second++;
    }
  }

  static void ExpireBrokenAlternateProtocolMappings(
      HttpServerProperties* impl) {
    impl->broken_alternative_services_.ExpireBrokenAlternateProtocolMappings();
  }
};

namespace {

// Creates a ServerInfoMapKey without a NetworkAnonymizationKey.
HttpServerProperties::ServerInfoMapKey CreateSimpleKey(
    const url::SchemeHostPort& server) {
  return HttpServerProperties::ServerInfoMapKey(
      server, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
}

class HttpServerPropertiesTest : public TestWithTaskEnvironment {
 protected:
  HttpServerPropertiesTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        // Many tests assume partitioning is disabled by default.
        feature_list_(CreateFeatureListWithPartitioningDisabled()),
        test_tick_clock_(GetMockTickClock()),
        impl_(nullptr /* pref_delegate */,
              nullptr /* net_log */,
              test_tick_clock_,
              &test_clock_) {
    // Set |test_clock_| to some random time.
    test_clock_.Advance(base::Seconds(12345));

    SchemefulSite site1(GURL("https://foo.test/"));
    network_anonymization_key1_ =
        NetworkAnonymizationKey::CreateSameSite(site1);
    SchemefulSite site2(GURL("https://bar.test/"));
    network_anonymization_key2_ =
        NetworkAnonymizationKey::CreateSameSite(site2);
  }

  // This is a little awkward, but need to create and configure the
  // ScopedFeatureList before creating the HttpServerProperties.
  static std::unique_ptr<base::test::ScopedFeatureList>
  CreateFeatureListWithPartitioningDisabled() {
    std::unique_ptr<base::test::ScopedFeatureList> feature_list =
        std::make_unique<base::test::ScopedFeatureList>();
    feature_list->InitAndDisableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    return feature_list;
  }

  bool HasAlternativeService(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key) {
    const AlternativeServiceInfoVector alternative_service_info_vector =
        impl_.GetAlternativeServiceInfos(origin, network_anonymization_key);
    return !alternative_service_info_vector.empty();
  }

  void SetAlternativeService(const url::SchemeHostPort& origin,
                             const AlternativeService& alternative_service) {
    const base::Time expiration = test_clock_.Now() + base::Days(1);
    if (alternative_service.protocol == kProtoQUIC) {
      impl_.SetQuicAlternativeService(origin, NetworkAnonymizationKey(),
                                      alternative_service, expiration,
                                      DefaultSupportedQuicVersions());
    } else {
      impl_.SetHttp2AlternativeService(origin, NetworkAnonymizationKey(),
                                       alternative_service, expiration);
    }
  }

  void MarkBrokenAndLetExpireAlternativeServiceNTimes(
      const AlternativeService& alternative_service,
      int num_times) {}

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;

  raw_ptr<const base::TickClock> test_tick_clock_;
  base::SimpleTestClock test_clock_;

  // Two different non-empty network isolation keys for use in tests that need
  // them.
  NetworkAnonymizationKey network_anonymization_key1_;
  NetworkAnonymizationKey network_anonymization_key2_;

  HttpServerProperties impl_;
};

TEST_F(HttpServerPropertiesTest, SetSupportsSpdy) {
  // Check spdy servers are correctly set with SchemeHostPort key.
  url::SchemeHostPort https_www_server("https", "www.google.com", 443);
  url::SchemeHostPort http_photo_server("http", "photos.google.com", 80);
  url::SchemeHostPort https_mail_server("https", "mail.google.com", 443);
  // Servers with port equal to default port in scheme will drop port components
  // when calling Serialize().

  url::SchemeHostPort http_google_server("http", "www.google.com", 443);
  url::SchemeHostPort https_photos_server("https", "photos.google.com", 443);
  url::SchemeHostPort valid_google_server((GURL("https://www.google.com")));

  impl_.SetSupportsSpdy(https_www_server, NetworkAnonymizationKey(), true);
  impl_.SetSupportsSpdy(http_photo_server, NetworkAnonymizationKey(), true);
  impl_.SetSupportsSpdy(https_mail_server, NetworkAnonymizationKey(), false);
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(https_www_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.SupportsRequestPriority(https_www_server,
                                            NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(http_photo_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.SupportsRequestPriority(http_photo_server,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(https_mail_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.SupportsRequestPriority(https_mail_server,
                                             NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(http_google_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.SupportsRequestPriority(http_google_server,
                                             NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(https_photos_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.SupportsRequestPriority(https_photos_server,
                                             NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(valid_google_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.SupportsRequestPriority(valid_google_server,
                                            NetworkAnonymizationKey()));

  // Flip values of two servers.
  impl_.SetSupportsSpdy(https_www_server, NetworkAnonymizationKey(), false);
  impl_.SetSupportsSpdy(https_mail_server, NetworkAnonymizationKey(), true);
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(https_www_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.SupportsRequestPriority(https_www_server,
                                             NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(https_mail_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.SupportsRequestPriority(https_mail_server,
                                            NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, SetSupportsSpdyWebSockets) {
  // The https and wss servers should be treated as the same server, as should
  // the http and ws servers.
  url::SchemeHostPort https_server("https", "www.test.com", 443);
  url::SchemeHostPort wss_server("wss", "www.test.com", 443);
  url::SchemeHostPort http_server("http", "www.test.com", 443);
  url::SchemeHostPort ws_server("ws", "www.test.com", 443);

  EXPECT_FALSE(impl_.GetSupportsSpdy(https_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(wss_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(http_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(ws_server, NetworkAnonymizationKey()));

  impl_.SetSupportsSpdy(wss_server, NetworkAnonymizationKey(), true);
  EXPECT_TRUE(impl_.GetSupportsSpdy(https_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.GetSupportsSpdy(wss_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(http_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(ws_server, NetworkAnonymizationKey()));

  impl_.SetSupportsSpdy(http_server, NetworkAnonymizationKey(), true);
  EXPECT_TRUE(impl_.GetSupportsSpdy(https_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.GetSupportsSpdy(wss_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.GetSupportsSpdy(http_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.GetSupportsSpdy(ws_server, NetworkAnonymizationKey()));

  impl_.SetSupportsSpdy(https_server, NetworkAnonymizationKey(), false);
  EXPECT_FALSE(impl_.GetSupportsSpdy(https_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(wss_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.GetSupportsSpdy(http_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.GetSupportsSpdy(ws_server, NetworkAnonymizationKey()));

  impl_.SetSupportsSpdy(ws_server, NetworkAnonymizationKey(), false);
  EXPECT_FALSE(impl_.GetSupportsSpdy(https_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(wss_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(http_server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetSupportsSpdy(ws_server, NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, SetSupportsSpdyWithNetworkIsolationKey) {
  const url::SchemeHostPort kServer("https", "foo.test", 443);

  EXPECT_FALSE(impl_.GetSupportsSpdy(kServer, network_anonymization_key1_));
  EXPECT_FALSE(
      impl_.SupportsRequestPriority(kServer, network_anonymization_key1_));
  EXPECT_FALSE(impl_.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));

  // Without network isolation keys enabled for HttpServerProperties, passing in
  // a NetworkAnonymizationKey should have no effect on behavior.
  for (const auto& network_anonymization_key_to_set :
       {NetworkAnonymizationKey(), network_anonymization_key1_}) {
    impl_.SetSupportsSpdy(kServer, network_anonymization_key_to_set, true);
    EXPECT_TRUE(impl_.GetSupportsSpdy(kServer, network_anonymization_key1_));
    EXPECT_TRUE(
        impl_.SupportsRequestPriority(kServer, network_anonymization_key1_));
    EXPECT_TRUE(impl_.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
    EXPECT_TRUE(
        impl_.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));

    impl_.SetSupportsSpdy(kServer, network_anonymization_key_to_set, false);
    EXPECT_FALSE(impl_.GetSupportsSpdy(kServer, network_anonymization_key1_));
    EXPECT_FALSE(
        impl_.SupportsRequestPriority(kServer, network_anonymization_key1_));
    EXPECT_FALSE(impl_.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
    EXPECT_FALSE(
        impl_.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));
  }

  // With network isolation keys enabled for HttpServerProperties, the
  // NetworkAnonymizationKey argument should be respected.

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  EXPECT_FALSE(
      properties.GetSupportsSpdy(kServer, network_anonymization_key1_));
  EXPECT_FALSE(
      properties.SupportsRequestPriority(kServer, network_anonymization_key1_));
  EXPECT_FALSE(properties.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      properties.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));

  properties.SetSupportsSpdy(kServer, network_anonymization_key1_, true);
  EXPECT_TRUE(properties.GetSupportsSpdy(kServer, network_anonymization_key1_));
  EXPECT_TRUE(
      properties.SupportsRequestPriority(kServer, network_anonymization_key1_));
  EXPECT_FALSE(properties.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      properties.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));

  properties.SetSupportsSpdy(kServer, NetworkAnonymizationKey(), true);
  EXPECT_TRUE(properties.GetSupportsSpdy(kServer, network_anonymization_key1_));
  EXPECT_TRUE(
      properties.SupportsRequestPriority(kServer, network_anonymization_key1_));
  EXPECT_TRUE(properties.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      properties.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));

  properties.SetSupportsSpdy(kServer, network_anonymization_key1_, false);
  EXPECT_FALSE(
      properties.GetSupportsSpdy(kServer, network_anonymization_key1_));
  EXPECT_FALSE(
      properties.SupportsRequestPriority(kServer, network_anonymization_key1_));
  EXPECT_TRUE(properties.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      properties.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));

  properties.SetSupportsSpdy(kServer, NetworkAnonymizationKey(), false);
  EXPECT_FALSE(
      properties.GetSupportsSpdy(kServer, network_anonymization_key1_));
  EXPECT_FALSE(
      properties.SupportsRequestPriority(kServer, network_anonymization_key1_));
  EXPECT_FALSE(properties.GetSupportsSpdy(kServer, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      properties.SupportsRequestPriority(kServer, NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, LoadSupportsSpdy) {
  HttpServerProperties::ServerInfo supports_spdy;
  supports_spdy.supports_spdy = true;
  HttpServerProperties::ServerInfo no_spdy;
  no_spdy.supports_spdy = false;

  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  url::SchemeHostPort spdy_server_photos("https", "photos.google.com", 443);
  url::SchemeHostPort spdy_server_docs("https", "docs.google.com", 443);
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);

  // Check by initializing empty spdy servers.
  std::unique_ptr<HttpServerProperties::ServerInfoMap> spdy_servers =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  impl_.OnServerInfoLoadedForTesting(std::move(spdy_servers));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));

  // Check by initializing www.google.com:443 and photos.google.com:443 as spdy
  // servers.
  std::unique_ptr<HttpServerProperties::ServerInfoMap> spdy_servers1 =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  spdy_servers1->Put(CreateSimpleKey(spdy_server_google), supports_spdy);
  spdy_servers1->Put(CreateSimpleKey(spdy_server_photos), no_spdy);
  impl_.OnServerInfoLoadedForTesting(std::move(spdy_servers1));
  // Note: these calls affect MRU order.
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(spdy_server_photos, NetworkAnonymizationKey()));

  // Verify google and photos are in the list in MRU order.
  ASSERT_EQ(2U, impl_.server_info_map_for_testing().size());
  auto it = impl_.server_info_map_for_testing().begin();
  EXPECT_EQ(spdy_server_photos, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_FALSE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_google, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);

  // Check by initializing mail.google.com:443 and docs.google.com:443.
  std::unique_ptr<HttpServerProperties::ServerInfoMap> spdy_servers2 =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  spdy_servers2->Put(CreateSimpleKey(spdy_server_mail), supports_spdy);
  spdy_servers2->Put(CreateSimpleKey(spdy_server_docs), supports_spdy);
  impl_.OnServerInfoLoadedForTesting(std::move(spdy_servers2));

  // Verify all the servers are in the list in MRU order. Note that
  // OnServerInfoLoadedForTesting will put existing spdy server entries in
  // front of newly added entries.
  ASSERT_EQ(4U, impl_.server_info_map_for_testing().size());
  it = impl_.server_info_map_for_testing().begin();
  EXPECT_EQ(spdy_server_photos, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_FALSE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_google, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_docs, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_mail, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);

  // Check these in reverse MRU order so that MRU order stays the same.
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_mail, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_docs, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(spdy_server_photos, NetworkAnonymizationKey()));

  // Verify that old values loaded from disk take precedence over newer learned
  // values and also verify the recency list order is unchanged.
  std::unique_ptr<HttpServerProperties::ServerInfoMap> spdy_servers3 =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  spdy_servers3->Put(CreateSimpleKey(spdy_server_mail), no_spdy);
  spdy_servers3->Put(CreateSimpleKey(spdy_server_photos), supports_spdy);
  impl_.OnServerInfoLoadedForTesting(std::move(spdy_servers3));

  // Verify the entries are in the same order.
  ASSERT_EQ(4U, impl_.server_info_map_for_testing().size());
  it = impl_.server_info_map_for_testing().begin();
  EXPECT_EQ(spdy_server_photos, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_google, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_docs, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_TRUE(*it->second.supports_spdy);
  ++it;
  EXPECT_EQ(spdy_server_mail, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.supports_spdy.has_value());
  EXPECT_FALSE(*it->second.supports_spdy);

  // Verify photos server doesn't support SPDY and other servers support SPDY.
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(spdy_server_mail, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_docs, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_photos, NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, SupportsRequestPriority) {
  url::SchemeHostPort spdy_server_empty("https", std::string(), 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_empty,
                                             NetworkAnonymizationKey()));

  // Add www.google.com:443 as supporting SPDY.
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey(), true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google,
                                            NetworkAnonymizationKey()));

  // Add mail.google.com:443 as not supporting SPDY.
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail,
                                             NetworkAnonymizationKey()));

  // Add docs.google.com:443 as supporting SPDY.
  url::SchemeHostPort spdy_server_docs("https", "docs.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_docs, NetworkAnonymizationKey(), true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs,
                                            NetworkAnonymizationKey()));

  // Add www.youtube.com:443 as supporting QUIC.
  url::SchemeHostPort youtube_server("https", "www.youtube.com", 443);
  const AlternativeService alternative_service1(kProtoQUIC, "www.youtube.com",
                                                443);
  SetAlternativeService(youtube_server, alternative_service1);
  EXPECT_TRUE(
      impl_.SupportsRequestPriority(youtube_server, NetworkAnonymizationKey()));

  // Add www.example.com:443 with two alternative services, one supporting QUIC.
  url::SchemeHostPort example_server("https", "www.example.com", 443);
  const AlternativeService alternative_service2(kProtoHTTP2, "", 443);
  SetAlternativeService(example_server, alternative_service2);
  SetAlternativeService(example_server, alternative_service1);
  EXPECT_TRUE(
      impl_.SupportsRequestPriority(example_server, NetworkAnonymizationKey()));

  // Verify all the entries are the same after additions.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail,
                                             NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs,
                                            NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.SupportsRequestPriority(youtube_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.SupportsRequestPriority(example_server, NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, ClearSupportsSpdy) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey(), true);
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, NetworkAnonymizationKey(), true);

  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_mail, NetworkAnonymizationKey()));

  base::RunLoop run_loop;
  bool callback_invoked_ = false;
  impl_.Clear(base::BindOnce(
      [](bool* callback_invoked, base::OnceClosure quit_closure) {
        *callback_invoked = true;
        std::move(quit_closure).Run();
      },
      &callback_invoked_, run_loop.QuitClosure()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));
  EXPECT_FALSE(
      impl_.GetSupportsSpdy(spdy_server_mail, NetworkAnonymizationKey()));

  // Callback should be run asynchronously.
  EXPECT_FALSE(callback_invoked_);
  run_loop.Run();
  EXPECT_TRUE(callback_invoked_);
}

TEST_F(HttpServerPropertiesTest, MRUOfServerInfoMap) {
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey(), true);
  ASSERT_EQ(1u, impl_.server_info_map_for_testing().size());
  auto it = impl_.server_info_map_for_testing().begin();
  ASSERT_EQ(spdy_server_google, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());

  // Add mail.google.com:443 as supporting SPDY. Verify mail.google.com:443 and
  // www.google.com:443 are in the list.
  impl_.SetSupportsSpdy(spdy_server_mail, NetworkAnonymizationKey(), true);
  ASSERT_EQ(2u, impl_.server_info_map_for_testing().size());
  it = impl_.server_info_map_for_testing().begin();
  ASSERT_EQ(spdy_server_mail, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ++it;
  ASSERT_EQ(spdy_server_google, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());

  // Get www.google.com:443. It should become the most-recently-used server.
  EXPECT_TRUE(
      impl_.GetSupportsSpdy(spdy_server_google, NetworkAnonymizationKey()));
  ASSERT_EQ(2u, impl_.server_info_map_for_testing().size());
  it = impl_.server_info_map_for_testing().begin();
  ASSERT_EQ(spdy_server_google, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ++it;
  ASSERT_EQ(spdy_server_mail, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
}

typedef HttpServerPropertiesTest AlternateProtocolServerPropertiesTest;

TEST_F(AlternateProtocolServerPropertiesTest, Basic) {
  url::SchemeHostPort test_server("http", "foo", 80);
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));

  AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service);
  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service,
            alternative_service_info_vector[0].alternative_service());

  impl_.Clear(base::OnceClosure());
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest, ExcludeOrigin) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = test_clock_.Now() + base::Days(1);
  // Same hostname, same port, TCP: should be ignored.
  AlternativeServiceInfo alternative_service_info1 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo", 443), expiration);
  alternative_service_info_vector.push_back(alternative_service_info1);
  // Different hostname: GetAlternativeServiceInfos should return this one.
  AlternativeServiceInfo alternative_service_info2 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "bar", 443), expiration);
  alternative_service_info_vector.push_back(alternative_service_info2);
  // Different port: GetAlternativeServiceInfos should return this one too.
  AlternativeServiceInfo alternative_service_info3 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo", 80), expiration);
  alternative_service_info_vector.push_back(alternative_service_info3);
  // QUIC: GetAlternativeServices should return this one too.
  AlternativeServiceInfo alternative_service_info4 =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(kProtoQUIC, "foo", 443), expiration,
          DefaultSupportedQuicVersions());
  alternative_service_info_vector.push_back(alternative_service_info4);

  url::SchemeHostPort test_server("https", "foo", 443);
  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  const AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(3u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service_info2, alternative_service_info_vector2[0]);
  EXPECT_EQ(alternative_service_info3, alternative_service_info_vector2[1]);
  EXPECT_EQ(alternative_service_info4, alternative_service_info_vector2[2]);
}

TEST_F(AlternateProtocolServerPropertiesTest, Set) {
  // |test_server1| has an alternative service, which will not be
  // affected by OnServerInfoLoadedForTesting(), because
  // |server_info_map| does not have an entry for
  // |test_server1|.
  url::SchemeHostPort test_server1("http", "foo1", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "bar1", 443);
  const base::Time now = test_clock_.Now();
  base::Time expiration1 = now + base::Days(1);
  // 1st entry in the memory.
  impl_.SetHttp2AlternativeService(test_server1, NetworkAnonymizationKey(),
                                   alternative_service1, expiration1);

  // |test_server2| has an alternative service, which will be
  // overwritten by OnServerInfoLoadedForTesting(), because
  // |server_info_map| has an entry for |test_server2|.
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service2(kProtoHTTP2, "bar2", 443);
  base::Time expiration2 = now + base::Days(2);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration2));
  url::SchemeHostPort test_server2("http", "foo2", 80);
  // 0th entry in the memory.
  impl_.SetAlternativeServices(test_server2, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  // Prepare |server_info_map| to be loaded by OnServerInfoLoadedForTesting().
  std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  const AlternativeService alternative_service3(kProtoHTTP2, "bar3", 123);
  base::Time expiration3 = now + base::Days(3);
  const AlternativeServiceInfo alternative_service_info1 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service3, expiration3);
  // Simulate updating data for 0th entry with data from Preferences.
  server_info_map->GetOrPut(CreateSimpleKey(test_server2))
      ->second.alternative_services =
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info1);

  url::SchemeHostPort test_server3("http", "foo3", 80);
  const AlternativeService alternative_service4(kProtoHTTP2, "bar4", 1234);
  base::Time expiration4 = now + base::Days(4);
  const AlternativeServiceInfo alternative_service_info2 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service4, expiration4);
  // Add an old entry from Preferences, this will be added to end of recency
  // list.
  server_info_map->GetOrPut(CreateSimpleKey(test_server3))
      ->second.alternative_services =
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info2);

  // MRU list will be test_server2, test_server1, test_server3.
  impl_.OnServerInfoLoadedForTesting(std::move(server_info_map));

  // Verify server_info_map.
  const HttpServerProperties::ServerInfoMap& map =
      impl_.server_info_map_for_testing();
  ASSERT_EQ(3u, map.size());
  auto map_it = map.begin();

  EXPECT_EQ(test_server2, map_it->first.server);
  EXPECT_TRUE(map_it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(map_it->second.alternative_services.has_value());
  const AlternativeServiceInfoVector* service_info =
      &map_it->second.alternative_services.value();
  ASSERT_EQ(1u, service_info->size());
  EXPECT_EQ(alternative_service3, (*service_info)[0].alternative_service());
  EXPECT_EQ(expiration3, (*service_info)[0].expiration());

  ++map_it;
  EXPECT_EQ(test_server1, map_it->first.server);
  EXPECT_TRUE(map_it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(map_it->second.alternative_services.has_value());
  service_info = &map_it->second.alternative_services.value();
  ASSERT_EQ(1u, service_info->size());
  EXPECT_EQ(alternative_service1, (*service_info)[0].alternative_service());
  EXPECT_EQ(expiration1, (*service_info)[0].expiration());

  ++map_it;
  EXPECT_EQ(map_it->first.server, test_server3);
  EXPECT_TRUE(map_it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(map_it->second.alternative_services.has_value());
  service_info = &map_it->second.alternative_services.value();
  ASSERT_EQ(1u, service_info->size());
  EXPECT_EQ(alternative_service4, (*service_info)[0].alternative_service());
  EXPECT_EQ(expiration4, (*service_info)[0].expiration());
}

TEST_F(AlternateProtocolServerPropertiesTest, SetWebSockets) {
  // The https and wss servers should be treated as the same server, as should
  // the http and ws servers.
  url::SchemeHostPort https_server("https", "www.test.com", 443);
  url::SchemeHostPort wss_server("wss", "www.test.com", 443);
  url::SchemeHostPort http_server("http", "www.test.com", 443);
  url::SchemeHostPort ws_server("ws", "www.test.com", 443);

  AlternativeService alternative_service(kProtoHTTP2, "bar", 443);

  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(wss_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u, impl_.GetAlternativeServiceInfos(ws_server, NetworkAnonymizationKey())
              .size());

  SetAlternativeService(wss_server, alternative_service);
  EXPECT_EQ(
      1u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      1u,
      impl_.GetAlternativeServiceInfos(wss_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u, impl_.GetAlternativeServiceInfos(ws_server, NetworkAnonymizationKey())
              .size());

  SetAlternativeService(http_server, alternative_service);
  EXPECT_EQ(
      1u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      1u,
      impl_.GetAlternativeServiceInfos(wss_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      1u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      1u, impl_.GetAlternativeServiceInfos(ws_server, NetworkAnonymizationKey())
              .size());

  impl_.SetAlternativeServices(https_server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(wss_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      1u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      1u, impl_.GetAlternativeServiceInfos(ws_server, NetworkAnonymizationKey())
              .size());

  impl_.SetAlternativeServices(ws_server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(wss_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      0u, impl_.GetAlternativeServiceInfos(ws_server, NetworkAnonymizationKey())
              .size());
}

TEST_F(AlternateProtocolServerPropertiesTest, SetWithNetworkIsolationKey) {
  const url::SchemeHostPort kServer("https", "foo.test", 443);
  const AlternativeServiceInfoVector kAlternativeServices(
      {AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo", 443),
          base::Time::Now() + base::Days(1) /* expiration */)});

  EXPECT_TRUE(
      impl_.GetAlternativeServiceInfos(kServer, network_anonymization_key1_)
          .empty());
  EXPECT_TRUE(
      impl_.GetAlternativeServiceInfos(kServer, NetworkAnonymizationKey())
          .empty());

  // Without network isolation keys enabled for HttpServerProperties, passing in
  // a NetworkAnonymizationKey should have no effect on behavior.
  for (const auto& network_anonymization_key_to_set :
       {NetworkAnonymizationKey(), network_anonymization_key1_}) {
    impl_.SetAlternativeServices(kServer, network_anonymization_key_to_set,
                                 kAlternativeServices);
    EXPECT_EQ(kAlternativeServices, impl_.GetAlternativeServiceInfos(
                                        kServer, network_anonymization_key1_));
    EXPECT_EQ(kAlternativeServices, impl_.GetAlternativeServiceInfos(
                                        kServer, NetworkAnonymizationKey()));

    impl_.SetAlternativeServices(kServer, network_anonymization_key_to_set,
                                 AlternativeServiceInfoVector());
    EXPECT_TRUE(
        impl_.GetAlternativeServiceInfos(kServer, network_anonymization_key1_)
            .empty());
    EXPECT_TRUE(
        impl_.GetAlternativeServiceInfos(kServer, NetworkAnonymizationKey())
            .empty());
  }

  // Check that with network isolation keys enabled for HttpServerProperties,
  // the NetworkAnonymizationKey argument is respected.

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  properties.SetAlternativeServices(kServer, network_anonymization_key1_,
                                    kAlternativeServices);
  EXPECT_EQ(kAlternativeServices, properties.GetAlternativeServiceInfos(
                                      kServer, network_anonymization_key1_));
  EXPECT_TRUE(
      properties.GetAlternativeServiceInfos(kServer, NetworkAnonymizationKey())
          .empty());

  properties.SetAlternativeServices(kServer, NetworkAnonymizationKey(),
                                    kAlternativeServices);
  EXPECT_EQ(kAlternativeServices, properties.GetAlternativeServiceInfos(
                                      kServer, network_anonymization_key1_));
  EXPECT_EQ(kAlternativeServices, properties.GetAlternativeServiceInfos(
                                      kServer, NetworkAnonymizationKey()));

  properties.SetAlternativeServices(kServer, network_anonymization_key1_,
                                    AlternativeServiceInfoVector());
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(kServer, network_anonymization_key1_)
          .empty());
  EXPECT_EQ(kAlternativeServices, properties.GetAlternativeServiceInfos(
                                      kServer, NetworkAnonymizationKey()));

  properties.SetAlternativeServices(kServer, NetworkAnonymizationKey(),
                                    AlternativeServiceInfoVector());
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(kServer, network_anonymization_key1_)
          .empty());
  EXPECT_TRUE(
      properties.GetAlternativeServiceInfos(kServer, NetworkAnonymizationKey())
          .empty());
}

// Regression test for https://crbug.com/504032:
// OnServerInfoLoadedForTesting() should not crash if there is an
// empty hostname is the mapping.
TEST_F(AlternateProtocolServerPropertiesTest, SetWithEmptyHostname) {
  url::SchemeHostPort server("https", "foo", 443);
  const AlternativeService alternative_service_with_empty_hostname(kProtoHTTP2,
                                                                   "", 1234);
  const AlternativeService alternative_service_with_foo_hostname(kProtoHTTP2,
                                                                 "foo", 1234);
  SetAlternativeService(server, alternative_service_with_empty_hostname);
  impl_.MarkAlternativeServiceBroken(alternative_service_with_foo_hostname,
                                     NetworkAnonymizationKey());

  std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  impl_.OnServerInfoLoadedForTesting(std::move(server_info_map));

  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(
      alternative_service_with_foo_hostname, NetworkAnonymizationKey()));
  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service_with_foo_hostname,
            alternative_service_info_vector[0].alternative_service());
}

// GetAlternativeServiceInfos() should remove |server_info_map_|
// elements with empty value.
TEST_F(AlternateProtocolServerPropertiesTest, EmptyVector) {
  url::SchemeHostPort server("https", "foo", 443);
  const AlternativeService alternative_service(kProtoHTTP2, "bar", 443);
  base::Time expiration = test_clock_.Now() - base::Days(1);
  const AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration);
  std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  server_info_map->GetOrPut(CreateSimpleKey(server))
      ->second.alternative_services = AlternativeServiceInfoVector(
      /*size=*/1, alternative_service_info);

  // Prepare |server_info_map_| with a single key that has a single
  // AlternativeServiceInfo with identical hostname and port.
  impl_.OnServerInfoLoadedForTesting(std::move(server_info_map));

  // GetAlternativeServiceInfos() should remove such AlternativeServiceInfo from
  // |server_info_map_|, emptying the AlternativeServiceInfoVector
  // corresponding to |server|.
  ASSERT_TRUE(
      impl_.GetAlternativeServiceInfos(server, NetworkAnonymizationKey())
          .empty());

  // GetAlternativeServiceInfos() should remove this key from
  // |server_info_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(
      server, NetworkAnonymizationKey(),
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // There should still be no alternative service assigned to |server|.
  ASSERT_TRUE(
      impl_.GetAlternativeServiceInfos(server, NetworkAnonymizationKey())
          .empty());
}

// Regression test for https://crbug.com/516486 for the canonical host case.
TEST_F(AlternateProtocolServerPropertiesTest, EmptyVectorForCanonical) {
  url::SchemeHostPort server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  const AlternativeService alternative_service(kProtoHTTP2, "", 443);
  base::Time expiration = test_clock_.Now() - base::Days(1);
  const AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration);
  std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  server_info_map->GetOrPut(CreateSimpleKey(canonical_server))
      ->second.alternative_services =
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info);

  // Prepare |server_info_map_| with a single key that has a single
  // AlternativeServiceInfo with identical hostname and port.
  impl_.OnServerInfoLoadedForTesting(std::move(server_info_map));

  // GetAlternativeServiceInfos() should remove such AlternativeServiceInfo from
  // |server_info_map_|, emptying the AlternativeServiceInfoVector
  // corresponding to |canonical_server|, even when looking up
  // alternative services for |server|.
  ASSERT_TRUE(
      impl_.GetAlternativeServiceInfos(server, NetworkAnonymizationKey())
          .empty());

  // GetAlternativeServiceInfos() should remove this key from
  // |server_info_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(
      canonical_server, NetworkAnonymizationKey(),
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // There should still be no alternative service assigned to
  // |canonical_server|.
  ASSERT_TRUE(impl_
                  .GetAlternativeServiceInfos(canonical_server,
                                              NetworkAnonymizationKey())
                  .empty());
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearServerWithCanonical) {
  url::SchemeHostPort server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  const AlternativeService alternative_service(kProtoQUIC, "", 443);
  base::Time expiration = test_clock_.Now() + base::Days(1);
  const AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration, DefaultSupportedQuicVersions());

  impl_.SetAlternativeServices(
      canonical_server, NetworkAnonymizationKey(),
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // Make sure the canonical service is returned for the other server.
  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(kProtoQUIC,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ(443, alternative_service_info_vector[0].alternative_service().port);

  // Now clear the alternatives for the other server and make sure it stays
  // cleared.
  // GetAlternativeServices() should remove this key from
  // |server_info_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());

  ASSERT_TRUE(
      impl_.GetAlternativeServiceInfos(server, NetworkAnonymizationKey())
          .empty());
}

TEST_F(AlternateProtocolServerPropertiesTest, MRUOfGetAlternativeServiceInfos) {
  url::SchemeHostPort test_server1("http", "foo1", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "foo1", 443);
  SetAlternativeService(test_server1, alternative_service1);
  url::SchemeHostPort test_server2("http", "foo2", 80);
  const AlternativeService alternative_service2(kProtoHTTP2, "foo2", 1234);
  SetAlternativeService(test_server2, alternative_service2);

  const HttpServerProperties::ServerInfoMap& map =
      impl_.server_info_map_for_testing();
  auto it = map.begin();
  EXPECT_EQ(test_server2, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.alternative_services.has_value());
  ASSERT_EQ(1u, it->second.alternative_services->size());
  EXPECT_EQ(alternative_service2,
            it->second.alternative_services.value()[0].alternative_service());

  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server1, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());

  // GetAlternativeServices should reorder the AlternateProtocol map.
  it = map.begin();
  EXPECT_EQ(test_server1, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.alternative_services.has_value());
  ASSERT_EQ(1u, it->second.alternative_services->size());
  EXPECT_EQ(alternative_service1,
            it->second.alternative_services.value()[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, SetBroken) {
  url::SchemeHostPort test_server("http", "foo", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service1);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                                NetworkAnonymizationKey()));

  // GetAlternativeServiceInfos should return the broken alternative service.
  impl_.MarkAlternativeServiceBroken(alternative_service1,
                                     NetworkAnonymizationKey());
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));

  // SetAlternativeServices should add a broken alternative service to the map.
  AlternativeServiceInfoVector alternative_service_info_vector2;
  base::Time expiration = test_clock_.Now() + base::Days(1);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "foo", 1234);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector2);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(2u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector[1].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2,
                                                NetworkAnonymizationKey()));

  // SetAlternativeService should add a broken alternative service to the map.
  SetAlternativeService(test_server, alternative_service1);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       SetBrokenUntilDefaultNetworkChanges) {
  url::SchemeHostPort test_server("http", "foo", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service1);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                                NetworkAnonymizationKey()));

  // Mark the alternative service as broken until the default network changes.
  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service1, NetworkAnonymizationKey());
  // The alternative service should be persisted and marked as broken.
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));

  // SetAlternativeServices should add a broken alternative service to the map.
  AlternativeServiceInfoVector alternative_service_info_vector2;
  base::Time expiration = test_clock_.Now() + base::Days(1);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "foo", 1234);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector2);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(2u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector[1].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2,
                                                NetworkAnonymizationKey()));

  // SetAlternativeService should add a broken alternative service to the map.
  SetAlternativeService(test_server, alternative_service1);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest, MaxAge) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time now = test_clock_.Now();
  base::TimeDelta one_day = base::Days(1);

  // First alternative service expired one day ago, should not be returned by
  // GetAlternativeServiceInfos().
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, now - one_day));

  // Second alterrnative service will expire one day from now, should be
  // returned by GetAlternativeSerices().
  const AlternativeService alternative_service2(kProtoHTTP2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, now + one_day));

  url::SchemeHostPort test_server("http", "foo", 80);
  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector2[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, MaxAgeCanonical) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time now = test_clock_.Now();
  base::TimeDelta one_day = base::Days(1);

  // First alternative service expired one day ago, should not be returned by
  // GetAlternativeServiceInfos().
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, now - one_day));

  // Second alterrnative service will expire one day from now, should be
  // returned by GetAlternativeSerices().
  const AlternativeService alternative_service2(kProtoHTTP2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, now + one_day));

  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  impl_.SetAlternativeServices(canonical_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector2[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, AlternativeServiceWithScheme) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  base::Time expiration = test_clock_.Now() + base::Days(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  // Set Alt-Svc list for |http_server|.
  url::SchemeHostPort http_server("http", "foo", 80);
  impl_.SetAlternativeServices(http_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  const HttpServerProperties::ServerInfoMap& map =
      impl_.server_info_map_for_testing();
  auto it = map.begin();
  EXPECT_EQ(http_server, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.alternative_services.has_value());
  ASSERT_EQ(2u, it->second.alternative_services->size());
  EXPECT_EQ(alternative_service1,
            it->second.alternative_services.value()[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            it->second.alternative_services.value()[1].alternative_service());

  // Check Alt-Svc list should not be set for |https_server|.
  url::SchemeHostPort https_server("https", "foo", 80);
  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());

  // Set Alt-Svc list for |https_server|.
  impl_.SetAlternativeServices(https_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);
  EXPECT_EQ(
      2u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      2u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());

  // Clear Alt-Svc list for |http_server|.
  impl_.SetAlternativeServices(http_server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());

  EXPECT_EQ(
      0u,
      impl_.GetAlternativeServiceInfos(http_server, NetworkAnonymizationKey())
          .size());
  EXPECT_EQ(
      2u,
      impl_.GetAlternativeServiceInfos(https_server, NetworkAnonymizationKey())
          .size());
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearAlternativeServices) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  base::Time expiration = test_clock_.Now() + base::Days(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  url::SchemeHostPort test_server("http", "foo", 80);
  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  const HttpServerProperties::ServerInfoMap& map =
      impl_.server_info_map_for_testing();
  auto it = map.begin();
  EXPECT_EQ(test_server, it->first.server);
  EXPECT_TRUE(it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(it->second.alternative_services.has_value());
  ASSERT_EQ(2u, it->second.alternative_services->size());
  EXPECT_EQ(alternative_service1,
            it->second.alternative_services.value()[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            it->second.alternative_services.value()[1].alternative_service());

  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());
  EXPECT_TRUE(map.empty());
}

// A broken alternative service in the mapping carries meaningful information,
// therefore it should not be ignored by SetAlternativeService().  In
// particular, an alternative service mapped to an origin shadows alternative
// services of canonical hosts.
TEST_F(AlternateProtocolServerPropertiesTest, BrokenShadowsCanonical) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);
  SetAlternativeService(canonical_server, canonical_alternative_service);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(canonical_alternative_service,
            alternative_service_info_vector[0].alternative_service());

  const AlternativeService broken_alternative_service(kProtoHTTP2, "foo", 443);
  impl_.MarkAlternativeServiceBroken(broken_alternative_service,
                                     NetworkAnonymizationKey());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(broken_alternative_service,
                                               NetworkAnonymizationKey()));

  SetAlternativeService(test_server, broken_alternative_service);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(broken_alternative_service,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(broken_alternative_service,
                                               NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearBroken) {
  url::SchemeHostPort test_server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service);
  impl_.MarkAlternativeServiceBroken(alternative_service,
                                     NetworkAnonymizationKey());
  ASSERT_TRUE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  // SetAlternativeServices should leave a broken alternative service marked
  // as such.
  impl_.SetAlternativeServices(test_server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       MarkBrokenWithNetworkIsolationKey) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  const base::Time expiration = test_clock_.Now() + base::Days(1);

  // Without NetworkIsolationKeys enabled, the NetworkAnonymizationKey parameter
  // should be ignored.
  impl_.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                   alternative_service, expiration);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  impl_.MarkAlternativeServiceBroken(alternative_service,
                                     network_anonymization_key1_);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               network_anonymization_key1_));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               network_anonymization_key2_));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  impl_.ConfirmAlternativeService(alternative_service,
                                  network_anonymization_key2_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  properties.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                        alternative_service, expiration);
  properties.SetHttp2AlternativeService(server, network_anonymization_key2_,
                                        alternative_service, expiration);

  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceBroken(alternative_service,
                                          network_anonymization_key1_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceBroken(alternative_service,
                                          network_anonymization_key2_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.ConfirmAlternativeService(alternative_service,
                                       network_anonymization_key1_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.ConfirmAlternativeService(alternative_service,
                                       network_anonymization_key2_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));
}

TEST_F(AlternateProtocolServerPropertiesTest, MarkRecentlyBroken) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(server, alternative_service);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.MarkAlternativeServiceRecentlyBroken(alternative_service,
                                             NetworkAnonymizationKey());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.ConfirmAlternativeService(alternative_service,
                                  NetworkAnonymizationKey());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       MarkRecentlyBrokenWithNetworkIsolationKey) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  const base::Time expiration = test_clock_.Now() + base::Days(1);

  // Without NetworkIsolationKeys enabled, the NetworkAnonymizationKey parameter
  // should be ignored.
  impl_.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                   alternative_service, expiration);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  impl_.MarkAlternativeServiceRecentlyBroken(alternative_service,
                                             network_anonymization_key1_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  impl_.ConfirmAlternativeService(alternative_service,
                                  network_anonymization_key2_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  properties.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                        alternative_service, expiration);
  properties.SetHttp2AlternativeService(server, network_anonymization_key2_,
                                        alternative_service, expiration);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceRecentlyBroken(alternative_service,
                                                  network_anonymization_key1_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceRecentlyBroken(alternative_service,
                                                  network_anonymization_key2_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.ConfirmAlternativeService(alternative_service,
                                       network_anonymization_key1_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.ConfirmAlternativeService(alternative_service,
                                       network_anonymization_key2_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       MarkBrokenUntilDefaultNetworkChanges) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(server, alternative_service);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, NetworkAnonymizationKey());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.ConfirmAlternativeService(alternative_service,
                                  NetworkAnonymizationKey());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       MarkBrokenUntilDefaultNetworkChangesWithNetworkIsolationKey) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  const base::Time expiration = test_clock_.Now() + base::Days(1);

  // Without NetworkIsolationKeys enabled, the NetworkAnonymizationKey parameter
  // should be ignored.
  impl_.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                   alternative_service, expiration);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, network_anonymization_key1_);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               network_anonymization_key1_));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               network_anonymization_key2_));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  impl_.ConfirmAlternativeService(alternative_service,
                                  network_anonymization_key2_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key1_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                network_anonymization_key2_));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  properties.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                        alternative_service, expiration);
  properties.SetHttp2AlternativeService(server, network_anonymization_key2_,
                                        alternative_service, expiration);

  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, network_anonymization_key1_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, network_anonymization_key2_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.ConfirmAlternativeService(alternative_service,
                                       network_anonymization_key1_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.ConfirmAlternativeService(alternative_service,
                                       network_anonymization_key2_);
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));
}

TEST_F(AlternateProtocolServerPropertiesTest, OnDefaultNetworkChanged) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);

  SetAlternativeService(server, alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, NetworkAnonymizationKey());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  // Default network change clears alt svc broken until default network changes.
  impl_.OnDefaultNetworkChanged();
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, NetworkAnonymizationKey());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.MarkAlternativeServiceBroken(alternative_service,
                                     NetworkAnonymizationKey());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  // Default network change doesn't affect alt svc that was simply marked broken
  // most recently.
  impl_.OnDefaultNetworkChanged();
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, NetworkAnonymizationKey());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  // Default network change clears alt svc that was marked broken until default
  // network change most recently even if the alt svc was initially marked
  // broken.
  impl_.OnDefaultNetworkChanged();
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       OnDefaultNetworkChangedWithNetworkIsolationKey) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  const base::Time expiration = test_clock_.Now() + base::Days(1);
  properties.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                        alternative_service, expiration);
  properties.SetHttp2AlternativeService(server, network_anonymization_key2_,
                                        alternative_service, expiration);

  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  properties.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, network_anonymization_key1_);
  properties.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, network_anonymization_key2_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  // Default network change clears alt svc broken until default network changes.
  properties.OnDefaultNetworkChanged();
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));
}

TEST_F(AlternateProtocolServerPropertiesTest, Canonical) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));

  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  EXPECT_FALSE(
      HasAlternativeService(canonical_server, NetworkAnonymizationKey()));

  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService canonical_alternative_service1(
      kProtoQUIC, "bar.c.youtube.com", 1234);
  base::Time expiration = test_clock_.Now() + base::Days(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          canonical_alternative_service1, expiration,
          DefaultSupportedQuicVersions()));
  const AlternativeService canonical_alternative_service2(kProtoHTTP2, "", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          canonical_alternative_service2, expiration));
  impl_.SetAlternativeServices(canonical_server, NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  // Since |test_server| does not have an alternative service itself,
  // GetAlternativeServiceInfos should return those of |canonical_server|.
  AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey());
  ASSERT_EQ(2u, alternative_service_info_vector2.size());
  EXPECT_EQ(canonical_alternative_service1,
            alternative_service_info_vector2[0].alternative_service());

  // Since |canonical_alternative_service2| has an empty host,
  // GetAlternativeServiceInfos should substitute the hostname of its |origin|
  // argument.
  EXPECT_EQ(test_server.host(),
            alternative_service_info_vector2[1].alternative_service().host);
  EXPECT_EQ(canonical_alternative_service2.protocol,
            alternative_service_info_vector2[1].alternative_service().protocol);
  EXPECT_EQ(canonical_alternative_service2.port,
            alternative_service_info_vector2[1].alternative_service().port);

  // Verify the canonical suffix.
  EXPECT_EQ(".c.youtube.com",
            *impl_.GetCanonicalSuffixForTesting(test_server.host()));
  EXPECT_EQ(".c.youtube.com",
            *impl_.GetCanonicalSuffixForTesting(canonical_server.host()));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearCanonical) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  impl_.SetAlternativeServices(canonical_server, NetworkAnonymizationKey(),
                               AlternativeServiceInfoVector());
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       CanonicalWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  EXPECT_FALSE(HasAlternativeService(test_server, network_anonymization_key1_));

  url::SchemeHostPort canonical_server1("https", "bar.c.youtube.com", 443);
  EXPECT_FALSE(
      HasAlternativeService(canonical_server1, network_anonymization_key1_));

  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService canonical_alternative_service1(
      kProtoQUIC, "bar.c.youtube.com", 1234);
  base::Time expiration = test_clock_.Now() + base::Days(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          canonical_alternative_service1, expiration,
          DefaultSupportedQuicVersions()));
  const AlternativeService canonical_alternative_service2(kProtoHTTP2, "", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          canonical_alternative_service2, expiration));
  properties.SetAlternativeServices(canonical_server1,
                                    network_anonymization_key1_,
                                    alternative_service_info_vector);

  // Since |test_server| does not have an alternative service itself,
  // GetAlternativeServiceInfos should return those of |canonical_server|.
  AlternativeServiceInfoVector alternative_service_info_vector2 =
      properties.GetAlternativeServiceInfos(test_server,
                                            network_anonymization_key1_);
  ASSERT_EQ(2u, alternative_service_info_vector2.size());
  EXPECT_EQ(canonical_alternative_service1,
            alternative_service_info_vector2[0].alternative_service());

  // Canonical information should not be visible for other NetworkIsolationKeys.
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(test_server, network_anonymization_key2_)
          .empty());
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey())
          .empty());

  // Now add an alternative service entry for network_anonymization_key2_ for a
  // different server and different NetworkAnonymizationKey, but with the same
  // canonical suffix.
  url::SchemeHostPort canonical_server2("https", "shrimp.c.youtube.com", 443);
  properties.SetAlternativeServices(canonical_server2,
                                    network_anonymization_key2_,
                                    {alternative_service_info_vector[0]});

  // The canonical server information should reachable, and different, for both
  // NetworkIsolationKeys.
  EXPECT_EQ(1u, properties
                    .GetAlternativeServiceInfos(test_server,
                                                network_anonymization_key2_)
                    .size());
  EXPECT_EQ(2u, properties
                    .GetAlternativeServiceInfos(test_server,
                                                network_anonymization_key1_)
                    .size());
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey())
          .empty());

  // Clearing the alternate service state of network_anonymization_key1_'s
  // canonical server should only affect network_anonymization_key1_.
  properties.SetAlternativeServices(canonical_server1,
                                    network_anonymization_key1_, {});
  EXPECT_EQ(1u, properties
                    .GetAlternativeServiceInfos(test_server,
                                                network_anonymization_key2_)
                    .size());
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(test_server, network_anonymization_key1_)
          .empty());
  EXPECT_TRUE(
      properties
          .GetAlternativeServiceInfos(test_server, NetworkAnonymizationKey())
          .empty());
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalBroken) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  EXPECT_TRUE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
  impl_.MarkAlternativeServiceBroken(canonical_alternative_service,
                                     NetworkAnonymizationKey());
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       CanonicalBrokenUntilDefaultNetworkChanges) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  EXPECT_TRUE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      canonical_alternative_service, NetworkAnonymizationKey());
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
}

// Adding an alternative service for a new host overrides canonical host.
TEST_F(AlternateProtocolServerPropertiesTest, CanonicalOverride) {
  url::SchemeHostPort foo_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort bar_server("https", "bar.c.youtube.com", 443);
  AlternativeService bar_alternative_service(kProtoQUIC, "bar.c.youtube.com",
                                             1234);
  SetAlternativeService(bar_server, bar_alternative_service);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(foo_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(bar_alternative_service,
            alternative_service_info_vector[0].alternative_service());

  url::SchemeHostPort qux_server("https", "qux.c.youtube.com", 443);
  AlternativeService qux_alternative_service(kProtoQUIC, "qux.c.youtube.com",
                                             443);
  SetAlternativeService(qux_server, qux_alternative_service);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(foo_server, NetworkAnonymizationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(qux_alternative_service,
            alternative_service_info_vector[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearWithCanonical) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  impl_.Clear(base::OnceClosure());
  EXPECT_FALSE(HasAlternativeService(test_server, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       ExpireBrokenAlternateProtocolMappings) {
  url::SchemeHostPort server("https", "foo", 443);
  AlternativeService alternative_service(kProtoQUIC, "foo", 443);
  SetAlternativeService(server, alternative_service);
  EXPECT_TRUE(HasAlternativeService(server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  base::TimeTicks past = test_tick_clock_->NowTicks() - base::Seconds(42);
  HttpServerPropertiesPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &impl_, alternative_service, past);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                               NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));

  HttpServerPropertiesPeer::ExpireBrokenAlternateProtocolMappings(&impl_);
  EXPECT_FALSE(HasAlternativeService(server, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                NetworkAnonymizationKey()));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       ExpireBrokenAlternateProtocolMappingsWithNetworkIsolationKey) {
  url::SchemeHostPort server("https", "foo", 443);
  AlternativeService alternative_service(kProtoHTTP2, "foo", 444);
  base::TimeTicks past = test_tick_clock_->NowTicks() - base::Seconds(42);
  base::TimeTicks future = test_tick_clock_->NowTicks() + base::Seconds(42);
  const base::Time alt_service_expiration = test_clock_.Now() + base::Days(1);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  properties.SetHttp2AlternativeService(server, network_anonymization_key1_,
                                        alternative_service,
                                        alt_service_expiration);
  properties.SetHttp2AlternativeService(server, network_anonymization_key2_,
                                        alternative_service,
                                        alt_service_expiration);

  EXPECT_FALSE(
      properties.GetAlternativeServiceInfos(server, network_anonymization_key1_)
          .empty());
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(
      properties.GetAlternativeServiceInfos(server, network_anonymization_key2_)
          .empty());
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  // Set broken alternative service with expiration date in the past for
  // |network_anonymization_key1_|.
  HttpServerPropertiesPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &properties, alternative_service, past, network_anonymization_key1_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_FALSE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  // Set broken alternative service with expiration date in the future for
  // |network_anonymization_key1_|.
  HttpServerPropertiesPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &properties, alternative_service, future, network_anonymization_key2_);
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));

  // Only the broken entry for |network_anonymization_key1_| should be expired.
  HttpServerPropertiesPeer::ExpireBrokenAlternateProtocolMappings(&properties);
  EXPECT_TRUE(
      properties.GetAlternativeServiceInfos(server, network_anonymization_key1_)
          .empty());
  EXPECT_FALSE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key1_));
  EXPECT_FALSE(
      properties.GetAlternativeServiceInfos(server, network_anonymization_key2_)
          .empty());
  EXPECT_TRUE(properties.IsAlternativeServiceBroken(
      alternative_service, network_anonymization_key2_));
  EXPECT_TRUE(properties.WasAlternativeServiceRecentlyBroken(
      alternative_service, network_anonymization_key2_));
}

// Regression test for https://crbug.com/505413.
TEST_F(AlternateProtocolServerPropertiesTest, RemoveExpiredBrokenAltSvc) {
  url::SchemeHostPort foo_server("https", "foo", 443);
  AlternativeService bar_alternative_service(kProtoQUIC, "bar", 443);
  SetAlternativeService(foo_server, bar_alternative_service);
  EXPECT_TRUE(HasAlternativeService(foo_server, NetworkAnonymizationKey()));

  url::SchemeHostPort bar_server1("http", "bar", 80);
  AlternativeService nohost_alternative_service(kProtoQUIC, "", 443);
  SetAlternativeService(bar_server1, nohost_alternative_service);
  EXPECT_TRUE(HasAlternativeService(bar_server1, NetworkAnonymizationKey()));

  url::SchemeHostPort bar_server2("https", "bar", 443);
  AlternativeService baz_alternative_service(kProtoQUIC, "baz", 1234);
  SetAlternativeService(bar_server2, baz_alternative_service);
  EXPECT_TRUE(HasAlternativeService(bar_server2, NetworkAnonymizationKey()));

  // Mark "bar:443" as broken.
  base::TimeTicks past = test_tick_clock_->NowTicks() - base::Seconds(42);
  HttpServerPropertiesPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &impl_, bar_alternative_service, past);

  // Expire brokenness of "bar:443".
  HttpServerPropertiesPeer::ExpireBrokenAlternateProtocolMappings(&impl_);

  // "foo:443" should have no alternative service now.
  EXPECT_FALSE(HasAlternativeService(foo_server, NetworkAnonymizationKey()));
  // "bar:80" should have no alternative service now.
  EXPECT_FALSE(HasAlternativeService(bar_server1, NetworkAnonymizationKey()));
  // The alternative service of "bar:443" should be unaffected.
  EXPECT_TRUE(HasAlternativeService(bar_server2, NetworkAnonymizationKey()));

  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(
      bar_alternative_service, NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(
      baz_alternative_service, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       SetBrokenAlternativeServicesDelayParams1) {
  url::SchemeHostPort server("https", "foo", 443);
  AlternativeService alternative_service(kProtoQUIC, "foo", 443);
  SetAlternativeService(server, alternative_service);

  const base::TimeDelta initial_delay = base::Seconds(1);
  impl_.SetBrokenAlternativeServicesDelayParams(initial_delay, true);
  for (int i = 0; i < 10; ++i) {
    impl_.MarkAlternativeServiceBroken(alternative_service,
                                       NetworkAnonymizationKey());
    // |impl_| should have posted task to expire the brokenness of
    // |alternative_service|
    EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
    EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                 NetworkAnonymizationKey()));

    // Advance time by just enough so that |alternative_service|'s brokenness
    // expires.
    FastForwardBy(initial_delay * (1 << i));

    // Ensure brokenness of |alternative_service| has expired.
    EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
    EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                  NetworkAnonymizationKey()));
  }
}

TEST_F(AlternateProtocolServerPropertiesTest,
       SetBrokenAlternativeServicesDelayParams2) {
  url::SchemeHostPort server("https", "foo", 443);
  AlternativeService alternative_service(kProtoQUIC, "foo", 443);
  SetAlternativeService(server, alternative_service);

  const base::TimeDelta initial_delay = base::Seconds(5);
  impl_.SetBrokenAlternativeServicesDelayParams(initial_delay, false);
  for (int i = 0; i < 10; ++i) {
    impl_.MarkAlternativeServiceBroken(alternative_service,
                                       NetworkAnonymizationKey());
    // |impl_| should have posted task to expire the brokenness of
    // |alternative_service|
    EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
    EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                 NetworkAnonymizationKey()));

    // Advance time by just enough so that |alternative_service|'s brokenness
    // expires.
    if (i == 0) {
      FastForwardBy(initial_delay);
    } else {
      FastForwardBy(base::Seconds(300) * (1 << (i - 1)));
    }

    // Ensure brokenness of |alternative_service| has expired.
    EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
    EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service,
                                                  NetworkAnonymizationKey()));
  }
}

// Regression test for https://crbug.com/724302
TEST_F(AlternateProtocolServerPropertiesTest, RemoveExpiredBrokenAltSvc2) {
  // This test will mark an alternative service A that has already been marked
  // broken many times, then immediately mark another alternative service B as
  // broken for the first time. Because A's been marked broken many times
  // already, its brokenness will be scheduled to expire much further in the
  // future than B, even though it was marked broken before B. This test makes
  // sure that even though A was marked broken before B, B's brokenness should
  // expire before A.

  url::SchemeHostPort server1("https", "foo", 443);
  AlternativeService alternative_service1(kProtoQUIC, "foo", 443);
  SetAlternativeService(server1, alternative_service1);

  url::SchemeHostPort server2("https", "bar", 443);
  AlternativeService alternative_service2(kProtoQUIC, "bar", 443);
  SetAlternativeService(server2, alternative_service2);

  // Repeatedly mark alt svc 1 broken and wait for its brokenness to expire.
  // This will increase its time until expiration.
  for (int i = 0; i < 3; ++i) {
    impl_.MarkAlternativeServiceBroken(alternative_service1,
                                       NetworkAnonymizationKey());

    // |impl_| should have posted task to expire the brokenness of
    // |alternative_service1|
    EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
    EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                                 NetworkAnonymizationKey()));

    // Advance time by just enough so that |alternative_service1|'s brokenness
    // expires.
    FastForwardBy(BROKEN_ALT_SVC_EXPIRE_DELAYS[i]);

    // Ensure brokenness of |alternative_service1| has expired.
    EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
    EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                                  NetworkAnonymizationKey()));
  }

  impl_.MarkAlternativeServiceBroken(alternative_service1,
                                     NetworkAnonymizationKey());
  impl_.MarkAlternativeServiceBroken(alternative_service2,
                                     NetworkAnonymizationKey());

  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service2,
                                               NetworkAnonymizationKey()));

  // Advance time by just enough so that |alternative_service2|'s brokennness
  // expires.
  FastForwardBy(BROKEN_ALT_SVC_EXPIRE_DELAYS[0]);

  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                               NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2,
                                                NetworkAnonymizationKey()));

  // Advance time by enough so that |alternative_service1|'s brokenness expires.
  FastForwardBy(BROKEN_ALT_SVC_EXPIRE_DELAYS[3] -
                BROKEN_ALT_SVC_EXPIRE_DELAYS[0]);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1,
                                                NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2,
                                                NetworkAnonymizationKey()));
}

// Regression test for https://crbug.com/994537. Having a ServerInfo entry
// without a populated |alternative_services| value would cause
// OnExpireBrokenAlternativeService() to hang..
TEST_F(AlternateProtocolServerPropertiesTest, RemoveExpiredBrokenAltSvc3) {
  // Add an altertive service entry.
  const url::SchemeHostPort kServer1("https", "foo", 443);
  const AlternativeService kAltService(kProtoQUIC, "bar", 443);
  SetAlternativeService(kServer1, kAltService);
  EXPECT_TRUE(HasAlternativeService(kServer1, NetworkAnonymizationKey()));

  // Add an entry to ServerInfo for another server, without an alternative
  // service value.
  const url::SchemeHostPort kServer2("http", "bar", 80);
  impl_.SetSupportsSpdy(kServer2, NetworkAnonymizationKey(), false);

  // Mark kAltService as broken.
  base::TimeTicks past = test_tick_clock_->NowTicks() - base::Seconds(42);
  HttpServerPropertiesPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &impl_, kAltService, past);

  // Expire brokenness of kAltService. This call should not hang.
  HttpServerPropertiesPeer::ExpireBrokenAlternateProtocolMappings(&impl_);

  EXPECT_FALSE(HasAlternativeService(kServer1, NetworkAnonymizationKey()));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       GetAlternativeServiceInfoAsValue) {
  constexpr base::Time::Exploded kNowExploded = {.year = 2018,
                                                 .month = 1,
                                                 .day_of_week = 3,
                                                 .day_of_month = 24,
                                                 .hour = 15,
                                                 .minute = 12,
                                                 .second = 53};
  base::Time now;
  bool result = base::Time::FromLocalExploded(kNowExploded, &now);
  DCHECK(result);
  test_clock_.SetNow(now);

  AlternativeServiceInfoVector alternative_service_info_vector;
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo", 443), now + base::Minutes(1)));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(kProtoQUIC, "bar", 443), now + base::Hours(1),
          DefaultSupportedQuicVersions()));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(kProtoQUIC, "baz", 443), now + base::Hours(1),
          DefaultSupportedQuicVersions()));

  impl_.SetAlternativeServices(url::SchemeHostPort("https", "youtube.com", 443),
                               NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  impl_.MarkAlternativeServiceBroken(AlternativeService(kProtoQUIC, "bar", 443),
                                     NetworkAnonymizationKey());

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      AlternativeService(kProtoQUIC, "baz", 443), NetworkAnonymizationKey());

  alternative_service_info_vector.clear();
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo2", 443), now + base::Days(1)));
  impl_.SetAlternativeServices(url::SchemeHostPort("http", "test.com", 80),
                               NetworkAnonymizationKey(),
                               alternative_service_info_vector);

  const char expected_json[] =
      "["
      "{"
      "\"alternative_service\":"
      "[\"h2 foo2:443, expires 2018-01-25 15:12:53\"],"
      "\"network_anonymization_key\":\"null\","
      "\"server\":\"http://test.com\""
      "},"
      "{"
      "\"alternative_service\":"
      "[\"h2 foo:443, expires 2018-01-24 15:13:53\","
      "\"quic bar:443, expires 2018-01-24 16:12:53"
      " (broken until 2018-01-24 15:17:53)\","
      "\"quic baz:443, expires 2018-01-24 16:12:53"
      " (broken until 2018-01-24 15:17:53)\"],"
      "\"network_anonymization_key\":\"null\","
      "\"server\":\"https://youtube.com\""
      "}"
      "]";

  base::Value alternative_service_info_value =
      impl_.GetAlternativeServiceInfoAsValue();
  std::string alternative_service_info_json;
  base::JSONWriter::Write(alternative_service_info_value,
                          &alternative_service_info_json);
  EXPECT_EQ(expected_json, alternative_service_info_json);
}

TEST_F(HttpServerPropertiesTest, LoadLastLocalAddressWhenQuicWorked) {
  const IPAddress kEmptyAddress;
  const IPAddress kValidAddress1 = IPAddress::IPv4Localhost();
  const IPAddress kValidAddress2 = IPAddress::IPv6Localhost();

  // Check by initializing empty address.
  impl_.OnLastLocalAddressWhenQuicWorkedForTesting(kEmptyAddress);
  EXPECT_FALSE(impl_.HasLastLocalAddressWhenQuicWorked());
  // Empty address should not be considered an address that was used when QUIC
  // worked.
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // Check by initializing with a valid address.
  impl_.OnLastLocalAddressWhenQuicWorkedForTesting(kValidAddress1);
  EXPECT_TRUE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_TRUE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // Try another valid address.
  impl_.OnLastLocalAddressWhenQuicWorkedForTesting(kValidAddress2);
  EXPECT_TRUE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_TRUE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // And loading an empty address clears the current one.
  // TODO(mmenke): This seems like a bug, since if we've learned the current
  // network supports QUIC, surely we want to save that to disk? Seems like a
  // pre-existing value should take precedence, if non-empty, since if the
  // current network is already known to support QUIC, the loaded value is no
  // longer relevant.
  impl_.OnLastLocalAddressWhenQuicWorkedForTesting(kEmptyAddress);
  EXPECT_FALSE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));
}

TEST_F(HttpServerPropertiesTest, SetLastLocalAddressWhenQuicWorked) {
  const IPAddress kEmptyAddress;
  const IPAddress kValidAddress1 = IPAddress::IPv4Localhost();
  const IPAddress kValidAddress2 = IPAddress::IPv6Localhost();

  EXPECT_FALSE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // Set to a valid address.
  impl_.SetLastLocalAddressWhenQuicWorked(kValidAddress1);
  EXPECT_TRUE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_TRUE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // Clear only this value.
  impl_.ClearLastLocalAddressWhenQuicWorked();
  EXPECT_FALSE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // Try another valid address.
  impl_.SetLastLocalAddressWhenQuicWorked(kValidAddress2);
  EXPECT_TRUE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_TRUE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));

  // Clear all values.
  impl_.Clear(base::OnceClosure());
  EXPECT_FALSE(impl_.HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kEmptyAddress));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress1));
  EXPECT_FALSE(impl_.WasLastLocalAddressWhenQuicWorked(kValidAddress2));
}

TEST_F(HttpServerPropertiesTest, LoadServerNetworkStats) {
  url::SchemeHostPort google_server("https", "www.google.com", 443);

  // Check by initializing empty ServerNetworkStats.
  std::unique_ptr<HttpServerProperties::ServerInfoMap> load_server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  impl_.OnServerInfoLoadedForTesting(std::move(load_server_info_map));
  const ServerNetworkStats* stats =
      impl_.GetServerNetworkStats(google_server, NetworkAnonymizationKey());
  EXPECT_EQ(nullptr, stats);

  // Check by initializing with www.google.com:443.
  ServerNetworkStats stats_google;
  stats_google.srtt = base::Microseconds(10);
  stats_google.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(100);
  load_server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();
  load_server_info_map->GetOrPut(CreateSimpleKey(google_server))
      ->second.server_network_stats = stats_google;
  impl_.OnServerInfoLoadedForTesting(std::move(load_server_info_map));

  // Verify data for www.google.com:443.
  ASSERT_EQ(1u, impl_.server_info_map_for_testing().size());
  EXPECT_EQ(stats_google, *(impl_.GetServerNetworkStats(
                              google_server, NetworkAnonymizationKey())));

  // Test recency order and overwriting of data.
  //
  // |docs_server| has a ServerNetworkStats, which will be overwritten by
  // OnServerInfoLoadedForTesting(), because |server_network_stats_map| has an
  // entry for |docs_server|.
  url::SchemeHostPort docs_server("https", "docs.google.com", 443);
  ServerNetworkStats stats_docs;
  stats_docs.srtt = base::Microseconds(20);
  stats_docs.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(200);
  // Recency order will be |docs_server| and |google_server|.
  impl_.SetServerNetworkStats(docs_server, NetworkAnonymizationKey(),
                              stats_docs);

  // Prepare |server_info_map| to be loaded by OnServerInfoLoadedForTesting().
  std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map =
      std::make_unique<HttpServerProperties::ServerInfoMap>();

  // Change the values for |docs_server|.
  ServerNetworkStats new_stats_docs;
  new_stats_docs.srtt = base::Microseconds(25);
  new_stats_docs.bandwidth_estimate =
      quic::QuicBandwidth::FromBitsPerSecond(250);
  server_info_map->GetOrPut(CreateSimpleKey(docs_server))
      ->second.server_network_stats = new_stats_docs;
  // Add data for mail.google.com:443.
  url::SchemeHostPort mail_server("https", "mail.google.com", 443);
  ServerNetworkStats stats_mail;
  stats_mail.srtt = base::Microseconds(30);
  stats_mail.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(300);
  server_info_map->GetOrPut(CreateSimpleKey(mail_server))
      ->second.server_network_stats = stats_mail;

  // Recency order will be |docs_server|, |google_server| and |mail_server|.
  impl_.OnServerInfoLoadedForTesting(std::move(server_info_map));

  const HttpServerProperties::ServerInfoMap& map =
      impl_.server_info_map_for_testing();
  ASSERT_EQ(3u, map.size());
  auto map_it = map.begin();

  EXPECT_EQ(docs_server, map_it->first.server);
  EXPECT_TRUE(map_it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(map_it->second.server_network_stats.has_value());
  EXPECT_EQ(new_stats_docs, *map_it->second.server_network_stats);
  ++map_it;
  EXPECT_EQ(google_server, map_it->first.server);
  EXPECT_TRUE(map_it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(map_it->second.server_network_stats.has_value());
  EXPECT_EQ(stats_google, *map_it->second.server_network_stats);
  ++map_it;
  EXPECT_EQ(mail_server, map_it->first.server);
  EXPECT_TRUE(map_it->first.network_anonymization_key.IsEmpty());
  ASSERT_TRUE(map_it->second.server_network_stats.has_value());
  EXPECT_EQ(stats_mail, *map_it->second.server_network_stats);
}

TEST_F(HttpServerPropertiesTest, SetServerNetworkStats) {
  url::SchemeHostPort foo_http_server("http", "foo", 443);
  url::SchemeHostPort foo_https_server("https", "foo", 443);
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_http_server,
                                                 NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_https_server,
                                                 NetworkAnonymizationKey()));

  ServerNetworkStats stats1;
  stats1.srtt = base::Microseconds(10);
  stats1.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(100);
  impl_.SetServerNetworkStats(foo_http_server, NetworkAnonymizationKey(),
                              stats1);

  const ServerNetworkStats* stats2 =
      impl_.GetServerNetworkStats(foo_http_server, NetworkAnonymizationKey());
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  EXPECT_EQ(100, stats2->bandwidth_estimate.ToBitsPerSecond());
  // Https server should have nothing set for server network stats.
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_https_server,
                                                 NetworkAnonymizationKey()));

  impl_.Clear(base::OnceClosure());
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_http_server,
                                                 NetworkAnonymizationKey()));
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_https_server,
                                                 NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, ClearServerNetworkStats) {
  ServerNetworkStats stats;
  stats.srtt = base::Microseconds(10);
  stats.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(100);
  url::SchemeHostPort foo_https_server("https", "foo", 443);
  impl_.SetServerNetworkStats(foo_https_server, NetworkAnonymizationKey(),
                              stats);

  impl_.ClearServerNetworkStats(foo_https_server, NetworkAnonymizationKey());
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_https_server,
                                                 NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, OnQuicServerInfoMapLoaded) {
  quic::QuicServerId google_quic_server_id("www.google.com", 443);
  HttpServerProperties::QuicServerInfoMapKey google_key(
      google_quic_server_id, PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);

  const int kMaxQuicServerEntries = 10;
  impl_.SetMaxServerConfigsStoredInProperties(kMaxQuicServerEntries);
  EXPECT_EQ(10u, impl_.quic_server_info_map().max_size());

  // Check empty map.
  std::unique_ptr<HttpServerProperties::QuicServerInfoMap>
      init_quic_server_info_map =
          std::make_unique<HttpServerProperties::QuicServerInfoMap>(
              kMaxQuicServerEntries);
  impl_.OnQuicServerInfoMapLoadedForTesting(
      std::move(init_quic_server_info_map));
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());

  // Check by initializing with www.google.com:443.
  std::string google_server_info("google_quic_server_info");
  init_quic_server_info_map =
      std::make_unique<HttpServerProperties::QuicServerInfoMap>(
          kMaxQuicServerEntries);
  init_quic_server_info_map->Put(google_key, google_server_info);
  impl_.OnQuicServerInfoMapLoadedForTesting(
      std::move(init_quic_server_info_map));

  // Verify data for www.google.com:443.
  EXPECT_EQ(1u, impl_.quic_server_info_map().size());
  EXPECT_EQ(google_server_info, *impl_.GetQuicServerInfo(
                                    google_quic_server_id, PRIVACY_MODE_ENABLED,
                                    NetworkAnonymizationKey()));

  // Test recency order and overwriting of data.
  //
  // |docs_server| has a QuicServerInfo, which will be overwritten by
  // SetQuicServerInfoMap(), because |quic_server_info_map| has an
  // entry for |docs_server|.
  quic::QuicServerId docs_quic_server_id("docs.google.com", 443);
  HttpServerProperties::QuicServerInfoMapKey docs_key(
      docs_quic_server_id, PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
  std::string docs_server_info("docs_quic_server_info");
  impl_.SetQuicServerInfo(docs_quic_server_id, PRIVACY_MODE_ENABLED,
                          NetworkAnonymizationKey(), docs_server_info);

  // Recency order will be |docs_server| and |google_server|.
  const HttpServerProperties::QuicServerInfoMap& map =
      impl_.quic_server_info_map();
  ASSERT_EQ(2u, map.size());
  auto map_it = map.begin();
  EXPECT_EQ(map_it->first, docs_key);
  EXPECT_EQ(docs_server_info, map_it->second);
  ++map_it;
  EXPECT_EQ(map_it->first, google_key);
  EXPECT_EQ(google_server_info, map_it->second);

  // Prepare |quic_server_info_map| to be loaded by
  // SetQuicServerInfoMap().
  std::unique_ptr<HttpServerProperties::QuicServerInfoMap>
      quic_server_info_map =
          std::make_unique<HttpServerProperties::QuicServerInfoMap>(
              kMaxQuicServerEntries);
  // Change the values for |docs_server|.
  std::string new_docs_server_info("new_docs_quic_server_info");
  quic_server_info_map->Put(docs_key, new_docs_server_info);
  // Add data for mail.google.com:443.
  quic::QuicServerId mail_quic_server_id("mail.google.com", 443);
  HttpServerProperties::QuicServerInfoMapKey mail_key(
      mail_quic_server_id, PRIVACY_MODE_ENABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
  std::string mail_server_info("mail_quic_server_info");
  quic_server_info_map->Put(mail_key, mail_server_info);
  impl_.OnQuicServerInfoMapLoadedForTesting(std::move(quic_server_info_map));

  // Recency order will be |docs_server|, |google_server| and |mail_server|.
  const HttpServerProperties::QuicServerInfoMap& memory_map =
      impl_.quic_server_info_map();
  ASSERT_EQ(3u, memory_map.size());
  auto memory_map_it = memory_map.begin();
  EXPECT_EQ(memory_map_it->first, docs_key);
  EXPECT_EQ(new_docs_server_info, memory_map_it->second);
  ++memory_map_it;
  EXPECT_EQ(memory_map_it->first, google_key);
  EXPECT_EQ(google_server_info, memory_map_it->second);
  ++memory_map_it;
  EXPECT_EQ(memory_map_it->first, mail_key);
  EXPECT_EQ(mail_server_info, memory_map_it->second);

  // Shrink the size of |quic_server_info_map| and verify the MRU order is
  // maintained.
  impl_.SetMaxServerConfigsStoredInProperties(2);
  EXPECT_EQ(2u, impl_.quic_server_info_map().max_size());

  const HttpServerProperties::QuicServerInfoMap& memory_map1 =
      impl_.quic_server_info_map();
  ASSERT_EQ(2u, memory_map1.size());
  auto memory_map1_it = memory_map1.begin();
  EXPECT_EQ(memory_map1_it->first, docs_key);
  EXPECT_EQ(new_docs_server_info, memory_map1_it->second);
  ++memory_map1_it;
  EXPECT_EQ(memory_map1_it->first, google_key);
  EXPECT_EQ(google_server_info, memory_map1_it->second);
  // |QuicServerInfo| for |mail_quic_server_id| shouldn't be there.
  EXPECT_EQ(nullptr,
            impl_.GetQuicServerInfo(mail_quic_server_id, PRIVACY_MODE_ENABLED,
                                    NetworkAnonymizationKey()));
}

TEST_F(HttpServerPropertiesTest, SetQuicServerInfo) {
  quic::QuicServerId server1("foo", 80);
  quic::QuicServerId server2("foo", 80);

  std::string quic_server_info1("quic_server_info1");
  std::string quic_server_info2("quic_server_info2");
  std::string quic_server_info3("quic_server_info3");

  // Without network isolation keys enabled for HttpServerProperties, passing in
  // a NetworkAnonymizationKey should have no effect on behavior.
  impl_.SetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), quic_server_info1);
  EXPECT_EQ(quic_server_info1,
            *(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                      NetworkAnonymizationKey())));
  EXPECT_FALSE(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                       NetworkAnonymizationKey()));
  EXPECT_EQ(quic_server_info1,
            *(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                      network_anonymization_key1_)));
  EXPECT_FALSE(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                       network_anonymization_key1_));

  impl_.SetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                          network_anonymization_key1_, quic_server_info2);
  EXPECT_EQ(quic_server_info1,
            *(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                      NetworkAnonymizationKey())));
  EXPECT_EQ(quic_server_info2,
            *(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                      NetworkAnonymizationKey())));
  EXPECT_EQ(quic_server_info1,
            *(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                      network_anonymization_key1_)));
  EXPECT_EQ(quic_server_info2,
            *(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                      network_anonymization_key1_)));

  impl_.SetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                          network_anonymization_key1_, quic_server_info3);
  EXPECT_EQ(quic_server_info3,
            *(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                      NetworkAnonymizationKey())));
  EXPECT_EQ(quic_server_info2,
            *(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                      NetworkAnonymizationKey())));
  EXPECT_EQ(quic_server_info3,
            *(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                      network_anonymization_key1_)));
  EXPECT_EQ(quic_server_info2,
            *(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                      network_anonymization_key1_)));

  impl_.Clear(base::OnceClosure());
  EXPECT_FALSE(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                       NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                       NetworkAnonymizationKey()));
  EXPECT_FALSE(impl_.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                       network_anonymization_key1_));
  EXPECT_FALSE(impl_.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                       network_anonymization_key1_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  properties.SetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                               NetworkAnonymizationKey(), quic_server_info1);
  EXPECT_EQ(quic_server_info1,
            *(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                           NetworkAnonymizationKey())));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key1_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            network_anonymization_key1_));

  properties.SetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                               network_anonymization_key1_, quic_server_info2);
  EXPECT_EQ(quic_server_info1,
            *(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                           NetworkAnonymizationKey())));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_EQ(quic_server_info2,
            *(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                           network_anonymization_key1_)));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            network_anonymization_key1_));

  properties.SetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                               network_anonymization_key1_, quic_server_info3);
  EXPECT_EQ(quic_server_info1,
            *(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                           NetworkAnonymizationKey())));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_EQ(quic_server_info2,
            *(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                           network_anonymization_key1_)));
  EXPECT_EQ(quic_server_info3,
            *(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                           network_anonymization_key1_)));

  properties.Clear(base::OnceClosure());
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key1_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_ENABLED,
                                            network_anonymization_key1_));
}

// Tests that GetQuicServerInfo() returns server info of a host
// with the same canonical suffix when there is no exact host match.
TEST_F(HttpServerPropertiesTest, QuicServerInfoCanonicalSuffixMatch) {
  // Set up HttpServerProperties.
  // Add a host with a canonical suffix.
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443);
  std::string foo_server_info("foo_server_info");
  impl_.SetQuicServerInfo(foo_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), foo_server_info);

  // Add a host that has a different canonical suffix.
  quic::QuicServerId baz_server_id("baz.video.com", 443);
  std::string baz_server_info("baz_server_info");
  impl_.SetQuicServerInfo(baz_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), baz_server_info);

  // Create SchemeHostPort with a host that has the initial canonical suffix.
  quic::QuicServerId bar_server_id("bar.googlevideo.com", 443);

  // Check the the server info associated with "foo" is returned for "bar".
  const std::string* bar_server_info = impl_.GetQuicServerInfo(
      bar_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey());
  ASSERT_TRUE(bar_server_info != nullptr);
  EXPECT_EQ(foo_server_info, *bar_server_info);
}

// Make sure that canonical suffices respect NetworkIsolationKeys when using
// QuicServerInfo methods.
TEST_F(HttpServerPropertiesTest,
       QuicServerInfoCanonicalSuffixMatchWithNetworkIsolationKey) {
  // Two servers with same canonical suffix.
  quic::QuicServerId server1("foo.googlevideo.com", 80);
  quic::QuicServerId server2("bar.googlevideo.com", 80);

  std::string server_info1("server_info1");
  std::string server_info2("server_info2");

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  // Since HttpServerProperties caches the feature value, have to create a new
  // one.
  HttpServerProperties properties(nullptr /* pref_delegate */,
                                  nullptr /* net_log */, test_tick_clock_,
                                  &test_clock_);

  // Set QuicServerInfo for one canononical suffix and
  // |network_anonymization_key1_|. It should be accessible via another
  // SchemeHostPort, but only when the NetworkIsolationKeys match.
  properties.SetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                               network_anonymization_key1_, server_info1);
  const std::string* fetched_server_info = properties.GetQuicServerInfo(
      server1, PRIVACY_MODE_DISABLED, network_anonymization_key1_);
  ASSERT_TRUE(fetched_server_info);
  EXPECT_EQ(server_info1, *fetched_server_info);
  fetched_server_info = properties.GetQuicServerInfo(
      server2, PRIVACY_MODE_DISABLED, network_anonymization_key1_);
  ASSERT_TRUE(fetched_server_info);
  EXPECT_EQ(server_info1, *fetched_server_info);
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key2_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key2_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));

  // Set different QuicServerInfo for the same canononical suffix and
  // |network_anonymization_key2_|. Both infos should be retriveable by using
  // the different NetworkIsolationKeys.
  properties.SetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                               network_anonymization_key2_, server_info2);
  fetched_server_info = properties.GetQuicServerInfo(
      server1, PRIVACY_MODE_DISABLED, network_anonymization_key1_);
  ASSERT_TRUE(fetched_server_info);
  EXPECT_EQ(server_info1, *fetched_server_info);
  fetched_server_info = properties.GetQuicServerInfo(
      server2, PRIVACY_MODE_DISABLED, network_anonymization_key1_);
  ASSERT_TRUE(fetched_server_info);
  EXPECT_EQ(server_info1, *fetched_server_info);
  fetched_server_info = properties.GetQuicServerInfo(
      server1, PRIVACY_MODE_DISABLED, network_anonymization_key2_);
  ASSERT_TRUE(fetched_server_info);
  EXPECT_EQ(server_info2, *fetched_server_info);
  fetched_server_info = properties.GetQuicServerInfo(
      server2, PRIVACY_MODE_DISABLED, network_anonymization_key2_);
  ASSERT_TRUE(fetched_server_info);
  EXPECT_EQ(server_info2, *fetched_server_info);
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));

  // Clearing should destroy all information.
  properties.Clear(base::OnceClosure());
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key1_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key1_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key2_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_DISABLED,
                                            network_anonymization_key2_));
  EXPECT_FALSE(properties.GetQuicServerInfo(server1, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));
  EXPECT_FALSE(properties.GetQuicServerInfo(server2, PRIVACY_MODE_DISABLED,
                                            NetworkAnonymizationKey()));
}

// Verifies that GetQuicServerInfo() returns the MRU entry if multiple records
// match a given canonical host.
TEST_F(HttpServerPropertiesTest,
       QuicServerInfoCanonicalSuffixMatchReturnsMruEntry) {
  // Set up HttpServerProperties by adding two hosts with the same canonical
  // suffixes.
  quic::QuicServerId h1_server_id("h1.googlevideo.com", 443);
  std::string h1_server_info("h1_server_info");
  impl_.SetQuicServerInfo(h1_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), h1_server_info);

  quic::QuicServerId h2_server_id("h2.googlevideo.com", 443);
  std::string h2_server_info("h2_server_info");
  impl_.SetQuicServerInfo(h2_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), h2_server_info);

  // Create quic::QuicServerId to use for the search.
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443);

  // Check that 'h2' info is returned since it is MRU.
  const std::string* server_info = impl_.GetQuicServerInfo(
      foo_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey());
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_EQ(h2_server_info, *server_info);

  // Access 'h1' info, so it becomes MRU.
  impl_.GetQuicServerInfo(h1_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey());

  // Check that 'h1' info is returned since it is MRU now.
  server_info = impl_.GetQuicServerInfo(foo_server_id, PRIVACY_MODE_DISABLED,
                                        NetworkAnonymizationKey());
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_EQ(h1_server_info, *server_info);
}

// Verifies that |GetQuicServerInfo| doesn't change the MRU order of the server
// info map when a record is matched based on a canonical name.
TEST_F(HttpServerPropertiesTest,
       QuicServerInfoCanonicalSuffixMatchDoesntChangeOrder) {
  // Add a host with a matching canonical name.
  quic::QuicServerId h1_server_id("h1.googlevideo.com", 443);
  HttpServerProperties::QuicServerInfoMapKey h1_key(
      h1_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
  std::string h1_server_info("h1_server_info");
  impl_.SetQuicServerInfo(h1_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), h1_server_info);

  // Add a host hosts with a non-matching canonical name.
  quic::QuicServerId h2_server_id("h2.video.com", 443);
  HttpServerProperties::QuicServerInfoMapKey h2_key(
      h2_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
  std::string h2_server_info("h2_server_info");
  impl_.SetQuicServerInfo(h2_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), h2_server_info);

  // Check that "h2.video.com" is the MRU entry in the map.
  EXPECT_EQ(h2_key, impl_.quic_server_info_map().begin()->first);

  // Search for the entry that matches the canonical name
  // ("h1.googlevideo.com").
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443);
  const std::string* server_info = impl_.GetQuicServerInfo(
      foo_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey());
  ASSERT_TRUE(server_info != nullptr);

  // Check that the search (although successful) hasn't changed the MRU order of
  // the map.
  EXPECT_EQ(h2_key, impl_.quic_server_info_map().begin()->first);

  // Search for "h1.googlevideo.com" directly, so it becomes MRU
  impl_.GetQuicServerInfo(h1_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey());

  // Check that "h1.googlevideo.com" is the MRU entry now.
  EXPECT_EQ(h1_key, impl_.quic_server_info_map().begin()->first);
}

// Tests that the canonical host matching works for hosts stored in memory cache
// and the ones loaded from persistent storage, i.e. server info added
// using SetQuicServerInfo() and SetQuicServerInfoMap() is taken into
// cosideration when searching for server info for a host with the same
// canonical suffix.
TEST_F(HttpServerPropertiesTest, QuicServerInfoCanonicalSuffixMatchSetInfoMap) {
  // Add a host info using SetQuicServerInfo(). That will simulate an info
  // entry stored in memory cache.
  quic::QuicServerId h1_server_id("h1.googlevideo.com", 443);
  std::string h1_server_info("h1_server_info_memory_cache");
  impl_.SetQuicServerInfo(h1_server_id, PRIVACY_MODE_DISABLED,
                          NetworkAnonymizationKey(), h1_server_info);

  // Prepare a map with host info and add it using SetQuicServerInfoMap(). That
  // will simulate info records read from the persistence storage.
  quic::QuicServerId h2_server_id("h2.googlevideo.com", 443);
  HttpServerProperties::QuicServerInfoMapKey h2_key(
      h2_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
  std::string h2_server_info("h2_server_info_from_disk");

  quic::QuicServerId h3_server_id("h3.ggpht.com", 443);
  HttpServerProperties::QuicServerInfoMapKey h3_key(
      h3_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      false /* use_network_anonymization_key */);
  std::string h3_server_info("h3_server_info_from_disk");

  const int kMaxQuicServerEntries = 10;
  impl_.SetMaxServerConfigsStoredInProperties(kMaxQuicServerEntries);

  std::unique_ptr<HttpServerProperties::QuicServerInfoMap>
      quic_server_info_map =
          std::make_unique<HttpServerProperties::QuicServerInfoMap>(
              kMaxQuicServerEntries);
  quic_server_info_map->Put(h2_key, h2_server_info);
  quic_server_info_map->Put(h3_key, h3_server_info);
  impl_.OnQuicServerInfoMapLoadedForTesting(std::move(quic_server_info_map));

  // Check that the server info from the memory cache is returned since unique
  // entries from the memory cache are added after entries from the
  // persistence storage and, therefore, are most recently used.
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443);
  const std::string* server_info = impl_.GetQuicServerInfo(
      foo_server_id, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey());
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_EQ(h1_server_info, *server_info);

  // Check that server info that was added using SetQuicServerInfoMap() can be
  // found.
  foo_server_id = quic::QuicServerId("foo.ggpht.com", 443);
  server_info = impl_.GetQuicServerInfo(foo_server_id, PRIVACY_MODE_DISABLED,
                                        NetworkAnonymizationKey());
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_EQ(h3_server_info, *server_info);
}

}  // namespace

}  // namespace net
