// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/embedded_worker_instance_test_harness.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_hid_delegate_observer.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_usb_delegate_observer.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

EmbeddedWorkerInstanceTestHarness::~EmbeddedWorkerInstanceTestHarness() =
    default;

void EmbeddedWorkerInstanceTestHarness::SetUp() {
  helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath(),
                                                       CreateBrowserContext());
}

void EmbeddedWorkerInstanceTestHarness::TearDown() {
  helper_.reset();
}

std::unique_ptr<BrowserContext>
EmbeddedWorkerInstanceTestHarness::CreateBrowserContext() {
  return std::make_unique<TestBrowserContext>();
}

void EmbeddedWorkerInstanceTestHarness::CreateAndStartWorker(
    const GURL& origin,
    const GURL& worker_url) {
  EXPECT_EQ(worker_version_, nullptr);
  EmbeddedWorkerTestHelper::RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(origin, worker_url);

  worker_version_ = pair.second;
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(WriteToDiskCacheWithIdSync(
      helper_->context()->GetStorageControl(), worker_version_->script_url(),
      10, {} /* headers */, "I'm a body", "I'm a meta data"));
  worker_version_->script_cache_map()->SetResources(records);
  worker_version_->SetMainScriptResponse(
      EmbeddedWorkerTestHelper::CreateMainScriptResponse());
  worker_version_->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);

  content::HidDelegate* hid_delegate =
      content::GetContentClientForTesting()->browser()->GetHidDelegate();
  worker_version_->set_has_hid_event_handlers(
      hid_delegate && hid_delegate->IsServiceWorkerAllowedForOrigin(
                          url::Origin::Create(origin)));

  content::UsbDelegate* usb_delegate =
      content::GetContentClientForTesting()->browser()->GetUsbDelegate();
  worker_version_->set_has_usb_event_handlers(
      usb_delegate && usb_delegate->IsServiceWorkerAllowedForOrigin(
                          url::Origin::Create(origin)));

  worker_version_->SetStatus(ServiceWorkerVersion::Status::ACTIVATED);
  pair.first->SetActiveVersion(worker_version_);

  // Make the registration findable via storage functions.
  base::test::TestFuture<blink::ServiceWorkerStatusCode> status;
  helper_->context()->registry()->StoreRegistration(
      pair.first.get(), pair.second.get(), status.GetCallback());
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.Get());

  StartServiceWorker(worker_version_.get());
  ASSERT_EQ(worker_version_->embedded_worker()->status(),
            blink::EmbeddedWorkerStatus::kRunning);
}

void EmbeddedWorkerInstanceTestHarness::StopAndResetWorker() {
  EXPECT_NE(worker_version_, nullptr);
  StopServiceWorker(worker_version_.get());
  ASSERT_EQ(worker_version_->embedded_worker()->status(),
            blink::EmbeddedWorkerStatus::kStopped);
  worker_version_.reset();
}

#if !BUILDFLAG(IS_ANDROID)
void EmbeddedWorkerInstanceTestHarness::BindHidServiceToWorker(
    const GURL& origin,
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  EmbeddedWorkerInstance* worker = worker_version_->embedded_worker();
  EXPECT_NE(worker, nullptr);
  worker->BindHidService(url::Origin::Create(origin), std::move(receiver));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void EmbeddedWorkerInstanceTestHarness::BindUsbServiceToWorker(
    const GURL& origin,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  EmbeddedWorkerInstance* worker = worker_version_->embedded_worker();
  EXPECT_NE(worker, nullptr);
  worker->BindUsbService(url::Origin::Create(origin), std::move(receiver));
}

EmbeddedWorkerInstanceTestHarness::EmbeddedWorkerInstanceTestHarness(
    std::unique_ptr<BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {}

}  // namespace content
