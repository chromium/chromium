// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_device_delegate_observer_unittest.h"

#include <cstdint>

#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "components/services/storage/public/mojom/service_worker_database.mojom-shared.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_device_delegate_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

using base::test::TestFuture;
using testing::Pair;
using testing::UnorderedElementsAre;
using RegistrationAndVersionPair =
    EmbeddedWorkerTestHelper::RegistrationAndVersionPair;

class MockServiceWorkerDeviceDelegateObserver
    : public ServiceWorkerDeviceDelegateObserver {
 public:
  explicit MockServiceWorkerDeviceDelegateObserver(
      ServiceWorkerContextCore* context)
      : ServiceWorkerDeviceDelegateObserver(context) {}
  MockServiceWorkerDeviceDelegateObserver(
      const MockServiceWorkerDeviceDelegateObserver&) = delete;
  MockServiceWorkerDeviceDelegateObserver& operator=(
      const MockServiceWorkerDeviceDelegateObserver&) = delete;
  ~MockServiceWorkerDeviceDelegateObserver() override = default;

  MOCK_METHOD(void, RegistrationAdded, (int64_t), (override));
  MOCK_METHOD(void, RegistrationRemoved, (int64_t), (override));
};

void RegisterServiceWorkerDeviceDelegateObserver(
    MockServiceWorkerDeviceDelegateObserver& mock,
    int64_t registration_id,
    bool has_event_handlers,
    const blink::StorageKey& key) {
  EXPECT_CALL(mock, RegistrationAdded(registration_id));
  mock.Register(registration_id);
  mock.UpdateHasEventHandlers(registration_id, has_event_handlers);
  EXPECT_THAT(mock.registration_id_map(),
              UnorderedElementsAre(
                  Pair(registration_id,
                       ServiceWorkerDeviceDelegateObserver::RegistrationInfo(
                           key, has_event_handlers))));
}

}  // namespace

ServiceWorkerDeviceDelegateObserverTest::
    ServiceWorkerDeviceDelegateObserverTest()
    : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

ServiceWorkerDeviceDelegateObserverTest::
    ~ServiceWorkerDeviceDelegateObserverTest() = default;

void ServiceWorkerDeviceDelegateObserverTest::SetUp() {
  CHECK(user_data_directory_.CreateUniqueTempDir());
  user_data_directory_path_ = user_data_directory_.GetPath();
  InitializeTestHelper();
}

void ServiceWorkerDeviceDelegateObserverTest::TearDown() {
  helper_.reset();
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerDeviceDelegateObserverTest::InstallServiceWorker(
    const GURL& origin) {
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto worker_url =
      GURL(base::StringPrintf("%s/worker.js", origin.spec().c_str()));

  auto [registration, version] =
      helper()->PrepareRegistrationAndVersion(origin, worker_url);

  version->SetStatus(ServiceWorkerVersion::Status::INSTALLING);
  registration->SetInstallingVersion(version);
  ServiceWorkerInstalling(version);

  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(WriteToDiskCacheWithIdSync(
      helper()->context()->GetStorageControl(), version->script_url(), 10,
      /*headers=*/{}, "I'm a body", "I'm a meta data"));
  version->script_cache_map()->SetResources(records);
  version->SetMainScriptResponse(
      EmbeddedWorkerTestHelper::CreateMainScriptResponse());
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);

  version->SetStatus(ServiceWorkerVersion::Status::ACTIVATED);
  registration->SetActiveVersion(version);

  // Store the registration so that it is findable via storage functions.
  StoreRegistration({registration, version});

  return registration;
}

void ServiceWorkerDeviceDelegateObserverTest::ServiceWorkerInstalling(
    scoped_refptr<ServiceWorkerVersion> version) {}

void ServiceWorkerDeviceDelegateObserverTest::InitializeTestHelper() {
  helper_ =
      std::make_unique<EmbeddedWorkerTestHelper>(user_data_directory_path_);
}

void ServiceWorkerDeviceDelegateObserverTest::StoreRegistration(
    const RegistrationAndVersionPair& pair) {
  TestFuture<blink::ServiceWorkerStatusCode> status;
  registry()->StoreRegistration(pair.first.get(), pair.second.get(),
                                status.GetCallback());
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.Get());
}

TEST_F(ServiceWorkerDeviceDelegateObserverTest,
       DispatchEventNoLiveRegistration) {
  const GURL origin("https://wwww.example.com");
  auto key = blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();

  MockServiceWorkerDeviceDelegateObserver mock(context());
  RegisterServiceWorkerDeviceDelegateObserver(mock, registration_id,
                                              /*has_event_handlers=*/true, key);

  // Destroy the registration so there is no live registration.
  registration.reset();
  EXPECT_FALSE(context()->GetLiveRegistration(registration_id));
  TestFuture<scoped_refptr<ServiceWorkerVersion>,
             blink::ServiceWorkerStatusCode>
      future;
  mock.DispatchEventToWorker(registration_id, future.GetCallback());
  EXPECT_EQ(future.Get<0>()->running_status(),
            blink::EmbeddedWorkerStatus::kRunning);
  EXPECT_EQ(future.Get<1>(), blink::ServiceWorkerStatusCode::kOk);
}

TEST_F(ServiceWorkerDeviceDelegateObserverTest, DispatchEventLiveRegistration) {
  const GURL origin("https://wwww.example.com");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));
  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();

  MockServiceWorkerDeviceDelegateObserver mock(context());
  RegisterServiceWorkerDeviceDelegateObserver(mock, registration_id,
                                              /*has_event_handlers=*/true, key);

  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));
  TestFuture<scoped_refptr<ServiceWorkerVersion>,
             blink::ServiceWorkerStatusCode>
      future;
  mock.DispatchEventToWorker(registration_id, future.GetCallback());
  EXPECT_EQ(future.Get<0>()->running_status(),
            blink::EmbeddedWorkerStatus::kRunning);
  EXPECT_EQ(future.Get<1>(), blink::ServiceWorkerStatusCode::kOk);
}

TEST_F(ServiceWorkerDeviceDelegateObserverTest,
       DispatchEventCannotFindRegistration) {
  const GURL origin("https://wwww.example.com");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));

  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();

  MockServiceWorkerDeviceDelegateObserver mock(context());
  RegisterServiceWorkerDeviceDelegateObserver(mock, registration_id,
                                              /*has_event_handlers=*/true, key);

  registration.reset();
  EXPECT_FALSE(context()->GetLiveRegistration(registration_id));
  base::test::TestFuture<storage::mojom::ServiceWorkerDatabaseStatus>
      delete_future;
  registry()->GetRemoteStorageControl()->Delete(delete_future.GetCallback());
  EXPECT_EQ(delete_future.Take(),
            storage::mojom::ServiceWorkerDatabaseStatus::kOk);

  TestFuture<scoped_refptr<ServiceWorkerVersion>,
             blink::ServiceWorkerStatusCode>
      dispatch_event_future;
  mock.DispatchEventToWorker(registration_id,
                             dispatch_event_future.GetCallback());
  EXPECT_EQ(dispatch_event_future.Get<0>(), nullptr);
  EXPECT_EQ(dispatch_event_future.Get<1>(),
            blink::ServiceWorkerStatusCode::kErrorAbort);
}

TEST_F(ServiceWorkerDeviceDelegateObserverTest, DispatchEventStartWorkerFail) {
  // Simulate failing to start the service worker by using http scheme, this
  // would result in an error code of kErrorDisallowed.
  const GURL origin("http://wwww.example.com");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));

  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();

  MockServiceWorkerDeviceDelegateObserver mock(context());
  RegisterServiceWorkerDeviceDelegateObserver(mock, registration_id,
                                              /*has_event_handlers=*/true, key);

  EXPECT_TRUE(context()->GetLiveRegistration(registration_id));
  TestFuture<scoped_refptr<ServiceWorkerVersion>,
             blink::ServiceWorkerStatusCode>
      future;
  mock.DispatchEventToWorker(registration_id, future.GetCallback());
  EXPECT_EQ(future.Get<0>()->running_status(),
            blink::EmbeddedWorkerStatus::kStopped);
  EXPECT_EQ(future.Get<1>(), blink::ServiceWorkerStatusCode::kErrorDisallowed);
}

TEST_F(ServiceWorkerDeviceDelegateObserverTest, UpdateHasEventHandlers) {
  const GURL origin("http://wwww.example.com");
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin));

  auto registration = InstallServiceWorker(origin);
  auto registration_id = registration->id();

  MockServiceWorkerDeviceDelegateObserver mock(context());
  // No event handlers doesn't register `registration_id`.
  mock.UpdateHasEventHandlers(registration_id, /*has_event_handlers=*/false);
  EXPECT_TRUE(mock.registration_id_map().empty());

  // Register `registration_id` if it has event handlers.
  EXPECT_CALL(mock, RegistrationAdded(registration_id));
  mock.UpdateHasEventHandlers(registration_id, /*has_event_handlers=*/true);
  EXPECT_THAT(
      mock.registration_id_map(),
      UnorderedElementsAre(Pair(
          registration_id,
          ServiceWorkerDeviceDelegateObserver::RegistrationInfo(key, true))));
}

}  // namespace content
