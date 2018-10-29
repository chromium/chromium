// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/http/http_network_session.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace base {
class ListValue;
}

namespace net {

const base::TimeDelta BROKEN_ALT_SVC_EXPIRE_DELAYS[10] = {
    base::TimeDelta::FromSeconds(300),   base::TimeDelta::FromSeconds(600),
    base::TimeDelta::FromSeconds(1200),  base::TimeDelta::FromSeconds(2400),
    base::TimeDelta::FromSeconds(4800),  base::TimeDelta::FromSeconds(9600),
    base::TimeDelta::FromSeconds(19200), base::TimeDelta::FromSeconds(38400),
    base::TimeDelta::FromSeconds(76800), base::TimeDelta::FromSeconds(153600),
};

class HttpServerPropertiesImplPeer {
 public:
  static void AddBrokenAlternativeServiceWithExpirationTime(
      HttpServerPropertiesImpl* impl,
      const AlternativeService& alternative_service,
      base::TimeTicks when) {
    BrokenAlternativeServiceList::iterator unused_it;
    impl->broken_alternative_services_.AddToBrokenListAndMap(
        alternative_service, when, &unused_it);
    auto it =
        impl->broken_alternative_services_.recently_broken_alternative_services_
            .Get(alternative_service);
    if (it == impl->broken_alternative_services_
                  .recently_broken_alternative_services_.end()) {
      impl->broken_alternative_services_.recently_broken_alternative_services_
          .Put(alternative_service, 1);
    } else {
      it->second++;
    }
  }

  static void ExpireBrokenAlternateProtocolMappings(
      HttpServerPropertiesImpl* impl) {
    impl->broken_alternative_services_.ExpireBrokenAlternateProtocolMappings();
  }
};

namespace {

class HttpServerPropertiesImplTest : public TestWithScopedTaskEnvironment {
 protected:
  HttpServerPropertiesImplTest()
      : TestWithScopedTaskEnvironment(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        test_tick_clock_(GetMockTickClock()),
        impl_(test_tick_clock_, &test_clock_) {
    // Set |test_clock_| to some random time.
    test_clock_.Advance(base::TimeDelta::FromSeconds(12345));
  }

  bool HasAlternativeService(const url::SchemeHostPort& origin) {
    const AlternativeServiceInfoVector alternative_service_info_vector =
        impl_.GetAlternativeServiceInfos(origin);
    return !alternative_service_info_vector.empty();
  }

  bool SetAlternativeService(const url::SchemeHostPort& origin,
                             const AlternativeService& alternative_service) {
    const base::Time expiration =
        test_clock_.Now() + base::TimeDelta::FromDays(1);
    if (alternative_service.protocol == kProtoQUIC) {
      return impl_.SetQuicAlternativeService(
          origin, alternative_service, expiration,
          HttpNetworkSession::Params().quic_supported_versions);
    } else {
      return impl_.SetHttp2AlternativeService(origin, alternative_service,
                                              expiration);
    }
  }

  void MarkBrokenAndLetExpireAlternativeServiceNTimes(
      const AlternativeService& alternative_service,
      int num_times) {}

  const base::TickClock* test_tick_clock_;
  base::SimpleTestClock test_clock_;

  HttpServerPropertiesImpl impl_;
};

typedef HttpServerPropertiesImplTest SpdyServerPropertiesTest;

TEST_F(SpdyServerPropertiesTest, SetWithSchemeHostPort) {
  // Check spdy servers are correctly set with SchemeHostPort key.
  url::SchemeHostPort https_www_server("https", "www.google.com", 443);
  url::SchemeHostPort http_photo_server("http", "photos.google.com", 80);
  // Servers with port equal to default port in scheme will drop port components
  // when calling Serialize().
  std::string spdy_server_g = https_www_server.Serialize();
  std::string spdy_server_p = http_photo_server.Serialize();

  url::SchemeHostPort http_google_server("http", "www.google.com", 443);
  url::SchemeHostPort https_photos_server("https", "photos.google.com", 443);
  url::SchemeHostPort valid_google_server((GURL("https://www.google.com")));

  // Initializing https://www.google.com:443 and https://photos.google.com:443
  // as spdy servers.
  std::unique_ptr<SpdyServersMap> spdy_servers1 =
      std::make_unique<SpdyServersMap>();
  spdy_servers1->Put(spdy_server_g, true);
  spdy_servers1->Put(spdy_server_p, true);
  impl_.SetSpdyServers(std::move(spdy_servers1));
  EXPECT_TRUE(impl_.SupportsRequestPriority(http_photo_server));
  EXPECT_TRUE(impl_.SupportsRequestPriority(https_www_server));
  EXPECT_FALSE(impl_.SupportsRequestPriority(http_google_server));
  EXPECT_FALSE(impl_.SupportsRequestPriority(https_photos_server));
  EXPECT_TRUE(impl_.SupportsRequestPriority(valid_google_server));
}

TEST_F(SpdyServerPropertiesTest, Set) {
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.Serialize();

  url::SchemeHostPort spdy_server_photos("https", "photos.google.com", 443);
  std::string spdy_server_p = spdy_server_photos.Serialize();

  url::SchemeHostPort spdy_server_docs("https", "docs.google.com", 443);
  std::string spdy_server_d = spdy_server_docs.Serialize();

  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);
  std::string spdy_server_m = spdy_server_mail.Serialize();

  // Check by initializing empty spdy servers.
  std::unique_ptr<SpdyServersMap> spdy_servers =
      std::make_unique<SpdyServersMap>();
  impl_.SetSpdyServers(std::move(spdy_servers));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing www.google.com:443 and photos.google.com:443 as spdy
  // servers.
  std::unique_ptr<SpdyServersMap> spdy_servers1 =
      std::make_unique<SpdyServersMap>();
  spdy_servers1->Put(spdy_server_g, true);
  spdy_servers1->Put(spdy_server_p, true);
  impl_.SetSpdyServers(std::move(spdy_servers1));
  // Note: these calls affect MRU order.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_photos));

  // Verify spdy_server_g and spdy_server_d are in the list in MRU order.
  ASSERT_EQ(2U, impl_.spdy_servers_map().size());
  auto it = impl_.spdy_servers_map().begin();
  EXPECT_EQ(spdy_server_p, it->first);
  EXPECT_TRUE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_g, it->first);
  EXPECT_TRUE(it->second);

  // Check by initializing mail.google.com:443 and docs.google.com:443 as spdy
  // servers.
  std::unique_ptr<SpdyServersMap> spdy_servers2 =
      std::make_unique<SpdyServersMap>();
  spdy_servers2->Put(spdy_server_m, true);
  spdy_servers2->Put(spdy_server_d, true);
  impl_.SetSpdyServers(std::move(spdy_servers2));

  // Verify all the servers are in the list in MRU order. Note that
  // SetSpdyServers will put existing spdy server entries in front of newly
  // added entries.
  ASSERT_EQ(4U, impl_.spdy_servers_map().size());
  it = impl_.spdy_servers_map().begin();
  EXPECT_EQ(spdy_server_p, it->first);
  EXPECT_TRUE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_g, it->first);
  EXPECT_TRUE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_d, it->first);
  EXPECT_TRUE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_m, it->first);
  EXPECT_TRUE(it->second);

  // Check these in reverse MRU order so that MRU order stays the same.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_photos));

  // Verify new data that is being initialized overwrites what is already in the
  // memory and also verify the recency list order.
  //
  // Change supports SPDY value for photos and mails servers and order of
  // initalization shouldn't matter.
  std::unique_ptr<SpdyServersMap> spdy_servers3 =
      std::make_unique<SpdyServersMap>();
  spdy_servers3->Put(spdy_server_m, false);
  spdy_servers3->Put(spdy_server_p, false);
  impl_.SetSpdyServers(std::move(spdy_servers3));

  // Verify the entries are in the same order.
  ASSERT_EQ(4U, impl_.spdy_servers_map().size());
  it = impl_.spdy_servers_map().begin();
  EXPECT_EQ(spdy_server_p, it->first);
  EXPECT_FALSE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_g, it->first);
  EXPECT_TRUE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_d, it->first);
  EXPECT_TRUE(it->second);
  ++it;
  EXPECT_EQ(spdy_server_m, it->first);
  EXPECT_FALSE(it->second);

  // Verify photos and mail servers don't support SPDY and other servers support
  // SPDY.
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_photos));
}

TEST_F(SpdyServerPropertiesTest, SupportsRequestPriorityTest) {
  url::SchemeHostPort spdy_server_empty("https", std::string(), 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));

  // Add mail.google.com:443 as not supporting SPDY.
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));

  // Add docs.google.com:443 as supporting SPDY.
  url::SchemeHostPort spdy_server_docs("https", "docs.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_docs, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));

  // Add www.youtube.com:443 as supporting QUIC.
  url::SchemeHostPort youtube_server("https", "www.youtube.com", 443);
  const AlternativeService alternative_service1(kProtoQUIC, "www.youtube.com",
                                                443);
  SetAlternativeService(youtube_server, alternative_service1);
  EXPECT_TRUE(impl_.SupportsRequestPriority(youtube_server));

  // Add www.example.com:443 with two alternative services, one supporting QUIC.
  url::SchemeHostPort example_server("https", "www.example.com", 443);
  const AlternativeService alternative_service2(kProtoHTTP2, "", 443);
  SetAlternativeService(example_server, alternative_service2);
  SetAlternativeService(example_server, alternative_service1);
  EXPECT_TRUE(impl_.SupportsRequestPriority(example_server));

  // Verify all the entries are the same after additions.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
  EXPECT_TRUE(impl_.SupportsRequestPriority(youtube_server));
  EXPECT_TRUE(impl_.SupportsRequestPriority(example_server));
}

TEST_F(SpdyServerPropertiesTest, Clear) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, true);

  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_mail));

  base::RunLoop run_loop;
  bool callback_invoked_ = false;
  impl_.Clear(base::BindOnce(
      [](bool* callback_invoked, base::OnceClosure quit_closure) {
        *callback_invoked = true;
        std::move(quit_closure).Run();
      },
      &callback_invoked_, run_loop.QuitClosure()));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));

  // Callback should be run asynchronously.
  EXPECT_FALSE(callback_invoked_);
  run_loop.Run();
  EXPECT_TRUE(callback_invoked_);
}

TEST_F(SpdyServerPropertiesTest, MRUOfSpdyServersMap) {
  url::SchemeHostPort spdy_server_google("https", "www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.Serialize();
  url::SchemeHostPort spdy_server_mail("https", "mail.google.com", 443);
  std::string spdy_server_m = spdy_server_mail.Serialize();

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  ASSERT_EQ(1u, impl_.spdy_servers_map().size());
  auto it = impl_.spdy_servers_map().begin();
  ASSERT_EQ(spdy_server_g, it->first);

  // Add mail.google.com:443 as supporting SPDY. Verify mail.google.com:443 and
  // www.google.com:443 are in the list.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  ASSERT_EQ(2u, impl_.spdy_servers_map().size());
  it = impl_.spdy_servers_map().begin();
  ASSERT_EQ(spdy_server_m, it->first);
  ++it;
  ASSERT_EQ(spdy_server_g, it->first);

  // Get www.google.com:443. It should become the most-recently-used server.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  ASSERT_EQ(2u, impl_.spdy_servers_map().size());
  it = impl_.spdy_servers_map().begin();
  ASSERT_EQ(spdy_server_g, it->first);
  ++it;
  ASSERT_EQ(spdy_server_m, it->first);
}

typedef HttpServerPropertiesImplTest AlternateProtocolServerPropertiesTest;

TEST_F(AlternateProtocolServerPropertiesTest, Basic) {
  url::SchemeHostPort test_server("http", "foo", 80);
  EXPECT_FALSE(HasAlternativeService(test_server));

  AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service);
  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service,
            alternative_service_info_vector[0].alternative_service());

  impl_.Clear(base::OnceClosure());
  EXPECT_FALSE(HasAlternativeService(test_server));
}

TEST_F(AlternateProtocolServerPropertiesTest, ExcludeOrigin) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
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
          HttpNetworkSession::Params().quic_supported_versions);
  alternative_service_info_vector.push_back(alternative_service_info4);

  url::SchemeHostPort test_server("https", "foo", 443);
  impl_.SetAlternativeServices(test_server, alternative_service_info_vector);

  const AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(3u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service_info2, alternative_service_info_vector2[0]);
  EXPECT_EQ(alternative_service_info3, alternative_service_info_vector2[1]);
  EXPECT_EQ(alternative_service_info4, alternative_service_info_vector2[2]);
}

TEST_F(AlternateProtocolServerPropertiesTest, Set) {
  // |test_server1| has an alternative service, which will not be
  // affected by SetAlternativeServiceServers(), because
  // |alternative_service_map| does not have an entry for
  // |test_server1|.
  url::SchemeHostPort test_server1("http", "foo1", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "bar1", 443);
  const base::Time now = test_clock_.Now();
  base::Time expiration1 = now + base::TimeDelta::FromDays(1);
  // 1st entry in the memory.
  impl_.SetHttp2AlternativeService(test_server1, alternative_service1,
                                   expiration1);

  // |test_server2| has an alternative service, which will be
  // overwritten by SetAlternativeServiceServers(), because
  // |alternative_service_map| has an entry for
  // |test_server2|.
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service2(kProtoHTTP2, "bar2", 443);
  base::Time expiration2 = now + base::TimeDelta::FromDays(2);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration2));
  url::SchemeHostPort test_server2("http", "foo2", 80);
  // 0th entry in the memory.
  impl_.SetAlternativeServices(test_server2, alternative_service_info_vector);

  // Prepare |alternative_service_map| to be loaded by
  // SetAlternativeServiceServers().
  std::unique_ptr<AlternativeServiceMap> alternative_service_map =
      std::make_unique<AlternativeServiceMap>();
  const AlternativeService alternative_service3(kProtoHTTP2, "bar3", 123);
  base::Time expiration3 = now + base::TimeDelta::FromDays(3);
  const AlternativeServiceInfo alternative_service_info1 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service3, expiration3);
  // Simulate updating data for 0th entry with data from Preferences.
  alternative_service_map->Put(
      test_server2,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info1));

  url::SchemeHostPort test_server3("http", "foo3", 80);
  const AlternativeService alternative_service4(kProtoHTTP2, "bar4", 1234);
  base::Time expiration4 = now + base::TimeDelta::FromDays(4);
  const AlternativeServiceInfo alternative_service_info2 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service4, expiration4);
  // Add an old entry from Preferences, this will be added to end of recency
  // list.
  alternative_service_map->Put(
      test_server3,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info2));

  // MRU list will be test_server2, test_server1, test_server3.
  impl_.SetAlternativeServiceServers(std::move(alternative_service_map));

  // Verify alternative_service_map.
  const AlternativeServiceMap& map = impl_.alternative_service_map();
  ASSERT_EQ(3u, map.size());
  auto map_it = map.begin();

  EXPECT_EQ(map_it->first, test_server2);
  ASSERT_EQ(1u, map_it->second.size());
  EXPECT_EQ(alternative_service3, map_it->second[0].alternative_service());
  EXPECT_EQ(expiration3, map_it->second[0].expiration());
  ++map_it;
  EXPECT_EQ(map_it->first, test_server1);
  ASSERT_EQ(1u, map_it->second.size());
  EXPECT_EQ(alternative_service1, map_it->second[0].alternative_service());
  EXPECT_EQ(expiration1, map_it->second[0].expiration());
  ++map_it;
  EXPECT_EQ(map_it->first, test_server3);
  ASSERT_EQ(1u, map_it->second.size());
  EXPECT_EQ(alternative_service4, map_it->second[0].alternative_service());
  EXPECT_EQ(expiration4, map_it->second[0].expiration());
}

// Regression test for https://crbug.com/504032:
// SetAlternativeServiceServers() should not crash if there is an empty
// hostname is the mapping.
TEST_F(AlternateProtocolServerPropertiesTest, SetWithEmptyHostname) {
  url::SchemeHostPort server("https", "foo", 443);
  const AlternativeService alternative_service_with_empty_hostname(kProtoHTTP2,
                                                                   "", 1234);
  const AlternativeService alternative_service_with_foo_hostname(kProtoHTTP2,
                                                                 "foo", 1234);
  SetAlternativeService(server, alternative_service_with_empty_hostname);
  impl_.MarkAlternativeServiceBroken(alternative_service_with_foo_hostname);

  std::unique_ptr<AlternativeServiceMap> alternative_service_map =
      std::make_unique<AlternativeServiceMap>();
  impl_.SetAlternativeServiceServers(std::move(alternative_service_map));

  EXPECT_TRUE(
      impl_.IsAlternativeServiceBroken(alternative_service_with_foo_hostname));
  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service_with_foo_hostname,
            alternative_service_info_vector[0].alternative_service());
}

// Regression test for https://crbug.com/516486:
// GetAlternativeServiceInfos() should remove |alternative_service_map_|
// elements with empty value.
TEST_F(AlternateProtocolServerPropertiesTest, EmptyVector) {
  url::SchemeHostPort server("https", "foo", 443);
  const AlternativeService alternative_service(kProtoHTTP2, "bar", 443);
  base::Time expiration = test_clock_.Now() - base::TimeDelta::FromDays(1);
  const AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration);
  std::unique_ptr<AlternativeServiceMap> alternative_service_map =
      std::make_unique<AlternativeServiceMap>();
  alternative_service_map->Put(
      server,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // Prepare |alternative_service_map_| with a single key that has a single
  // AlternativeServiceInfo with identical hostname and port.
  impl_.SetAlternativeServiceServers(std::move(alternative_service_map));

  // GetAlternativeServiceInfos() should remove such AlternativeServiceInfo from
  // |alternative_service_map_|, emptying the AlternativeServiceInfoVector
  // corresponding to |server|.
  ASSERT_TRUE(impl_.GetAlternativeServiceInfos(server).empty());

  // GetAlternativeServiceInfos() should remove this key from
  // |alternative_service_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(
      server,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // There should still be no alternative service assigned to |server|.
  ASSERT_TRUE(impl_.GetAlternativeServiceInfos(server).empty());
}

// Regression test for https://crbug.com/516486 for the canonical host case.
TEST_F(AlternateProtocolServerPropertiesTest, EmptyVectorForCanonical) {
  url::SchemeHostPort server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  const AlternativeService alternative_service(kProtoHTTP2, "", 443);
  base::Time expiration = test_clock_.Now() - base::TimeDelta::FromDays(1);
  const AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration);
  std::unique_ptr<AlternativeServiceMap> alternative_service_map =
      std::make_unique<AlternativeServiceMap>();
  alternative_service_map->Put(
      canonical_server,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // Prepare |alternative_service_map_| with a single key that has a single
  // AlternativeServiceInfo with identical hostname and port.
  impl_.SetAlternativeServiceServers(std::move(alternative_service_map));

  // GetAlternativeServiceInfos() should remove such AlternativeServiceInfo from
  // |alternative_service_map_|, emptying the AlternativeServiceInfoVector
  // corresponding to |canonical_server|, even when looking up
  // alternative services for |server|.
  ASSERT_TRUE(impl_.GetAlternativeServiceInfos(server).empty());

  // GetAlternativeServiceInfos() should remove this key from
  // |alternative_service_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(
      canonical_server,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // There should still be no alternative service assigned to
  // |canonical_server|.
  ASSERT_TRUE(impl_.GetAlternativeServiceInfos(canonical_server).empty());
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearServerWithCanonical) {
  url::SchemeHostPort server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  const AlternativeService alternative_service(kProtoQUIC, "", 443);
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
  const AlternativeServiceInfo alternative_service_info =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration,
          HttpNetworkSession::Params().quic_supported_versions);

  impl_.SetAlternativeServices(
      canonical_server,
      AlternativeServiceInfoVector(/*size=*/1, alternative_service_info));

  // Make sure the canonical service is returned for the other server.
  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(kProtoQUIC,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ(443, alternative_service_info_vector[0].alternative_service().port);

  // Now clear the alternatives for the other server and make sure it stays
  // cleared.
  // GetAlternativeServices() should remove this key from
  // |alternative_service_map_|, and SetAlternativeServices() should not crash.
  impl_.SetAlternativeServices(server, AlternativeServiceInfoVector());

  ASSERT_TRUE(impl_.GetAlternativeServiceInfos(server).empty());
}

TEST_F(AlternateProtocolServerPropertiesTest, MRUOfGetAlternativeServiceInfos) {
  url::SchemeHostPort test_server1("http", "foo1", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "foo1", 443);
  SetAlternativeService(test_server1, alternative_service1);
  url::SchemeHostPort test_server2("http", "foo2", 80);
  const AlternativeService alternative_service2(kProtoHTTP2, "foo2", 1234);
  SetAlternativeService(test_server2, alternative_service2);

  const AlternativeServiceMap& map = impl_.alternative_service_map();
  auto it = map.begin();
  EXPECT_EQ(it->first, test_server2);
  ASSERT_EQ(1u, it->second.size());
  EXPECT_EQ(alternative_service2, it->second[0].alternative_service());

  const AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server1);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());

  // GetAlternativeServices should reorder the AlternateProtocol map.
  it = map.begin();
  EXPECT_EQ(it->first, test_server1);
  ASSERT_EQ(1u, it->second.size());
  EXPECT_EQ(alternative_service1, it->second[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, SetBroken) {
  url::SchemeHostPort test_server("http", "foo", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service1);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1));

  // GetAlternativeServiceInfos should return the broken alternative service.
  impl_.MarkAlternativeServiceBroken(alternative_service1);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));

  // SetAlternativeServices should add a broken alternative service to the map.
  AlternativeServiceInfoVector alternative_service_info_vector2;
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "foo", 1234);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  impl_.SetAlternativeServices(test_server, alternative_service_info_vector2);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(2u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector[1].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2));

  // SetAlternativeService should add a broken alternative service to the map.
  SetAlternativeService(test_server, alternative_service1);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       SetBrokenUntilDefaultNetworkChanges) {
  url::SchemeHostPort test_server("http", "foo", 80);
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service1);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1));

  // Mark the alternative service as broken until the default network changes.
  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service1);
  // The alternative service should be persisted and marked as broken.
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));

  // SetAlternativeServices should add a broken alternative service to the map.
  AlternativeServiceInfoVector alternative_service_info_vector2;
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "foo", 1234);
  alternative_service_info_vector2.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  impl_.SetAlternativeServices(test_server, alternative_service_info_vector2);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(2u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector[1].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2));

  // SetAlternativeService should add a broken alternative service to the map.
  SetAlternativeService(test_server, alternative_service1);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
}

TEST_F(AlternateProtocolServerPropertiesTest, MaxAge) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time now = test_clock_.Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

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
  impl_.SetAlternativeServices(test_server, alternative_service_info_vector);

  AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector2[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, MaxAgeCanonical) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  base::Time now = test_clock_.Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

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
  impl_.SetAlternativeServices(canonical_server,
                               alternative_service_info_vector);

  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector2[0].alternative_service());
}

TEST_F(AlternateProtocolServerPropertiesTest, AlternativeServiceWithScheme) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  // Set Alt-Svc list for |http_server|.
  url::SchemeHostPort http_server("http", "foo", 80);
  impl_.SetAlternativeServices(http_server, alternative_service_info_vector);

  const net::AlternativeServiceMap& map = impl_.alternative_service_map();
  auto it = map.begin();
  EXPECT_EQ(it->first, http_server);
  ASSERT_EQ(2u, it->second.size());
  EXPECT_EQ(alternative_service1, it->second[0].alternative_service());
  EXPECT_EQ(alternative_service2, it->second[1].alternative_service());

  // Check Alt-Svc list should not be set for |https_server|.
  url::SchemeHostPort https_server("https", "foo", 80);
  EXPECT_EQ(0u, impl_.GetAlternativeServiceInfos(https_server).size());

  // Set Alt-Svc list for |https_server|.
  impl_.SetAlternativeServices(https_server, alternative_service_info_vector);
  EXPECT_EQ(2u, impl_.GetAlternativeServiceInfos(https_server).size());
  EXPECT_EQ(2u, impl_.GetAlternativeServiceInfos(http_server).size());

  // Clear Alt-Svc list for |http_server|.
  impl_.SetAlternativeServices(http_server, AlternativeServiceInfoVector());

  EXPECT_EQ(0u, impl_.GetAlternativeServiceInfos(http_server).size());
  EXPECT_EQ(2u, impl_.GetAlternativeServiceInfos(https_server).size());
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearAlternativeServices) {
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service1(kProtoHTTP2, "foo", 443);
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, expiration));
  const AlternativeService alternative_service2(kProtoHTTP2, "bar", 1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service2, expiration));
  url::SchemeHostPort test_server("http", "foo", 80);
  impl_.SetAlternativeServices(test_server, alternative_service_info_vector);

  const net::AlternativeServiceMap& map = impl_.alternative_service_map();
  auto it = map.begin();
  EXPECT_EQ(it->first, test_server);
  ASSERT_EQ(2u, it->second.size());
  EXPECT_EQ(alternative_service1, it->second[0].alternative_service());
  EXPECT_EQ(alternative_service2, it->second[1].alternative_service());

  impl_.SetAlternativeServices(test_server, AlternativeServiceInfoVector());
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
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(canonical_alternative_service,
            alternative_service_info_vector[0].alternative_service());

  const AlternativeService broken_alternative_service(kProtoHTTP2, "foo", 443);
  impl_.MarkAlternativeServiceBroken(broken_alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(broken_alternative_service));

  SetAlternativeService(test_server, broken_alternative_service);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(test_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(broken_alternative_service,
            alternative_service_info_vector[0].alternative_service());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(broken_alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearBroken) {
  url::SchemeHostPort test_server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(test_server, alternative_service);
  impl_.MarkAlternativeServiceBroken(alternative_service);
  ASSERT_TRUE(HasAlternativeService(test_server));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  // SetAlternativeServices should leave a broken alternative service marked
  // as such.
  impl_.SetAlternativeServices(test_server, AlternativeServiceInfoVector());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, MarkRecentlyBroken) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(server, alternative_service);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceRecentlyBroken(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.ConfirmAlternativeService(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       MarkBrokenUntilDefaultNetworkChanges) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);
  SetAlternativeService(server, alternative_service);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.ConfirmAlternativeService(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, OnDefaultNetworkChanged) {
  url::SchemeHostPort server("http", "foo", 80);
  const AlternativeService alternative_service(kProtoHTTP2, "foo", 443);

  SetAlternativeService(server, alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  // Default network change clears alt svc broken until default network changes.
  EXPECT_TRUE(impl_.OnDefaultNetworkChanged());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceBroken(alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  // Default network change doesn't affect alt svc that was simply marked broken
  // most recently.
  EXPECT_FALSE(impl_.OnDefaultNetworkChanged());
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  // Default network change clears alt svc that was marked broken until default
  // network change most recently even if the alt svc was initially marked
  // broken.
  EXPECT_TRUE(impl_.OnDefaultNetworkChanged());
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, Canonical) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  EXPECT_FALSE(HasAlternativeService(test_server));

  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  EXPECT_FALSE(HasAlternativeService(canonical_server));

  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService canonical_alternative_service1(
      kProtoQUIC, "bar.c.youtube.com", 1234);
  base::Time expiration = test_clock_.Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          canonical_alternative_service1, expiration,
          HttpNetworkSession::Params().quic_supported_versions));
  const AlternativeService canonical_alternative_service2(kProtoHTTP2, "", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          canonical_alternative_service2, expiration));
  impl_.SetAlternativeServices(canonical_server,
                               alternative_service_info_vector);

  // Since |test_server| does not have an alternative service itself,
  // GetAlternativeServiceInfos should return those of |canonical_server|.
  AlternativeServiceInfoVector alternative_service_info_vector2 =
      impl_.GetAlternativeServiceInfos(test_server);
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
  EXPECT_EQ(".c.youtube.com", *impl_.GetCanonicalSuffix(test_server.host()));
  EXPECT_EQ(".c.youtube.com",
            *impl_.GetCanonicalSuffix(canonical_server.host()));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearCanonical) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  impl_.SetAlternativeServices(canonical_server,
                               AlternativeServiceInfoVector());
  EXPECT_FALSE(HasAlternativeService(test_server));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalBroken) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  EXPECT_TRUE(HasAlternativeService(test_server));
  impl_.MarkAlternativeServiceBroken(canonical_alternative_service);
  EXPECT_FALSE(HasAlternativeService(test_server));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       CanonicalBrokenUntilDefaultNetworkChanges) {
  url::SchemeHostPort test_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort canonical_server("https", "bar.c.youtube.com", 443);
  AlternativeService canonical_alternative_service(kProtoQUIC,
                                                   "bar.c.youtube.com", 1234);

  SetAlternativeService(canonical_server, canonical_alternative_service);
  EXPECT_TRUE(HasAlternativeService(test_server));
  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      canonical_alternative_service);
  EXPECT_FALSE(HasAlternativeService(test_server));
}

// Adding an alternative service for a new host overrides canonical host.
TEST_F(AlternateProtocolServerPropertiesTest, CanonicalOverride) {
  url::SchemeHostPort foo_server("https", "foo.c.youtube.com", 443);
  url::SchemeHostPort bar_server("https", "bar.c.youtube.com", 443);
  AlternativeService bar_alternative_service(kProtoQUIC, "bar.c.youtube.com",
                                             1234);
  SetAlternativeService(bar_server, bar_alternative_service);
  AlternativeServiceInfoVector alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(foo_server);
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(bar_alternative_service,
            alternative_service_info_vector[0].alternative_service());

  url::SchemeHostPort qux_server("https", "qux.c.youtube.com", 443);
  AlternativeService qux_alternative_service(kProtoQUIC, "qux.c.youtube.com",
                                             443);
  SetAlternativeService(qux_server, qux_alternative_service);
  alternative_service_info_vector =
      impl_.GetAlternativeServiceInfos(foo_server);
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
  EXPECT_FALSE(HasAlternativeService(test_server));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       ExpireBrokenAlternateProtocolMappings) {
  url::SchemeHostPort server("https", "foo", 443);
  AlternativeService alternative_service(kProtoQUIC, "foo", 443);
  SetAlternativeService(server, alternative_service);
  EXPECT_TRUE(HasAlternativeService(server));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  base::TimeTicks past =
      test_tick_clock_->NowTicks() - base::TimeDelta::FromSeconds(42);
  HttpServerPropertiesImplPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &impl_, alternative_service, past);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  HttpServerPropertiesImplPeer::ExpireBrokenAlternateProtocolMappings(&impl_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

// Regression test for https://crbug.com/505413.
TEST_F(AlternateProtocolServerPropertiesTest, RemoveExpiredBrokenAltSvc) {
  url::SchemeHostPort foo_server("https", "foo", 443);
  AlternativeService bar_alternative_service(kProtoQUIC, "bar", 443);
  SetAlternativeService(foo_server, bar_alternative_service);
  EXPECT_TRUE(HasAlternativeService(foo_server));

  url::SchemeHostPort bar_server1("http", "bar", 80);
  AlternativeService nohost_alternative_service(kProtoQUIC, "", 443);
  SetAlternativeService(bar_server1, nohost_alternative_service);
  EXPECT_TRUE(HasAlternativeService(bar_server1));

  url::SchemeHostPort bar_server2("https", "bar", 443);
  AlternativeService baz_alternative_service(kProtoQUIC, "baz", 1234);
  SetAlternativeService(bar_server2, baz_alternative_service);
  EXPECT_TRUE(HasAlternativeService(bar_server2));

  // Mark "bar:443" as broken.
  base::TimeTicks past =
      test_tick_clock_->NowTicks() - base::TimeDelta::FromSeconds(42);
  HttpServerPropertiesImplPeer::AddBrokenAlternativeServiceWithExpirationTime(
      &impl_, bar_alternative_service, past);

  // Expire brokenness of "bar:443".
  HttpServerPropertiesImplPeer::ExpireBrokenAlternateProtocolMappings(&impl_);

  // "foo:443" should have no alternative service now.
  EXPECT_FALSE(HasAlternativeService(foo_server));
  // "bar:80" should have no alternative service now.
  EXPECT_FALSE(HasAlternativeService(bar_server1));
  // The alternative service of "bar:443" should be unaffected.
  EXPECT_TRUE(HasAlternativeService(bar_server2));

  EXPECT_TRUE(
      impl_.WasAlternativeServiceRecentlyBroken(bar_alternative_service));
  EXPECT_FALSE(
      impl_.WasAlternativeServiceRecentlyBroken(baz_alternative_service));
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
    impl_.MarkAlternativeServiceBroken(alternative_service1);

    // |impl_| should have posted task to expire the brokenness of
    // |alternative_service1|
    EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
    EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));

    // Advance time by just enough so that |alternative_service1|'s brokenness
    // expires.
    FastForwardBy(BROKEN_ALT_SVC_EXPIRE_DELAYS[i]);

    // Ensure brokenness of |alternative_service1| has expired.
    EXPECT_FALSE(MainThreadHasPendingTask());
    EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1));
  }

  impl_.MarkAlternativeServiceBroken(alternative_service1);
  impl_.MarkAlternativeServiceBroken(alternative_service2);

  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service2));

  // Advance time by just enough so that |alternative_service2|'s brokennness
  // expires.
  FastForwardBy(BROKEN_ALT_SVC_EXPIRE_DELAYS[0]);

  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2));

  // Advance time by enough so that |alternative_service1|'s brokenness expires.
  FastForwardBy(BROKEN_ALT_SVC_EXPIRE_DELAYS[3] -
                BROKEN_ALT_SVC_EXPIRE_DELAYS[0]);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service1));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       GetAlternativeServiceInfoAsValue) {
  base::Time::Exploded now_exploded;
  now_exploded.year = 2018;
  now_exploded.month = 1;
  now_exploded.day_of_week = 3;
  now_exploded.day_of_month = 24;
  now_exploded.hour = 15;
  now_exploded.minute = 12;
  now_exploded.second = 53;
  now_exploded.millisecond = 0;
  base::Time now;
  bool result = base::Time::FromLocalExploded(now_exploded, &now);
  DCHECK(result);
  test_clock_.SetNow(now);

  AlternativeServiceInfoVector alternative_service_info_vector;
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo", 443),
          now + base::TimeDelta::FromMinutes(1)));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(kProtoQUIC, "bar", 443),
          now + base::TimeDelta::FromHours(1),
          HttpNetworkSession::Params().quic_supported_versions));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(kProtoQUIC, "baz", 443),
          now + base::TimeDelta::FromHours(1),
          HttpNetworkSession::Params().quic_supported_versions));

  impl_.SetAlternativeServices(url::SchemeHostPort("https", "youtube.com", 443),
                               alternative_service_info_vector);

  impl_.MarkAlternativeServiceBroken(
      AlternativeService(kProtoQUIC, "bar", 443));

  impl_.MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      AlternativeService(kProtoQUIC, "baz", 443));

  alternative_service_info_vector.clear();
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foo2", 443),
          now + base::TimeDelta::FromDays(1)));
  impl_.SetAlternativeServices(url::SchemeHostPort("http", "test.com", 80),
                               alternative_service_info_vector);

  const char expected_json[] =
      "["
      "{"
      "\"alternative_service\":"
      "[\"h2 foo2:443, expires 2018-01-25 15:12:53\"],"
      "\"server\":\"http://test.com\""
      "},"
      "{"
      "\"alternative_service\":"
      "[\"h2 foo:443, expires 2018-01-24 15:13:53\","
      "\"quic bar:443, expires 2018-01-24 16:12:53"
      " (broken until 2018-01-24 15:17:53)\","
      "\"quic baz:443, expires 2018-01-24 16:12:53"
      " (broken until 2018-01-24 15:17:53)\"],"
      "\"server\":\"https://youtube.com\""
      "}"
      "]";

  std::unique_ptr<base::Value> alternative_service_info_value =
      impl_.GetAlternativeServiceInfoAsValue();
  std::string alternative_service_info_json;
  base::JSONWriter::Write(*alternative_service_info_value,
                          &alternative_service_info_json);
  EXPECT_EQ(expected_json, alternative_service_info_json);
}

typedef HttpServerPropertiesImplTest SupportsQuicServerPropertiesTest;

TEST_F(SupportsQuicServerPropertiesTest, Set) {
  HostPortPair quic_server_google("www.google.com", 443);

  // Check by initializing empty address.
  IPAddress initial_address;
  impl_.SetSupportsQuic(initial_address);

  IPAddress address;
  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
  EXPECT_TRUE(address.empty());

  // Check by initializing with a valid address.
  initial_address = IPAddress::IPv4Localhost();
  impl_.SetSupportsQuic(initial_address);

  EXPECT_TRUE(impl_.GetSupportsQuic(&address));
  EXPECT_EQ(initial_address, address);
}

TEST_F(SupportsQuicServerPropertiesTest, SetSupportsQuic) {
  IPAddress address;
  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
  EXPECT_TRUE(address.empty());

  IPAddress actual_address(127, 0, 0, 1);
  impl_.SetSupportsQuic(true, actual_address);

  EXPECT_TRUE(impl_.GetSupportsQuic(&address));
  EXPECT_EQ(actual_address, address);

  impl_.Clear(base::OnceClosure());

  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
}

typedef HttpServerPropertiesImplTest ServerNetworkStatsServerPropertiesTest;

TEST_F(ServerNetworkStatsServerPropertiesTest, Set) {
  url::SchemeHostPort google_server("https", "www.google.com", 443);

  // Check by initializing empty ServerNetworkStats.
  std::unique_ptr<ServerNetworkStatsMap> init_server_network_stats_map =
      std::make_unique<ServerNetworkStatsMap>();
  impl_.SetServerNetworkStats(std::move(init_server_network_stats_map));
  const ServerNetworkStats* stats = impl_.GetServerNetworkStats(google_server);
  EXPECT_EQ(NULL, stats);

  // Check by initializing with www.google.com:443.
  ServerNetworkStats stats_google;
  stats_google.srtt = base::TimeDelta::FromMicroseconds(10);
  stats_google.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(100);
  init_server_network_stats_map = std::make_unique<ServerNetworkStatsMap>();
  init_server_network_stats_map->Put(google_server, stats_google);
  impl_.SetServerNetworkStats(std::move(init_server_network_stats_map));

  // Verify data for www.google.com:443.
  ASSERT_EQ(1u, impl_.server_network_stats_map().size());
  EXPECT_EQ(stats_google, *(impl_.GetServerNetworkStats(google_server)));

  // Test recency order and overwriting of data.
  //
  // |docs_server| has a ServerNetworkStats, which will be overwritten by
  // SetServerNetworkStats(), because |server_network_stats_map| has an
  // entry for |docs_server|.
  url::SchemeHostPort docs_server("https", "docs.google.com", 443);
  ServerNetworkStats stats_docs;
  stats_docs.srtt = base::TimeDelta::FromMicroseconds(20);
  stats_docs.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(200);
  // Recency order will be |docs_server| and |google_server|.
  impl_.SetServerNetworkStats(docs_server, stats_docs);

  // Prepare |server_network_stats_map| to be loaded by
  // SetServerNetworkStats().
  std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map =
      std::make_unique<ServerNetworkStatsMap>();

  // Change the values for |docs_server|.
  ServerNetworkStats new_stats_docs;
  new_stats_docs.srtt = base::TimeDelta::FromMicroseconds(25);
  new_stats_docs.bandwidth_estimate =
      quic::QuicBandwidth::FromBitsPerSecond(250);
  server_network_stats_map->Put(docs_server, new_stats_docs);
  // Add data for mail.google.com:443.
  url::SchemeHostPort mail_server("https", "mail.google.com", 443);
  ServerNetworkStats stats_mail;
  stats_mail.srtt = base::TimeDelta::FromMicroseconds(30);
  stats_mail.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(300);
  server_network_stats_map->Put(mail_server, stats_mail);

  // Recency order will be |docs_server|, |google_server| and |mail_server|.
  impl_.SetServerNetworkStats(std::move(server_network_stats_map));

  const ServerNetworkStatsMap& map = impl_.server_network_stats_map();
  ASSERT_EQ(3u, map.size());
  auto map_it = map.begin();

  EXPECT_EQ(map_it->first, docs_server);
  EXPECT_EQ(new_stats_docs, map_it->second);
  ++map_it;
  EXPECT_EQ(map_it->first, google_server);
  EXPECT_EQ(stats_google, map_it->second);
  ++map_it;
  EXPECT_EQ(map_it->first, mail_server);
  EXPECT_EQ(stats_mail, map_it->second);
}

TEST_F(ServerNetworkStatsServerPropertiesTest, SetServerNetworkStats) {
  url::SchemeHostPort foo_http_server("http", "foo", 443);
  url::SchemeHostPort foo_https_server("https", "foo", 443);
  EXPECT_EQ(NULL, impl_.GetServerNetworkStats(foo_http_server));
  EXPECT_EQ(NULL, impl_.GetServerNetworkStats(foo_https_server));

  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  stats1.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(100);
  impl_.SetServerNetworkStats(foo_http_server, stats1);

  const ServerNetworkStats* stats2 =
      impl_.GetServerNetworkStats(foo_http_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  EXPECT_EQ(100, stats2->bandwidth_estimate.ToBitsPerSecond());
  // Https server should have nothing set for server network stats.
  EXPECT_EQ(NULL, impl_.GetServerNetworkStats(foo_https_server));

  impl_.Clear(base::OnceClosure());
  EXPECT_EQ(NULL, impl_.GetServerNetworkStats(foo_http_server));
  EXPECT_EQ(NULL, impl_.GetServerNetworkStats(foo_https_server));
}

TEST_F(ServerNetworkStatsServerPropertiesTest, ClearServerNetworkStats) {
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMicroseconds(10);
  stats.bandwidth_estimate = quic::QuicBandwidth::FromBitsPerSecond(100);
  url::SchemeHostPort foo_https_server("https", "foo", 443);
  impl_.SetServerNetworkStats(foo_https_server, stats);

  impl_.ClearServerNetworkStats(foo_https_server);
  EXPECT_EQ(nullptr, impl_.GetServerNetworkStats(foo_https_server));
}

typedef HttpServerPropertiesImplTest QuicServerInfoServerPropertiesTest;

TEST_F(QuicServerInfoServerPropertiesTest, Set) {
  quic::QuicServerId google_quic_server_id("www.google.com", 443, true);

  const int kMaxQuicServerEntries = 10;
  impl_.SetMaxServerConfigsStoredInProperties(kMaxQuicServerEntries);
  EXPECT_EQ(10u, impl_.quic_server_info_map().max_size());

  // Check empty map.
  std::unique_ptr<QuicServerInfoMap> init_quic_server_info_map =
      std::make_unique<QuicServerInfoMap>(kMaxQuicServerEntries);
  impl_.SetQuicServerInfoMap(std::move(init_quic_server_info_map));
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());

  // Check by initializing with www.google.com:443.
  std::string google_server_info("google_quic_server_info");
  init_quic_server_info_map =
      std::make_unique<QuicServerInfoMap>(kMaxQuicServerEntries);
  init_quic_server_info_map->Put(google_quic_server_id, google_server_info);
  impl_.SetQuicServerInfoMap(std::move(init_quic_server_info_map));

  // Verify data for www.google.com:443.
  EXPECT_EQ(1u, impl_.quic_server_info_map().size());
  EXPECT_EQ(google_server_info,
            *impl_.GetQuicServerInfo(google_quic_server_id));

  // Test recency order and overwriting of data.
  //
  // |docs_server| has a QuicServerInfo, which will be overwritten by
  // SetQuicServerInfoMap(), because |quic_server_info_map| has an
  // entry for |docs_server|.
  quic::QuicServerId docs_quic_server_id("docs.google.com", 443, true);
  std::string docs_server_info("docs_quic_server_info");
  impl_.SetQuicServerInfo(docs_quic_server_id, docs_server_info);

  // Recency order will be |docs_server| and |google_server|.
  const QuicServerInfoMap& map = impl_.quic_server_info_map();
  ASSERT_EQ(2u, map.size());
  auto map_it = map.begin();
  EXPECT_EQ(map_it->first, docs_quic_server_id);
  EXPECT_EQ(docs_server_info, map_it->second);
  ++map_it;
  EXPECT_EQ(map_it->first, google_quic_server_id);
  EXPECT_EQ(google_server_info, map_it->second);

  // Prepare |quic_server_info_map| to be loaded by
  // SetQuicServerInfoMap().
  std::unique_ptr<QuicServerInfoMap> quic_server_info_map =
      std::make_unique<QuicServerInfoMap>(kMaxQuicServerEntries);
  // Change the values for |docs_server|.
  std::string new_docs_server_info("new_docs_quic_server_info");
  quic_server_info_map->Put(docs_quic_server_id, new_docs_server_info);
  // Add data for mail.google.com:443.
  quic::QuicServerId mail_quic_server_id("mail.google.com", 443, true);
  std::string mail_server_info("mail_quic_server_info");
  quic_server_info_map->Put(mail_quic_server_id, mail_server_info);
  impl_.SetQuicServerInfoMap(std::move(quic_server_info_map));

  // Recency order will be |docs_server|, |google_server| and |mail_server|.
  const QuicServerInfoMap& memory_map = impl_.quic_server_info_map();
  ASSERT_EQ(3u, memory_map.size());
  auto memory_map_it = memory_map.begin();
  EXPECT_EQ(memory_map_it->first, docs_quic_server_id);
  EXPECT_EQ(new_docs_server_info, memory_map_it->second);
  ++memory_map_it;
  EXPECT_EQ(memory_map_it->first, google_quic_server_id);
  EXPECT_EQ(google_server_info, memory_map_it->second);
  ++memory_map_it;
  EXPECT_EQ(memory_map_it->first, mail_quic_server_id);
  EXPECT_EQ(mail_server_info, memory_map_it->second);

  // Shrink the size of |quic_server_info_map| and verify the MRU order is
  // maintained.
  impl_.SetMaxServerConfigsStoredInProperties(2);
  EXPECT_EQ(2u, impl_.quic_server_info_map().max_size());

  const QuicServerInfoMap& memory_map1 = impl_.quic_server_info_map();
  ASSERT_EQ(2u, memory_map1.size());
  auto memory_map1_it = memory_map1.begin();
  EXPECT_EQ(memory_map1_it->first, docs_quic_server_id);
  EXPECT_EQ(new_docs_server_info, memory_map1_it->second);
  ++memory_map1_it;
  EXPECT_EQ(memory_map1_it->first, google_quic_server_id);
  EXPECT_EQ(google_server_info, memory_map1_it->second);
  // |QuicServerInfo| for |mail_quic_server_id| shouldn't be there.
  EXPECT_EQ(nullptr, impl_.GetQuicServerInfo(mail_quic_server_id));
}

TEST_F(QuicServerInfoServerPropertiesTest, SetQuicServerInfo) {
  quic::QuicServerId quic_server_id("foo", 80, true);
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());

  std::string quic_server_info1("quic_server_info1");
  impl_.SetQuicServerInfo(quic_server_id, quic_server_info1);

  EXPECT_EQ(1u, impl_.quic_server_info_map().size());
  EXPECT_EQ(quic_server_info1, *(impl_.GetQuicServerInfo(quic_server_id)));

  impl_.Clear(base::OnceClosure());
  EXPECT_EQ(0u, impl_.quic_server_info_map().size());
  EXPECT_EQ(nullptr, impl_.GetQuicServerInfo(quic_server_id));
}

// Tests that GetQuicServerInfo() returns server info of a host
// with the same canonical suffix when there is no exact host match.
TEST_F(QuicServerInfoServerPropertiesTest, TestCanonicalSuffixMatch) {
  // Set up HttpServerProperties.
  // Add a host that has the same canonical suffix.
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443, false);
  std::string foo_server_info("foo_server_info");
  impl_.SetQuicServerInfo(foo_server_id, foo_server_info);

  // Add a host that has a different canonical suffix.
  quic::QuicServerId baz_server_id("baz.video.com", 443, false);
  std::string baz_server_info("baz_server_info");
  impl_.SetQuicServerInfo(baz_server_id, baz_server_info);

  // Create quic::QuicServerId with a host that has the same canonical suffix.
  quic::QuicServerId bar_server_id("bar.googlevideo.com", 443, false);

  // Check the the server info associated with "foo" is returned for "bar".
  const std::string* bar_server_info = impl_.GetQuicServerInfo(bar_server_id);
  ASSERT_TRUE(bar_server_info != nullptr);
  EXPECT_STREQ(foo_server_info.c_str(), bar_server_info->c_str());
}

// Verifies that GetQuicServerInfo() returns the MRU entry if multiple records
// match a given canonical host.
TEST_F(QuicServerInfoServerPropertiesTest,
       TestCanonicalSuffixMatchReturnsMruEntry) {
  // Set up HttpServerProperties by adding two hosts with the same canonical
  // suffixes.
  quic::QuicServerId h1_server_id("h1.googlevideo.com", 443, false);
  std::string h1_server_info("h1_server_info");
  impl_.SetQuicServerInfo(h1_server_id, h1_server_info);

  quic::QuicServerId h2_server_id("h2.googlevideo.com", 443, false);
  std::string h2_server_info("h2_server_info");
  impl_.SetQuicServerInfo(h2_server_id, h2_server_info);

  // Create quic::QuicServerId to use for the search.
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443, false);

  // Check that 'h2' info is returned since it is MRU.
  const std::string* server_info = impl_.GetQuicServerInfo(foo_server_id);
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_STREQ(h2_server_info.c_str(), server_info->c_str());

  // Access 'h1' info, so it becomes MRU.
  impl_.GetQuicServerInfo(h1_server_id);

  // Check that 'h1' info is returned since it is MRU now.
  server_info = impl_.GetQuicServerInfo(foo_server_id);
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_STREQ(h1_server_info.c_str(), server_info->c_str());
}

// Verifies that |GetQuicServerInfo| doesn't change the MRU order of the server
// info map when a record is matched based on a canonical name.
TEST_F(QuicServerInfoServerPropertiesTest,
       TestCanonicalSuffixMatchDoesntChangeOrder) {
  // Add a host with a matching canonical name.
  quic::QuicServerId h1_server_id("h1.googlevideo.com", 443, false);
  std::string h1_server_info("h1_server_info");
  impl_.SetQuicServerInfo(h1_server_id, h1_server_info);

  // Add a host hosts with a non-matching canonical name.
  quic::QuicServerId h2_server_id("h2.video.com", 443, false);
  std::string h2_server_info("h2_server_info");
  impl_.SetQuicServerInfo(h2_server_id, h2_server_info);

  // Check that "h2.video.com" is the MRU entry in the map.
  EXPECT_EQ(h2_server_id, impl_.quic_server_info_map().begin()->first);

  // Search for the entry that matches the canonical name
  // ("h1.googlevideo.com").
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443, false);
  const std::string* server_info = impl_.GetQuicServerInfo(foo_server_id);
  ASSERT_TRUE(server_info != nullptr);

  // Check that the search (although successful) hasn't changed the MRU order of
  // the map.
  EXPECT_EQ(h2_server_id, impl_.quic_server_info_map().begin()->first);

  // Search for "h1.googlevideo.com" directly, so it becomes MRU
  impl_.GetQuicServerInfo(h1_server_id);

  // Check that "h1.googlevideo.com" is the MRU entry now.
  EXPECT_EQ(h1_server_id, impl_.quic_server_info_map().begin()->first);
}

// Tests that the canonical host matching works for hosts stored in memory cache
// and the ones loaded from persistent storage, i.e. server info added
// using SetQuicServerInfo() and SetQuicServerInfoMap() is taken into
// cosideration when searching for server info for a host with the same
// canonical suffix.
TEST_F(QuicServerInfoServerPropertiesTest, TestCanonicalSuffixMatchSetInfoMap) {
  // Add a host info using SetQuicServerInfo(). That will simulate an info
  // entry stored in memory cache.
  quic::QuicServerId h1_server_id("h1.googlevideo.com", 443, false);
  std::string h1_server_info("h1_server_info_memory_cache");
  impl_.SetQuicServerInfo(h1_server_id, h1_server_info);

  // Prepare a map with host info and add it using SetQuicServerInfoMap(). That
  // will simulate info records read from the persistence storage.
  quic::QuicServerId h2_server_id("h2.googlevideo.com", 443, false);
  std::string h2_server_info("h2_server_info_from_disk");

  quic::QuicServerId h3_server_id("h3.ggpht.com", 443, false);
  std::string h3_server_info("h3_server_info_from_disk");

  const int kMaxQuicServerEntries = 10;
  impl_.SetMaxServerConfigsStoredInProperties(kMaxQuicServerEntries);

  std::unique_ptr<QuicServerInfoMap> quic_server_info_map(
      new QuicServerInfoMap(kMaxQuicServerEntries));
  quic_server_info_map->Put(h2_server_id, h2_server_info);
  quic_server_info_map->Put(h3_server_id, h3_server_info);
  impl_.SetQuicServerInfoMap(std::move(quic_server_info_map));

  // Check that the server info from the memory cache is returned since unique
  // entries from the memory cache are added after entries from the
  // persistence storage and, therefore, are most recently used.
  quic::QuicServerId foo_server_id("foo.googlevideo.com", 443, false);
  const std::string* server_info = impl_.GetQuicServerInfo(foo_server_id);
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_STREQ(h1_server_info.c_str(), server_info->c_str());

  // Check that server info that was added using SetQuicServerInfoMap() can be
  // found.
  foo_server_id = quic::QuicServerId("foo.ggpht.com", 443, false);
  server_info = impl_.GetQuicServerInfo(foo_server_id);
  ASSERT_TRUE(server_info != nullptr);
  EXPECT_STREQ(h3_server_info.c_str(), server_info->c_str());
}

}  // namespace

}  // namespace net
