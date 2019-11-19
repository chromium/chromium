// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/media_perception_api_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "chromeos/dbus/media_analytics/fake_media_analytics_client.h"
#include "chromeos/dbus/media_analytics/media_analytics_client.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

namespace {

class TestUpstartClient : public chromeos::FakeUpstartClient {
 public:
  TestUpstartClient() = default;

  ~TestUpstartClient() override = default;

  // Overrides behavior to queue start requests.
  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           chromeos::VoidDBusMethodCallback callback) override {
    HandleUpstartRequest(std::move(callback));
  }

  // Overrides behavior to queue restart requests.
  void RestartMediaAnalytics(
      chromeos::VoidDBusMethodCallback callback) override {
    HandleUpstartRequest(std::move(callback));
  }

  // Overrides behavior to queue stop requests.
  void StopMediaAnalytics(chromeos::VoidDBusMethodCallback callback) override {
    HandleUpstartRequest(std::move(callback));
  }

  // Triggers the next queue'd start request to succeed or fail.
  bool HandleNextUpstartRequest(bool should_succeed) {
    if (pending_upstart_request_callbacks_.empty())
      return false;

    chromeos::VoidDBusMethodCallback callback =
        std::move(pending_upstart_request_callbacks_.front());
    pending_upstart_request_callbacks_.pop();

    if (!should_succeed) {
      std::move(callback).Run(false);
      return true;
    }

    chromeos::FakeUpstartClient::StartMediaAnalytics({}, std::move(callback));
    return true;
  }

  void set_enqueue_requests(bool enqueue_requests) {
    enqueue_requests_ = enqueue_requests;
  }

 private:
  void HandleUpstartRequest(chromeos::VoidDBusMethodCallback callback) {
    pending_upstart_request_callbacks_.push(std::move(callback));
    if (!enqueue_requests_) {
      HandleNextUpstartRequest(true);
    }
  }

  base::queue<chromeos::VoidDBusMethodCallback>
      pending_upstart_request_callbacks_;

  bool enqueue_requests_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestUpstartClient);
};

void RecordServiceErrorFromStateAndRunClosure(
    base::Closure quit_run_loop,
    media_perception::ServiceError* service_error,
    media_perception::State result_state) {
  *service_error = result_state.service_error;
  quit_run_loop.Run();
}

void RecordServiceErrorFromProcessStateAndRunClosure(
    base::OnceClosure quit_run_loop,
    media_perception::ServiceError* service_error,
    media_perception::ProcessState result_state) {
  *service_error = result_state.service_error;
  std::move(quit_run_loop).Run();
}

void RecordServiceErrorFromDiagnosticsAndRunClosure(
    base::Closure quit_run_loop,
    media_perception::ServiceError* service_error,
    media_perception::Diagnostics result_diagnostics) {
  *service_error = result_diagnostics.service_error;
  quit_run_loop.Run();
}

media_perception::ServiceError SetStateAndWaitForResponse(
    MediaPerceptionAPIManager* manager,
    const media_perception::State& state) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->SetState(state, base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                      run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

media_perception::ServiceError GetStateAndWaitForResponse(
    MediaPerceptionAPIManager* manager) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->GetState(base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                               run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

media_perception::ServiceError SetComponentProcessStateAndWaitForResponse(
    MediaPerceptionAPIManager* manager,
    const media_perception::ProcessState& process_state) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->SetComponentProcessState(
      process_state,
      base::BindOnce(&RecordServiceErrorFromProcessStateAndRunClosure,
                     run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

media_perception::ServiceError GetDiagnosticsAndWaitForResponse(
    MediaPerceptionAPIManager* manager) {
  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager->GetDiagnostics(
      base::Bind(&RecordServiceErrorFromDiagnosticsAndRunClosure,
                 run_loop.QuitClosure(), &service_error));
  run_loop.Run();
  return service_error;
}

}  // namespace

class MediaPerceptionAPIManagerTest : public testing::Test {
 public:
  MediaPerceptionAPIManagerTest() = default;
  ~MediaPerceptionAPIManagerTest() override = default;

  void SetUp() override {
    chromeos::MediaAnalyticsClient::InitializeFake();

    upstart_client_ = std::make_unique<TestUpstartClient>();

    manager_ = std::make_unique<MediaPerceptionAPIManager>(&browser_context_);
    manager_->SetMountPointNonEmptyForTesting();
  }

  void TearDown() override {
    // Need to make sure that the MediaPerceptionAPIManager is destructed before
    // MediaAnalyticsClient.
    manager_.reset();
    upstart_client_.reset();
    chromeos::MediaAnalyticsClient::Shutdown();
  }

  std::unique_ptr<MediaPerceptionAPIManager> manager_;

  TestUpstartClient* upstart_client() { return upstart_client_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  std::unique_ptr<TestUpstartClient> upstart_client_;

  DISALLOW_COPY_AND_ASSIGN(MediaPerceptionAPIManagerTest);
};

TEST_F(MediaPerceptionAPIManagerTest, UpstartFailure) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RUNNING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING, service_error);

  // Check that after a failed request, setState RUNNING will go through.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, ProcessStateUpstartFailure) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::ProcessState process_state;
  process_state.status = media_perception::PROCESS_STATUS_STARTED;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetComponentProcessState(
      process_state,
      base::BindOnce(&RecordServiceErrorFromProcessStateAndRunClosure,
                     run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING, service_error);

  // Check that after a failed request, setState RUNNING will go through.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetComponentProcessStateAndWaitForResponse(manager_.get(),
                                                       process_state));
}

TEST_F(MediaPerceptionAPIManagerTest, UpstartStopFailure) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_STOPPED;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE, service_error);

  // Check that after a failed request, STOPPED will go through.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, ProcessStateUpstartStopFailure) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::ProcessState process_state;
  process_state.status = media_perception::PROCESS_STATUS_STOPPED;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetComponentProcessState(
      process_state,
      base::BindOnce(&RecordServiceErrorFromProcessStateAndRunClosure,
                     run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE, service_error);

  // Check that after a failed request, STOPPED will go through.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetComponentProcessStateAndWaitForResponse(manager_.get(),
                                                       process_state));
}

TEST_F(MediaPerceptionAPIManagerTest, UpstartRestartFailure) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RESTARTING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(false));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING, service_error);

  // Check that after a failed request, setState restarted will still go
  // through.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, UpstartStall) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RUNNING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));

  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            GetStateAndWaitForResponse(manager_.get()));
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            SetStateAndWaitForResponse(manager_.get(), state));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(true));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE, service_error);

  // Verify that after the slow start, things works as normal.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            GetStateAndWaitForResponse(manager_.get()));
  state.status = media_perception::STATUS_SUSPENDED;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, SetComponentProcessStateUpstartStall) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::ProcessState process_state;
  process_state.status = media_perception::PROCESS_STATUS_STARTED;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetComponentProcessState(
      process_state,
      base::BindOnce(&RecordServiceErrorFromProcessStateAndRunClosure,
                     run_loop.QuitClosure(), &service_error));

  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            SetComponentProcessStateAndWaitForResponse(manager_.get(),
                                                       process_state));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(true));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE, service_error);

  // Verify that after the slow start, things works as normal.
  upstart_client()->set_enqueue_requests(false);
  process_state.status = media_perception::PROCESS_STATUS_STARTED;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetComponentProcessStateAndWaitForResponse(manager_.get(),
                                                       process_state));
}
TEST_F(MediaPerceptionAPIManagerTest, UpstartRestartStall) {
  upstart_client()->set_enqueue_requests(true);
  media_perception::State state;
  state.status = media_perception::STATUS_RESTARTING;

  base::RunLoop run_loop;
  media_perception::ServiceError service_error;
  manager_->SetState(state,
                     base::Bind(&RecordServiceErrorFromStateAndRunClosure,
                                run_loop.QuitClosure(), &service_error));

  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            GetStateAndWaitForResponse(manager_.get()));
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING,
            SetStateAndWaitForResponse(manager_.get(), state));
  EXPECT_TRUE(upstart_client()->HandleNextUpstartRequest(true));
  run_loop.Run();
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE, service_error);

  // Verify that after the slow start, things works as normal.
  upstart_client()->set_enqueue_requests(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            GetStateAndWaitForResponse(manager_.get()));
  state.status = media_perception::STATUS_RUNNING;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
}

TEST_F(MediaPerceptionAPIManagerTest, MediaAnalyticsDbusError) {
  media_perception::State state;
  state.status = media_perception::STATUS_RUNNING;
  EXPECT_EQ(media_perception::SERVICE_ERROR_NONE,
            SetStateAndWaitForResponse(manager_.get(), state));
  // Disable the functionality of the fake process.
  chromeos::FakeMediaAnalyticsClient::Get()->set_process_running(false);
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE,
            GetStateAndWaitForResponse(manager_.get()));
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE,
            SetStateAndWaitForResponse(manager_.get(), state));
  // Check that getting diagnostics also errors in the same way.
  EXPECT_EQ(media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE,
            GetDiagnosticsAndWaitForResponse(manager_.get()));
}

}  // namespace extensions
