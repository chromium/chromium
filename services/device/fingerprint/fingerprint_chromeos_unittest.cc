// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/fingerprint/fingerprint_chromeos.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/biod/fake_biod_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class FakeFingerprintObserver : public mojom::FingerprintObserver {
 public:
  explicit FakeFingerprintObserver(
      mojo::PendingReceiver<mojom::FingerprintObserver> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~FakeFingerprintObserver() override {}

  // mojom::FingerprintObserver
  void OnRestarted() override { restarts_++; }

  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int percent_complete) override {
    enroll_scan_dones_++;
  }

  void OnAuthScanDone(
      device::mojom::ScanResult scan_result,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override {
    auth_scan_dones_++;
  }

  void OnSessionFailed() override { session_failures_++; }

  // Test status counts.
  int enroll_scan_dones() { return enroll_scan_dones_; }
  int auth_scan_dones() { return auth_scan_dones_; }
  int restarts() { return restarts_; }
  int session_failures() { return session_failures_; }

 private:
  mojo::Receiver<mojom::FingerprintObserver> receiver_;
  int enroll_scan_dones_ = 0;  // Count of enroll scan done signal received.
  int auth_scan_dones_ = 0;    // Count of auth scan done signal received.
  int restarts_ = 0;           // Count of restart signal received.
  int session_failures_ = 0;   // Count of session failed signal received.

  DISALLOW_COPY_AND_ASSIGN(FakeFingerprintObserver);
};

class FingerprintChromeOSTest : public testing::Test {
 public:
  FingerprintChromeOSTest() = default;

  ~FingerprintChromeOSTest() override = default;

  void SetUp() override {
    chromeos::BiodClient::InitializeFake();
    fingerprint_ = base::WrapUnique(new FingerprintChromeOS());
  }

  void TearDown() override {
    fingerprint_.reset();
    chromeos::BiodClient::Shutdown();
  }

  FingerprintChromeOS* fingerprint() { return fingerprint_.get(); }

  void GenerateRestartSignal() { fingerprint_->BiodServiceRestarted(); }

  void GenerateEnrollScanDoneSignal() {
    std::string fake_fingerprint_data;
    chromeos::FakeBiodClient::Get()->SendEnrollScanDone(
        fake_fingerprint_data, biod::SCAN_RESULT_SUCCESS, true,
        -1 /* percent_complete */);
  }

  void GenerateAuthScanDoneSignal() {
    std::string fake_fingerprint_data;
    chromeos::FakeBiodClient::Get()->SendAuthScanDone(
        fake_fingerprint_data, biod::SCAN_RESULT_SUCCESS);
  }

  void GenerateSessionFailedSignal() {
    chromeos::FakeBiodClient::Get()->SendSessionFailed();
  }

  void onStartSession(const dbus::ObjectPath& path) {}

  void SimulateRequestRunning(bool is_running) {
    fingerprint_->is_request_running_ = is_running;
    if (!is_running)
      fingerprint_->StartNextRequest();
  }

  bool RequestDataIsReset() {
    return fingerprint_->records_path_to_label_.empty() &&
           !fingerprint_->on_get_records_;
  }

  void GenerateGetRecordsForUserRequest(int num_of_request) {
    for (int i = 0; i < num_of_request; i++) {
      fingerprint_->GetRecordsForUser(
          "" /*user_id*/, base::BindOnce(&FingerprintChromeOSTest::OnGetRecords,
                                         base::Unretained(this)));
    }
  }

  void OnGetRecords(const base::flat_map<std::string, std::string>&
                        fingerprints_list_mapping) {
    ++get_records_results_;
  }

  int GetPendingRequests() {
    return fingerprint_->get_records_pending_requests_.size();
  }

  bool IsRequestRunning() { return fingerprint_->is_request_running_; }
  int get_records_results() { return get_records_results_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FingerprintChromeOS> fingerprint_;
  int get_records_results_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FingerprintChromeOSTest);
};

TEST_F(FingerprintChromeOSTest, FingerprintObserverTest) {
  mojo::PendingRemote<mojom::FingerprintObserver> pending_observer;
  FakeFingerprintObserver observer(
      pending_observer.InitWithNewPipeAndPassReceiver());
  fingerprint()->AddFingerprintObserver(std::move(pending_observer));

  GenerateRestartSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.restarts(), 1);

  std::string user_id;
  std::string label;
  chromeos::FakeBiodClient::Get()->StartEnrollSession(
      user_id, label,
      base::Bind(&FingerprintChromeOSTest::onStartSession,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  GenerateEnrollScanDoneSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.enroll_scan_dones(), 1);

  chromeos::FakeBiodClient::Get()->StartAuthSession(base::Bind(
      &FingerprintChromeOSTest::onStartSession, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  GenerateAuthScanDoneSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.auth_scan_dones(), 1);

  GenerateSessionFailedSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.session_failures(), 1);
}

TEST_F(FingerprintChromeOSTest, SimultaneousGetRecordsRequests) {
  EXPECT_EQ(GetPendingRequests(), 0);
  EXPECT_FALSE(IsRequestRunning());

  // Single request.
  GenerateGetRecordsForUserRequest(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(get_records_results(), 1);
  EXPECT_FALSE(IsRequestRunning());
  EXPECT_EQ(GetPendingRequests(), 0);
  EXPECT_TRUE(RequestDataIsReset());

  // Multiple requests at the same time.
  SimulateRequestRunning(true);
  GenerateGetRecordsForUserRequest(5);
  EXPECT_EQ(GetPendingRequests(), 5);
  SimulateRequestRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(get_records_results(), 6);
  EXPECT_FALSE(IsRequestRunning());
  EXPECT_EQ(GetPendingRequests(), 0);
  EXPECT_TRUE(RequestDataIsReset());
}

}  // namespace device
