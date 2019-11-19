// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using base::StringPrintf;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;

enum class NetworkIsolationKeyMode {
  kDisabled,
  kTopFrameOriginOnly,
  kTopFrameOriginAndFrameOrigin,
};

const NetworkIsolationKeyMode kNetworkIsolationKeyModes[] = {
    NetworkIsolationKeyMode::kDisabled,
    NetworkIsolationKeyMode::kTopFrameOriginOnly,
    NetworkIsolationKeyMode::kTopFrameOriginAndFrameOrigin,
};

std::unique_ptr<base::test::ScopedFeatureList> SetNetworkIsolationKeyMode(
    NetworkIsolationKeyMode mode) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  switch (mode) {
    case NetworkIsolationKeyMode::kDisabled:
      feature_list->InitAndDisableFeature(
          features::kPartitionHttpServerPropertiesByNetworkIsolationKey);
      break;
    case NetworkIsolationKeyMode::kTopFrameOriginOnly:
      feature_list->InitWithFeatures(
          // enabled_features
          {features::kPartitionHttpServerPropertiesByNetworkIsolationKey},
          // disabled_features
          {features::kAppendFrameOriginToNetworkIsolationKey});
      break;
    case NetworkIsolationKeyMode::kTopFrameOriginAndFrameOrigin:
      feature_list->InitWithFeatures(
          // enabled_features
          {features::kPartitionHttpServerPropertiesByNetworkIsolationKey,
           features::kAppendFrameOriginToNetworkIsolationKey},
          // disabled_features
          {});
      break;
  }
  return feature_list;
}

class MockPrefDelegate : public net::HttpServerProperties::PrefDelegate {
 public:
  MockPrefDelegate() = default;
  ~MockPrefDelegate() override = default;

  // HttpServerProperties::PrefDelegate implementation.
  const base::DictionaryValue* GetServerProperties() const override {
    return &prefs_;
  }

  void SetServerProperties(const base::DictionaryValue& value,
                           base::OnceClosure callback) override {
    prefs_.Clear();
    prefs_.MergeDictionary(&value);
    ++num_pref_updates_;
    if (!prefs_changed_callback_.is_null())
      std::move(prefs_changed_callback_).Run();
    if (!extra_prefs_changed_callback_.is_null())
      extra_prefs_changed_callback_.Run();
    set_properties_callback_ = std::move(callback);
  }

  void WaitForPrefLoad(base::OnceClosure callback) override {
    CHECK(prefs_changed_callback_.is_null());
    prefs_changed_callback_ = std::move(callback);
  }

  void InitializePrefs(const base::DictionaryValue& value) {
    ASSERT_FALSE(prefs_changed_callback_.is_null());
    prefs_.Clear();
    prefs_.MergeDictionary(&value);
    std::move(prefs_changed_callback_).Run();
  }

  int GetAndClearNumPrefUpdates() {
    int out = num_pref_updates_;
    num_pref_updates_ = 0;
    return out;
  }

  // Additional callback to call when prefs are updated, used to check prefs are
  // updated on destruction.
  void set_extra_update_prefs_callback(const base::Closure& callback) {
    extra_prefs_changed_callback_ = callback;
  }

  // Returns the base::OnceCallback, if any, passed to the last call to
  // SetServerProperties().
  base::OnceClosure GetSetPropertiesCallback() {
    return std::move(set_properties_callback_);
  }

 private:
  base::DictionaryValue prefs_;
  base::OnceClosure prefs_changed_callback_;
  base::Closure extra_prefs_changed_callback_;
  int num_pref_updates_ = 0;

  base::OnceClosure set_properties_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockPrefDelegate);
};

// Converts |server_info_map| to a base::Value by running it through an
// HttpServerPropertiesManager. Other fields are left empty.
base::Value ServerInfoMapToValue(
    const HttpServerProperties::ServerInfoMap& server_info_map) {
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  // Callback that shouldn't be invoked - this method short-circuits loading
  // prefs by calling HttpServerPropertiesManager::WriteToPrefs() before prefs
  // are loaded.
  HttpServerPropertiesManager::OnPrefsLoadedCallback on_prefs_loaded_callback =
      base::BindOnce(
          [](std::unique_ptr<HttpServerProperties::ServerInfoMap>
                 server_info_map,
             const IPAddress& last_quic_address,
             std::unique_ptr<HttpServerProperties::QuicServerInfoMap>
                 quic_server_info_map,
             std::unique_ptr<BrokenAlternativeServiceList>
                 broken_alternative_service_list,
             std::unique_ptr<RecentlyBrokenAlternativeServices>
                 recently_broken_alternative_services) { ADD_FAILURE(); });
  HttpServerPropertiesManager manager(
      std::move(pref_delegate), std::move(on_prefs_loaded_callback),
      10 /* max_server_configs_stored_in_properties */, nullptr /* net_log */,
      base::DefaultTickClock::GetInstance());
  manager.WriteToPrefs(
      server_info_map, HttpServerPropertiesManager::GetCannonicalSuffix(),
      IPAddress() /* last_quic_address */,
      HttpServerProperties::QuicServerInfoMap(10),
      BrokenAlternativeServiceList(), RecentlyBrokenAlternativeServices(10),
      base::OnceClosure());

  return unowned_pref_delegate->GetServerProperties()->Clone();
}

// Does the inverse of ServerInfoMapToValue(). Ignores fields other than the
// ServerInfoMap.
std::unique_ptr<HttpServerProperties::ServerInfoMap> ValueToServerInfoMap(
    const base::Value& value) {
  const base::DictionaryValue* dictionary_value;
  if (!value.GetAsDictionary(&dictionary_value))
    return nullptr;

  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();

  std::unique_ptr<HttpServerProperties::ServerInfoMap> out;
  bool callback_invoked = false;
  HttpServerPropertiesManager::OnPrefsLoadedCallback on_prefs_loaded_callback =
      base::BindLambdaForTesting(
          [&](std::unique_ptr<HttpServerProperties::ServerInfoMap>
                  server_info_map,
              const IPAddress& last_quic_address,
              std::unique_ptr<HttpServerProperties::QuicServerInfoMap>
                  quic_server_info_map,
              std::unique_ptr<BrokenAlternativeServiceList>
                  broken_alternative_service_list,
              std::unique_ptr<RecentlyBrokenAlternativeServices>
                  recently_broken_alternative_services) {
            ASSERT_FALSE(callback_invoked);
            callback_invoked = true;
            out = std::move(server_info_map);
          });

  HttpServerPropertiesManager manager(
      std::move(pref_delegate), std::move(on_prefs_loaded_callback),
      10 /* max_server_configs_stored_in_properties */, nullptr /* net_log */,
      base::DefaultTickClock::GetInstance());

  unowned_pref_delegate->InitializePrefs(*dictionary_value);
  EXPECT_TRUE(callback_invoked);
  return out;
}

}  // namespace

class HttpServerPropertiesManagerTest : public testing::Test,
                                        public WithTaskEnvironment {
 protected:
  HttpServerPropertiesManagerTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    one_day_from_now_ = base::Time::Now() + base::TimeDelta::FromDays(1);
    advertised_versions_ =
        HttpNetworkSession::Params().quic_params.supported_versions;
    pref_delegate_ = new MockPrefDelegate;

    http_server_props_ = std::make_unique<HttpServerProperties>(
        base::WrapUnique(pref_delegate_), /*net_log=*/nullptr,
        GetMockTickClock());

    EXPECT_FALSE(http_server_props_->IsInitialized());
    EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
    EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  }

  // Wrapper around |pref_delegate_|'s InitializePrefs() method that has a
  // couple extra expectations about whether any tasks are posted, and if a pref
  // update is queued.
  //
  // |expect_pref_update| should be true if a pref update is expected to be
  // queued in response to the load.
  void InitializePrefs(
      const base::DictionaryValue& dict = base::DictionaryValue(),
      bool expect_pref_update = false) {
    EXPECT_FALSE(http_server_props_->IsInitialized());
    pref_delegate_->InitializePrefs(dict);
    EXPECT_TRUE(http_server_props_->IsInitialized());
    if (!expect_pref_update) {
      EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
      EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
    } else {
      EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
      EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
      FastForwardUntilNoTasksRemain();
      EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
    }
  }

  void TearDown() override {
    // Run pending non-delayed tasks but don't FastForwardUntilNoTasksRemain()
    // as some delayed tasks may forever repost (e.g. because impl doesn't use a
    // mock clock and doesn't see timings as having expired, ref.
    // HttpServerProperties::
    //     ScheduleBrokenAlternateProtocolMappingsExpiration()).
    base::RunLoop().RunUntilIdle();
    http_server_props_.reset();
  }

  bool HasAlternativeService(const url::SchemeHostPort& server,
                             const NetworkIsolationKey& network_isolation_key) {
    const AlternativeServiceInfoVector alternative_service_info_vector =
        http_server_props_->GetAlternativeServiceInfos(server,
                                                       network_isolation_key);
    return !alternative_service_info_vector.empty();
  }

  // Returns a dictionary with only the version field populated.
  static base::DictionaryValue DictWithVersion() {
    base::DictionaryValue http_server_properties_dict;
    http_server_properties_dict.SetInteger("version", 5);
    return http_server_properties_dict;
  }

  MockPrefDelegate* pref_delegate_;  // Owned by HttpServerPropertiesManager.
  std::unique_ptr<HttpServerProperties> http_server_props_;
  base::Time one_day_from_now_;
  quic::ParsedQuicVersionVector advertised_versions_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerTest);
};

TEST_F(HttpServerPropertiesManagerTest, BadCachedHostPortPair) {
  auto server_pref_dict = std::make_unique<base::DictionaryValue>();

  // Set supports_spdy for www.google.com:65536.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up alternative_service for www.google.com:65536.
  auto alternative_service_dict = std::make_unique<base::DictionaryValue>();
  alternative_service_dict->SetString("protocol_str", "h2");
  alternative_service_dict->SetInteger("port", 80);
  auto alternative_service_list = std::make_unique<base::ListValue>();
  alternative_service_list->Append(std::move(alternative_service_dict));
  server_pref_dict->SetWithoutPathExpansion(
      "alternative_service", std::move(alternative_service_list));

  // Set up ServerNetworkStats for www.google.com:65536.
  auto stats = std::make_unique<base::DictionaryValue>();
  stats->SetInteger("srtt", 10);
  server_pref_dict->SetWithoutPathExpansion("network_stats", std::move(stats));

  // Set the server preference for www.google.com:65536.
  auto servers_dict = std::make_unique<base::DictionaryValue>();
  servers_dict->SetWithoutPathExpansion("www.google.com:65536",
                                        std::move(server_pref_dict));
    auto servers_list = std::make_unique<base::ListValue>();
    servers_list->Append(std::move(servers_dict));
    base::DictionaryValue http_server_properties_dict = DictWithVersion();
    http_server_properties_dict.SetWithoutPathExpansion(
        "servers", std::move(servers_list));

  // Set quic_server_info for www.google.com:65536.
  auto quic_servers_dict = std::make_unique<base::DictionaryValue>();
  auto quic_server_pref_dict1 = std::make_unique<base::DictionaryValue>();
  quic_server_pref_dict1->SetKey("server_info",
                                 base::Value("quic_server_info1"));
  quic_servers_dict->SetWithoutPathExpansion("http://mail.google.com:65536",
                                             std::move(quic_server_pref_dict1));

  http_server_properties_dict.SetWithoutPathExpansion(
      "quic_servers", std::move(quic_servers_dict));

  // Set up the pref.
  InitializePrefs(http_server_properties_dict);

  // Verify that nothing is set.
  HostPortPair google_host_port_pair =
      HostPortPair::FromString("www.google.com:65536");
  url::SchemeHostPort gooler_server("http", google_host_port_pair.host(),
                                    google_host_port_pair.port());

  EXPECT_FALSE(http_server_props_->SupportsRequestPriority(
      gooler_server, NetworkIsolationKey()));
  EXPECT_FALSE(HasAlternativeService(gooler_server, NetworkIsolationKey()));
  const ServerNetworkStats* stats1 = http_server_props_->GetServerNetworkStats(
      gooler_server, NetworkIsolationKey());
  EXPECT_EQ(nullptr, stats1);
  EXPECT_EQ(0u, http_server_props_->quic_server_info_map().size());
}

TEST_F(HttpServerPropertiesManagerTest, BadCachedAltProtocolPort) {
  auto server_pref_dict = std::make_unique<base::DictionaryValue>();

  // Set supports_spdy for www.google.com:80.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up alternative_service for www.google.com:80.
  auto alternative_service_dict = std::make_unique<base::DictionaryValue>();
  alternative_service_dict->SetString("protocol_str", "h2");
  alternative_service_dict->SetInteger("port", 65536);
  auto alternative_service_list = std::make_unique<base::ListValue>();
  alternative_service_list->Append(std::move(alternative_service_dict));
  server_pref_dict->SetWithoutPathExpansion(
      "alternative_service", std::move(alternative_service_list));

  // Set the server preference for www.google.com:80.
  auto servers_dict = std::make_unique<base::DictionaryValue>();
  servers_dict->SetWithoutPathExpansion("www.google.com:80",
                                        std::move(server_pref_dict));
    auto servers_list = std::make_unique<base::ListValue>();
    servers_list->Append(std::move(servers_dict));
    base::DictionaryValue http_server_properties_dict = DictWithVersion();
    http_server_properties_dict.SetWithoutPathExpansion(
        "servers", std::move(servers_list));

    // Set up the pref.
    InitializePrefs(http_server_properties_dict);

    // Verify alternative service is not set.
    EXPECT_FALSE(
        HasAlternativeService(url::SchemeHostPort("http", "www.google.com", 80),
                              NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest, SupportsSpdy) {
  InitializePrefs();

  // Add mail.google.com:443 as a supporting spdy server.
  url::SchemeHostPort spdy_server("https", "mail.google.com", 443);
  EXPECT_FALSE(http_server_props_->SupportsRequestPriority(
      spdy_server, NetworkIsolationKey()));
  http_server_props_->SetSupportsSpdy(spdy_server, NetworkIsolationKey(), true);
  // Setting the value to the same thing again should not trigger another pref
  // update.
  http_server_props_->SetSupportsSpdy(spdy_server, NetworkIsolationKey(), true);

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  // Setting the value to the same thing again should not trigger another pref
  // update.
  http_server_props_->SetSupportsSpdy(spdy_server, NetworkIsolationKey(), true);
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());

  EXPECT_TRUE(http_server_props_->SupportsRequestPriority(
      spdy_server, NetworkIsolationKey()));
}

// Regression test for crbug.com/670519. Test that there is only one pref update
// scheduled if multiple updates happen in a given time period. Subsequent pref
// update could also be scheduled once the previous scheduled update is
// completed.
TEST_F(HttpServerPropertiesManagerTest,
       SinglePrefUpdateForTwoSpdyServerCacheChanges) {
  InitializePrefs();

  // Post an update task. SetSupportsSpdy calls ScheduleUpdatePrefs with a delay
  // of 60ms.
  url::SchemeHostPort spdy_server("https", "mail.google.com", 443);
  EXPECT_FALSE(http_server_props_->SupportsRequestPriority(
      spdy_server, NetworkIsolationKey()));
  http_server_props_->SetSupportsSpdy(spdy_server, NetworkIsolationKey(), true);
  // The pref update task should be scheduled.
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  // Move forward the task runner short by 20ms.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting() -
                base::TimeDelta::FromMilliseconds(20));

  // Set another spdy server to trigger another call to
  // ScheduleUpdatePrefs. There should be no new update posted.
  url::SchemeHostPort spdy_server2("https", "drive.google.com", 443);
  http_server_props_->SetSupportsSpdy(spdy_server2, NetworkIsolationKey(),
                                      true);
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  // Move forward the extra 20ms. The pref update should be executed.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  FastForwardBy(base::TimeDelta::FromMilliseconds(20));
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());

  EXPECT_TRUE(http_server_props_->SupportsRequestPriority(
      spdy_server, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->SupportsRequestPriority(
      spdy_server2, NetworkIsolationKey()));
  // Set the third spdy server to trigger one more call to
  // ScheduleUpdatePrefs. A new update task should be posted now since the
  // previous one is completed.
  url::SchemeHostPort spdy_server3("https", "maps.google.com", 443);
  http_server_props_->SetSupportsSpdy(spdy_server3, NetworkIsolationKey(),
                                      true);
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
}

TEST_F(HttpServerPropertiesManagerTest, GetAlternativeServiceInfos) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  const AlternativeService alternative_service(kProtoHTTP2, "mail.google.com",
                                               443);
  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);
  // ExpectScheduleUpdatePrefs() should be called only once.
  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service,
            alternative_service_info_vector[0].alternative_service());
}

TEST_F(HttpServerPropertiesManagerTest, SetAlternativeServices) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  AlternativeServiceInfoVector alternative_service_info_vector;
  const AlternativeService alternative_service1(kProtoHTTP2, "mail.google.com",
                                                443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service1, one_day_from_now_));
  const AlternativeService alternative_service2(kProtoQUIC, "mail.google.com",
                                                1234);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service2, one_day_from_now_, advertised_versions_));
  http_server_props_->SetAlternativeServices(
      spdy_server_mail, NetworkIsolationKey(), alternative_service_info_vector);
  // ExpectScheduleUpdatePrefs() should be called only once.
  http_server_props_->SetAlternativeServices(
      spdy_server_mail, NetworkIsolationKey(), alternative_service_info_vector);

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  AlternativeServiceInfoVector alternative_service_info_vector2 =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  ASSERT_EQ(2u, alternative_service_info_vector2.size());
  EXPECT_EQ(alternative_service1,
            alternative_service_info_vector2[0].alternative_service());
  EXPECT_EQ(alternative_service2,
            alternative_service_info_vector2[1].alternative_service());
}

TEST_F(HttpServerPropertiesManagerTest, SetAlternativeServicesEmpty) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  const AlternativeService alternative_service(kProtoHTTP2, "mail.google.com",
                                               443);
  http_server_props_->SetAlternativeServices(
      spdy_server_mail, NetworkIsolationKey(), AlternativeServiceInfoVector());

  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest, ConfirmAlternativeService) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail;
  AlternativeService alternative_service;

  spdy_server_mail = url::SchemeHostPort("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  alternative_service = AlternativeService(kProtoHTTP2, "mail.google.com", 443);

  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  http_server_props_->MarkAlternativeServiceBroken(alternative_service,
                                                   NetworkIsolationKey());
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  // In addition to the pref update task, there's now a task to mark the
  // alternative service as no longer broken.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  http_server_props_->ConfirmAlternativeService(alternative_service,
                                                NetworkIsolationKey());
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  // Run the task.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));
}

// Check the case that prefs are loaded only after setting alternative service
// info. Prefs should not be written until after the load happens.
TEST_F(HttpServerPropertiesManagerTest, LateLoadAlternativeServiceInfo) {
  url::SchemeHostPort spdy_server_mail("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  const AlternativeService alternative_service(kProtoHTTP2, "mail.google.com",
                                               443);
  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);

  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());

  AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service,
            alternative_service_info_vector[0].alternative_service());

  // Initializing prefs does not result in a task to write the prefs.
  InitializePrefs(base::DictionaryValue(), true /* expect_pref_update */);
  alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  EXPECT_EQ(1u, alternative_service_info_vector.size());

  // Updating the entry should result in a task to save prefs. Have to at least
  // double (or half) the lifetime, to ensure the change triggers a save to
  // prefs.
  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_ + base::TimeDelta::FromDays(2));
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
  alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  EXPECT_EQ(1u, alternative_service_info_vector.size());
}

// Check the case that prefs are cleared before they're loaded.
TEST_F(HttpServerPropertiesManagerTest,
       ClearPrefsBeforeLoadAlternativeServiceInfo) {
  url::SchemeHostPort spdy_server_mail("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  const AlternativeService alternative_service(kProtoHTTP2, "mail.google.com",
                                               443);
  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);

  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());

  AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());
  EXPECT_EQ(alternative_service,
            alternative_service_info_vector[0].alternative_service());

  // Clearing prefs should result in a task to write the prefs.
  bool callback_invoked_ = false;
  http_server_props_->Clear(base::BindOnce(
      [](bool* callback_invoked) {
        EXPECT_FALSE(*callback_invoked);
        *callback_invoked = true;
      },
      &callback_invoked_));
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_FALSE(callback_invoked_);
  std::move(pref_delegate_->GetSetPropertiesCallback()).Run();
  EXPECT_TRUE(callback_invoked_);
  alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  EXPECT_EQ(0u, alternative_service_info_vector.size());

  // Re-creating the entry should result in a task to save prefs.
  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
  alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(spdy_server_mail,
                                                     NetworkIsolationKey());
  EXPECT_EQ(1u, alternative_service_info_vector.size());
}

TEST_F(HttpServerPropertiesManagerTest,
       ConfirmBrokenUntilDefaultNetworkChanges) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail;
  AlternativeService alternative_service;

  spdy_server_mail = url::SchemeHostPort("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  alternative_service = AlternativeService(kProtoHTTP2, "mail.google.com", 443);

  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  http_server_props_->MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, NetworkIsolationKey());
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  // In addition to the pref update task, there's now a task to mark the
  // alternative service as no longer broken.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  http_server_props_->ConfirmAlternativeService(alternative_service,
                                                NetworkIsolationKey());
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  // Run the task.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest,
       OnDefaultNetworkChangedWithBrokenUntilDefaultNetworkChanges) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail;
  AlternativeService alternative_service;

  spdy_server_mail = url::SchemeHostPort("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  alternative_service = AlternativeService(kProtoHTTP2, "mail.google.com", 443);

  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  http_server_props_->MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      alternative_service, NetworkIsolationKey());
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  // In addition to the pref update task, there's now a task to mark the
  // alternative service as no longer broken.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  http_server_props_->OnDefaultNetworkChanged();
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  // Run the task.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest, OnDefaultNetworkChangedWithBrokenOnly) {
  InitializePrefs();

  url::SchemeHostPort spdy_server_mail;
  AlternativeService alternative_service;

  spdy_server_mail = url::SchemeHostPort("http", "mail.google.com", 80);
  EXPECT_FALSE(HasAlternativeService(spdy_server_mail, NetworkIsolationKey()));
  alternative_service = AlternativeService(kProtoHTTP2, "mail.google.com", 443);

  http_server_props_->SetHttp2AlternativeService(
      spdy_server_mail, NetworkIsolationKey(), alternative_service,
      one_day_from_now_);
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());

  http_server_props_->MarkAlternativeServiceBroken(alternative_service,
                                                   NetworkIsolationKey());
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  // In addition to the pref update task, there's now a task to mark the
  // alternative service as no longer broken.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  http_server_props_->OnDefaultNetworkChanged();
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));

  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());

  // Run the task.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      alternative_service, NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest, LastLocalAddressWhenQuicWorked) {
  InitializePrefs();

  IPAddress actual_address(127, 0, 0, 1);
  EXPECT_FALSE(http_server_props_->HasLastLocalAddressWhenQuicWorked());
  EXPECT_FALSE(
      http_server_props_->WasLastLocalAddressWhenQuicWorked(actual_address));
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);
  // Another task should not be scheduled.
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_TRUE(
      http_server_props_->WasLastLocalAddressWhenQuicWorked(actual_address));

  // Another task should not be scheduled.
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
}

TEST_F(HttpServerPropertiesManagerTest, ServerNetworkStats) {
  InitializePrefs();

  url::SchemeHostPort mail_server("http", "mail.google.com", 80);
  const ServerNetworkStats* stats = http_server_props_->GetServerNetworkStats(
      mail_server, NetworkIsolationKey());
  EXPECT_EQ(nullptr, stats);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  http_server_props_->SetServerNetworkStats(mail_server, NetworkIsolationKey(),
                                            stats1);
  // Another task should not be scheduled.
  http_server_props_->SetServerNetworkStats(mail_server, NetworkIsolationKey(),
                                            stats1);

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  // Another task should not be scheduled.
  http_server_props_->SetServerNetworkStats(mail_server, NetworkIsolationKey(),
                                            stats1);
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(GetPendingMainThreadTaskCount(), 0u);

  const ServerNetworkStats* stats2 = http_server_props_->GetServerNetworkStats(
      mail_server, NetworkIsolationKey());
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());

  http_server_props_->ClearServerNetworkStats(mail_server,
                                              NetworkIsolationKey());

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_EQ(nullptr, http_server_props_->GetServerNetworkStats(
                         mail_server, NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest, QuicServerInfo) {
  InitializePrefs();

  quic::QuicServerId mail_quic_server_id("mail.google.com", 80, false);
  EXPECT_EQ(nullptr, http_server_props_->GetQuicServerInfo(
                         mail_quic_server_id, NetworkIsolationKey()));
  std::string quic_server_info1("quic_server_info1");
  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);
  // Another task should not be scheduled.
  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);

  // Run the task.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_EQ(quic_server_info1, *http_server_props_->GetQuicServerInfo(
                                   mail_quic_server_id, NetworkIsolationKey()));

  // Another task should not be scheduled.
  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
}

TEST_F(HttpServerPropertiesManagerTest, Clear) {
  InitializePrefs();

  const url::SchemeHostPort spdy_server("https", "mail.google.com", 443);
  const IPAddress actual_address(127, 0, 0, 1);
  const quic::QuicServerId mail_quic_server_id("mail.google.com", 80, false);
  const std::string quic_server_info1("quic_server_info1");
  const AlternativeService alternative_service(kProtoHTTP2, "mail.google.com",
                                               1234);
  const AlternativeService broken_alternative_service(
      kProtoHTTP2, "broken.google.com", 1234);

  AlternativeServiceInfoVector alt_svc_info_vector;
  alt_svc_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, one_day_from_now_));
  alt_svc_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          broken_alternative_service, one_day_from_now_));
  http_server_props_->SetAlternativeServices(spdy_server, NetworkIsolationKey(),
                                             alt_svc_info_vector);

  http_server_props_->MarkAlternativeServiceBroken(broken_alternative_service,
                                                   NetworkIsolationKey());
  http_server_props_->SetSupportsSpdy(spdy_server, NetworkIsolationKey(), true);
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMicroseconds(10);
  http_server_props_->SetServerNetworkStats(spdy_server, NetworkIsolationKey(),
                                            stats);

  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);

  // Advance time by just enough so that the prefs update task is executed but
  // not the task to expire the brokenness of |broken_alternative_service|.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      broken_alternative_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->SupportsRequestPriority(
      spdy_server, NetworkIsolationKey()));
  EXPECT_TRUE(HasAlternativeService(spdy_server, NetworkIsolationKey()));
  EXPECT_TRUE(
      http_server_props_->WasLastLocalAddressWhenQuicWorked(actual_address));
  const ServerNetworkStats* stats1 = http_server_props_->GetServerNetworkStats(
      spdy_server, NetworkIsolationKey());
  EXPECT_EQ(10, stats1->srtt.ToInternalValue());
  EXPECT_EQ(quic_server_info1, *http_server_props_->GetQuicServerInfo(
                                   mail_quic_server_id, NetworkIsolationKey()));

  // Clear http server data, which should instantly update prefs.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  bool callback_invoked_ = false;
  http_server_props_->Clear(base::BindOnce(
      [](bool* callback_invoked) {
        EXPECT_FALSE(*callback_invoked);
        *callback_invoked = true;
      },
      &callback_invoked_));
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_FALSE(callback_invoked_);
  std::move(pref_delegate_->GetSetPropertiesCallback()).Run();
  EXPECT_TRUE(callback_invoked_);

  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      broken_alternative_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->SupportsRequestPriority(
      spdy_server, NetworkIsolationKey()));
  EXPECT_FALSE(HasAlternativeService(spdy_server, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->HasLastLocalAddressWhenQuicWorked());
  const ServerNetworkStats* stats2 = http_server_props_->GetServerNetworkStats(
      spdy_server, NetworkIsolationKey());
  EXPECT_EQ(nullptr, stats2);
  EXPECT_EQ(nullptr, http_server_props_->GetQuicServerInfo(
                         mail_quic_server_id, NetworkIsolationKey()));
}

// https://crbug.com/444956: Add 200 alternative_service servers followed by
// supports_quic and verify we have read supports_quic from prefs.
TEST_F(HttpServerPropertiesManagerTest, BadLastLocalAddressWhenQuicWorked) {
  std::unique_ptr<base::ListValue> servers_list =
      std::make_unique<base::ListValue>();

  for (int i = 1; i <= 200; ++i) {
    // Set up alternative_service for www.google.com:i.
    base::Value server_dict(base::Value::Type::DICTIONARY);
    base::Value alternative_service_dict(base::Value::Type::DICTIONARY);
    alternative_service_dict.SetStringKey("protocol_str", "quic");
    alternative_service_dict.SetIntKey("port", i);
    base::Value alternative_service_list(base::Value::Type::LIST);
    alternative_service_list.Append(std::move(alternative_service_dict));
    server_dict.SetKey("alternative_service",
                       std::move(alternative_service_list));
    server_dict.SetStringKey("server",
                             StringPrintf("https://www.google.com:%d", i));
    server_dict.SetKey("isolation", base::Value(base::Value::Type::LIST));
    servers_list->Append(std::move(server_dict));
  }

  // Set the server preference for http://mail.google.com server.
  base::Value server_dict2(base::Value::Type::DICTIONARY);
  server_dict2.SetStringKey("server", "https://mail.google.com");
  server_dict2.SetKey("isolation", base::Value(base::Value::Type::LIST));
  servers_list->Append(std::move(server_dict2));

  base::DictionaryValue http_server_properties_dict = DictWithVersion();
  http_server_properties_dict.SetWithoutPathExpansion("servers",
                                                      std::move(servers_list));

  // Set up SupportsQuic for 127.0.0.1
  auto supports_quic = std::make_unique<base::DictionaryValue>();
  supports_quic->SetBoolean("used_quic", true);
  supports_quic->SetString("address", "127.0.0.1");
  http_server_properties_dict.SetWithoutPathExpansion("supports_quic",
                                                      std::move(supports_quic));

  // Set up the pref.
  InitializePrefs(http_server_properties_dict);

  // Verify alternative service.
  for (int i = 1; i <= 200; ++i) {
    GURL server_gurl;
      server_gurl = GURL(StringPrintf("https://www.google.com:%d", i));
    url::SchemeHostPort server(server_gurl);
    AlternativeServiceInfoVector alternative_service_info_vector =
        http_server_props_->GetAlternativeServiceInfos(server,
                                                       NetworkIsolationKey());
    ASSERT_EQ(1u, alternative_service_info_vector.size());
    EXPECT_EQ(
        kProtoQUIC,
        alternative_service_info_vector[0].alternative_service().protocol);
    EXPECT_EQ(i, alternative_service_info_vector[0].alternative_service().port);
  }

  // Verify WasLastLocalAddressWhenQuicWorked.
  ASSERT_TRUE(http_server_props_->WasLastLocalAddressWhenQuicWorked(
      IPAddress::IPv4Localhost()));
}

TEST_F(HttpServerPropertiesManagerTest, UpdatePrefsWithCache) {
  InitializePrefs();

  const url::SchemeHostPort server_www("https", "www.google.com", 80);
  const url::SchemeHostPort server_mail("https", "mail.google.com", 80);

  // #1 & #2: Set alternate protocol.
  AlternativeServiceInfoVector alternative_service_info_vector;
  AlternativeService www_alternative_service1(kProtoHTTP2, "", 443);
  base::Time expiration1;
  ASSERT_TRUE(base::Time::FromUTCString("2036-12-01 10:00:00", &expiration1));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          www_alternative_service1, expiration1));

  AlternativeService www_alternative_service2(kProtoHTTP2, "www.google.com",
                                              1234);
  base::Time expiration2;
  ASSERT_TRUE(base::Time::FromUTCString("2036-12-31 10:00:00", &expiration2));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          www_alternative_service2, expiration2));
  http_server_props_->SetAlternativeServices(server_www, NetworkIsolationKey(),
                                             alternative_service_info_vector);

  AlternativeService mail_alternative_service(kProtoHTTP2, "foo.google.com",
                                              444);
  base::Time expiration3 = base::Time::Max();
  http_server_props_->SetHttp2AlternativeService(
      server_mail, NetworkIsolationKey(), mail_alternative_service,
      expiration3);

  http_server_props_->MarkAlternativeServiceBroken(www_alternative_service2,
                                                   NetworkIsolationKey());
  http_server_props_->MarkAlternativeServiceRecentlyBroken(
      mail_alternative_service, NetworkIsolationKey());

  // #3: Set SPDY server map
  http_server_props_->SetSupportsSpdy(server_www, NetworkIsolationKey(), false);
  http_server_props_->SetSupportsSpdy(server_mail, NetworkIsolationKey(), true);
  http_server_props_->SetSupportsSpdy(
      url::SchemeHostPort("http", "not_persisted.com", 80),
      NetworkIsolationKey(), false);

  // #4: Set ServerNetworkStats.
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromInternalValue(42);
  http_server_props_->SetServerNetworkStats(server_mail, NetworkIsolationKey(),
                                            stats);

  // #5: Set quic_server_info string.
  quic::QuicServerId mail_quic_server_id("mail.google.com", 80, false);
  std::string quic_server_info1("quic_server_info1");
  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);

  // #6: Set SupportsQuic.
  IPAddress actual_address(127, 0, 0, 1);
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);

  base::Time time_before_prefs_update = base::Time::Now();

  // Update Prefs.
  // The task runner has a remaining pending task to expire
  // |www_alternative_service2| in 5 minutes. Fast forward enough such
  // that the prefs update task is executed but not the task to expire
  // |broken_alternative_service|.
  EXPECT_EQ(2u, GetPendingMainThreadTaskCount());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  EXPECT_EQ(1u, GetPendingMainThreadTaskCount());
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  base::Time time_after_prefs_update = base::Time::Now();

  // Verify |pref_delegate_|'s server dict.
  // In HttpServerPropertiesManager, broken alternative services' expiration
  // times are converted from TimeTicks to Time before being written to JSON by
  // using the difference between Time::Now() and TimeTicks::Now().
  // To verify these expiration times, |time_before_prefs_update| and
  // |time_after_prefs_update| provide lower and upper bounds for the
  // Time::Now() value used by the manager for this conversion.
  //
  // A copy of |pref_delegate_|'s server dict will be created, and the broken
  // alternative service's "broken_until" field is removed and verified
  // separately. The rest of the server dict copy is verified afterwards.
  base::Value server_value_copy =
      pref_delegate_->GetServerProperties()->Clone();

  // Extract and remove the "broken_until" string for "www.google.com:1234".
  base::DictionaryValue* server_dict;
  ASSERT_TRUE(server_value_copy.GetAsDictionary(&server_dict));
  base::ListValue* broken_alt_svc_list;
  ASSERT_TRUE(server_dict->GetList("broken_alternative_services",
                                   &broken_alt_svc_list));
  ASSERT_EQ(2u, broken_alt_svc_list->GetSize());
  base::DictionaryValue* broken_alt_svcs_list_entry;
  ASSERT_TRUE(
      broken_alt_svc_list->GetDictionary(0, &broken_alt_svcs_list_entry));
  ASSERT_TRUE(broken_alt_svcs_list_entry->HasKey("broken_until"));
  std::string expiration_string;
  ASSERT_TRUE(broken_alt_svcs_list_entry->GetStringWithoutPathExpansion(
      "broken_until", &expiration_string));
  broken_alt_svcs_list_entry->RemoveWithoutPathExpansion("broken_until",
                                                         nullptr);

  // Expiration time of "www.google.com:1234" should be 5 minutes minus the
  // update-prefs-delay from when the prefs were written.
  int64_t expiration_int64;
  ASSERT_TRUE(base::StringToInt64(expiration_string, &expiration_int64));
  base::TimeDelta expiration_delta =
      base::TimeDelta::FromMinutes(5) -
      HttpServerProperties::GetUpdatePrefsDelayForTesting();
  time_t time_t_of_prefs_update = static_cast<time_t>(expiration_int64);
  EXPECT_LE((time_before_prefs_update + expiration_delta).ToTimeT(),
            time_t_of_prefs_update);
  EXPECT_GE((time_after_prefs_update + expiration_delta).ToTimeT(),
            time_t_of_prefs_update);

  // Verify all other preferences.
  const char expected_json[] =
      "{"
      "\"broken_alternative_services\":"
      "[{\"broken_count\":1,\"host\":\"www.google.com\",\"isolation\":[],"
      "\"port\":1234,\"protocol_str\":\"h2\"},"
      "{\"broken_count\":1,\"host\":\"foo.google.com\",\"isolation\":[],"
      "\"port\":444,\"protocol_str\":\"h2\"}],"
      "\"quic_servers\":"
      "[{\"isolation\":[],"
      "\"server_id\":\"https://mail.google.com:80\","
      "\"server_info\":\"quic_server_info1\"}],"
      "\"servers\":["
      "{\"alternative_service\":[{\"advertised_versions\":[],"
      "\"expiration\":\"13756212000000000\",\"port\":443,"
      "\"protocol_str\":\"h2\"},"
      "{\"advertised_versions\":[],\"expiration\":\"13758804000000000\","
      "\"host\":\"www.google.com\",\"port\":1234,\"protocol_str\":\"h2\"}],"
      "\"isolation\":[],"
      "\"server\":\"https://www.google.com:80\"},"
      "{\"alternative_service\":[{\"advertised_versions\":[],"
      "\"expiration\":\"9223372036854775807\",\"host\":\"foo.google.com\","
      "\"port\":444,\"protocol_str\":\"h2\"}],"
      "\"isolation\":[],"
      "\"network_stats\":{\"srtt\":42},"
      "\"server\":\"https://mail.google.com:80\","
      "\"supports_spdy\":true}],"
      "\"supports_quic\":{\"address\":\"127.0.0.1\",\"used_quic\":true},"
      "\"version\":5}";

  std::string preferences_json;
  EXPECT_TRUE(base::JSONWriter::Write(server_value_copy, &preferences_json));
  EXPECT_EQ(expected_json, preferences_json);
}

TEST_F(HttpServerPropertiesManagerTest, ParseAlternativeServiceInfo) {
  InitializePrefs();

  std::unique_ptr<base::Value> server_value = base::JSONReader::ReadDeprecated(
      "{\"alternative_service\":[{\"port\":443,\"protocol_str\":\"h2\"},"
      "{\"port\":123,\"protocol_str\":\"quic\","
      "\"expiration\":\"9223372036854775807\"},{\"host\":\"example.org\","
      "\"port\":1234,\"protocol_str\":\"h2\","
      "\"expiration\":\"13758804000000000\"}]}");
  ASSERT_TRUE(server_value);
  base::DictionaryValue* server_dict;
  ASSERT_TRUE(server_value->GetAsDictionary(&server_dict));

  const url::SchemeHostPort server("https", "example.com", 443);
  HttpServerProperties::ServerInfo server_info;
  EXPECT_TRUE(HttpServerPropertiesManager::ParseAlternativeServiceInfo(
      server, *server_dict, &server_info));

  ASSERT_TRUE(server_info.alternative_services.has_value());
  AlternativeServiceInfoVector alternative_service_info_vector =
      server_info.alternative_services.value();
  ASSERT_EQ(3u, alternative_service_info_vector.size());

  EXPECT_EQ(kProtoHTTP2,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("", alternative_service_info_vector[0].alternative_service().host);
  EXPECT_EQ(443, alternative_service_info_vector[0].alternative_service().port);
  // Expiration defaults to one day from now, testing with tolerance.
  const base::Time now = base::Time::Now();
  const base::Time expiration = alternative_service_info_vector[0].expiration();
  EXPECT_LE(now + base::TimeDelta::FromHours(23), expiration);
  EXPECT_GE(now + base::TimeDelta::FromDays(1), expiration);

  EXPECT_EQ(kProtoQUIC,
            alternative_service_info_vector[1].alternative_service().protocol);
  EXPECT_EQ("", alternative_service_info_vector[1].alternative_service().host);
  EXPECT_EQ(123, alternative_service_info_vector[1].alternative_service().port);
  // numeric_limits<int64_t>::max() represents base::Time::Max().
  EXPECT_EQ(base::Time::Max(), alternative_service_info_vector[1].expiration());

  EXPECT_EQ(kProtoHTTP2,
            alternative_service_info_vector[2].alternative_service().protocol);
  EXPECT_EQ("example.org",
            alternative_service_info_vector[2].alternative_service().host);
  EXPECT_EQ(1234,
            alternative_service_info_vector[2].alternative_service().port);
  base::Time expected_expiration;
  ASSERT_TRUE(
      base::Time::FromUTCString("2036-12-31 10:00:00", &expected_expiration));
  EXPECT_EQ(expected_expiration,
            alternative_service_info_vector[2].expiration());

  // No other fields should have been populated.
  server_info.alternative_services.reset();
  EXPECT_TRUE(server_info.empty());
}

// Regression test for https://crbug.com/615497.
TEST_F(HttpServerPropertiesManagerTest, DoNotLoadAltSvcForInsecureOrigins) {
  InitializePrefs();

  std::unique_ptr<base::Value> server_value = base::JSONReader::ReadDeprecated(
      "{\"alternative_service\":[{\"port\":443,\"protocol_str\":\"h2\","
      "\"expiration\":\"9223372036854775807\"}]}");
  ASSERT_TRUE(server_value);
  base::DictionaryValue* server_dict;
  ASSERT_TRUE(server_value->GetAsDictionary(&server_dict));

  const url::SchemeHostPort server("http", "example.com", 80);
  HttpServerProperties::ServerInfo server_info;
  EXPECT_FALSE(HttpServerPropertiesManager::ParseAlternativeServiceInfo(
      server, *server_dict, &server_info));
  EXPECT_TRUE(server_info.empty());
}

// Do not persist expired alternative service entries to disk.
TEST_F(HttpServerPropertiesManagerTest, DoNotPersistExpiredAlternativeService) {
  InitializePrefs();

  AlternativeServiceInfoVector alternative_service_info_vector;

  const AlternativeService broken_alternative_service(
      kProtoHTTP2, "broken.example.com", 443);
  const base::Time time_one_day_later =
      base::Time::Now() + base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          broken_alternative_service, time_one_day_later));
  // #1: MarkAlternativeServiceBroken().
  http_server_props_->MarkAlternativeServiceBroken(broken_alternative_service,
                                                   NetworkIsolationKey());

  const AlternativeService expired_alternative_service(
      kProtoHTTP2, "expired.example.com", 443);
  const base::Time time_one_day_ago =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          expired_alternative_service, time_one_day_ago));

  const AlternativeService valid_alternative_service(kProtoHTTP2,
                                                     "valid.example.com", 443);
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          valid_alternative_service, time_one_day_later));

  const url::SchemeHostPort server("https", "www.example.com", 443);
  // #2: SetAlternativeServices().
  http_server_props_->SetAlternativeServices(server, NetworkIsolationKey(),
                                             alternative_service_info_vector);

  // |net_test_task_runner_| has a remaining pending task to expire
  // |broken_alternative_service| at |time_one_day_later|. Fast forward enough
  // such that the prefs update task is executed but not the task to expire
  // |broken_alternative_service|.
  EXPECT_EQ(2U, GetPendingMainThreadTaskCount());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  EXPECT_EQ(1U, GetPendingMainThreadTaskCount());
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  const base::DictionaryValue* pref_dict =
      pref_delegate_->GetServerProperties();

  const base::ListValue* servers_list = nullptr;
  ASSERT_TRUE(pref_dict->GetListWithoutPathExpansion("servers", &servers_list));
  auto it = servers_list->begin();
  const base::DictionaryValue* server_pref_dict;
  ASSERT_TRUE(it->GetAsDictionary(&server_pref_dict));

  const std::string* server_str = server_pref_dict->FindStringKey("server");
  ASSERT_TRUE(server_str);
  EXPECT_EQ("https://www.example.com", *server_str);

  const base::Value* network_isolation_key_value =
      server_pref_dict->FindKey("isolation");
  ASSERT_TRUE(network_isolation_key_value);
  ASSERT_EQ(base::Value::Type::LIST, network_isolation_key_value->type());
  EXPECT_TRUE(network_isolation_key_value->GetList().empty());

  const base::ListValue* altsvc_list;
  ASSERT_TRUE(server_pref_dict->GetList("alternative_service", &altsvc_list));

  ASSERT_EQ(2u, altsvc_list->GetSize());

  const base::DictionaryValue* altsvc_entry;
  std::string hostname;

  ASSERT_TRUE(altsvc_list->GetDictionary(0, &altsvc_entry));
  ASSERT_TRUE(altsvc_entry->GetString("host", &hostname));
  EXPECT_EQ("broken.example.com", hostname);

  ASSERT_TRUE(altsvc_list->GetDictionary(1, &altsvc_entry));
  ASSERT_TRUE(altsvc_entry->GetString("host", &hostname));
  EXPECT_EQ("valid.example.com", hostname);
}

// Test that expired alternative service entries on disk are ignored.
TEST_F(HttpServerPropertiesManagerTest, DoNotLoadExpiredAlternativeService) {
  InitializePrefs();

  auto alternative_service_list = std::make_unique<base::ListValue>();
  auto expired_dict = std::make_unique<base::DictionaryValue>();
  expired_dict->SetString("protocol_str", "h2");
  expired_dict->SetString("host", "expired.example.com");
  expired_dict->SetInteger("port", 443);
  base::Time time_one_day_ago =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  expired_dict->SetString(
      "expiration", base::NumberToString(time_one_day_ago.ToInternalValue()));
  alternative_service_list->Append(std::move(expired_dict));

  auto valid_dict = std::make_unique<base::DictionaryValue>();
  valid_dict->SetString("protocol_str", "h2");
  valid_dict->SetString("host", "valid.example.com");
  valid_dict->SetInteger("port", 443);
  valid_dict->SetString(
      "expiration", base::NumberToString(one_day_from_now_.ToInternalValue()));
  alternative_service_list->Append(std::move(valid_dict));

  base::DictionaryValue server_pref_dict;
  server_pref_dict.SetWithoutPathExpansion("alternative_service",
                                           std::move(alternative_service_list));

  const url::SchemeHostPort server("https", "example.com", 443);
  HttpServerProperties::ServerInfo server_info;
  ASSERT_TRUE(HttpServerPropertiesManager::ParseAlternativeServiceInfo(
      server, server_pref_dict, &server_info));

  ASSERT_TRUE(server_info.alternative_services.has_value());
  AlternativeServiceInfoVector alternative_service_info_vector =
      server_info.alternative_services.value();
  ASSERT_EQ(1u, alternative_service_info_vector.size());

  EXPECT_EQ(kProtoHTTP2,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("valid.example.com",
            alternative_service_info_vector[0].alternative_service().host);
  EXPECT_EQ(443, alternative_service_info_vector[0].alternative_service().port);
  EXPECT_EQ(one_day_from_now_, alternative_service_info_vector[0].expiration());

  // No other fields should have been populated.
  server_info.alternative_services.reset();
  EXPECT_TRUE(server_info.empty());
}

// Make sure prefs are updated on destruction.
TEST_F(HttpServerPropertiesManagerTest, UpdatePrefsOnShutdown) {
  InitializePrefs();

  int pref_updates = 0;
  pref_delegate_->set_extra_update_prefs_callback(
      base::Bind([](int* updates) { (*updates)++; }, &pref_updates));
  http_server_props_.reset();
  EXPECT_EQ(1, pref_updates);
}

TEST_F(HttpServerPropertiesManagerTest, PersistAdvertisedVersionsToPref) {
  InitializePrefs();

  const url::SchemeHostPort server_www("https", "www.google.com", 80);
  const url::SchemeHostPort server_mail("https", "mail.google.com", 80);

  // #1 & #2: Set alternate protocol.
  AlternativeServiceInfoVector alternative_service_info_vector;
  // Quic alternative service set with two advertised QUIC versions.
  AlternativeService quic_alternative_service1(kProtoQUIC, "", 443);
  base::Time expiration1;
  ASSERT_TRUE(base::Time::FromUTCString("2036-12-01 10:00:00", &expiration1));
  quic::ParsedQuicVersionVector advertised_versions = {
      quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                              quic::QUIC_VERSION_46),
      quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                              quic::QUIC_VERSION_43)};
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          quic_alternative_service1, expiration1, advertised_versions));
  // HTTP/2 alternative service should not set any advertised version.
  AlternativeService h2_alternative_service(kProtoHTTP2, "www.google.com",
                                            1234);
  base::Time expiration2;
  ASSERT_TRUE(base::Time::FromUTCString("2036-12-31 10:00:00", &expiration2));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          h2_alternative_service, expiration2));
  http_server_props_->SetAlternativeServices(server_www, NetworkIsolationKey(),
                                             alternative_service_info_vector);

  // Set another QUIC alternative service with a single advertised QUIC version.
  AlternativeService mail_alternative_service(kProtoQUIC, "foo.google.com",
                                              444);
  base::Time expiration3 = base::Time::Max();
  http_server_props_->SetQuicAlternativeService(
      server_mail, NetworkIsolationKey(), mail_alternative_service, expiration3,
      advertised_versions_);
  // #3: Set ServerNetworkStats.
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromInternalValue(42);
  http_server_props_->SetServerNetworkStats(server_mail, NetworkIsolationKey(),
                                            stats);

  // #4: Set quic_server_info string.
  quic::QuicServerId mail_quic_server_id("mail.google.com", 80, false);
  std::string quic_server_info1("quic_server_info1");
  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);

  // #5: Set SupportsQuic.
  IPAddress actual_address(127, 0, 0, 1);
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);

  // Update Prefs.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  // Verify preferences with correct advertised version field.
  const char expected_json[] =
      "{\"quic_servers\":["
      "{\"isolation\":[],"
      "\"server_id\":\"https://mail.google.com:80\","
      "\"server_info\":\"quic_server_info1\"}],"
      "\"servers\":["
      "{\"alternative_service\":[{"
      "\"advertised_versions\":[43,46],\"expiration\":\"13756212000000000\","
      "\"port\":443,\"protocol_str\":\"quic\"},{\"advertised_versions\":[],"
      "\"expiration\":\"13758804000000000\",\"host\":\"www.google.com\","
      "\"port\":1234,\"protocol_str\":\"h2\"}],"
      "\"isolation\":[],"
      "\"server\":\"https://www.google.com:80\"},"
      "{\"alternative_service\":[{"
      "\"advertised_versions\":[46],\"expiration\":\"9223372036854775807\","
      "\"host\":\"foo.google.com\",\"port\":444,\"protocol_str\":\"quic\"}],"
      "\"isolation\":[],"
      "\"network_stats\":{\"srtt\":42},"
      "\"server\":\"https://mail.google.com:80\"}],"
      "\"supports_quic\":{"
      "\"address\":\"127.0.0.1\",\"used_quic\":true},\"version\":5}";

  const base::Value* http_server_properties =
      pref_delegate_->GetServerProperties();
  std::string preferences_json;
  EXPECT_TRUE(
      base::JSONWriter::Write(*http_server_properties, &preferences_json));
  EXPECT_EQ(expected_json, preferences_json);
}

TEST_F(HttpServerPropertiesManagerTest, ReadAdvertisedVersionsFromPref) {
  InitializePrefs();

  std::unique_ptr<base::Value> server_value = base::JSONReader::ReadDeprecated(
      "{\"alternative_service\":["
      "{\"port\":443,\"protocol_str\":\"quic\"},"
      "{\"port\":123,\"protocol_str\":\"quic\","
      "\"expiration\":\"9223372036854775807\","
      "\"advertised_versions\":[46,43]}]}");
  ASSERT_TRUE(server_value);
  base::DictionaryValue* server_dict;
  ASSERT_TRUE(server_value->GetAsDictionary(&server_dict));

  const url::SchemeHostPort server("https", "example.com", 443);
  HttpServerProperties::ServerInfo server_info;
  EXPECT_TRUE(HttpServerPropertiesManager::ParseAlternativeServiceInfo(
      server, *server_dict, &server_info));

  ASSERT_TRUE(server_info.alternative_services.has_value());
  AlternativeServiceInfoVector alternative_service_info_vector =
      server_info.alternative_services.value();
  ASSERT_EQ(2u, alternative_service_info_vector.size());

  // Verify the first alternative service with no advertised version listed.
  EXPECT_EQ(kProtoQUIC,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("", alternative_service_info_vector[0].alternative_service().host);
  EXPECT_EQ(443, alternative_service_info_vector[0].alternative_service().port);
  // Expiration defaults to one day from now, testing with tolerance.
  const base::Time now = base::Time::Now();
  const base::Time expiration = alternative_service_info_vector[0].expiration();
  EXPECT_LE(now + base::TimeDelta::FromHours(23), expiration);
  EXPECT_GE(now + base::TimeDelta::FromDays(1), expiration);
  EXPECT_TRUE(alternative_service_info_vector[0].advertised_versions().empty());

  // Verify the second alterntaive service with two advertised versions.
  EXPECT_EQ(kProtoQUIC,
            alternative_service_info_vector[1].alternative_service().protocol);
  EXPECT_EQ("", alternative_service_info_vector[1].alternative_service().host);
  EXPECT_EQ(123, alternative_service_info_vector[1].alternative_service().port);
  EXPECT_EQ(base::Time::Max(), alternative_service_info_vector[1].expiration());
  // Verify advertised versions.
  const quic::ParsedQuicVersionVector loaded_advertised_versions =
      alternative_service_info_vector[1].advertised_versions();
  EXPECT_EQ(2u, loaded_advertised_versions.size());
  EXPECT_EQ(quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                                    quic::QUIC_VERSION_43),
            loaded_advertised_versions[0]);
  EXPECT_EQ(quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                                    quic::QUIC_VERSION_46),
            loaded_advertised_versions[1]);

  // No other fields should have been populated.
  server_info.alternative_services.reset();
  EXPECT_TRUE(server_info.empty());
}

TEST_F(HttpServerPropertiesManagerTest,
       UpdatePrefWhenAdvertisedVersionsChange) {
  InitializePrefs();

  const url::SchemeHostPort server_www("https", "www.google.com", 80);

  // #1: Set alternate protocol.
  AlternativeServiceInfoVector alternative_service_info_vector;
  // Quic alternative service set with a single QUIC version:
  // quic::QUIC_VERSION_46.
  AlternativeService quic_alternative_service1(kProtoQUIC, "", 443);
  base::Time expiration1;
  ASSERT_TRUE(base::Time::FromUTCString("2036-12-01 10:00:00", &expiration1));
  alternative_service_info_vector.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          quic_alternative_service1, expiration1, advertised_versions_));
  http_server_props_->SetAlternativeServices(server_www, NetworkIsolationKey(),
                                             alternative_service_info_vector);

  // Set quic_server_info string.
  quic::QuicServerId mail_quic_server_id("mail.google.com", 80, false);
  std::string quic_server_info1("quic_server_info1");
  http_server_props_->SetQuicServerInfo(
      mail_quic_server_id, NetworkIsolationKey(), quic_server_info1);

  // Set SupportsQuic.
  IPAddress actual_address(127, 0, 0, 1);
  http_server_props_->SetLastLocalAddressWhenQuicWorked(actual_address);

  // Update Prefs.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  // Verify preferences with correct advertised version field.
  const char expected_json[] =
      "{\"quic_servers\":"
      "[{\"isolation\":[],"
      "\"server_id\":\"https://mail.google.com:80\","
      "\"server_info\":\"quic_server_info1\"}],"
      "\"servers\":["
      "{\"alternative_service\":[{\"advertised_versions\":[46],"
      "\"expiration\":\"13756212000000000\",\"port\":443,"
      "\"protocol_str\":\"quic\"}],"
      "\"isolation\":[],"
      "\"server\":\"https://www.google.com:80\"}],"
      "\"supports_quic\":"
      "{\"address\":\"127.0.0.1\",\"used_quic\":true},\"version\":5}";

  const base::Value* http_server_properties =
      pref_delegate_->GetServerProperties();
  std::string preferences_json;
  EXPECT_TRUE(
      base::JSONWriter::Write(*http_server_properties, &preferences_json));
  EXPECT_EQ(expected_json, preferences_json);

  // #2: Set AlternativeService with different advertised_versions for the same
  // AlternativeService.
  AlternativeServiceInfoVector alternative_service_info_vector_2;
  // Quic alternative service set with two advertised QUIC versions.
  quic::ParsedQuicVersionVector advertised_versions = {
      quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                              quic::QUIC_VERSION_46),
      quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                              quic::QUIC_VERSION_43)};
  alternative_service_info_vector_2.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          quic_alternative_service1, expiration1, advertised_versions));
  http_server_props_->SetAlternativeServices(server_www, NetworkIsolationKey(),
                                             alternative_service_info_vector_2);

  // Update Prefs.
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());

  // Verify preferences updated with new advertised versions.
  const char expected_json_updated[] =
      "{\"quic_servers\":"
      "[{\"isolation\":[],"
      "\"server_id\":\"https://mail.google.com:80\","
      "\"server_info\":\"quic_server_info1\"}],"
      "\"servers\":["
      "{\"alternative_service\":[{\"advertised_versions\":[43,46],"
      "\"expiration\":\"13756212000000000\",\"port\":443,"
      "\"protocol_str\":\"quic\"}],"
      "\"isolation\":[],"
      "\"server\":\"https://www.google.com:80\"}],"
      "\"supports_quic\":"
      "{\"address\":\"127.0.0.1\",\"used_quic\":true},\"version\":5}";
  EXPECT_TRUE(
      base::JSONWriter::Write(*http_server_properties, &preferences_json));
  EXPECT_EQ(expected_json_updated, preferences_json);

  // #3: Set AlternativeService with same advertised_versions.
  AlternativeServiceInfoVector alternative_service_info_vector_3;
  // A same set of QUIC versions but listed in a different order.
  quic::ParsedQuicVersionVector advertised_versions_2 = {
      quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                              quic::QUIC_VERSION_43),
      quic::ParsedQuicVersion(quic::PROTOCOL_QUIC_CRYPTO,
                              quic::QUIC_VERSION_46)};
  alternative_service_info_vector_3.push_back(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          quic_alternative_service1, expiration1, advertised_versions_2));
  http_server_props_->SetAlternativeServices(server_www, NetworkIsolationKey(),
                                             alternative_service_info_vector_3);

  // No Prefs update.
  EXPECT_EQ(0u, GetPendingMainThreadTaskCount());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
}

TEST_F(HttpServerPropertiesManagerTest, UpdateCacheWithPrefs) {
  AlternativeService cached_broken_service(kProtoQUIC, "cached_broken", 443);
  AlternativeService cached_broken_service2(kProtoQUIC, "cached_broken2", 443);
  AlternativeService cached_recently_broken_service(kProtoQUIC,
                                                    "cached_rbroken", 443);

  http_server_props_->MarkAlternativeServiceBroken(cached_broken_service,
                                                   NetworkIsolationKey());
  http_server_props_->MarkAlternativeServiceBroken(cached_broken_service2,
                                                   NetworkIsolationKey());
  http_server_props_->MarkAlternativeServiceRecentlyBroken(
      cached_recently_broken_service, NetworkIsolationKey());

  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  // There should be a task to remove remove alt services from the cache of
  // broken alt services. There should be no task to update the prefs, since the
  // prefs file hasn't been loaded yet.
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());

  // Load the |pref_delegate_| with some JSON to verify updating the cache from
  // prefs. For the broken alternative services "www.google.com:1234" and
  // "cached_broken", the expiration time will be one day from now.

  std::string expiration_str =
      base::NumberToString(static_cast<int64_t>(one_day_from_now_.ToTimeT()));

  std::unique_ptr<base::Value> server_value = base::JSONReader::ReadDeprecated(
      "{"
      "\"broken_alternative_services\":["
      "{\"broken_until\":\"" +
      expiration_str +
      "\","
      "\"host\":\"www.google.com\",\"isolation\":[],"
      "\"port\":1234,\"protocol_str\":\"h2\"},"
      "{\"broken_count\":2,\"broken_until\":\"" +
      expiration_str +
      "\","
      "\"host\":\"cached_broken\",\"isolation\":[],"
      "\"port\":443,\"protocol_str\":\"quic\"},"
      "{\"broken_count\":3,"
      "\"host\":\"cached_rbroken\",\"isolation\":[],"
      "\"port\":443,\"protocol_str\":\"quic\"}],"
      "\"quic_servers\":["
      "{\"isolation\":[],"
      "\"server_id\":\"https://mail.google.com:80\","
      "\"server_info\":\"quic_server_info1\"}"
      "],"
      "\"servers\":["
      "{\"server\":\"https://www.google.com:80\","
      "\"isolation\":[],"
      "\"alternative_service\":["
      "{\"expiration\":\"13756212000000000\",\"port\":443,"
      "\"protocol_str\":\"h2\"},"
      "{\"expiration\":\"13758804000000000\",\"host\":\"www.google.com\","
      "\"port\":1234,\"protocol_str\":\"h2\"}"
      "]"
      "},"
      "{\"server\":\"https://mail.google.com:80\","
      "\"isolation\":[],"
      "\"alternative_service\":["
      "{\"expiration\":\"9223372036854775807\",\"host\":\"foo.google.com\","
      "\"port\":444,\"protocol_str\":\"h2\"}"
      "],"
      "\"network_stats\":{\"srtt\":42}"
      "}"
      "],"
      "\"supports_quic\":"
      "{\"address\":\"127.0.0.1\",\"used_quic\":true},"
      "\"version\":5"
      "}");
  ASSERT_TRUE(server_value);
  base::DictionaryValue* server_dict;
  ASSERT_TRUE(server_value->GetAsDictionary(&server_dict));

  // Don't use the test fixture's InitializePrefs() method, since there are
  // pending tasks. Initializing prefs should queue a pref update task, since
  // prefs have been modified.
  pref_delegate_->InitializePrefs(*server_dict);
  EXPECT_TRUE(http_server_props_->IsInitialized());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());

  // Run until prefs are updated.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  EXPECT_EQ(1, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());

  //
  // Verify alternative service info for https://www.google.com
  //
  AlternativeServiceInfoVector alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(
          url::SchemeHostPort("https", "www.google.com", 80),
          NetworkIsolationKey());
  ASSERT_EQ(2u, alternative_service_info_vector.size());

  EXPECT_EQ(kProtoHTTP2,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("www.google.com",
            alternative_service_info_vector[0].alternative_service().host);
  EXPECT_EQ(443, alternative_service_info_vector[0].alternative_service().port);
  EXPECT_EQ(
      "13756212000000000",
      base::NumberToString(
          alternative_service_info_vector[0].expiration().ToInternalValue()));

  EXPECT_EQ(kProtoHTTP2,
            alternative_service_info_vector[1].alternative_service().protocol);
  EXPECT_EQ("www.google.com",
            alternative_service_info_vector[1].alternative_service().host);
  EXPECT_EQ(1234,
            alternative_service_info_vector[1].alternative_service().port);
  EXPECT_EQ(
      "13758804000000000",
      base::NumberToString(
          alternative_service_info_vector[1].expiration().ToInternalValue()));

  //
  // Verify alternative service info for https://mail.google.com
  //
  alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(
          url::SchemeHostPort("https", "mail.google.com", 80),
          NetworkIsolationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());

  EXPECT_EQ(kProtoHTTP2,
            alternative_service_info_vector[0].alternative_service().protocol);
  EXPECT_EQ("foo.google.com",
            alternative_service_info_vector[0].alternative_service().host);
  EXPECT_EQ(444, alternative_service_info_vector[0].alternative_service().port);
  EXPECT_EQ(
      "9223372036854775807",
      base::NumberToString(
          alternative_service_info_vector[0].expiration().ToInternalValue()));

  //
  // Verify broken alternative services.
  //
  AlternativeService prefs_broken_service(kProtoHTTP2, "www.google.com", 1234);
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service2, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      prefs_broken_service, NetworkIsolationKey()));

  // Verify brokenness expiration times.
  // |cached_broken_service|'s expiration time should've been overwritten by the
  // prefs to be approximately 1 day from now. |cached_broken_service2|'s
  // expiration time should still be 5 minutes due to being marked broken.
  // |prefs_broken_service|'s expiration time should be approximately 1 day from
  // now which comes from the prefs.
  FastForwardBy(base::TimeDelta::FromMinutes(5) -
                HttpServerProperties::GetUpdatePrefsDelayForTesting());
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service2, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      prefs_broken_service, NetworkIsolationKey()));
  FastForwardBy(base::TimeDelta::FromDays(1));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service2, NetworkIsolationKey()));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      prefs_broken_service, NetworkIsolationKey()));

  // Now that |prefs_broken_service|'s brokenness has expired, it should've
  // been removed from the alternative services info vectors of all servers.
  alternative_service_info_vector =
      http_server_props_->GetAlternativeServiceInfos(
          url::SchemeHostPort("https", "www.google.com", 80),
          NetworkIsolationKey());
  ASSERT_EQ(1u, alternative_service_info_vector.size());

  //
  // Verify recently broken alternative services.
  //

  // If an entry is already in cache, the broken count in the prefs should
  // overwrite the one in the cache.
  // |prefs_broken_service| should have broken-count 1 from prefs.
  // |cached_recently_broken_service| should have broken-count 3 from prefs.
  // |cached_broken_service| should have broken-count 2 from prefs.
  // |cached_broken_service2| should have broken-count 1 from being marked
  // broken.

  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      prefs_broken_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      cached_recently_broken_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      cached_broken_service, NetworkIsolationKey()));
  EXPECT_TRUE(http_server_props_->WasAlternativeServiceRecentlyBroken(
      cached_broken_service2, NetworkIsolationKey()));
  // Make sure |prefs_broken_service| has the right expiration delay when marked
  // broken. Since |prefs_broken_service| had no broken_count specified in the
  // prefs, a broken_count value of 1 should have been assumed by
  // |http_server_props_|.
  http_server_props_->MarkAlternativeServiceBroken(prefs_broken_service,
                                                   NetworkIsolationKey());
  EXPECT_EQ(0, pref_delegate_->GetAndClearNumPrefUpdates());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardBy(base::TimeDelta::FromMinutes(10) -
                base::TimeDelta::FromInternalValue(1));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      prefs_broken_service, NetworkIsolationKey()));
  FastForwardBy(base::TimeDelta::FromInternalValue(1));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      prefs_broken_service, NetworkIsolationKey()));
  // Make sure |cached_recently_broken_service| has the right expiration delay
  // when marked broken.
  http_server_props_->MarkAlternativeServiceBroken(
      cached_recently_broken_service, NetworkIsolationKey());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardBy(base::TimeDelta::FromMinutes(40) -
                base::TimeDelta::FromInternalValue(1));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      cached_recently_broken_service, NetworkIsolationKey()));
  FastForwardBy(base::TimeDelta::FromInternalValue(1));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      cached_recently_broken_service, NetworkIsolationKey()));
  // Make sure |cached_broken_service| has the right expiration delay when
  // marked broken.
  http_server_props_->MarkAlternativeServiceBroken(cached_broken_service,
                                                   NetworkIsolationKey());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardBy(base::TimeDelta::FromMinutes(20) -
                base::TimeDelta::FromInternalValue(1));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service, NetworkIsolationKey()));
  FastForwardBy(base::TimeDelta::FromInternalValue(1));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service, NetworkIsolationKey()));
  // Make sure |cached_broken_service2| has the right expiration delay when
  // marked broken.
  http_server_props_->MarkAlternativeServiceBroken(cached_broken_service2,
                                                   NetworkIsolationKey());
  EXPECT_NE(0u, GetPendingMainThreadTaskCount());
  FastForwardBy(base::TimeDelta::FromMinutes(10) -
                base::TimeDelta::FromInternalValue(1));
  EXPECT_TRUE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service2, NetworkIsolationKey()));
  FastForwardBy(base::TimeDelta::FromInternalValue(1));
  EXPECT_FALSE(http_server_props_->IsAlternativeServiceBroken(
      cached_broken_service2, NetworkIsolationKey()));

  //
  // Verify ServerNetworkStats.
  //
  const ServerNetworkStats* server_network_stats =
      http_server_props_->GetServerNetworkStats(
          url::SchemeHostPort("https", "mail.google.com", 80),
          NetworkIsolationKey());
  EXPECT_TRUE(server_network_stats);
  EXPECT_EQ(server_network_stats->srtt, base::TimeDelta::FromInternalValue(42));

  //
  // Verify QUIC server info.
  //
  const std::string* quic_server_info = http_server_props_->GetQuicServerInfo(
      quic::QuicServerId("mail.google.com", 80, false), NetworkIsolationKey());
  EXPECT_EQ("quic_server_info1", *quic_server_info);

  //
  // Verify supports QUIC.
  //
  IPAddress actual_address(127, 0, 0, 1);
  EXPECT_TRUE(
      http_server_props_->WasLastLocalAddressWhenQuicWorked(actual_address));
  EXPECT_EQ(4, pref_delegate_->GetAndClearNumPrefUpdates());
}

// Check the interaction of ForceHTTP11 with saving/restoring settings.
// In particular, ForceHTTP11 is not saved, and it should not overwrite or be
// overitten by loaded data.
TEST_F(HttpServerPropertiesManagerTest, ForceHTTP11) {
  const url::SchemeHostPort kServer1("https", "foo.test", 443);
  const url::SchemeHostPort kServer2("https", "bar.test", 443);
  const url::SchemeHostPort kServer3("https", "baz.test", 443);

  // Create and initialize an HttpServerProperties with no state.
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  std::unique_ptr<HttpServerProperties> properties =
      std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                             /*net_log=*/nullptr,
                                             GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

  // Set kServer1 to support H2, but require HTTP/1.1.  Set kServer2 to only
  // require HTTP/1.1.
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer1, NetworkIsolationKey()));
  EXPECT_FALSE(properties->RequiresHTTP11(kServer1, NetworkIsolationKey()));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, NetworkIsolationKey()));
  EXPECT_FALSE(properties->RequiresHTTP11(kServer2, NetworkIsolationKey()));
  properties->SetSupportsSpdy(kServer1, NetworkIsolationKey(), true);
  properties->SetHTTP11Required(kServer1, NetworkIsolationKey());
  properties->SetHTTP11Required(kServer2, NetworkIsolationKey());
  EXPECT_TRUE(properties->GetSupportsSpdy(kServer1, NetworkIsolationKey()));
  EXPECT_TRUE(properties->RequiresHTTP11(kServer1, NetworkIsolationKey()));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, NetworkIsolationKey()));
  EXPECT_TRUE(properties->RequiresHTTP11(kServer2, NetworkIsolationKey()));

  // Wait until the data's been written to prefs, and then tear down the
  // HttpServerProperties.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  std::unique_ptr<base::DictionaryValue> saved_value =
      unowned_pref_delegate->GetServerProperties()->CreateDeepCopy();
  properties.reset();

  // Only information on kServer1 should have been saved to prefs.
  std::string preferences_json;
  base::JSONWriter::Write(*saved_value, &preferences_json);
  EXPECT_EQ(
      "{\"servers\":["
      "{\"isolation\":[],"
      "\"server\":\"https://foo.test\","
      "\"supports_spdy\":true}],"
      "\"version\":5}",
      preferences_json);

  // Create a new HttpServerProperties using the value saved to prefs above.
  pref_delegate = std::make_unique<MockPrefDelegate>();
  unowned_pref_delegate = pref_delegate.get();
  properties = std::make_unique<HttpServerProperties>(
      std::move(pref_delegate), /*net_log=*/nullptr, GetMockTickClock());

  // Before the data has loaded, set kServer1 and kServer3 as requiring
  // HTTP/1.1.
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer1, NetworkIsolationKey()));
  EXPECT_FALSE(properties->RequiresHTTP11(kServer1, NetworkIsolationKey()));
  properties->SetHTTP11Required(kServer1, NetworkIsolationKey());
  properties->SetHTTP11Required(kServer3, NetworkIsolationKey());
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer1, NetworkIsolationKey()));
  EXPECT_TRUE(properties->RequiresHTTP11(kServer1, NetworkIsolationKey()));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, NetworkIsolationKey()));
  EXPECT_FALSE(properties->RequiresHTTP11(kServer2, NetworkIsolationKey()));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer3, NetworkIsolationKey()));
  EXPECT_TRUE(properties->RequiresHTTP11(kServer3, NetworkIsolationKey()));

  // The data loads.
  unowned_pref_delegate->InitializePrefs(*saved_value);

  // The properties should contain a combination of the old and new data.
  EXPECT_TRUE(properties->GetSupportsSpdy(kServer1, NetworkIsolationKey()));
  EXPECT_TRUE(properties->RequiresHTTP11(kServer1, NetworkIsolationKey()));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, NetworkIsolationKey()));
  EXPECT_FALSE(properties->RequiresHTTP11(kServer2, NetworkIsolationKey()));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer3, NetworkIsolationKey()));
  EXPECT_TRUE(properties->RequiresHTTP11(kServer3, NetworkIsolationKey()));
}

TEST_F(HttpServerPropertiesManagerTest, NetworkIsolationKeyServerInfo) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const url::Origin kOpaqueOrigin =
      url::Origin::Create(GURL("data:text/plain,Hello World"));
  const url::SchemeHostPort kServer("https", "baz.test", 443);
  const url::SchemeHostPort kServer2("https", "zab.test", 443);

  HttpServerProperties::ServerInfo server_info;
  server_info.supports_spdy = true;

  for (auto save_network_isolation_key_mode : kNetworkIsolationKeyModes) {
    SCOPED_TRACE(static_cast<int>(save_network_isolation_key_mode));

    // Save prefs using |save_network_isolation_key_mode|.
    base::Value saved_value;
    {
      // Configure the the feature.
      std::unique_ptr<base::test::ScopedFeatureList> feature_list =
          SetNetworkIsolationKeyMode(save_network_isolation_key_mode);

      // This parameter is normally calculated by HttpServerProperties based on
      // the kPartitionHttpServerPropertiesByNetworkIsolationKey feature, but
      // this test doesn't use that class.
      bool use_network_isolation_key =
          save_network_isolation_key_mode != NetworkIsolationKeyMode::kDisabled;

      HttpServerProperties::ServerInfoMap server_info_map;

      // Add server info entry using two origins with value of |server_info|.
      // NetworkIsolationKey's constructor takes the state of the
      // kAppendFrameOriginToNetworkIsolationKey feature into account, so need
      // to make sure to call the constructor after setting up the feature
      // above.
      HttpServerProperties::ServerInfoMapKey server_info_key(
          kServer, NetworkIsolationKey(kOrigin1, kOrigin2),
          use_network_isolation_key);
      server_info_map.Put(server_info_key, server_info);

      // Also add an etry with an opaque origin, if |use_network_isolation_key|
      // is true. This value should not be saved to disk, since opaque origins
      // are only meaningful within a browsing session.
      if (use_network_isolation_key) {
        HttpServerProperties::ServerInfoMapKey server_info_key2(
            kServer2, NetworkIsolationKey(kOpaqueOrigin, kOpaqueOrigin),
            use_network_isolation_key);
        server_info_map.Put(server_info_key2, server_info);
      }

      saved_value = ServerInfoMapToValue(server_info_map);
    }

    for (auto load_network_isolation_key_mode : kNetworkIsolationKeyModes) {
      SCOPED_TRACE(static_cast<int>(load_network_isolation_key_mode));

      std::unique_ptr<base::test::ScopedFeatureList> feature_list =
          SetNetworkIsolationKeyMode(load_network_isolation_key_mode);
      std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map2 =
          ValueToServerInfoMap(saved_value);
      ASSERT_TRUE(server_info_map2);
      if (save_network_isolation_key_mode ==
          NetworkIsolationKeyMode::kDisabled) {
        // If NetworkIsolationKey was disabled when saving, it was saved with an
        // empty NetworkIsolationKey, which should always be loaded
        // successfully. This is needed to continue to support consumers that
        // don't use NetworkIsolationKeys.
        ASSERT_EQ(1u, server_info_map2->size());
        const HttpServerProperties::ServerInfoMapKey& server_info_key2 =
            server_info_map2->begin()->first;
        const HttpServerProperties::ServerInfo& server_info2 =
            server_info_map2->begin()->second;
        EXPECT_EQ(kServer, server_info_key2.server);
        EXPECT_EQ(NetworkIsolationKey(),
                  server_info_key2.network_isolation_key);
        EXPECT_EQ(server_info, server_info2);
      } else if (save_network_isolation_key_mode ==
                 load_network_isolation_key_mode) {
        // If the save and load modes are the same, the load should succeed, and
        // the network isolation keys should match.
        ASSERT_EQ(1u, server_info_map2->size());
        const HttpServerProperties::ServerInfoMapKey& server_info_key2 =
            server_info_map2->begin()->first;
        const HttpServerProperties::ServerInfo& server_info2 =
            server_info_map2->begin()->second;
        EXPECT_EQ(kServer, server_info_key2.server);
        EXPECT_EQ(NetworkIsolationKey(kOrigin1, kOrigin2),
                  server_info_key2.network_isolation_key);
        EXPECT_EQ(server_info, server_info2);
      } else {
        // Otherwise, the NetworkIsolationKey doesn't make sense with the
        // current feature values, so the ServerInfo should be discarded.
        EXPECT_EQ(0u, server_info_map2->size());
      }
    }
  }
}

// Tests a full round trip with a NetworkIsolationKey, using the
// HttpServerProperties interface.
TEST_F(HttpServerPropertiesManagerTest, NetworkIsolationKeyIntegration) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://foo.test/"));
  const NetworkIsolationKey kNetworkIsolationKey(kOrigin, kOrigin);
  const url::SchemeHostPort kServer("https", "baz.test", 443);

  const url::Origin kOpaqueOrigin =
      url::Origin::Create(GURL("data:text/plain,Hello World"));
  const NetworkIsolationKey kOpaqueOriginNetworkIsolationKey(kOpaqueOrigin,
                                                             kOpaqueOrigin);
  const url::SchemeHostPort kServer2("https", "zab.test", 443);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  // Create and initialize an HttpServerProperties with no state.
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  std::unique_ptr<HttpServerProperties> properties =
      std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                             /*net_log=*/nullptr,
                                             GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

  // Set a values using kNetworkIsolationKey.
  properties->SetSupportsSpdy(kServer, kNetworkIsolationKey, true);
  EXPECT_TRUE(properties->GetSupportsSpdy(kServer, kNetworkIsolationKey));
  EXPECT_FALSE(
      properties->GetSupportsSpdy(kServer, kOpaqueOriginNetworkIsolationKey));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer, NetworkIsolationKey()));

  // Opaque origins should works with HttpServerProperties, but not be persisted
  // to disk.
  properties->SetSupportsSpdy(kServer2, kOpaqueOriginNetworkIsolationKey, true);
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, kNetworkIsolationKey));
  EXPECT_TRUE(
      properties->GetSupportsSpdy(kServer2, kOpaqueOriginNetworkIsolationKey));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, NetworkIsolationKey()));

  // Wait until the data's been written to prefs, and then tear down the
  // HttpServerProperties.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  std::unique_ptr<base::DictionaryValue> saved_value =
      unowned_pref_delegate->GetServerProperties()->CreateDeepCopy();
  properties.reset();

  // Create a new HttpServerProperties using the value saved to prefs above.
  pref_delegate = std::make_unique<MockPrefDelegate>();
  unowned_pref_delegate = pref_delegate.get();
  properties = std::make_unique<HttpServerProperties>(
      std::move(pref_delegate), /*net_log=*/nullptr, GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(*saved_value);

  // The information set using kNetworkIsolationKey on the original
  // HttpServerProperties should also be set on the restored
  // HttpServerProperties.
  EXPECT_TRUE(properties->GetSupportsSpdy(kServer, kNetworkIsolationKey));
  EXPECT_FALSE(
      properties->GetSupportsSpdy(kServer, kOpaqueOriginNetworkIsolationKey));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer, NetworkIsolationKey()));

  // The information set using kOpaqueOriginNetworkIsolationKey should not have
  // been restored.
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, kNetworkIsolationKey));
  EXPECT_FALSE(
      properties->GetSupportsSpdy(kServer2, kOpaqueOriginNetworkIsolationKey));
  EXPECT_FALSE(properties->GetSupportsSpdy(kServer2, NetworkIsolationKey()));
}

// Tests a full round trip to prefs and back in the canonical suffix case.
// Enable NetworkIsolationKeys, as they have some interactions with the
// canonical suffix logic.
TEST_F(HttpServerPropertiesManagerTest,
       CanonicalSuffixRoundTripWithNetworkIsolationKey) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);
  // Three servers with the same canonical suffix (".c.youtube.com").
  const url::SchemeHostPort kServer1("https", "foo.c.youtube.com", 443);
  const url::SchemeHostPort kServer2("https", "bar.c.youtube.com", 443);
  const url::SchemeHostPort kServer3("https", "baz.c.youtube.com", 443);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  // Create three alt service vectors of different lengths.
  base::Time expiration = base::Time::Now() + base::TimeDelta::FromDays(1);
  AlternativeServiceInfo alt_service1 =
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          AlternativeService(kProtoQUIC, "foopy.c.youtube.com", 1234),
          expiration,
          HttpNetworkSession::Params().quic_params.supported_versions);
  AlternativeServiceInfo alt_service2 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foopy.c.youtube.com", 443),
          expiration);
  AlternativeServiceInfo alt_service3 =
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          AlternativeService(kProtoHTTP2, "foopy2.c.youtube.com", 443),
          expiration);
  AlternativeServiceInfoVector alt_service_vector1 = {alt_service1};
  AlternativeServiceInfoVector alt_service_vector2 = {alt_service1,
                                                      alt_service2};
  AlternativeServiceInfoVector alt_service_vector3 = {
      alt_service1, alt_service2, alt_service3};

  // Create and initialize an HttpServerProperties with no state.
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  std::unique_ptr<HttpServerProperties> properties =
      std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                             /*net_log=*/nullptr,
                                             GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

  // Set alternative services for kServer1 using kNetworkIsolationKey1. That
  // information should be retrieved when fetching information for any server
  // with the same canonical suffix, when using kNetworkIsolationKey1.
  properties->SetAlternativeServices(kServer1, kNetworkIsolationKey1,
                                     alt_service_vector1);
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer2, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer3, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      0u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey2)
          .size());

  // Set different alternative services for kServer2 using
  // kNetworkIsolationKey1. It should not affect information retrieved for
  // kServer1, but should for kServer2 and kServer3.
  properties->SetAlternativeServices(kServer2, kNetworkIsolationKey1,
                                     alt_service_vector2);
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      2u,
      properties->GetAlternativeServiceInfos(kServer2, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      2u,
      properties->GetAlternativeServiceInfos(kServer3, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      0u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey2)
          .size());

  // Set different information for kServer1 using kNetworkIsolationKey2. It
  // should not affect information stored for kNetworkIsolationKey1.
  properties->SetAlternativeServices(kServer1, kNetworkIsolationKey2,
                                     alt_service_vector3);
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      2u,
      properties->GetAlternativeServiceInfos(kServer2, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      2u,
      properties->GetAlternativeServiceInfos(kServer3, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      3u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey2)
          .size());
  EXPECT_EQ(
      3u,
      properties->GetAlternativeServiceInfos(kServer2, kNetworkIsolationKey2)
          .size());
  EXPECT_EQ(
      3u,
      properties->GetAlternativeServiceInfos(kServer3, kNetworkIsolationKey2)
          .size());

  // Wait until the data's been written to prefs, and then tear down the
  // HttpServerProperties.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  std::unique_ptr<base::DictionaryValue> saved_value =
      unowned_pref_delegate->GetServerProperties()->CreateDeepCopy();
  properties.reset();

  // Create a new HttpServerProperties using the value saved to prefs above.
  pref_delegate = std::make_unique<MockPrefDelegate>();
  unowned_pref_delegate = pref_delegate.get();
  properties = std::make_unique<HttpServerProperties>(
      std::move(pref_delegate), /*net_log=*/nullptr, GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(*saved_value);

  // Only the first of the values learned for kNetworkIsolationKey1 should have
  // been saved, and the value for kNetworkIsolationKey2 as well. The canonical
  // suffix logic should still be respected.
  //
  // TODO(mmenke): Preferring the oldest value seems unexpected.
  // https://crbug.com/994842.
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer2, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      1u,
      properties->GetAlternativeServiceInfos(kServer3, kNetworkIsolationKey1)
          .size());
  EXPECT_EQ(
      3u,
      properties->GetAlternativeServiceInfos(kServer1, kNetworkIsolationKey2)
          .size());
  EXPECT_EQ(
      3u,
      properties->GetAlternativeServiceInfos(kServer2, kNetworkIsolationKey2)
          .size());
  EXPECT_EQ(
      3u,
      properties->GetAlternativeServiceInfos(kServer3, kNetworkIsolationKey2)
          .size());
}

// Tests a full round trip with a NetworkIsolationKey, using the
// HttpServerProperties interface and setting alternative services as broken.
TEST_F(HttpServerPropertiesManagerTest,
       NetworkIsolationKeyBrokenAltServiceRoundTrip) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo1.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://foo2.test/"));

  const AlternativeService kAlternativeService1(kProtoHTTP2,
                                                "alt.service1.test", 443);
  const AlternativeService kAlternativeService2(kProtoHTTP2,
                                                "alt.service2.test", 443);

  for (auto save_network_isolation_key_mode : kNetworkIsolationKeyModes) {
    SCOPED_TRACE(static_cast<int>(save_network_isolation_key_mode));

    // Save prefs using |save_network_isolation_key_mode|.
    std::unique_ptr<base::DictionaryValue> saved_value;
    {
      // Configure the the feature.
      std::unique_ptr<base::test::ScopedFeatureList> feature_list =
          SetNetworkIsolationKeyMode(save_network_isolation_key_mode);

      // The NetworkIsolationKey constructor checks the field trial state, so
      // need to create the keys only after setting up the field trials.
      const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
      const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

      // Create and initialize an HttpServerProperties, must be done after
      // setting the feature.
      std::unique_ptr<MockPrefDelegate> pref_delegate =
          std::make_unique<MockPrefDelegate>();
      MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
      std::unique_ptr<HttpServerProperties> properties =
          std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                                 /*net_log=*/nullptr,
                                                 GetMockTickClock());
      unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

      // Set kAlternativeService1 as broken in the context of
      // kNetworkIsolationKey1, and kAlternativeService2 as broken in the
      // context of the empty NetworkIsolationKey2, and recently broken in the
      // context of the empty NetworkIsolationKey.
      properties->MarkAlternativeServiceBroken(kAlternativeService1,
                                               kNetworkIsolationKey1);
      properties->MarkAlternativeServiceRecentlyBroken(kAlternativeService2,
                                                       NetworkIsolationKey());
      properties->MarkAlternativeServiceBroken(kAlternativeService2,
                                               kNetworkIsolationKey2);

      // Verify values were set.
      EXPECT_TRUE(properties->IsAlternativeServiceBroken(
          kAlternativeService1, kNetworkIsolationKey1));
      EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
          kAlternativeService1, kNetworkIsolationKey1));
      // When NetworkIsolationKeys are disabled, kAlternativeService2 is marked
      // as broken regardless of the values passed to NetworkIsolationKey's
      // constructor.
      EXPECT_EQ(
          save_network_isolation_key_mode == NetworkIsolationKeyMode::kDisabled,
          properties->IsAlternativeServiceBroken(kAlternativeService2,
                                                 NetworkIsolationKey()));
      EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
          kAlternativeService2, NetworkIsolationKey()));
      EXPECT_TRUE(properties->IsAlternativeServiceBroken(
          kAlternativeService2, kNetworkIsolationKey2));
      EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
          kAlternativeService2, kNetworkIsolationKey2));

      // If NetworkIsolationKeys are enabled, there should be no
      // cross-contamination of the NetworkIsolationKeys.
      if (save_network_isolation_key_mode !=
          NetworkIsolationKeyMode::kDisabled) {
        EXPECT_FALSE(properties->IsAlternativeServiceBroken(
            kAlternativeService2, kNetworkIsolationKey1));
        EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService2, kNetworkIsolationKey1));
        EXPECT_FALSE(properties->IsAlternativeServiceBroken(
            kAlternativeService1, NetworkIsolationKey()));
        EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService1, NetworkIsolationKey()));
        EXPECT_FALSE(properties->IsAlternativeServiceBroken(
            kAlternativeService1, kNetworkIsolationKey2));
        EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService1, kNetworkIsolationKey2));
      }

      // Wait until the data's been written to prefs, and then create a copy of
      // the prefs data.
      FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
      saved_value =
          unowned_pref_delegate->GetServerProperties()->CreateDeepCopy();
    }

    // Now try and load the data in each of the feature modes.
    for (auto load_network_isolation_key_mode : kNetworkIsolationKeyModes) {
      SCOPED_TRACE(static_cast<int>(load_network_isolation_key_mode));

      std::unique_ptr<base::test::ScopedFeatureList> feature_list =
          SetNetworkIsolationKeyMode(load_network_isolation_key_mode);

      // The NetworkIsolationKey constructor checks the field trial state, so
      // need to create the keys only after setting up the field trials.
      const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
      const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

      // Create a new HttpServerProperties, loading the data from before.
      std::unique_ptr<MockPrefDelegate> pref_delegate =
          std::make_unique<MockPrefDelegate>();
      MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
      std::unique_ptr<HttpServerProperties> properties =
          std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                                 /*net_log=*/nullptr,
                                                 GetMockTickClock());
      unowned_pref_delegate->InitializePrefs(*saved_value);

      if (save_network_isolation_key_mode ==
          NetworkIsolationKeyMode::kDisabled) {
        // If NetworkIsolationKey was disabled when saving, it was saved with an
        // empty NetworkIsolationKey, which should always be loaded
        // successfully. This is needed to continue to support consumers that
        // don't use NetworkIsolationKeys.
        EXPECT_TRUE(properties->IsAlternativeServiceBroken(
            kAlternativeService1, NetworkIsolationKey()));
        EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService1, NetworkIsolationKey()));
        EXPECT_TRUE(properties->IsAlternativeServiceBroken(
            kAlternativeService2, NetworkIsolationKey()));
        EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService2, NetworkIsolationKey()));
      } else if (save_network_isolation_key_mode ==
                 load_network_isolation_key_mode) {
        // If the save and load modes are the same, the load should succeed, and
        // the network isolation keys should match.
        EXPECT_TRUE(properties->IsAlternativeServiceBroken(
            kAlternativeService1, kNetworkIsolationKey1));
        EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService1, kNetworkIsolationKey1));
        // When NetworkIsolationKeys are disabled, kAlternativeService2 is
        // marked as broken regardless of the values passed to
        // NetworkIsolationKey's constructor.
        EXPECT_EQ(save_network_isolation_key_mode ==
                      NetworkIsolationKeyMode::kDisabled,
                  properties->IsAlternativeServiceBroken(
                      kAlternativeService2, NetworkIsolationKey()));
        EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService2, NetworkIsolationKey()));
        EXPECT_TRUE(properties->IsAlternativeServiceBroken(
            kAlternativeService2, kNetworkIsolationKey2));
        EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService2, kNetworkIsolationKey2));

        // If NetworkIsolationKeys are enabled, there should be no
        // cross-contamination of the NetworkIsolationKeys.
        if (save_network_isolation_key_mode !=
            NetworkIsolationKeyMode::kDisabled) {
          EXPECT_FALSE(properties->IsAlternativeServiceBroken(
              kAlternativeService2, kNetworkIsolationKey1));
          EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
              kAlternativeService2, kNetworkIsolationKey1));
          EXPECT_FALSE(properties->IsAlternativeServiceBroken(
              kAlternativeService1, NetworkIsolationKey()));
          EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
              kAlternativeService1, NetworkIsolationKey()));
          EXPECT_FALSE(properties->IsAlternativeServiceBroken(
              kAlternativeService1, kNetworkIsolationKey2));
          EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
              kAlternativeService1, kNetworkIsolationKey2));
        }
      } else {
        // Otherwise, only the values set with an empty NetworkIsolationKey
        // should have been loaded successfully.
        EXPECT_FALSE(properties->IsAlternativeServiceBroken(
            kAlternativeService1, kNetworkIsolationKey1));
        EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService1, kNetworkIsolationKey1));
        EXPECT_FALSE(properties->IsAlternativeServiceBroken(
            kAlternativeService2, NetworkIsolationKey()));
        EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
            kAlternativeService2, NetworkIsolationKey()));
        EXPECT_FALSE(properties->IsAlternativeServiceBroken(
            kAlternativeService2, kNetworkIsolationKey2));
        // If the load mode is NetworkIsolationKeyMode::kDisabled,
        // kNetworkIsolationKey2 is NetworkIsolationKey().
        EXPECT_EQ(load_network_isolation_key_mode ==
                      NetworkIsolationKeyMode::kDisabled,
                  properties->WasAlternativeServiceRecentlyBroken(
                      kAlternativeService2, kNetworkIsolationKey2));

        // There should be no cross-contamination of NetworkIsolationKeys, if
        // NetworkIsolationKeys are enabled.
        if (load_network_isolation_key_mode !=
            NetworkIsolationKeyMode::kDisabled) {
          EXPECT_FALSE(properties->IsAlternativeServiceBroken(
              kAlternativeService2, kNetworkIsolationKey1));
          EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
              kAlternativeService2, kNetworkIsolationKey1));
          EXPECT_FALSE(properties->IsAlternativeServiceBroken(
              kAlternativeService1, NetworkIsolationKey()));
          EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
              kAlternativeService1, NetworkIsolationKey()));
          EXPECT_FALSE(properties->IsAlternativeServiceBroken(
              kAlternativeService1, kNetworkIsolationKey2));
          EXPECT_FALSE(properties->WasAlternativeServiceRecentlyBroken(
              kAlternativeService1, kNetworkIsolationKey2));
        }
      }
    }
  }
}

// Make sure broken alt services with opaque origins aren't saved.
TEST_F(HttpServerPropertiesManagerTest,
       NetworkIsolationKeyBrokenAltServiceOpaqueOrigin) {
  const url::Origin kOpaqueOrigin =
      url::Origin::Create(GURL("data:text/plain,Hello World"));
  const NetworkIsolationKey kNetworkIsolationKey(kOpaqueOrigin, kOpaqueOrigin);
  const AlternativeService kAlternativeService(kProtoHTTP2, "alt.service1.test",
                                               443);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  // Create and initialize an HttpServerProperties, must be done after
  // setting the feature.
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  std::unique_ptr<HttpServerProperties> properties =
      std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                             /*net_log=*/nullptr,
                                             GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

  properties->MarkAlternativeServiceBroken(kAlternativeService,
                                           kNetworkIsolationKey);

  // Verify values were set.
  EXPECT_TRUE(properties->IsAlternativeServiceBroken(kAlternativeService,
                                                     kNetworkIsolationKey));
  EXPECT_TRUE(properties->WasAlternativeServiceRecentlyBroken(
      kAlternativeService, kNetworkIsolationKey));

  // Wait until the data's been written to prefs, and then create a copy of
  // the prefs data.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());

  // No information should have been saved to prefs.
  std::string preferences_json;
  base::JSONWriter::Write(*unowned_pref_delegate->GetServerProperties(),
                          &preferences_json);
  EXPECT_EQ("{\"servers\":[],\"version\":5}", preferences_json);
}

// Tests a full round trip with a NetworkIsolationKey, using the
// HttpServerProperties interface and setting QuicServerInfo.
TEST_F(HttpServerPropertiesManagerTest,
       NetworkIsolationKeyQuicServerInfoRoundTrip) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo1.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://foo2.test/"));

  const quic::QuicServerId kServer1("foo", 443,
                                    false /* privacy_mode_enabled */);
  const quic::QuicServerId kServer2("foo", 443,
                                    true /* privacy_mode_enabled */);

  const char kQuicServerInfo1[] = "info1";
  const char kQuicServerInfo2[] = "info2";
  const char kQuicServerInfo3[] = "info3";

  for (auto save_network_isolation_key_mode : kNetworkIsolationKeyModes) {
    SCOPED_TRACE(static_cast<int>(save_network_isolation_key_mode));

    // Save prefs using |save_network_isolation_key_mode|.
    std::unique_ptr<base::DictionaryValue> saved_value;
    {
      // Configure the the feature.
      std::unique_ptr<base::test::ScopedFeatureList> feature_list =
          SetNetworkIsolationKeyMode(save_network_isolation_key_mode);

      // The NetworkIsolationKey constructor checks the field trial state, so
      // need to create the keys only after setting up the field trials.
      const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
      const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

      // Create and initialize an HttpServerProperties, must be done after
      // setting the feature.
      std::unique_ptr<MockPrefDelegate> pref_delegate =
          std::make_unique<MockPrefDelegate>();
      MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
      std::unique_ptr<HttpServerProperties> properties =
          std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                                 /*net_log=*/nullptr,
                                                 GetMockTickClock());
      unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

      // Set kServer1 to kQuicServerInfo1 in the context of
      // kNetworkIsolationKey1, Set kServer2 to kQuicServerInfo2 in the context
      // of kNetworkIsolationKey2, and kServer1 to kQuicServerInfo3 in the
      // context of NetworkIsolationKey().
      properties->SetQuicServerInfo(kServer1, kNetworkIsolationKey1,
                                    kQuicServerInfo1);
      properties->SetQuicServerInfo(kServer2, kNetworkIsolationKey2,
                                    kQuicServerInfo2);
      properties->SetQuicServerInfo(kServer1, NetworkIsolationKey(),
                                    kQuicServerInfo3);

      // Verify values were set.
      if (save_network_isolation_key_mode !=
          NetworkIsolationKeyMode::kDisabled) {
        EXPECT_EQ(kQuicServerInfo1, *properties->GetQuicServerInfo(
                                        kServer1, kNetworkIsolationKey1));
        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer1, kNetworkIsolationKey2));
        EXPECT_EQ(kQuicServerInfo3, *properties->GetQuicServerInfo(
                                        kServer1, NetworkIsolationKey()));

        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, kNetworkIsolationKey1));
        EXPECT_EQ(kQuicServerInfo2, *properties->GetQuicServerInfo(
                                        kServer2, kNetworkIsolationKey2));
        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, NetworkIsolationKey()));
      } else {
        EXPECT_EQ(kQuicServerInfo3, *properties->GetQuicServerInfo(
                                        kServer1, NetworkIsolationKey()));
        EXPECT_EQ(kQuicServerInfo2, *properties->GetQuicServerInfo(
                                        kServer2, NetworkIsolationKey()));
      }

      // Wait until the data's been written to prefs, and then create a copy of
      // the prefs data.
      FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
      saved_value =
          unowned_pref_delegate->GetServerProperties()->CreateDeepCopy();
    }

    // Now try and load the data in each of the feature modes.
    for (auto load_network_isolation_key_mode : kNetworkIsolationKeyModes) {
      SCOPED_TRACE(static_cast<int>(load_network_isolation_key_mode));

      std::unique_ptr<base::test::ScopedFeatureList> feature_list =
          SetNetworkIsolationKeyMode(load_network_isolation_key_mode);

      // The NetworkIsolationKey constructor checks the field trial state, so
      // need to create the keys only after setting up the field trials.
      const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
      const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

      // Create a new HttpServerProperties, loading the data from before.
      std::unique_ptr<MockPrefDelegate> pref_delegate =
          std::make_unique<MockPrefDelegate>();
      MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
      std::unique_ptr<HttpServerProperties> properties =
          std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                                 /*net_log=*/nullptr,
                                                 GetMockTickClock());
      unowned_pref_delegate->InitializePrefs(*saved_value);

      if (save_network_isolation_key_mode ==
          NetworkIsolationKeyMode::kDisabled) {
        // If NetworkIsolationKey was disabled when saving, entries were saved
        // with an empty NetworkIsolationKey, which should always be loaded
        // successfully. This is needed to continue to support consumers that
        // don't use NetworkIsolationKeys.
        EXPECT_EQ(kQuicServerInfo3, *properties->GetQuicServerInfo(
                                        kServer1, NetworkIsolationKey()));
        EXPECT_EQ(kQuicServerInfo2, *properties->GetQuicServerInfo(
                                        kServer2, NetworkIsolationKey()));
        if (load_network_isolation_key_mode !=
            NetworkIsolationKeyMode::kDisabled) {
          EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                                 kServer1, kNetworkIsolationKey1));
          EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                                 kServer1, kNetworkIsolationKey2));

          EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                                 kServer2, kNetworkIsolationKey1));
          EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                                 kServer2, kNetworkIsolationKey2));
        }
      } else if (save_network_isolation_key_mode ==
                 load_network_isolation_key_mode) {
        // If the save and load modes are the same, the load should succeed, and
        // the network isolation keys should match.
        EXPECT_EQ(kQuicServerInfo1, *properties->GetQuicServerInfo(
                                        kServer1, kNetworkIsolationKey1));
        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer1, kNetworkIsolationKey2));
        EXPECT_EQ(kQuicServerInfo3, *properties->GetQuicServerInfo(
                                        kServer1, NetworkIsolationKey()));

        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, kNetworkIsolationKey1));
        EXPECT_EQ(kQuicServerInfo2, *properties->GetQuicServerInfo(
                                        kServer2, kNetworkIsolationKey2));
        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, NetworkIsolationKey()));
      } else {
        // Otherwise, only the value set with an empty NetworkIsolationKey
        // should have been loaded successfully.
        EXPECT_EQ(kQuicServerInfo3, *properties->GetQuicServerInfo(
                                        kServer1, NetworkIsolationKey()));

        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, kNetworkIsolationKey1));
        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, kNetworkIsolationKey2));
        EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                               kServer2, NetworkIsolationKey()));

        // There should be no cross-contamination of NetworkIsolationKeys, if
        // NetworkIsolationKeys are enabled.
        if (load_network_isolation_key_mode !=
            NetworkIsolationKeyMode::kDisabled) {
          EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                                 kServer1, kNetworkIsolationKey1));
          EXPECT_EQ(nullptr, properties->GetQuicServerInfo(
                                 kServer1, kNetworkIsolationKey2));
        }
      }
    }
  }
}

// Tests a full round trip to prefs and back in the canonical suffix for
// QuicServerInfo case. Enable NetworkIsolationKeys, as they have some
// interactions with the canonical suffix logic.
TEST_F(HttpServerPropertiesManagerTest,
       NetworkIsolationKeyQuicServerInfoCanonicalSuffixRoundTrip) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const NetworkIsolationKey kNetworkIsolationKey1(kOrigin1, kOrigin1);
  const NetworkIsolationKey kNetworkIsolationKey2(kOrigin2, kOrigin2);

  // Three servers with the same canonical suffix (".c.youtube.com").
  const quic::QuicServerId kServer1("foo.c.youtube.com", 443,
                                    false /* privacy_mode_enabled */);
  const quic::QuicServerId kServer2("bar.c.youtube.com", 443,
                                    false /* privacy_mode_enabled */);
  const quic::QuicServerId kServer3("baz.c.youtube.com", 443,
                                    false /* privacy_mode_enabled */);

  const char kQuicServerInfo1[] = "info1";
  const char kQuicServerInfo2[] = "info2";
  const char kQuicServerInfo3[] = "info3";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  // Create and initialize an HttpServerProperties with no state.
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  std::unique_ptr<HttpServerProperties> properties =
      std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                             /*net_log=*/nullptr,
                                             GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

  // Set kQuicServerInfo1 for kServer1 using kNetworkIsolationKey1. That
  // information should be retrieved when fetching information for any server
  // with the same canonical suffix, when using kNetworkIsolationKey1.
  properties->SetQuicServerInfo(kServer1, kNetworkIsolationKey1,
                                kQuicServerInfo1);
  EXPECT_EQ(kQuicServerInfo1,
            *properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo1,
            *properties->GetQuicServerInfo(kServer2, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo1,
            *properties->GetQuicServerInfo(kServer3, kNetworkIsolationKey1));
  EXPECT_FALSE(properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey2));

  // Set kQuicServerInfo2 for kServer2 using kNetworkIsolationKey1. It should
  // not affect information retrieved for kServer1, but should for kServer2 and
  // kServer3.
  properties->SetQuicServerInfo(kServer2, kNetworkIsolationKey1,
                                kQuicServerInfo2);
  EXPECT_EQ(kQuicServerInfo1,
            *properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo2,
            *properties->GetQuicServerInfo(kServer2, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo2,
            *properties->GetQuicServerInfo(kServer3, kNetworkIsolationKey1));
  EXPECT_FALSE(properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey2));

  // Set kQuicServerInfo3 for kServer1 using kNetworkIsolationKey2. It should
  // not affect information stored for kNetworkIsolationKey1.
  properties->SetQuicServerInfo(kServer1, kNetworkIsolationKey2,
                                kQuicServerInfo3);
  EXPECT_EQ(kQuicServerInfo1,
            *properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo2,
            *properties->GetQuicServerInfo(kServer2, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo2,
            *properties->GetQuicServerInfo(kServer3, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo3,
            *properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey2));
  EXPECT_EQ(kQuicServerInfo3,
            *properties->GetQuicServerInfo(kServer2, kNetworkIsolationKey2));
  EXPECT_EQ(kQuicServerInfo3,
            *properties->GetQuicServerInfo(kServer3, kNetworkIsolationKey2));

  // Wait until the data's been written to prefs, and then tear down the
  // HttpServerProperties.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());
  std::unique_ptr<base::DictionaryValue> saved_value =
      unowned_pref_delegate->GetServerProperties()->CreateDeepCopy();
  properties.reset();

  // Create a new HttpServerProperties using the value saved to prefs above.
  pref_delegate = std::make_unique<MockPrefDelegate>();
  unowned_pref_delegate = pref_delegate.get();
  properties = std::make_unique<HttpServerProperties>(
      std::move(pref_delegate), /*net_log=*/nullptr, GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(*saved_value);

  // All values should have been saved and be retrievable by suffix-matching
  // servers.
  //
  // TODO(mmenke): The rest of this test corresponds exactly to behavior in
  // CanonicalSuffixRoundTripWithNetworkIsolationKey. It seems like these lines
  // should correspond as well.
  EXPECT_EQ(kQuicServerInfo1,
            *properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo2,
            *properties->GetQuicServerInfo(kServer2, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo2,
            *properties->GetQuicServerInfo(kServer3, kNetworkIsolationKey1));
  EXPECT_EQ(kQuicServerInfo3,
            *properties->GetQuicServerInfo(kServer1, kNetworkIsolationKey2));
  EXPECT_EQ(kQuicServerInfo3,
            *properties->GetQuicServerInfo(kServer2, kNetworkIsolationKey2));
  EXPECT_EQ(kQuicServerInfo3,
            *properties->GetQuicServerInfo(kServer3, kNetworkIsolationKey2));
}

// Make sure QuicServerInfo associated with NetworkIsolationKeys with opaque
// origins aren't saved.
TEST_F(HttpServerPropertiesManagerTest,
       NetworkIsolationKeyQuicServerInfoOpaqueOrigin) {
  const url::Origin kOpaqueOrigin =
      url::Origin::Create(GURL("data:text/plain,Hello World"));
  const NetworkIsolationKey kNetworkIsolationKey(kOpaqueOrigin, kOpaqueOrigin);
  const quic::QuicServerId kServer("foo", 443,
                                   false /* privacy_mode_enabled */);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionHttpServerPropertiesByNetworkIsolationKey);

  // Create and initialize an HttpServerProperties, must be done after
  // setting the feature.
  std::unique_ptr<MockPrefDelegate> pref_delegate =
      std::make_unique<MockPrefDelegate>();
  MockPrefDelegate* unowned_pref_delegate = pref_delegate.get();
  std::unique_ptr<HttpServerProperties> properties =
      std::make_unique<HttpServerProperties>(std::move(pref_delegate),
                                             /*net_log=*/nullptr,
                                             GetMockTickClock());
  unowned_pref_delegate->InitializePrefs(base::DictionaryValue());

  properties->SetQuicServerInfo(kServer, kNetworkIsolationKey,
                                "QuicServerInfo");
  EXPECT_TRUE(properties->GetQuicServerInfo(kServer, kNetworkIsolationKey));

  // Wait until the data's been written to prefs, and then create a copy of
  // the prefs data.
  FastForwardBy(HttpServerProperties::GetUpdatePrefsDelayForTesting());

  // No information should have been saved to prefs.
  std::string preferences_json;
  base::JSONWriter::Write(*unowned_pref_delegate->GetServerProperties(),
                          &preferences_json);
  EXPECT_EQ("{\"quic_servers\":[],\"servers\":[],\"version\":5}",
            preferences_json);
}

}  // namespace net
