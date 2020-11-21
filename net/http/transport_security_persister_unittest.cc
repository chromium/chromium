// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_persister.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/transport_security_state.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kReportUri[] = "http://www.example.test/report";

// The bool indicates whether kPartitionExpectCTStateByNetworkIsolationKey
// should be enabled.
class TransportSecurityPersisterTest : public ::testing::TestWithParam<bool>,
                                       public WithTaskEnvironment {
 public:
  TransportSecurityPersisterTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Mock out time so that entries with hard-coded json data can be
    // successfully loaded. Use a large enough value that dynamically created
    // entries have at least somewhat interesting expiration times.
    FastForwardBy(base::TimeDelta::FromDays(3660));
  }

  ~TransportSecurityPersisterTest() override {
    EXPECT_TRUE(base::CurrentIOThread::IsSet());
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CurrentIOThread::IsSet());
    scoped_refptr<base::SequencedTaskRunner> background_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
    // This feature is used in initializing |state_|.
    if (partition_expect_ct_state()) {
      feature_list_.InitAndEnableFeature(
          features::kPartitionExpectCTStateByNetworkIsolationKey);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kPartitionExpectCTStateByNetworkIsolationKey);
    }
    state_ = std::make_unique<TransportSecurityState>();
    persister_ = std::make_unique<TransportSecurityPersister>(
        state_.get(), temp_dir_.GetPath(), std::move(background_runner));
  }

  bool partition_expect_ct_state() const { return GetParam(); }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TransportSecurityState> state_;
  std::unique_ptr<TransportSecurityPersister> persister_;
};

INSTANTIATE_TEST_SUITE_P(All, TransportSecurityPersisterTest, testing::Bool());

// Tests that LoadEntries() clears existing non-static entries.
TEST_P(TransportSecurityPersisterTest, LoadEntriesClearsExistingState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  bool data_in_old_format;

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::ExpectCTState expect_ct_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));

  state_->AddHSTS(kYahooDomain, expiry, false /* include subdomains */);
  state_->AddExpectCT(kYahooDomain, expiry, true /* enforce */, GURL(),
                      NetworkIsolationKey());

  EXPECT_TRUE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));
  EXPECT_TRUE(state_->GetDynamicExpectCTState(
      kYahooDomain, NetworkIsolationKey(), &expect_ct_state));

  EXPECT_TRUE(persister_->LoadEntries("{\"version\":2}", &data_in_old_format));
  EXPECT_FALSE(data_in_old_format);

  EXPECT_FALSE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));
  EXPECT_FALSE(state_->GetDynamicExpectCTState(
      kYahooDomain, NetworkIsolationKey(), &expect_ct_state));
}

TEST_P(TransportSecurityPersisterTest, SerializeData1) {
  std::string output;
  bool data_in_old_format;

  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &data_in_old_format));
  EXPECT_FALSE(data_in_old_format);
}

TEST_P(TransportSecurityPersisterTest, SerializeData2) {
  TransportSecurityState::STSState sts_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));

  bool include_subdomains = true;
  state_->AddHSTS(kYahooDomain, expiry, include_subdomains);

  std::string output;
  bool data_in_old_format;
  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &data_in_old_format));
  EXPECT_FALSE(data_in_old_format);

  EXPECT_TRUE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_->GetDynamicSTSState("foo.yahoo.com", &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_->GetDynamicSTSState("foo.bar.yahoo.com", &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_->GetDynamicSTSState("foo.bar.baz.yahoo.com", &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
}

TEST_P(TransportSecurityPersisterTest, SerializeData3) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  // Add an entry.
  base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  bool include_subdomains = false;
  state_->AddHSTS("www.example.com", expiry, include_subdomains);
  state_->AddExpectCT("www.example.com", expiry, true /* enforce */, GURL(),
                      NetworkIsolationKey());

  // Add another entry.
  expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(3000);
  state_->AddHSTS("www.example.net", expiry, include_subdomains);
  state_->AddExpectCT("www.example.net", expiry, false /* enforce */,
                      report_uri, NetworkIsolationKey());

  // Save a copy of everything.
  std::set<std::string> sts_saved;
  TransportSecurityState::STSStateIterator sts_iter(*state_);
  while (sts_iter.HasNext()) {
    sts_saved.insert(sts_iter.hostname());
    sts_iter.Advance();
  }

  std::set<std::string> expect_ct_saved;
  TransportSecurityState::ExpectCTStateIterator expect_ct_iter(*state_);
  while (expect_ct_iter.HasNext()) {
    expect_ct_saved.insert(expect_ct_iter.hostname());
    expect_ct_iter.Advance();
  }

  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));

  // Persist the data to the file.
  base::RunLoop run_loop;
  persister_->WriteNow(state_.get(), run_loop.QuitClosure());
  run_loop.Run();

  // Read the data back.
  base::FilePath path = temp_dir_.GetPath().AppendASCII("TransportSecurity");
  std::string persisted;
  EXPECT_TRUE(base::ReadFileToString(path, &persisted));
  EXPECT_EQ(persisted, serialized);
  bool data_in_old_format;
  EXPECT_TRUE(persister_->LoadEntries(persisted, &data_in_old_format));
  EXPECT_FALSE(data_in_old_format);

  // Check that states are the same as saved.
  size_t count = 0;
  TransportSecurityState::STSStateIterator sts_iter2(*state_);
  while (sts_iter2.HasNext()) {
    count++;
    sts_iter2.Advance();
  }
  EXPECT_EQ(count, sts_saved.size());

  count = 0;
  TransportSecurityState::ExpectCTStateIterator expect_ct_iter2(*state_);
  while (expect_ct_iter2.HasNext()) {
    count++;
    expect_ct_iter2.Advance();
  }
  EXPECT_EQ(count, expect_ct_saved.size());
}

TEST_P(TransportSecurityPersisterTest, DeserializeBadData) {
  bool data_in_old_format;
  EXPECT_FALSE(persister_->LoadEntries("", &data_in_old_format));
  EXPECT_FALSE(persister_->LoadEntries("Foopy", &data_in_old_format));
  EXPECT_FALSE(persister_->LoadEntries("15", &data_in_old_format));
  EXPECT_FALSE(persister_->LoadEntries("[15]", &data_in_old_format));
  EXPECT_FALSE(persister_->LoadEntries("{\"version\":1}", &data_in_old_format));
}

TEST_P(TransportSecurityPersisterTest, DeserializeDataOldWithoutCreationDate) {
  const char kDomain[] = "example.test";

  // This is an old-style piece of transport state JSON, which has no creation
  // date.
  const std::string kInput =
      "{ "
      "\"G0EywIek2XnIhLrUjaK4TrHBT1+2TcixDVRXwM3/CCo=\": {"
      "\"expiry\": 1266815027.983453, "
      "\"include_subdomains\": false, "
      "\"mode\": \"strict\" "
      "}"
      "}";
  bool data_in_old_format;
  EXPECT_TRUE(persister_->LoadEntries(kInput, &data_in_old_format));
  EXPECT_TRUE(data_in_old_format);

  TransportSecurityState::STSState sts_state;
  EXPECT_TRUE(state_->GetDynamicSTSState(kDomain, &sts_state));
  EXPECT_EQ(kDomain, sts_state.domain);
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
}

TEST_P(TransportSecurityPersisterTest, DeserializeDataOldMergedDictionary) {
  const char kStsDomain[] = "sts.test";
  const char kExpectCTDomain[] = "expect_ct.test";
  const GURL kExpectCTReportUri = GURL("https://expect_ct.test/report_uri");
  const char kBothDomain[] = "both.test";

  // This is an old-style piece of transport state JSON, which uses a single
  // unversioned host-keyed dictionary of merged ExpectCT and HSTS data.
  const std::string kInput =
      "{"
      "   \"CxLbri+JPdi5pZ8/a/2rjyzq+IYs07WJJ1yxjB4Lpw0=\": {"
      "      \"expect_ct\": {"
      "         \"expect_ct_enforce\": true,"
      "         \"expect_ct_expiry\": 1590512843.283966,"
      "         \"expect_ct_observed\": 1590511843.284064,"
      "         \"expect_ct_report_uri\": \"https://expect_ct.test/report_uri\""
      "      },"
      "      \"expiry\": 0.0,"
      "      \"mode\": \"default\","
      "      \"sts_include_subdomains\": false,"
      "      \"sts_observed\": 0.0"
      "   },"
      "   \"DkgjGShIBmYtgJcJf5lfX3rTr2S6dqyF+O8IAgjuleE=\": {"
      "      \"expiry\": 1590512843.283966,"
      "      \"mode\": \"force-https\","
      "      \"sts_include_subdomains\": false,"
      "      \"sts_observed\": 1590511843.284025"
      "   },"
      "   \"M5lkNV3JBeoPMlKrTOKRYT+mrUsZCS5eoQWsc9/r1MU=\": {"
      "      \"expect_ct\": {"
      "         \"expect_ct_enforce\": true,"
      "         \"expect_ct_expiry\": 1590512843.283966,"
      "         \"expect_ct_observed\": 1590511843.284098,"
      "         \"expect_ct_report_uri\": \"\""
      "      },"
      "      \"expiry\": 1590512843.283966,"
      "      \"mode\": \"force-https\","
      "      \"sts_include_subdomains\": true,"
      "      \"sts_observed\": 1590511843.284091"
      "   }"
      "}";

  bool data_in_old_format;
  EXPECT_TRUE(persister_->LoadEntries(kInput, &data_in_old_format));
  EXPECT_TRUE(data_in_old_format);

  // kStsDomain should only have HSTS information.
  TransportSecurityState::STSState sts_state;
  EXPECT_TRUE(state_->GetDynamicSTSState(kStsDomain, &sts_state));
  EXPECT_EQ(kStsDomain, sts_state.domain);
  EXPECT_FALSE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_LT(base::Time::Now(), sts_state.last_observed);
  EXPECT_LT(sts_state.last_observed, sts_state.expiry);
  TransportSecurityState::ExpectCTState expect_ct_state;
  EXPECT_FALSE(state_->GetDynamicExpectCTState(
      kStsDomain, NetworkIsolationKey(), &expect_ct_state));

  // kExpectCTDomain should only have HSTS information.
  sts_state = TransportSecurityState::STSState();
  EXPECT_FALSE(state_->GetDynamicSTSState(kExpectCTDomain, &sts_state));
  expect_ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(state_->GetDynamicExpectCTState(
      kExpectCTDomain, NetworkIsolationKey(), &expect_ct_state));
  EXPECT_EQ(kExpectCTReportUri, expect_ct_state.report_uri);
  EXPECT_TRUE(expect_ct_state.enforce);
  EXPECT_LT(base::Time::Now(), expect_ct_state.last_observed);
  EXPECT_LT(expect_ct_state.last_observed, expect_ct_state.expiry);

  // kBothDomain should have HSTS and ExpectCT information.
  sts_state = TransportSecurityState::STSState();
  EXPECT_TRUE(state_->GetDynamicSTSState(kBothDomain, &sts_state));
  EXPECT_EQ(kBothDomain, sts_state.domain);
  EXPECT_TRUE(sts_state.include_subdomains);
  EXPECT_EQ(TransportSecurityState::STSState::MODE_FORCE_HTTPS,
            sts_state.upgrade_mode);
  EXPECT_LT(base::Time::Now(), sts_state.last_observed);
  EXPECT_LT(sts_state.last_observed, sts_state.expiry);
  expect_ct_state = TransportSecurityState::ExpectCTState();
  EXPECT_TRUE(state_->GetDynamicExpectCTState(
      kBothDomain, NetworkIsolationKey(), &expect_ct_state));
  EXPECT_TRUE(expect_ct_state.report_uri.is_empty());
  EXPECT_TRUE(expect_ct_state.enforce);
  EXPECT_LT(base::Time::Now(), expect_ct_state.last_observed);
  EXPECT_LT(expect_ct_state.last_observed, expect_ct_state.expiry);
}

// Tests that dynamic Expect-CT state is serialized and deserialized correctly.
TEST_P(TransportSecurityPersisterTest, ExpectCT) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  TransportSecurityState::ExpectCTState expect_ct_state;
  static const char kTestDomain[] = "example.test";

  EXPECT_FALSE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &expect_ct_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state_->AddExpectCT(kTestDomain, expiry, true /* enforce */, GURL(),
                      NetworkIsolationKey());
  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool data_in_old_format;
  // LoadEntries() clears existing dynamic data before loading entries from
  // |serialized|.
  EXPECT_TRUE(persister_->LoadEntries(serialized, &data_in_old_format));

  TransportSecurityState::ExpectCTState new_expect_ct_state;
  EXPECT_TRUE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &new_expect_ct_state));
  EXPECT_TRUE(new_expect_ct_state.enforce);
  EXPECT_TRUE(new_expect_ct_state.report_uri.is_empty());
  EXPECT_EQ(expiry, new_expect_ct_state.expiry);

  // Update the state for the domain and check that it is
  // serialized/deserialized correctly.
  state_->AddExpectCT(kTestDomain, expiry, false /* enforce */, report_uri,
                      NetworkIsolationKey());
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  EXPECT_TRUE(persister_->LoadEntries(serialized, &data_in_old_format));
  EXPECT_TRUE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &new_expect_ct_state));
  EXPECT_FALSE(new_expect_ct_state.enforce);
  EXPECT_EQ(report_uri, new_expect_ct_state.report_uri);
  EXPECT_EQ(expiry, new_expect_ct_state.expiry);
}

// Tests that dynamic Expect-CT state is serialized and deserialized correctly
// when there is also STS data present.
TEST_P(TransportSecurityPersisterTest, ExpectCTWithSTSDataPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  TransportSecurityState::ExpectCTState expect_ct_state;
  static const char kTestDomain[] = "example.test";

  EXPECT_FALSE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &expect_ct_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state_->AddHSTS(kTestDomain, expiry, false /* include subdomains */);
  state_->AddExpectCT(kTestDomain, expiry, true /* enforce */, GURL(),
                      NetworkIsolationKey());

  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool data_in_old_format;
  // LoadEntries() clears existing dynamic data before loading entries from
  // |serialized|.
  EXPECT_TRUE(persister_->LoadEntries(serialized, &data_in_old_format));

  TransportSecurityState::ExpectCTState new_expect_ct_state;
  EXPECT_TRUE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &new_expect_ct_state));
  EXPECT_TRUE(new_expect_ct_state.enforce);
  EXPECT_TRUE(new_expect_ct_state.report_uri.is_empty());
  EXPECT_EQ(expiry, new_expect_ct_state.expiry);
  // Check that STS state is loaded properly as well.
  TransportSecurityState::STSState sts_state;
  EXPECT_TRUE(state_->GetDynamicSTSState(kTestDomain, &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
}

// Tests that Expect-CT state is not serialized and persisted when the feature
// is disabled.
TEST_P(TransportSecurityPersisterTest, ExpectCTDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  TransportSecurityState::ExpectCTState expect_ct_state;
  static const char kTestDomain[] = "example.test";

  EXPECT_FALSE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &expect_ct_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state_->AddExpectCT(kTestDomain, expiry, true /* enforce */, GURL(),
                      NetworkIsolationKey());
  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool data_in_old_format;
  EXPECT_TRUE(persister_->LoadEntries(serialized, &data_in_old_format));

  TransportSecurityState::ExpectCTState new_expect_ct_state;
  EXPECT_FALSE(state_->GetDynamicExpectCTState(
      kTestDomain, NetworkIsolationKey(), &new_expect_ct_state));
}

// Save data with several NetworkIsolationKeys with
// kPartitionExpectCTStateByNetworkIsolationKey enabled, and then load it with
// the feature enabled or disabled, based on partition_expect_ct_state().
TEST_P(TransportSecurityPersisterTest, ExpectCTWithNetworkIsolationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  const GURL report_uri(kReportUri);
  static const char kTestDomain[] = "example.test";
  const SchemefulSite kSite(GURL("https://somewhere.else.test"));
  const NetworkIsolationKey empty_network_isolation_key;
  const NetworkIsolationKey network_isolation_key(kSite /* top_frame_site */,
                                                  kSite /* frame_site */);
  const NetworkIsolationKey transient_network_isolation_key =
      NetworkIsolationKey::CreateTransient();

  const base::Time current_time(base::Time::Now());
  const base::Time expiry1 = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time expiry2 = current_time + base::TimeDelta::FromSeconds(2000);
  const base::Time expiry3 = current_time + base::TimeDelta::FromSeconds(3000);

  // Serialize data with kPartitionExpectCTStateByNetworkIsolationKey enabled,
  // and then revert the feature to its previous value.
  std::string serialized;
  {
    base::test::ScopedFeatureList feature_list2;
    feature_list2.InitAndEnableFeature(
        features::kPartitionExpectCTStateByNetworkIsolationKey);
    TransportSecurityState state2;
    TransportSecurityPersister persister2(
        &state2, temp_dir_.GetPath(),
        std::move(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})));
    TransportSecurityState::ExpectCTState expect_ct_state;
    state2.AddExpectCT(kTestDomain, expiry1, true /* enforce */, GURL(),
                       empty_network_isolation_key);
    state2.AddExpectCT(kTestDomain, expiry2, true /* enforce */, GURL(),
                       network_isolation_key);
    state2.AddExpectCT(kTestDomain, expiry3, true /* enforce */, GURL(),
                       transient_network_isolation_key);
    EXPECT_TRUE(persister2.SerializeData(&serialized));

    EXPECT_TRUE(state2.GetDynamicExpectCTState(
        kTestDomain, empty_network_isolation_key, &expect_ct_state));
    EXPECT_TRUE(state2.GetDynamicExpectCTState(
        kTestDomain, network_isolation_key, &expect_ct_state));
    EXPECT_TRUE(state2.GetDynamicExpectCTState(
        kTestDomain, transient_network_isolation_key, &expect_ct_state));
  }

  bool data_in_old_format;
  // Load entries into the other persister.
  EXPECT_TRUE(persister_->LoadEntries(serialized, &data_in_old_format));
  EXPECT_FALSE(data_in_old_format);

  if (partition_expect_ct_state()) {
    TransportSecurityState::ExpectCTState new_expect_ct_state;
    EXPECT_TRUE(state_->GetDynamicExpectCTState(
        kTestDomain, empty_network_isolation_key, &new_expect_ct_state));
    EXPECT_TRUE(new_expect_ct_state.enforce);
    EXPECT_TRUE(new_expect_ct_state.report_uri.is_empty());
    EXPECT_EQ(expiry1, new_expect_ct_state.expiry);

    EXPECT_TRUE(state_->GetDynamicExpectCTState(
        kTestDomain, network_isolation_key, &new_expect_ct_state));
    EXPECT_TRUE(new_expect_ct_state.enforce);
    EXPECT_TRUE(new_expect_ct_state.report_uri.is_empty());
    EXPECT_EQ(expiry2, new_expect_ct_state.expiry);

    // The data associated with the transient NetworkIsolationKey should not
    // have been saved.
    EXPECT_FALSE(state_->GetDynamicExpectCTState(
        kTestDomain, transient_network_isolation_key, &new_expect_ct_state));
  } else {
    std::set<std::string> expect_ct_saved;
    TransportSecurityState::ExpectCTStateIterator expect_ct_iter(*state_);
    ASSERT_TRUE(expect_ct_iter.HasNext());
    EXPECT_EQ(empty_network_isolation_key,
              expect_ct_iter.network_isolation_key());
    EXPECT_TRUE(expect_ct_iter.domain_state().enforce);
    EXPECT_TRUE(expect_ct_iter.domain_state().report_uri.is_empty());
    expect_ct_iter.Advance();
    EXPECT_FALSE(expect_ct_iter.HasNext());
  }
}

// Test the case when deserializing a NetworkIsolationKey fails. This happens
// when data is persisted with kAppendFrameOriginToNetworkIsolationKey, but
// loaded without it, or vice-versa.
TEST_P(TransportSecurityPersisterTest,
       ExpectCTNetworkIsolationKeyDeserializationFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);

  const GURL report_uri(kReportUri);
  static const char kTestDomain[] = "example.test";
  const SchemefulSite kSite(GURL("https://somewhere.else.test"));
  const NetworkIsolationKey empty_network_isolation_key;
  const NetworkIsolationKey network_isolation_key(kSite /* top_frame_site */,
                                                  kSite /* frame_site */);
  const base::Time current_time(base::Time::Now());
  const base::Time expiry1 = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time expiry2 = current_time + base::TimeDelta::FromSeconds(2000);

  // Serialize data with kPartitionExpectCTStateByNetworkIsolationKey and
  // kAppendFrameOriginToNetworkIsolationKey enabled, and then revert the
  // features to their previous values.
  std::string serialized;
  {
    base::test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures(
        // enabled_features
        {features::kPartitionExpectCTStateByNetworkIsolationKey,
         features::kAppendFrameOriginToNetworkIsolationKey},
        // disabled_features
        {});
    TransportSecurityState state2;
    TransportSecurityPersister persister2(
        &state2, temp_dir_.GetPath(),
        std::move(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})));
    TransportSecurityState::ExpectCTState expect_ct_state;
    state2.AddExpectCT(kTestDomain, expiry1, true /* enforce */, GURL(),
                       empty_network_isolation_key);
    state2.AddExpectCT(kTestDomain, expiry2, true /* enforce */, GURL(),
                       network_isolation_key);
    EXPECT_TRUE(persister2.SerializeData(&serialized));

    EXPECT_TRUE(state2.GetDynamicExpectCTState(
        kTestDomain, empty_network_isolation_key, &expect_ct_state));
    EXPECT_TRUE(state2.GetDynamicExpectCTState(
        kTestDomain, network_isolation_key, &expect_ct_state));
  }

  base::test::ScopedFeatureList feature_list3;
  feature_list3.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  bool data_in_old_format;
  // Load entries into the other persister.
  EXPECT_TRUE(persister_->LoadEntries(serialized, &data_in_old_format));
  EXPECT_FALSE(data_in_old_format);

  // Regardless of whether kPartitionExpectCTStateByNetworkIsolationKey is
  // enabled or not, the different kAppendFrameOriginToNetworkIsolationKey state
  // will cause the entry with a non-empty NetworkIsolationKey to be dropped.
  std::set<std::string> expect_ct_saved;
  TransportSecurityState::ExpectCTStateIterator expect_ct_iter(*state_);
  ASSERT_TRUE(expect_ct_iter.HasNext());
  EXPECT_EQ(empty_network_isolation_key,
            expect_ct_iter.network_isolation_key());
  EXPECT_TRUE(expect_ct_iter.domain_state().enforce);
  EXPECT_TRUE(expect_ct_iter.domain_state().report_uri.is_empty());
  expect_ct_iter.Advance();
  EXPECT_FALSE(expect_ct_iter.HasNext());
}

}  // namespace

}  // namespace net
