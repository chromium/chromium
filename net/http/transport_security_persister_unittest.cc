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
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/transport_security_state.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kReportUri[] = "http://www.example.test/report";

class TransportSecurityPersisterTest : public TestWithTaskEnvironment {
 public:
  TransportSecurityPersisterTest() = default;

  ~TransportSecurityPersisterTest() override {
    EXPECT_TRUE(base::MessageLoopCurrentForIO::IsSet());
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::MessageLoopCurrentForIO::IsSet());
    persister_ = std::make_unique<TransportSecurityPersister>(
        &state_, temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get());
  }

 protected:
  base::ScopedTempDir temp_dir_;
  TransportSecurityState state_;
  std::unique_ptr<TransportSecurityPersister> persister_;
};

// Tests that LoadEntries() clears existing non-static entries.
TEST_F(TransportSecurityPersisterTest, LoadEntriesClearsExistingState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  std::string output;
  bool dirty;

  TransportSecurityState::STSState sts_state;
  TransportSecurityState::ExpectCTState expect_ct_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_.GetDynamicSTSState(kYahooDomain, &sts_state));

  state_.AddHSTS(kYahooDomain, expiry, false /* include subdomains */);
  state_.AddExpectCT(kYahooDomain, expiry, true /* enforce */, GURL());

  EXPECT_TRUE(state_.GetDynamicSTSState(kYahooDomain, &sts_state));
  EXPECT_TRUE(state_.GetDynamicExpectCTState(kYahooDomain, &expect_ct_state));

  EXPECT_TRUE(persister_->LoadEntries("{}", &dirty));
  EXPECT_FALSE(dirty);

  EXPECT_FALSE(state_.GetDynamicSTSState(kYahooDomain, &sts_state));
  EXPECT_FALSE(state_.GetDynamicExpectCTState(kYahooDomain, &expect_ct_state));
}

TEST_F(TransportSecurityPersisterTest, SerializeData1) {
  std::string output;
  bool dirty;

  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));
  EXPECT_FALSE(dirty);
}

TEST_F(TransportSecurityPersisterTest, SerializeData2) {
  TransportSecurityState::STSState sts_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_.GetDynamicSTSState(kYahooDomain, &sts_state));

  bool include_subdomains = true;
  state_.AddHSTS(kYahooDomain, expiry, include_subdomains);

  std::string output;
  bool dirty;
  EXPECT_TRUE(persister_->SerializeData(&output));
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));

  EXPECT_TRUE(state_.GetDynamicSTSState(kYahooDomain, &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDynamicSTSState("foo.yahoo.com", &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDynamicSTSState("foo.bar.yahoo.com", &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
  EXPECT_TRUE(state_.GetDynamicSTSState("foo.bar.baz.yahoo.com", &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
}

TEST_F(TransportSecurityPersisterTest, SerializeData3) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  // Add an entry.
  base::Time expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(1000);
  bool include_subdomains = false;
  state_.AddHSTS("www.example.com", expiry, include_subdomains);
  state_.AddExpectCT("www.example.com", expiry, true /* enforce */, GURL());

  // Add another entry.
  expiry =
      base::Time::Now() + base::TimeDelta::FromSeconds(3000);
  state_.AddHSTS("www.example.net", expiry, include_subdomains);
  state_.AddExpectCT("www.example.net", expiry, false /* enforce */,
                     report_uri);

  // Save a copy of everything.
  std::set<std::string> sts_saved;
  TransportSecurityState::STSStateIterator sts_iter(state_);
  while (sts_iter.HasNext()) {
    sts_saved.insert(sts_iter.hostname());
    sts_iter.Advance();
  }

  std::set<std::string> expect_ct_saved;
  TransportSecurityState::ExpectCTStateIterator expect_ct_iter(state_);
  while (expect_ct_iter.HasNext()) {
    expect_ct_saved.insert(expect_ct_iter.hostname());
    expect_ct_iter.Advance();
  }

  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));

  // Persist the data to the file.
  base::RunLoop run_loop;
  persister_->WriteNow(&state_, run_loop.QuitClosure());
  run_loop.Run();

  // Read the data back.
  base::FilePath path = temp_dir_.GetPath().AppendASCII("TransportSecurity");
  std::string persisted;
  EXPECT_TRUE(base::ReadFileToString(path, &persisted));
  EXPECT_EQ(persisted, serialized);
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(persisted, &dirty));
  EXPECT_FALSE(dirty);

  // Check that states are the same as saved.
  size_t count = 0;
  TransportSecurityState::STSStateIterator sts_iter2(state_);
  while (sts_iter2.HasNext()) {
    count++;
    sts_iter2.Advance();
  }
  EXPECT_EQ(count, sts_saved.size());

  count = 0;
  TransportSecurityState::ExpectCTStateIterator expect_ct_iter2(state_);
  while (expect_ct_iter2.HasNext()) {
    count++;
    expect_ct_iter2.Advance();
  }
  EXPECT_EQ(count, expect_ct_saved.size());
}

TEST_F(TransportSecurityPersisterTest, SerializeDataOld) {
  // This is an old-style piece of transport state JSON, which has no creation
  // date.
  std::string output =
      "{ "
      "\"NiyD+3J1r6z1wjl2n1ALBu94Zj9OsEAMo0kCN8js0Uk=\": {"
      "\"expiry\": 1266815027.983453, "
      "\"include_subdomains\": false, "
      "\"mode\": \"strict\" "
      "}"
      "}";
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(output, &dirty));
  EXPECT_TRUE(dirty);
}

// Tests that dynamic Expect-CT state is serialized and deserialized correctly.
TEST_F(TransportSecurityPersisterTest, ExpectCT) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  TransportSecurityState::ExpectCTState expect_ct_state;
  static const char kTestDomain[] = "example.test";

  EXPECT_FALSE(state_.GetDynamicExpectCTState(kTestDomain, &expect_ct_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state_.AddExpectCT(kTestDomain, expiry, true /* enforce */, GURL());
  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool dirty;
  // LoadEntries() clears existing dynamic data before loading entries from
  // |serialized|.
  EXPECT_TRUE(persister_->LoadEntries(serialized, &dirty));

  TransportSecurityState::ExpectCTState new_expect_ct_state;
  EXPECT_TRUE(
      state_.GetDynamicExpectCTState(kTestDomain, &new_expect_ct_state));
  EXPECT_TRUE(new_expect_ct_state.enforce);
  EXPECT_TRUE(new_expect_ct_state.report_uri.is_empty());
  EXPECT_EQ(expiry, new_expect_ct_state.expiry);

  // Update the state for the domain and check that it is
  // serialized/deserialized correctly.
  state_.AddExpectCT(kTestDomain, expiry, false /* enforce */, report_uri);
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  EXPECT_TRUE(persister_->LoadEntries(serialized, &dirty));
  EXPECT_TRUE(
      state_.GetDynamicExpectCTState(kTestDomain, &new_expect_ct_state));
  EXPECT_FALSE(new_expect_ct_state.enforce);
  EXPECT_EQ(report_uri, new_expect_ct_state.report_uri);
  EXPECT_EQ(expiry, new_expect_ct_state.expiry);
}

// Tests that dynamic Expect-CT state is serialized and deserialized correctly
// when there is also STS data present.
TEST_F(TransportSecurityPersisterTest, ExpectCTWithSTSDataPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  TransportSecurityState::ExpectCTState expect_ct_state;
  static const char kTestDomain[] = "example.test";

  EXPECT_FALSE(state_.GetDynamicExpectCTState(kTestDomain, &expect_ct_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state_.AddHSTS(kTestDomain, expiry, false /* include subdomains */);
  state_.AddExpectCT(kTestDomain, expiry, true /* enforce */, GURL());

  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool dirty;
  // LoadEntries() clears existing dynamic data before loading entries from
  // |serialized|.
  EXPECT_TRUE(persister_->LoadEntries(serialized, &dirty));

  TransportSecurityState::ExpectCTState new_expect_ct_state;
  EXPECT_TRUE(
      state_.GetDynamicExpectCTState(kTestDomain, &new_expect_ct_state));
  EXPECT_TRUE(new_expect_ct_state.enforce);
  EXPECT_TRUE(new_expect_ct_state.report_uri.is_empty());
  EXPECT_EQ(expiry, new_expect_ct_state.expiry);
  // Check that STS state is loaded properly as well.
  TransportSecurityState::STSState sts_state;
  EXPECT_TRUE(state_.GetDynamicSTSState(kTestDomain, &sts_state));
  EXPECT_EQ(sts_state.upgrade_mode,
            TransportSecurityState::STSState::MODE_FORCE_HTTPS);
}

// Tests that Expect-CT state is not serialized and persisted when the feature
// is disabled.
TEST_F(TransportSecurityPersisterTest, ExpectCTDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      TransportSecurityState::kDynamicExpectCTFeature);
  const GURL report_uri(kReportUri);
  TransportSecurityState::ExpectCTState expect_ct_state;
  static const char kTestDomain[] = "example.test";

  EXPECT_FALSE(state_.GetDynamicExpectCTState(kTestDomain, &expect_ct_state));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  state_.AddExpectCT(kTestDomain, expiry, true /* enforce */, GURL());
  std::string serialized;
  EXPECT_TRUE(persister_->SerializeData(&serialized));
  bool dirty;
  EXPECT_TRUE(persister_->LoadEntries(serialized, &dirty));

  TransportSecurityState::ExpectCTState new_expect_ct_state;
  EXPECT_FALSE(
      state_.GetDynamicExpectCTState(kTestDomain, &new_expect_ct_state));
}

}  // namespace

}  // namespace net
