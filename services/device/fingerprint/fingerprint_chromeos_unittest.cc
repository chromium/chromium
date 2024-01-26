// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/fingerprint/fingerprint_chromeos.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/biod/fake_biod_client.h"
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

  FakeFingerprintObserver(const FakeFingerprintObserver&) = delete;
  FakeFingerprintObserver& operator=(const FakeFingerprintObserver&) = delete;

  ~FakeFingerprintObserver() override {}

  // mojom::FingerprintObserver
  void OnRestarted() override { restarts_++; }
  void OnStatusChanged(device::mojom::BiometricsManagerStatus status) override {
    DCHECK_EQ(status, device::mojom::BiometricsManagerStatus::INITIALIZED);
    status_changes_++;
  }

  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool is_complete,
                        int percent_complete) override {
    enroll_scan_dones_++;
  }

  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override {
    auth_scan_dones_++;
    last_message_ = *msg;
  }

  void OnSessionFailed() override { session_failures_++; }

  // Test status counts.
  int enroll_scan_dones() { return enroll_scan_dones_; }
  int auth_scan_dones() { return auth_scan_dones_; }
  int restarts() { return restarts_; }
  int session_failures() { return session_failures_; }

  const device::mojom::FingerprintMessage& last_message() const {
    return last_message_;
  }

 private:
  mojo::Receiver<mojom::FingerprintObserver> receiver_;
  int enroll_scan_dones_ = 0;  // Count of enroll scan done signal received.
  int auth_scan_dones_ = 0;    // Count of auth scan done signal received.
  int restarts_ = 0;           // Count of restart signal received.
  int status_changes_ = 0;     // Count of StatusChanged signal received.
  int session_failures_ = 0;   // Count of session failed signal received.

  device::mojom::FingerprintMessage
      last_message_;  // Last received FingerprintMessage.
};

class FingerprintChromeOSTest : public testing::Test {
 public:
  FingerprintChromeOSTest() = default;

  FingerprintChromeOSTest(const FingerprintChromeOSTest&) = delete;
  FingerprintChromeOSTest& operator=(const FingerprintChromeOSTest&) = delete;

  ~FingerprintChromeOSTest() override = default;

  void SetUp() override {
    ash::BiodClient::InitializeFake();
    fingerprint_ = base::WrapUnique(new FingerprintChromeOS());
  }

  void TearDown() override {
    fingerprint_.reset();
    ash::BiodClient::Shutdown();
  }

  FingerprintChromeOS* fingerprint() { return fingerprint_.get(); }

  void GenerateRestartSignal() { fingerprint_->BiodServiceRestarted(); }

  void GenerateSessionStateSignal() {
    fingerprint_->BiodServiceStatusChanged(
        biod::BiometricsManagerStatus::INITIALIZED);
  }

  void GenerateEnrollScanDoneSignal() {
    std::string fake_fingerprint_data;
    ash::FakeBiodClient::Get()->SendEnrollScanDone(
        fake_fingerprint_data, biod::SCAN_RESULT_SUCCESS, true,
        -1 /* percent_complete */);
  }

  void GenerateAuthScanDoneSignal(const biod::FingerprintMessage& msg) {
    std::string fake_fingerprint_data;
    ash::FakeBiodClient::Get()->SendAuthScanDone(fake_fingerprint_data, msg);
  }

  void GenerateSessionFailedSignal() {
    ash::FakeBiodClient::Get()->SendSessionFailed();
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

  void OnGetRecords(
      const base::flat_map<std::string, std::string>& fingerprints_list_mapping,
      bool success) {
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
};

TEST_F(FingerprintChromeOSTest, FingerprintObserverTest) {
  mojo::PendingRemote<mojom::FingerprintObserver> pending_observer;
  FakeFingerprintObserver observer(
      pending_observer.InitWithNewPipeAndPassReceiver());
  fingerprint()->AddFingerprintObserver(std::move(pending_observer));

  GenerateRestartSignal();
  GenerateSessionStateSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.restarts(), 1);

  std::string user_id;
  std::string label;
  ash::FakeBiodClient::Get()->StartEnrollSession(
      user_id, label,
      base::BindOnce(&FingerprintChromeOSTest::onStartSession,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  GenerateEnrollScanDoneSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.enroll_scan_dones(), 1);

  biod::FingerprintMessage msg;
  ash::FakeBiodClient::Get()->StartAuthSession(base::BindOnce(
      &FingerprintChromeOSTest::onStartSession, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  msg.set_scan_result(biod::SCAN_RESULT_SUCCESS);
  GenerateAuthScanDoneSignal(msg);
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

TEST_F(FingerprintChromeOSTest, FingerprintScanResultConvertTest) {
  mojo::PendingRemote<mojom::FingerprintObserver> pending_observer;
  FakeFingerprintObserver observer(
      pending_observer.InitWithNewPipeAndPassReceiver());
  fingerprint()->AddFingerprintObserver(std::move(pending_observer));

  ash::FakeBiodClient::Get()->StartAuthSession(base::BindOnce(
      &FingerprintChromeOSTest::onStartSession, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  biod::FingerprintMessage msg;
  msg.set_scan_result(biod::SCAN_RESULT_SUCCESS);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::SUCCESS);

  msg.set_scan_result(biod::SCAN_RESULT_PARTIAL);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::PARTIAL);

  msg.set_scan_result(biod::SCAN_RESULT_INSUFFICIENT);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::INSUFFICIENT);

  msg.set_scan_result(biod::SCAN_RESULT_SENSOR_DIRTY);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::SENSOR_DIRTY);

  msg.set_scan_result(biod::SCAN_RESULT_TOO_SLOW);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::TOO_SLOW);

  msg.set_scan_result(biod::SCAN_RESULT_TOO_FAST);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::TOO_FAST);

  msg.set_scan_result(biod::SCAN_RESULT_IMMOBILE);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::IMMOBILE);

  msg.set_scan_result(biod::SCAN_RESULT_NO_MATCH);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kScanResult);
  EXPECT_EQ(observer.last_message().get_scan_result(),
            device::mojom::ScanResult::NO_MATCH);
}

// Make sure that compilation fails if a new value is added and this assert is
// not updated. When updating this, please extend unit tests to check newly
// added value.
static_assert(device::mojom::ScanResult::kMaxValue ==
              device::mojom::ScanResult::NO_MATCH);

TEST_F(FingerprintChromeOSTest, FingerprintErrorConvertTest) {
  mojo::PendingRemote<mojom::FingerprintObserver> pending_observer;
  FakeFingerprintObserver observer(
      pending_observer.InitWithNewPipeAndPassReceiver());
  fingerprint()->AddFingerprintObserver(std::move(pending_observer));

  ash::FakeBiodClient::Get()->StartAuthSession(base::BindOnce(
      &FingerprintChromeOSTest::onStartSession, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  biod::FingerprintMessage msg;
  msg.set_error(biod::ERROR_HW_UNAVAILABLE);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::HW_UNAVAILABLE);

  msg.set_error(biod::ERROR_UNABLE_TO_PROCESS);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::UNABLE_TO_PROCESS);

  msg.set_error(biod::ERROR_TIMEOUT);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::TIMEOUT);

  msg.set_error(biod::ERROR_NO_SPACE);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::NO_SPACE);

  msg.set_error(biod::ERROR_CANCELED);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::CANCELED);

  msg.set_error(biod::ERROR_UNABLE_TO_REMOVE);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::UNABLE_TO_REMOVE);

  msg.set_error(biod::ERROR_LOCKOUT);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::LOCKOUT);

  msg.set_error(biod::ERROR_NO_TEMPLATES);
  GenerateAuthScanDoneSignal(msg);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.last_message().which(),
            device::mojom::FingerprintMessage::Tag::kFingerprintError);
  EXPECT_EQ(observer.last_message().get_fingerprint_error(),
            device::mojom::FingerprintError::NO_TEMPLATES);
}

// Make sure that compilation fails if a new value is added and this assert is
// not updated. When updating this, please extend unit tests to check newly
// added value.
static_assert(device::mojom::FingerprintError::kMaxValue ==
              device::mojom::FingerprintError::NO_TEMPLATES);

}  // namespace device
