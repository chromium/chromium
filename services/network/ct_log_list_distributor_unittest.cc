// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/logging.h"

#include "services/network/ct_log_list_distributor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_list.h"
#include "base/test/task_environment.h"
#include "crypto/sha2.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/test/ct_test_util.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class MockLogVerifier {
 public:
  MockLogVerifier() = default;
  ~MockLogVerifier() = default;

  void SetLogs(const std::vector<scoped_refptr<const net::CTLogVerifier>>&
                   log_verifiers) {
    logs_ = log_verifiers;
  }

  std::vector<scoped_refptr<const net::CTLogVerifier>> GetLogs() {
    return logs_;
  }

 private:
  std::vector<scoped_refptr<const net::CTLogVerifier>> logs_;
};

class CtLogListDistributorTest : public ::testing::Test {
 public:
  void SetUp() override {
    subscription_ = distributor_.RegisterLogsListCallback(base::BindRepeating(
        &MockLogVerifier::SetLogs, base::Unretained(&verifier_)));
  }
  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  CtLogListDistributor distributor_;
  MockLogVerifier verifier_;
  base::CallbackListSubscription subscription_;
};

TEST_F(CtLogListDistributorTest, TestOnNewCtConfig) {
  const char kLogDescription[] = "somelog";

  // Create log list with a single log.
  std::vector<network::mojom::CTLogInfoPtr> log_list_mojo;
  network::mojom::CTLogInfoPtr log_ptr = network::mojom::CTLogInfo::New();
  log_ptr->name = kLogDescription;
  log_ptr->public_key = net::ct::GetTestPublicKey();
  log_list_mojo.push_back(std::move(log_ptr));

  // Pass the log list to the distributor.
  distributor_.OnNewCtConfig(log_list_mojo);

  // Wait for parsing to finish.
  Wait();

  // Verifier should have been notified and have the log list.
  std::vector<scoped_refptr<const net::CTLogVerifier>> logs =
      verifier_.GetLogs();
  EXPECT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->description(), kLogDescription);
  EXPECT_EQ(logs[0]->key_id(),
            crypto::SHA256HashString(net::ct::GetTestPublicKey()));
}

}  // namespace network
