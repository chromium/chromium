// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_background_services_context_impl.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "content/browser/devtools/devtools_background_services.pb.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {
namespace {

using testing::_;

const std::string kEventName = "Test Event";
const std::string kInstanceId = "my-instance";

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() {}
  ~TestBrowserClient() override {}

  void UpdateDevToolsBackgroundServiceExpiration(
      BrowserContext* browser_context,
      int service,
      base::Time expiration_time) override {
    exp_dict_[service] = expiration_time;
  }

  base::flat_map<int, base::Time> GetDevToolsBackgroundServiceExpirations(
      BrowserContext* browser_context) override {
    return exp_dict_;
  }

 private:
  base::flat_map<int, base::Time> exp_dict_;
};

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::OnceClosure quit_closure,
                              blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  std::move(quit_closure).Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  std::move(quit_closure).Run();
}

void DidGetLoggedBackgroundServiceEvents(
    base::OnceClosure quit_closure,
    std::vector<devtools::proto::BackgroundServiceEvent>* out_feature_states,
    std::vector<devtools::proto::BackgroundServiceEvent> feature_states) {
  *out_feature_states = std::move(feature_states);
  std::move(quit_closure).Run();
}

}  // namespace

class DevToolsBackgroundServicesContextTest
    : public ::testing::Test,
      DevToolsBackgroundServicesContextImpl::EventObserver {
 public:
  DevToolsBackgroundServicesContextTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        embedded_worker_test_helper_(base::FilePath() /* in memory */) {}

  ~DevToolsBackgroundServicesContextTest() override = default;

  void SetUp() override {
    // Register Service Worker.
    service_worker_registration_id_ = RegisterServiceWorker();
    ASSERT_NE(service_worker_registration_id_,
              blink::mojom::kInvalidServiceWorkerRegistrationId);

    browser_client_ = std::make_unique<TestBrowserClient>();
    SetBrowserClientForTesting(browser_client_.get());

    SimulateBrowserRestart();
  }

  void TearDown() override { context_->RemoveObserver(this); }

 protected:
  MOCK_METHOD1(OnEventReceived,
               void(const devtools::proto::BackgroundServiceEvent& event));
  MOCK_METHOD2(OnRecordingStateChanged,
               void(bool shoul_record,
                    devtools::proto::BackgroundService service));

  void SimulateBrowserRestart() {
    if (context_)
      context_->RemoveObserver(this);
    // Create |context_|.
    context_ = base::MakeRefCounted<DevToolsBackgroundServicesContextImpl>(
        &browser_context_, embedded_worker_test_helper_.context_wrapper());
    context_->AddObserver(this);
    ASSERT_TRUE(context_);
  }

  void SimulateOneWeekPassing() {
    base::Time one_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);
    context_->expiration_times_
        [devtools::proto::BackgroundService::BACKGROUND_FETCH] = one_week_ago;
  }

  bool IsRecording() {
    return context_->IsRecording(
        devtools::proto::BackgroundService::BACKGROUND_FETCH);
  }

  base::Time GetExpirationTime() {
    return context_->expiration_times_
        [devtools::proto::BackgroundService::BACKGROUND_FETCH];
  }

  std::vector<devtools::proto::BackgroundServiceEvent>
  GetLoggedBackgroundServiceEvents() {
    std::vector<devtools::proto::BackgroundServiceEvent> feature_states;

    base::RunLoop run_loop;
    context_->GetLoggedBackgroundServiceEvents(
        devtools::proto::BackgroundService::BACKGROUND_FETCH,
        base::BindOnce(&DidGetLoggedBackgroundServiceEvents,
                       run_loop.QuitClosure(), &feature_states));
    run_loop.Run();

    return feature_states;
  }

  void LogTestBackgroundServiceEvent(const std::string& log_message) {
    context_->LogBackgroundServiceEventOnCoreThread(
        service_worker_registration_id_, origin_,
        DevToolsBackgroundService::kBackgroundFetch, kEventName, kInstanceId,
        {{"key", log_message}});
  }

  void StartRecording() {
    EXPECT_CALL(
        *this, OnRecordingStateChanged(
                   true, devtools::proto::BackgroundService::BACKGROUND_FETCH));
    context_->StartRecording(
        devtools::proto::BackgroundService::BACKGROUND_FETCH);

    // Wait for the messages to propagate to the browser client.
    task_environment_.RunUntilIdle();
  }

  void StopRecording() {
    EXPECT_CALL(
        *this,
        OnRecordingStateChanged(
            false, devtools::proto::BackgroundService::BACKGROUND_FETCH));
    context_->StopRecording(
        devtools::proto::BackgroundService::BACKGROUND_FETCH);

    // Wait for the messages to propagate to the browser client.
    task_environment_.RunUntilIdle();
  }

  void ClearLoggedBackgroundServiceEvents() {
    context_->ClearLoggedBackgroundServiceEvents(
        devtools::proto::BackgroundService::BACKGROUND_FETCH);
  }

  BrowserTaskEnvironment task_environment_;  // Must be first member.
  url::Origin origin_ = url::Origin::Create(GURL("https://example.com"));
  int64_t service_worker_registration_id_ =
      blink::mojom::kInvalidServiceWorkerRegistrationId;

 private:
  int64_t RegisterServiceWorker() {
    GURL script_url(origin_.GetURL().spec() + "sw.js");
    int64_t service_worker_registration_id =
        blink::mojom::kInvalidServiceWorkerRegistrationId;

    {
      blink::mojom::ServiceWorkerRegistrationOptions options;
      options.scope = origin_.GetURL();
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->RegisterServiceWorker(
          script_url, options, blink::mojom::FetchClientSettingsObject::New(),
          base::BindOnce(&DidRegisterServiceWorker,
                         &service_worker_registration_id,
                         run_loop.QuitClosure()));

      run_loop.Run();
    }

    if (service_worker_registration_id ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    {
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->storage()->FindRegistrationForId(
          service_worker_registration_id, origin_.GetURL(),
          base::BindOnce(&DidFindServiceWorkerRegistration,
                         &service_worker_registration_,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }

    // Wait for the worker to be activated.
    base::RunLoop().RunUntilIdle();

    if (!service_worker_registration_) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    return service_worker_registration_id;
  }

  EmbeddedWorkerTestHelper embedded_worker_test_helper_;
  TestBrowserContext browser_context_;
  scoped_refptr<DevToolsBackgroundServicesContextImpl> context_;
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration_;
  std::unique_ptr<ContentBrowserClient> browser_client_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBackgroundServicesContextTest);
};

TEST_F(DevToolsBackgroundServicesContextTest,
       NothingStoredWithRecordingModeOff) {
  // Initially there are no entries.
  EXPECT_TRUE(GetLoggedBackgroundServiceEvents().empty());

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  LogTestBackgroundServiceEvent("f2");

  // There should still be nothing since recording mode is off.
  EXPECT_TRUE(GetLoggedBackgroundServiceEvents().empty());
}

TEST_F(DevToolsBackgroundServicesContextTest, GetLoggedEvents) {
  StartRecording();

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  LogTestBackgroundServiceEvent("f2");

  // Check the values.
  auto feature_events = GetLoggedBackgroundServiceEvents();
  ASSERT_EQ(feature_events.size(), 2u);

  for (const auto& feature_event : feature_events) {
    EXPECT_EQ(feature_event.background_service(),
              devtools::proto::BackgroundService::BACKGROUND_FETCH);
    EXPECT_EQ(feature_event.origin(), origin_.GetURL().spec());
    EXPECT_EQ(feature_event.service_worker_registration_id(),
              service_worker_registration_id_);
    EXPECT_EQ(feature_event.event_name(), kEventName);
    EXPECT_EQ(feature_event.instance_id(), kInstanceId);
    ASSERT_EQ(feature_event.event_metadata().size(), 1u);
  }

  EXPECT_EQ(feature_events[0].event_metadata().at("key"), "f1");
  EXPECT_EQ(feature_events[1].event_metadata().at("key"), "f2");

  EXPECT_LE(feature_events[0].timestamp(), feature_events[1].timestamp());
}

TEST_F(DevToolsBackgroundServicesContextTest, StopRecording) {
  StartRecording();
  // Initially there are no entries.
  EXPECT_TRUE(GetLoggedBackgroundServiceEvents().empty());

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  StopRecording();
  LogTestBackgroundServiceEvent("f2");

  // Check the values.
  ASSERT_EQ(GetLoggedBackgroundServiceEvents().size(), 1u);
}

TEST_F(DevToolsBackgroundServicesContextTest, DelegateExpirationTimes) {
  // Initially expiration time is null.
  EXPECT_TRUE(GetExpirationTime().is_null());
  EXPECT_FALSE(IsRecording());

  // Toggle Recording mode, and now this should be non-null.
  StartRecording();
  EXPECT_FALSE(GetExpirationTime().is_null());
  EXPECT_TRUE(IsRecording());

  // The value should still be there on browser restarts.
  SimulateBrowserRestart();
  EXPECT_FALSE(GetExpirationTime().is_null());
  EXPECT_TRUE(IsRecording());

  // Stopping Recording mode should clear the value.
  StopRecording();
  EXPECT_TRUE(GetExpirationTime().is_null());
  EXPECT_FALSE(IsRecording());
  SimulateBrowserRestart();
  EXPECT_TRUE(GetExpirationTime().is_null());
  EXPECT_FALSE(IsRecording());
}

TEST_F(DevToolsBackgroundServicesContextTest, RecordingExpiration) {
  // Initially expiration time is null.
  EXPECT_FALSE(IsRecording());

  // Toggle Recording mode, and now this should be non-null.
  StartRecording();
  EXPECT_TRUE(IsRecording());

  SimulateOneWeekPassing();
  EXPECT_FALSE(GetExpirationTime().is_null());

  // Recording should be true, with an expired value.
  EXPECT_TRUE(IsRecording());

  // Logging should not happen.
  EXPECT_CALL(*this, OnEventReceived(_)).Times(0);
  // Observers should be informed when recording stops.
  EXPECT_CALL(*this,
              OnRecordingStateChanged(
                  false, devtools::proto::BackgroundService::BACKGROUND_FETCH));
  LogTestBackgroundServiceEvent("f1");

  task_environment_.RunUntilIdle();

  // The expiration time entry should be cleared.
  EXPECT_TRUE(GetExpirationTime().is_null());
  EXPECT_FALSE(IsRecording());
}

TEST_F(DevToolsBackgroundServicesContextTest, ClearLoggedEvents) {
  StartRecording();

  // "Log" some events and wait for them to finish.
  LogTestBackgroundServiceEvent("f1");
  LogTestBackgroundServiceEvent("f2");

  // Check the values.
  auto feature_events = GetLoggedBackgroundServiceEvents();
  ASSERT_EQ(feature_events.size(), 2u);

  ClearLoggedBackgroundServiceEvents();

  // Should be empty now.
  feature_events = GetLoggedBackgroundServiceEvents();
  EXPECT_TRUE(feature_events.empty());
}

TEST_F(DevToolsBackgroundServicesContextTest, EventObserverCalled) {
  {
    EXPECT_CALL(*this, OnEventReceived(_)).Times(0);
    LogTestBackgroundServiceEvent("f1");
    task_environment_.RunUntilIdle();
  }

  StartRecording();

  {
    EXPECT_CALL(*this, OnEventReceived(_));
    LogTestBackgroundServiceEvent("f2");
    task_environment_.RunUntilIdle();
  }
}

}  // namespace content
