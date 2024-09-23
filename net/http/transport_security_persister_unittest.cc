// Copyright 2012 The Chromium Authors
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
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/transport_security_state.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

const char kReportUri[] = "http://www.example.test/report";

class TransportSecurityPersisterTest : public ::testing::Test,
                                       public WithTaskEnvironment {
 public:
  TransportSecurityPersisterTest()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Mock out time so that entries with hard-coded json data can be
    // successfully loaded. Use a large enough value that dynamically created
    // entries have at least somewhat interesting expiration times.
    FastForwardBy(base::Days(3660));
  }

  ~TransportSecurityPersisterTest() override {
    EXPECT_TRUE(base::CurrentIOThread::IsSet());
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    transport_security_file_path_ =
        temp_dir_.GetPath().AppendASCII("TransportSecurity");
    ASSERT_TRUE(base::CurrentIOThread::IsSet());
    scoped_refptr<base::SequencedTaskRunner> background_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
    state_ = std::make_unique<TransportSecurityState>();
    persister_ = std::make_unique<TransportSecurityPersister>(
        state_.get(), std::move(background_runner),
        transport_security_file_path_);
  }

 protected:
  base::FilePath transport_security_file_path_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TransportSecurityState> state_;
  std::unique_ptr<TransportSecurityPersister> persister_;
};

// Tests that LoadEntries() clears existing non-static entries.
TEST_F(TransportSecurityPersisterTest, LoadEntriesClearsExistingState) {
  TransportSecurityState::STSState sts_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));

  state_->AddHSTS(kYahooDomain, expiry, false /* include subdomains */);
  EXPECT_TRUE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));

  persister_->LoadEntries("{\"version\":2}");

  EXPECT_FALSE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));
}

// Tests that serializing -> deserializing -> reserializing results in the same
// output.
TEST_F(TransportSecurityPersisterTest, SerializeData1) {
  std::optional<std::string> output = persister_->SerializeData();

  ASSERT_TRUE(output);
  persister_->LoadEntries(*output);

  std::optional<std::string> output2 = persister_->SerializeData();
  ASSERT_TRUE(output2);
  EXPECT_EQ(output, output2);
}

TEST_F(TransportSecurityPersisterTest, SerializeData2) {
  TransportSecurityState::STSState sts_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::Seconds(1000);
  static const char kYahooDomain[] = "yahoo.com";

  EXPECT_FALSE(state_->GetDynamicSTSState(kYahooDomain, &sts_state));

  bool include_subdomains = true;
  state_->AddHSTS(kYahooDomain, expiry, include_subdomains);

  std::optional<std::string> output = persister_->SerializeData();
  ASSERT_TRUE(output);
  persister_->LoadEntries(*output);

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

TEST_F(TransportSecurityPersisterTest, SerializeData3) {
  const GURL report_uri(kReportUri);
  // Add an entry.
  base::Time expiry = base::Time::Now() + base::Seconds(1000);
  bool include_subdomains = false;
  state_->AddHSTS("www.example.com", expiry, include_subdomains);

  // Add another entry.
  expiry = base::Time::Now() + base::Seconds(3000);
  state_->AddHSTS("www.example.net", expiry, include_subdomains);

  // Save a copy of everything.
  std::set<TransportSecurityState::HashedHost> sts_saved;
  TransportSecurityState::STSStateIterator sts_iter(*state_);
  while (sts_iter.HasNext()) {
    sts_saved.insert(sts_iter.hostname());
    sts_iter.Advance();
  }

  std::optional<std::string> serialized = persister_->SerializeData();
  ASSERT_TRUE(serialized);

  // Persist the data to the file.
  base::RunLoop run_loop;
  persister_->WriteNow(state_.get(), run_loop.QuitClosure());
  run_loop.Run();

  // Read the data back.
  std::string persisted;
  EXPECT_TRUE(
      base::ReadFileToString(transport_security_file_path_, &persisted));
  EXPECT_EQ(persisted, serialized);
  persister_->LoadEntries(persisted);

  // Check that states are the same as saved.
  size_t count = 0;
  TransportSecurityState::STSStateIterator sts_iter2(*state_);
  while (sts_iter2.HasNext()) {
    count++;
    sts_iter2.Advance();
  }
  EXPECT_EQ(count, sts_saved.size());
}

// Tests that deserializing bad data shouldn't result in any STS entries being
// added to the transport security state.
TEST_F(TransportSecurityPersisterTest, DeserializeBadData) {
  persister_->LoadEntries("");
  EXPECT_EQ(0u, state_->num_sts_entries());

  persister_->LoadEntries("Foopy");
  EXPECT_EQ(0u, state_->num_sts_entries());

  persister_->LoadEntries("15");
  EXPECT_EQ(0u, state_->num_sts_entries());

  persister_->LoadEntries("[15]");
  EXPECT_EQ(0u, state_->num_sts_entries());

  persister_->LoadEntries("{\"version\":1}");
  EXPECT_EQ(0u, state_->num_sts_entries());
}

TEST_F(TransportSecurityPersisterTest, DeserializeDataOldWithoutCreationDate) {
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
  persister_->LoadEntries(kInput);
  EXPECT_EQ(0u, state_->num_sts_entries());
}

TEST_F(TransportSecurityPersisterTest, DeserializeDataOldMergedDictionary) {
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

  persister_->LoadEntries(kInput);
  EXPECT_EQ(0u, state_->num_sts_entries());
}

TEST_F(TransportSecurityPersisterTest, DeserializeLegacyExpectCTData) {
  const std::string kHost = "CxLbri+JPdi5pZ8/a/2rjyzq+IYs07WJJ1yxjB4Lpw0=";
  const std::string kInput =
      R"({"version":2, "sts": [{ "host": ")" + kHost +
      R"(", "mode": "force-https", "sts_include_subdomains": false, )"
      R"("sts_observed": 0.0, "expiry": 4825336765.0}], "expect_ct": [{"host":)"
      R"("CxLbri+JPdi5pZ8/a/2rjyzq+IYs07WJJ1yxjB4Lpw0=", "nak": "test", )"
      R"("expect_ct_observed": 0.0, "expect_ct_expiry": 4825336765.0, )"
      R"("expect_ct_enforce": true, "expect_ct_report_uri": ""}]})";
  LOG(ERROR) << kInput;
  persister_->LoadEntries(kInput);
  FastForwardBy(TransportSecurityPersister::GetCommitInterval() +
                base::Seconds(1));
  EXPECT_EQ(1u, state_->num_sts_entries());
  // Now read the data and check that there are no Expect-CT entries.
  std::string persisted;
  ASSERT_TRUE(
      base::ReadFileToString(transport_security_file_path_, &persisted));
  // Smoke test that the file contains some data as expected...
  ASSERT_NE(std::string::npos, persisted.find(kHost));
  // But it shouldn't contain any Expect-CT data.
  EXPECT_EQ(std::string::npos, persisted.find("expect_ct"));
}

class TransportSecurityPersisterCommitTest
    : public TransportSecurityPersisterTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  TransportSecurityPersisterCommitTest() {
    if (GetParam().empty()) {
      feature_list_.InitAndDisableFeature(kTransportSecurityFileWriterSchedule);
    } else {
      feature_list_.InitAndEnableFeatureWithParameters(
          kTransportSecurityFileWriterSchedule,
          {{"commit_interval", GetParam()}});
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    TransportSecurityPersisterCommitTest,
    ::testing::Values(
        // The ImportantFileWriter default.
        "10s",
        // Anything less should use the default.
        "9s",
        "0",
        "-10s",
        "-inf",
        // Valid values.
        "1m",
        "10m",
        // Anything greater should use the max.
        "11m",
        "+inf",
        // Disable the feature. Should use the default interval.
        ""));

TEST_P(TransportSecurityPersisterCommitTest, CommitIntervalIsValid) {
  EXPECT_GE(TransportSecurityPersister::GetCommitInterval(), base::Seconds(10));
  EXPECT_LE(TransportSecurityPersister::GetCommitInterval(), base::Minutes(10));
}

TEST_P(TransportSecurityPersisterCommitTest, WriteAtCommitInterval) {
  const auto kLongExpiry = base::Time::Now() + base::Days(10);
  const bool kIncludeSubdomains = false;

  // Make sure the file starts empty.
  ASSERT_TRUE(base::WriteFile(transport_security_file_path_, ""));

  // Add an entry. Expect the persister NOT to write before the commit interval,
  // for performance.
  state_->AddHSTS("www.example.com", kLongExpiry, kIncludeSubdomains);
  FastForwardBy(TransportSecurityPersister::GetCommitInterval() / 2);
  std::string persisted;
  EXPECT_TRUE(
      base::ReadFileToString(transport_security_file_path_, &persisted));
  EXPECT_TRUE(persisted.empty());

  // Add another entry. After the commit interval passes, both should be
  // written.
  state_->AddHSTS("www.example.net", kLongExpiry, kIncludeSubdomains);
  FastForwardBy(TransportSecurityPersister::GetCommitInterval() / 2);
  EXPECT_TRUE(
      base::ReadFileToString(transport_security_file_path_, &persisted));
  EXPECT_FALSE(persisted.empty());

  // Ensure that state comes from the persisted file.
  persister_->LoadEntries("");
  TransportSecurityState::STSState dummy_state;
  ASSERT_FALSE(state_->GetDynamicSTSState("www.example.com", &dummy_state));
  ASSERT_FALSE(state_->GetDynamicSTSState("www.example.net", &dummy_state));

  // Check that both entries were persisted.
  persister_->LoadEntries(persisted);
  EXPECT_TRUE(state_->GetDynamicSTSState("www.example.com", &dummy_state));
  EXPECT_TRUE(state_->GetDynamicSTSState("www.example.net", &dummy_state));

  // Add a third entry and force a write before the commit interval
  state_->AddHSTS("www.example.org", kLongExpiry, kIncludeSubdomains);

  const auto time_before_write = base::TimeTicks::Now();
  base::RunLoop run_loop;
  persister_->WriteNow(state_.get(), run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_LT(base::TimeTicks::Now() - time_before_write,
            TransportSecurityPersister::GetCommitInterval());
  EXPECT_TRUE(
      base::ReadFileToString(transport_security_file_path_, &persisted));
  EXPECT_FALSE(persisted.empty());

  // Ensure that state comes from the persisted file.
  persister_->LoadEntries("");
  ASSERT_FALSE(state_->GetDynamicSTSState("www.example.com", &dummy_state));
  ASSERT_FALSE(state_->GetDynamicSTSState("www.example.net", &dummy_state));
  ASSERT_FALSE(state_->GetDynamicSTSState("www.example.org", &dummy_state));

  // Check that all entries were persisted.
  persister_->LoadEntries(persisted);
  EXPECT_TRUE(state_->GetDynamicSTSState("www.example.com", &dummy_state));
  EXPECT_TRUE(state_->GetDynamicSTSState("www.example.net", &dummy_state));
  EXPECT_TRUE(state_->GetDynamicSTSState("www.example.org", &dummy_state));
}

}  // namespace

}  // namespace net
