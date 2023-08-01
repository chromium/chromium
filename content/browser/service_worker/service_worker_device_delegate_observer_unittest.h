// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_UNITTEST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_UNITTEST_H_

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/test/browser_task_environment.h"

namespace content {

class ServiceWorkerDeviceDelegateObserverTest : public testing::Test {
 public:
  ServiceWorkerDeviceDelegateObserverTest();
  ServiceWorkerDeviceDelegateObserverTest(
      ServiceWorkerDeviceDelegateObserverTest&) = delete;
  ServiceWorkerDeviceDelegateObserverTest& operator=(
      ServiceWorkerDeviceDelegateObserverTest&) = delete;
  ~ServiceWorkerDeviceDelegateObserverTest() override;

  void SetUp() override;
  void TearDown() override;

  scoped_refptr<ServiceWorkerRegistration> InstallServiceWorker(
      const GURL& origin);

  // This will be called within `InstallServiceWorker(...)` when the
  // ServiceWorkerVersion is in the state of INSTALLING.
  virtual void ServiceWorkerInstalling(
      scoped_refptr<ServiceWorkerVersion> version);

  EmbeddedWorkerTestHelper* helper() { return helper_.get(); }
  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }

 private:
  void InitializeTestHelper();
  void StoreRegistration(
      const EmbeddedWorkerTestHelper::RegistrationAndVersionPair& pair);

  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir user_data_directory_;
  base::FilePath user_data_directory_path_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DEVICE_DELEGATE_OBSERVER_UNITTEST_H_
