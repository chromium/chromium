// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_context.h"

#include <stdint.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void SaveResponseCallback(bool* called,
                          int64_t* store_registration_id,
                          blink::ServiceWorkerStatusCode status,
                          const std::string& status_message,
                          int64_t registration_id) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

ServiceWorkerContextCore::RegistrationCallback MakeRegisteredCallback(
    bool* called,
    int64_t* store_registration_id) {
  return base::BindOnce(&SaveResponseCallback, called, store_registration_id);
}

void RegisteredCallback(base::OnceClosure quit_closure,
                        blink::ServiceWorkerStatusCode status,
                        const std::string& status_message,
                        int64_t registration_id) {
  std::move(quit_closure).Run();
}

void CallCompletedCallback(bool* called, blink::ServiceWorkerStatusCode) {
  *called = true;
}

ServiceWorkerContextCore::UnregistrationCallback MakeUnregisteredCallback(
    bool* called) {
  return base::BindOnce(&CallCompletedCallback, called);
}

void ExpectRegisteredWorkers(
    blink::ServiceWorkerStatusCode expect_status,
    bool expect_waiting,
    bool expect_active,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  ASSERT_EQ(expect_status, status);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    EXPECT_FALSE(registration.get());
    return;
  }

  if (expect_waiting) {
    EXPECT_TRUE(registration->waiting_version());
  } else {
    EXPECT_FALSE(registration->waiting_version());
  }

  if (expect_active) {
    EXPECT_TRUE(registration->active_version());
  } else {
    EXPECT_FALSE(registration->active_version());
  }

  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kImports,
            registration->update_via_cache());
}

class InstallActivateWorker : public FakeServiceWorker {
 public:
  explicit InstallActivateWorker(EmbeddedWorkerTestHelper* helper)
      : FakeServiceWorker(helper) {}
  ~InstallActivateWorker() override = default;

  const std::vector<ServiceWorkerMetrics::EventType>& events() const {
    return events_;
  }

  void SetToRejectInstall() { reject_install_ = true; }

  void SetToRejectActivate() { reject_activate_ = true; }

  void DispatchInstallEvent(
      blink::mojom::ServiceWorker::DispatchInstallEventCallback callback)
      override {
    events_.emplace_back(ServiceWorkerMetrics::EventType::INSTALL);
    std::move(callback).Run(
        reject_install_ ? blink::mojom::ServiceWorkerEventStatus::REJECTED
                        : blink::mojom::ServiceWorkerEventStatus::COMPLETED,
        /*fetch_count=*/0);
  }

  void DispatchActivateEvent(
      blink::mojom::ServiceWorker::DispatchActivateEventCallback callback)
      override {
    events_.emplace_back(ServiceWorkerMetrics::EventType::ACTIVATE);
    std::move(callback).Run(
        reject_activate_ ? blink::mojom::ServiceWorkerEventStatus::REJECTED
                         : blink::mojom::ServiceWorkerEventStatus::COMPLETED);
  }

  void OnConnectionError() override {
    // Do nothing. This allows the object to stay until the test is over, so
    // |events_| can be accessed even after the worker is stopped in the case of
    // rejected install.
  }

 private:
  std::vector<ServiceWorkerMetrics::EventType> events_;
  bool reject_install_ = false;
  bool reject_activate_ = false;
};

enum NotificationType {
  REGISTRATION_COMPLETED,
  REGISTRATION_STORED,
  REGISTRATION_DELETED,
  STORAGE_RECOVERED,
};

struct NotificationLog {
  NotificationType type;
  GURL scope;
  int64_t registration_id;
};

}  // namespace

class ServiceWorkerContextTest : public ServiceWorkerContextCoreObserver,
                                 public testing::Test {
 public:
  ServiceWorkerContextTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    helper_->context_wrapper()->AddObserver(this);
  }

  void TearDown() override {
    helper_.reset();
    // The helper may post tasks to release resources in |temp_dir_|. Allow
    // them to run now so that the directory may be deleted.
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(!temp_dir_.IsValid() || temp_dir_.Delete())
        << temp_dir_.GetPath();
  }

  // ServiceWorkerContextCoreObserver overrides.
  void OnRegistrationCompleted(int64_t registration_id,
                               const GURL& scope,
                               const blink::StorageKey& key) override {
    NotificationLog log;
    log.type = REGISTRATION_COMPLETED;
    log.scope = scope;
    log.registration_id = registration_id;
    notifications_.push_back(log);
  }
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope,
                            const blink::StorageKey& key) override {
    NotificationLog log;
    log.type = REGISTRATION_STORED;
    log.scope = scope;
    log.registration_id = registration_id;
    notifications_.push_back(log);
  }
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& scope,
                             const blink::StorageKey& key) override {
    NotificationLog log;
    log.type = REGISTRATION_DELETED;
    log.scope = scope;
    log.registration_id = registration_id;
    notifications_.push_back(log);
  }
  void OnStorageWiped() override {
    NotificationLog log;
    log.type = STORAGE_RECOVERED;
    notifications_.push_back(log);
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }
  void GetTemporaryDirectory(base::FilePath* temp_dir) {
    ASSERT_FALSE(temp_dir_.IsValid());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    *temp_dir = temp_dir_.GetPath();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<NotificationLog> notifications_;
};

class RecordableEmbeddedWorkerInstanceClient
    : public FakeEmbeddedWorkerInstanceClient {
 public:
  enum class Message { StartWorker, StopWorker };

  explicit RecordableEmbeddedWorkerInstanceClient(
      EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}

  RecordableEmbeddedWorkerInstanceClient(
      const RecordableEmbeddedWorkerInstanceClient&) = delete;
  RecordableEmbeddedWorkerInstanceClient& operator=(
      const RecordableEmbeddedWorkerInstanceClient&) = delete;

  void OnConnectionError() override {
    // Do nothing. This allows the object to stay until the test is over, so
    // |events_| can be accessed even after the worker is stopped in the case of
    // rejected install.
  }

  const std::vector<Message>& events() const { return events_; }

 protected:
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override {
    events_.push_back(Message::StartWorker);
    FakeEmbeddedWorkerInstanceClient::StartWorker(std::move(params));
  }

  void StopWorker() override {
    events_.push_back(Message::StopWorker);

    // Let stop complete, but don't call the base class's StopWorker(), which
    // would destroy |this| before the test can retrieve events().
    host()->OnStopped();
  }

 private:
  std::vector<Message> events_;
};

class TestServiceWorkerContextObserver : public ServiceWorkerContextObserver {
 public:
  enum class EventType {
    RegistrationCompleted,
    RegistrationStored,
    VersionActivated,
    VersionRedundant,
    ControlleeAdded,
    ControlleeRemoved,
    NoControllees,
    ControlleeNavigationCommitted,
    VersionStartedRunning,
    VersionStoppedRunning,
    Destruct
  };
  struct EventLog {
    EventType type;
    std::optional<GURL> url;
    std::optional<int64_t> version_id;
    std::optional<int64_t> registration_id;
    std::optional<bool> is_running;
  };

  explicit TestServiceWorkerContextObserver(ServiceWorkerContext* context) {
    scoped_observation_.Observe(context);
  }

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  ~TestServiceWorkerContextObserver() override = default;

  void OnRegistrationCompleted(const GURL& scope) override {
    EventLog log;
    log.type = EventType::RegistrationCompleted;
    log.url = scope;
    events_.push_back(log);
  }

  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override {
    EventLog log;
    log.type = EventType::RegistrationStored;
    log.registration_id = registration_id;
    log.url = scope;
    events_.push_back(log);
  }

  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    EventLog log;
    log.type = EventType::VersionActivated;
    log.version_id = version_id;
    log.url = scope;
    events_.push_back(log);
  }

  void OnVersionRedundant(int64_t version_id, const GURL& scope) override {
    EventLog log;
    log.type = EventType::VersionRedundant;
    log.version_id = version_id;
    log.url = scope;
    events_.push_back(log);
  }

  void OnControlleeAdded(int64_t version_id,
                         const std::string& client_uuid,
                         const ServiceWorkerClientInfo& client_info) override {
    EventLog log;
    log.type = EventType::ControlleeAdded;
    log.version_id = version_id;
    events_.push_back(log);
  }

  void OnControlleeRemoved(int64_t version_id,
                           const std::string& client_uuid) override {
    EventLog log;
    log.type = EventType::ControlleeRemoved;
    log.version_id = version_id;
    events_.push_back(log);
  }

  void OnNoControllees(int64_t version_id, const GURL& scope) override {
    EventLog log;
    log.type = EventType::NoControllees;
    log.version_id = version_id;
    log.url = scope;
    events_.push_back(log);
  }

  void OnControlleeNavigationCommitted(
      int64_t version_id,
      const std::string& client_uuid,
      GlobalRenderFrameHostId render_frame_host_id) override {
    EventLog log;
    log.type = EventType::ControlleeNavigationCommitted;
    log.version_id = version_id;
    events_.push_back(log);
  }

  void OnVersionStartedRunning(
      int64_t version_id,
      const ServiceWorkerRunningInfo& running_info) override {
    EventLog log;
    log.type = EventType::VersionStartedRunning;
    log.version_id = version_id;
    events_.push_back(log);
  }

  void OnVersionStoppedRunning(int64_t version_id) override {
    EventLog log;
    log.type = EventType::VersionStoppedRunning;
    log.version_id = version_id;
    events_.push_back(log);
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    DCHECK(scoped_observation_.IsObserving());
    scoped_observation_.Reset();

    EventLog log;
    log.type = EventType::Destruct;
    events_.push_back(log);
  }

  const std::vector<EventLog>& events() { return events_; }

 private:
  base::ScopedObservation<ServiceWorkerContext, ServiceWorkerContextObserver>
      scoped_observation_{this};
  std::vector<EventLog> events_;
};

// Make sure OnRegistrationCompleted is called on observer.
TEST_F(ServiceWorkerContextTest, RegistrationCompletedObserver) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  TestServiceWorkerContextObserver observer(context_wrapper());

  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(called);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  std::vector<TestServiceWorkerContextObserver::EventLog> events;

  // Filter the events to be verified.
  for (auto event : observer.events()) {
    if (event.type == TestServiceWorkerContextObserver::EventType::
                          RegistrationCompleted ||
        event.type ==
            TestServiceWorkerContextObserver::EventType::RegistrationStored ||
        event.type ==
            TestServiceWorkerContextObserver::EventType::VersionActivated)
      events.push_back(event);
  }
  ASSERT_EQ(3u, events.size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::RegistrationCompleted,
            events[0].type);
  EXPECT_EQ(scope, events[0].url);
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::RegistrationStored,
            events[1].type);
  EXPECT_EQ(scope, events[1].url);
  EXPECT_EQ(registration_id, events[1].registration_id);
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::VersionActivated,
            events[2].type);
  EXPECT_EQ(scope, events[2].url);
}

// Make sure OnControlleeAdded, OnControlleeRemoved and OnNoControllees are
// called on observer.
TEST_F(ServiceWorkerContextTest, Observer_ControlleeEvents) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto registration = ServiceWorkerRegistration::Create(
      options, key, 1l /* dummy registration id */, context()->AsWeakPtr(),
      blink::mojom::AncestorFrameType::kNormalFrame);

  auto version = base::MakeRefCounted<ServiceWorkerVersion>(
      registration.get(), script_url, blink::mojom::ScriptType::kClassic,
      2l /* dummy version id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Flush tasks related to `SetStatus(ACTIVATED)` above before creating
  // `observer`.
  base::RunLoop().RunUntilIdle();

  ScopedServiceWorkerClient service_worker_client =
      CreateServiceWorkerClient(context());

  TestServiceWorkerContextObserver observer(context_wrapper());

  version->AddControllee(service_worker_client.get());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, observer.events().size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::ControlleeAdded,
            observer.events()[0].type);

  CommittedServiceWorkerClient committed_service_worker_client(
      std::move(service_worker_client),
      GlobalRenderFrameHostId(helper_->mock_render_process_id(),
                              /*mock frame_routing_id=*/1));
  version->OnControlleeNavigationCommitted(
      committed_service_worker_client->client_uuid(),
      committed_service_worker_client->GetRenderFrameHostId());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, observer.events().size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::
                ControlleeNavigationCommitted,
            observer.events()[1].type);

  version->RemoveControllee(committed_service_worker_client->client_uuid());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(4u, observer.events().size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::ControlleeRemoved,
            observer.events()[2].type);
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::NoControllees,
            observer.events()[3].type);
}

// Make sure OnVersionActivated is called on observer.
TEST_F(ServiceWorkerContextTest, VersionActivatedObserver) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto registration = ServiceWorkerRegistration::Create(
      options, key, 1l /* dummy registration id */, context()->AsWeakPtr(),
      blink::mojom::AncestorFrameType::kNormalFrame);

  auto version = base::MakeRefCounted<ServiceWorkerVersion>(
      registration.get(), script_url, blink::mojom::ScriptType::kClassic,
      2l /* dummy version id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());

  TestServiceWorkerContextObserver observer(context_wrapper());

  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNoHandler);
  version->SetStatus(ServiceWorkerVersion::Status::ACTIVATED);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, observer.events().size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::VersionActivated,
            observer.events()[0].type);
  EXPECT_EQ(2l, observer.events()[0].version_id);
}

// Make sure OnVersionRedundant is called on observer.
TEST_F(ServiceWorkerContextTest, VersionRedundantObserver) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto registration = ServiceWorkerRegistration::Create(
      options, key, 1l /* dummy registration id */, context()->AsWeakPtr(),
      blink::mojom::AncestorFrameType::kNormalFrame);

  auto version = base::MakeRefCounted<ServiceWorkerVersion>(
      registration.get(), script_url, blink::mojom::ScriptType::kClassic,
      2l /* dummy version id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      context()->AsWeakPtr());

  TestServiceWorkerContextObserver observer(context_wrapper());

  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNoHandler);
  version->SetStatus(ServiceWorkerVersion::Status::REDUNDANT);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, observer.events().size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::VersionRedundant,
            observer.events()[0].type);
  EXPECT_EQ(2l, observer.events()[0].version_id);
}

// Make sure OnVersionStartedRunning and OnVersionStoppedRunning are called on
// observer.
TEST_F(ServiceWorkerContextTest, OnVersionRunningStatusChangedObserver) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  TestServiceWorkerContextObserver observer(context_wrapper());
  base::RunLoop run_loop;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      base::BindOnce(&RegisteredCallback, run_loop.QuitClosure()),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());
  run_loop.Run();

  context_wrapper()->StopAllServiceWorkersForStorageKey(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)));
  base::RunLoop().RunUntilIdle();

  std::vector<TestServiceWorkerContextObserver::EventLog> events;

  // Filter the events to be verified.
  for (auto event : observer.events()) {
    if (event.type == TestServiceWorkerContextObserver::EventType::
                          VersionStartedRunning ||
        event.type == TestServiceWorkerContextObserver::EventType::
                          VersionStoppedRunning) {
      events.push_back(event);
    }
  }

  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::VersionStartedRunning,
            events[0].type);
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::VersionStoppedRunning,
            events[1].type);
  EXPECT_EQ(events[0].version_id, events[1].version_id);
}

// Make sure OnDestruct is called on observer.
TEST_F(ServiceWorkerContextTest, OnDestructObserver) {
  ServiceWorkerContextWrapper* context = context_wrapper();
  TestServiceWorkerContextObserver observer(context);
  helper_->ShutdownContext();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, observer.events().size());
  EXPECT_EQ(TestServiceWorkerContextObserver::EventType::Destruct,
            observer.events()[0].type);
}

// Make sure basic registration is working.
TEST_F(ServiceWorkerContextTest, Register) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto* client =
      helper_
          ->AddNewPendingInstanceClient<RecordableEmbeddedWorkerInstanceClient>(
              helper_.get());
  auto* worker =
      helper_->AddNewPendingServiceWorker<InstallActivateWorker>(helper_.get());

  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  ASSERT_EQ(2u, worker->events().size());
  ASSERT_EQ(1u, client->events().size());
  EXPECT_EQ(RecordableEmbeddedWorkerInstanceClient::Message::StartWorker,
            client->events()[0]);
  EXPECT_EQ(ServiceWorkerMetrics::EventType::INSTALL, worker->events()[0]);
  EXPECT_EQ(ServiceWorkerMetrics::EventType::ACTIVATE, worker->events()[1]);

  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  ASSERT_EQ(2u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(scope, notifications_[1].scope);
  EXPECT_EQ(registration_id, notifications_[1].registration_id);

  context()->registry()->FindRegistrationForId(
      registration_id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kOk,
                     false /* expect_waiting */, true /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when the service worker rejects the install event. The
// registration callback should indicate success, but there should be no waiting
// or active worker in the registration.
TEST_F(ServiceWorkerContextTest, Register_RejectInstall) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto* client =
      helper_
          ->AddNewPendingInstanceClient<RecordableEmbeddedWorkerInstanceClient>(
              helper_.get());
  auto* worker =
      helper_->AddNewPendingServiceWorker<InstallActivateWorker>(helper_.get());
  worker->SetToRejectInstall();

  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  ASSERT_EQ(1u, worker->events().size());
  ASSERT_EQ(2u, client->events().size());
  EXPECT_EQ(RecordableEmbeddedWorkerInstanceClient::Message::StartWorker,
            client->events()[0]);
  EXPECT_EQ(ServiceWorkerMetrics::EventType::INSTALL, worker->events()[0]);
  EXPECT_EQ(RecordableEmbeddedWorkerInstanceClient::Message::StopWorker,
            client->events()[1]);

  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  ASSERT_EQ(1u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);

  context()->registry()->FindRegistrationForId(
      registration_id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kErrorNotFound,
                     false /* expect_waiting */, false /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Test registration when the service worker rejects the activate event. The
// worker should be activated anyway.
TEST_F(ServiceWorkerContextTest, Register_RejectActivate) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto* client =
      helper_
          ->AddNewPendingInstanceClient<RecordableEmbeddedWorkerInstanceClient>(
              helper_.get());
  auto* worker =
      helper_->AddNewPendingServiceWorker<InstallActivateWorker>(helper_.get());
  worker->SetToRejectActivate();

  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  ASSERT_EQ(2u, worker->events().size());
  ASSERT_EQ(1u, client->events().size());
  EXPECT_EQ(RecordableEmbeddedWorkerInstanceClient::Message::StartWorker,
            client->events()[0]);
  EXPECT_EQ(ServiceWorkerMetrics::EventType::INSTALL, worker->events()[0]);
  EXPECT_EQ(ServiceWorkerMetrics::EventType::ACTIVATE, worker->events()[1]);

  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  ASSERT_EQ(2u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(scope, notifications_[1].scope);
  EXPECT_EQ(registration_id, notifications_[1].registration_id);

  context()->registry()->FindRegistrationForId(
      registration_id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kOk,
                     false /* expect_waiting */, true /* expect_active */));
  base::RunLoop().RunUntilIdle();
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerContextTest, Unregister) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  bool called = false;
  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      GURL("https://www.example.com/service_worker.js"), key, options,
      blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, registration_id);

  called = false;
  context()->UnregisterServiceWorker(scope, key, /*is_immediate=*/false,
                                     MakeUnregisteredCallback(&called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  context()->registry()->FindRegistrationForId(
      registration_id,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kErrorNotFound,
                     false /* expect_waiting */, false /* expect_active */));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(scope, notifications_[1].scope);
  EXPECT_EQ(registration_id, notifications_[1].registration_id);
  EXPECT_EQ(REGISTRATION_DELETED, notifications_[2].type);
  EXPECT_EQ(scope, notifications_[2].scope);
  EXPECT_EQ(registration_id, notifications_[2].registration_id);
}

// Make sure registrations are cleaned up when they are unregistered in bulk.
TEST_F(ServiceWorkerContextTest, UnregisterMultiple) {
  GURL origin1_s1("https://www.example.com/test");
  GURL origin1_s2("https://www.example.com/hello");
  GURL origin2_s1("https://www.example.com:8080/again");
  GURL origin3_s1("https://www.other.com/");
  const blink::StorageKey key1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin1_s1));
  const blink::StorageKey key2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin2_s1));
  const blink::StorageKey key3 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin3_s1));

  EXPECT_EQ(key1, blink::StorageKey::CreateFirstParty(
                      url::Origin::Create(origin1_s2)));

  int64_t registration_id1 = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t registration_id2 = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t registration_id3 = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t registration_id4 = blink::mojom::kInvalidServiceWorkerRegistrationId;

  {
    bool called = false;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = origin1_s1;
    context()->RegisterServiceWorker(
        GURL("https://www.example.com/service_worker.js"), key1, options,
        blink::mojom::FetchClientSettingsObject::New(),
        MakeRegisteredCallback(&called, &registration_id1),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        PolicyContainerPolicies());
    ASSERT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(called);
  }

  {
    bool called = false;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = origin1_s2;
    context()->RegisterServiceWorker(
        GURL("https://www.example.com/service_worker2.js"), key1, options,
        blink::mojom::FetchClientSettingsObject::New(),
        MakeRegisteredCallback(&called, &registration_id2),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        PolicyContainerPolicies());
    ASSERT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(called);
  }

  {
    bool called = false;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = origin2_s1;
    context()->RegisterServiceWorker(
        GURL("https://www.example.com:8080/service_worker3.js"), key2, options,
        blink::mojom::FetchClientSettingsObject::New(),
        MakeRegisteredCallback(&called, &registration_id3),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        PolicyContainerPolicies());
    ASSERT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(called);
  }

  {
    bool called = false;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = origin3_s1;
    context()->RegisterServiceWorker(
        GURL("https://www.other.com/service_worker4.js"), key3, options,
        blink::mojom::FetchClientSettingsObject::New(),
        MakeRegisteredCallback(&called, &registration_id4),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        PolicyContainerPolicies());
    ASSERT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(called);
  }

  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id1);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id2);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id3);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration_id4);

  bool called = false;
  context()->DeleteForStorageKey(key1, MakeUnregisteredCallback(&called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  context()->registry()->FindRegistrationForId(
      registration_id1, key1,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kErrorNotFound,
                     false /* expect_waiting */, false /* expect_active */));
  context()->registry()->FindRegistrationForId(
      registration_id2, key1,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kErrorNotFound,
                     false /* expect_waiting */, false /* expect_active */));
  context()->registry()->FindRegistrationForId(
      registration_id3, key2,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kOk,
                     false /* expect_waiting */, true /* expect_active */));

  context()->registry()->FindRegistrationForId(
      registration_id4, key3,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kOk,
                     false /* expect_waiting */, true /* expect_active */));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(10u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(registration_id1, notifications_[0].registration_id);
  EXPECT_EQ(origin1_s1, notifications_[0].scope);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(registration_id1, notifications_[1].registration_id);
  EXPECT_EQ(origin1_s1, notifications_[1].scope);
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[2].type);
  EXPECT_EQ(origin1_s2, notifications_[2].scope);
  EXPECT_EQ(registration_id2, notifications_[2].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[3].type);
  EXPECT_EQ(origin1_s2, notifications_[3].scope);
  EXPECT_EQ(registration_id2, notifications_[3].registration_id);
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[4].type);
  EXPECT_EQ(origin2_s1, notifications_[4].scope);
  EXPECT_EQ(registration_id3, notifications_[4].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[5].type);
  EXPECT_EQ(origin2_s1, notifications_[5].scope);
  EXPECT_EQ(registration_id3, notifications_[5].registration_id);
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[6].type);
  EXPECT_EQ(origin3_s1, notifications_[6].scope);
  EXPECT_EQ(registration_id4, notifications_[6].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[7].type);
  EXPECT_EQ(origin3_s1, notifications_[7].scope);
  EXPECT_EQ(registration_id4, notifications_[7].registration_id);
  EXPECT_EQ(REGISTRATION_DELETED, notifications_[8].type);
  EXPECT_EQ(origin1_s1, notifications_[8].scope);
  EXPECT_EQ(registration_id1, notifications_[8].registration_id);
  EXPECT_EQ(REGISTRATION_DELETED, notifications_[9].type);
  EXPECT_EQ(origin1_s2, notifications_[9].scope);
  EXPECT_EQ(registration_id2, notifications_[9].registration_id);
}

// Make sure registering a new script shares an existing registration.
TEST_F(ServiceWorkerContextTest, RegisterNewScript) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  bool called = false;
  int64_t old_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      GURL("https://www.example.com/service_worker.js"), key, options,
      blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &old_registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            old_registration_id);

  called = false;
  int64_t new_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      GURL("https://www.example.com/service_worker_new.js"), key, options,
      blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &new_registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            new_registration_id);
  EXPECT_EQ(old_registration_id, new_registration_id);

  ASSERT_EQ(4u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(old_registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(scope, notifications_[1].scope);
  EXPECT_EQ(old_registration_id, notifications_[1].registration_id);
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[2].type);
  EXPECT_EQ(scope, notifications_[2].scope);
  EXPECT_EQ(new_registration_id, notifications_[2].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[3].type);
  EXPECT_EQ(scope, notifications_[3].scope);
  EXPECT_EQ(new_registration_id, notifications_[3].registration_id);
}

// Make sure that when registering a duplicate scope+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerContextTest, RegisterDuplicateScript) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  bool called = false;
  int64_t old_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &old_registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            old_registration_id);

  called = false;
  int64_t new_registration_id =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &new_registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  EXPECT_EQ(old_registration_id, new_registration_id);

  ASSERT_EQ(3u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(old_registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(scope, notifications_[1].scope);
  EXPECT_EQ(old_registration_id, notifications_[1].registration_id);
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[2].type);
  EXPECT_EQ(scope, notifications_[2].scope);
  EXPECT_EQ(old_registration_id, notifications_[2].registration_id);
}

TEST_F(ServiceWorkerContextTest, ContainerHostIterator) {
  const int kRenderProcessId2 = 2;
  const GURL kOrigin1 = GURL("https://www.example.com/");
  const GURL kOrigin2 = GURL("https://another-origin.example.net/");
  const blink::StorageKey kKey1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kOrigin1));
  const blink::StorageKey kKey2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kOrigin2));

  // Clients with kOrigin1, kOrigin2, and kOrigin1.
  ScopedServiceWorkerClient service_worker_client1 =
      CreateServiceWorkerClient(context(), kOrigin1);
  ScopedServiceWorkerClient service_worker_client2 =
      CreateServiceWorkerClient(context(), kOrigin2);
  ScopedServiceWorkerClient service_worker_client3 =
      CreateServiceWorkerClient(context(), kOrigin1);

  // Host4 : process_id=2, origin2, for ServiceWorker.
  blink::mojom::ServiceWorkerRegistrationOptions registration_opt;
  registration_opt.scope = GURL("https://another-origin.example.net/test/");
  const blink::StorageKey key_other = blink::StorageKey::CreateFirstParty(
      url::Origin::Create(registration_opt.scope));
  scoped_refptr<ServiceWorkerRegistration> registration =
      ServiceWorkerRegistration::Create(
          registration_opt, key_other, 1L /* registration_id */,
          helper_->context()->AsWeakPtr(),
          blink::mojom::AncestorFrameType::kNormalFrame);
  scoped_refptr<ServiceWorkerVersion> version =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(),
          GURL("https://another-origin.example.net/test/script_url"),
          blink::mojom::ScriptType::kClassic, 1L /* version_id */,
          mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
          helper_->context()->AsWeakPtr());
  // ServiceWorkerHost creates ServiceWorkerClient for a service worker
  // execution context.
  std::unique_ptr<ServiceWorkerHost> worker_host4 = CreateServiceWorkerHost(
      kRenderProcessId2, true /* is_parent_frame_secure */, *version,
      context()->AsWeakPtr());

  ASSERT_TRUE(service_worker_client1.get());
  ASSERT_TRUE(service_worker_client2.get());
  ASSERT_TRUE(service_worker_client3.get());
  ASSERT_TRUE(worker_host4->container_host());

  // Iterate over the client container hosts that belong to kOrigin1.
  std::set<ServiceWorkerClient*> results;
  for (auto it =
           context()->service_worker_client_owner().GetServiceWorkerClients(
               kKey1, true /* include_reserved_clients */,
               false /* include_back_forward_cached_clients */);
       !it.IsAtEnd(); ++it) {
    results.insert(&*it);
  }
  EXPECT_EQ(2u, results.size());
  EXPECT_TRUE(base::Contains(results, service_worker_client1.get()));
  EXPECT_TRUE(base::Contains(results, service_worker_client3.get()));

  // Iterate over the container hosts that belong to kOrigin2. This should not
  // include worker_host4->service_worker_client() as it's not for controllee.
  results.clear();
  for (auto it =
           context()->service_worker_client_owner().GetServiceWorkerClients(
               kKey2, true /* include_reserved_clients */,
               false /* include_back_forward_cached_clients */);
       !it.IsAtEnd(); ++it) {
    results.insert(&*it);
  }
  EXPECT_EQ(1u, results.size());
  EXPECT_TRUE(base::Contains(results, service_worker_client2.get()));
}

class ServiceWorkerContextRecoveryTest
    : public ServiceWorkerContextTest,
      public testing::WithParamInterface<bool /* is_storage_on_disk */> {
 public:
  ServiceWorkerContextRecoveryTest() {}
  virtual ~ServiceWorkerContextRecoveryTest() {}

 protected:
  bool is_storage_on_disk() const { return GetParam(); }
};

TEST_P(ServiceWorkerContextRecoveryTest, DeleteAndStartOver) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  if (is_storage_on_disk()) {
    // Reinitialize the helper to test on-disk storage.
    base::FilePath user_data_directory;
    ASSERT_NO_FATAL_FAILURE(GetTemporaryDirectory(&user_data_directory));
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(user_data_directory);
    helper_->context_wrapper()->AddObserver(this);
  }

  int64_t registration_id = blink::mojom::kInvalidServiceWorkerRegistrationId;
  bool called = false;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(called);

  context()->registry()->FindRegistrationForId(
      registration_id, key,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kOk,
                     false /* expect_waiting */, true /* expect_active */));
  content::RunAllTasksUntilIdle();

  // Emulate a service worker client is created before
  // `ScheduleDeleteAndStartOver()` and redirected, committed and destroyed
  // after `ScheduleDeleteAndStartOver()`.
  ScopedServiceWorkerClient service_worker_client =
      CreateServiceWorkerClient(context(), scope);
  EXPECT_EQ(service_worker_client->context().get(), context());

  context()->ScheduleDeleteAndStartOver();

  // The storage is disabled while the recovery process is running, so the
  // operation should be aborted.
  context()->registry()->FindRegistrationForId(
      registration_id, key,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kErrorAbort,
                     false /* expect_waiting */, true /* expect_active */));
  content::RunAllTasksUntilIdle();

  // The context started over and the storage was re-initialized, so the
  // registration should not be found.
  context()->registry()->FindRegistrationForId(
      registration_id, key,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kErrorNotFound,
                     false /* expect_waiting */, true /* expect_active */));
  content::RunAllTasksUntilIdle();

  {
    // Perform a cross-origin redirect for `service_worker_client`. This updates
    // the client UUID of `service_worker_client`, and should update the UUID
    // maintained by `ServiceWorkerClientOwner`, not to cause the client UUID
    // inconsistency.
    GURL cross_site_url("https://www.example.org/");
    EXPECT_FALSE(service_worker_client->context());
    service_worker_client->UpdateUrls(cross_site_url,
                                      url::Origin::Create(cross_site_url),
                                      blink::StorageKey::CreateFirstParty(
                                          url::Origin::Create(cross_site_url)));

    auto committed_service_worker_client = CommittedServiceWorkerClient(
        std::move(service_worker_client),
        GlobalRenderFrameHostId(/*child_id=*/1,
                                /*frame_routing_id=*/1));
  }
  // Destruct the service worker client via
  // `OnContainerHostReceiverDisconnected()` by destructing
  // `committed_service_worker_client`.
  // This doesn't crash if the client UUID was updated consistently above.
  content::RunAllTasksUntilIdle();

  called = false;
  context()->RegisterServiceWorker(
      script_url, key, options, blink::mojom::FetchClientSettingsObject::New(),
      MakeRegisteredCallback(&called, &registration_id),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      PolicyContainerPolicies());

  ASSERT_FALSE(called);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(called);

  context()->registry()->FindRegistrationForId(
      registration_id, key,
      base::BindOnce(&ExpectRegisteredWorkers,
                     blink::ServiceWorkerStatusCode::kOk,
                     false /* expect_waiting */, true /* expect_active */));
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(5u, notifications_.size());
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[0].type);
  EXPECT_EQ(scope, notifications_[0].scope);
  EXPECT_EQ(registration_id, notifications_[0].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[1].type);
  EXPECT_EQ(scope, notifications_[1].scope);
  EXPECT_EQ(registration_id, notifications_[1].registration_id);
  EXPECT_EQ(STORAGE_RECOVERED, notifications_[2].type);
  EXPECT_EQ(REGISTRATION_COMPLETED, notifications_[3].type);
  EXPECT_EQ(scope, notifications_[3].scope);
  EXPECT_EQ(registration_id, notifications_[3].registration_id);
  EXPECT_EQ(REGISTRATION_STORED, notifications_[4].type);
  EXPECT_EQ(scope, notifications_[4].scope);
  EXPECT_EQ(registration_id, notifications_[4].registration_id);
}

INSTANTIATE_TEST_SUITE_P(ServiceWorkerContextRecoveryTest,
                         ServiceWorkerContextRecoveryTest,
                         testing::Bool() /* is_storage_on_disk */);

}  // namespace content
