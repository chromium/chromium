// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_device_delegate_observer_unittest.h"

#include <cstdint>

#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

namespace {

using base::test::TestFuture;
using RegistrationAndVersionPair =
    EmbeddedWorkerTestHelper::RegistrationAndVersionPair;

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

}  // namespace content
