// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/embedded_worker_instance_test_harness.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "test_browser_context.h"
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
  EXPECT_EQ(worker_, nullptr);
  EmbeddedWorkerTestHelper::RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(origin, worker_url);
  worker_ = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  helper_->StartWorker(worker_.get(), helper_->CreateStartParams(pair.second));
}

void EmbeddedWorkerInstanceTestHarness::StopAndResetWorker() {
  EmbeddedWorkerInstance* worker = worker_.get();
  EXPECT_NE(worker_, nullptr);
  // Stop the worker.
  worker->Stop();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The 'WorkerStopped' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  worker_.reset();
}

#if !BUILDFLAG(IS_ANDROID)
void EmbeddedWorkerInstanceTestHarness::BindHidServiceToWorker(
    const GURL& origin,
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  EXPECT_NE(worker_, nullptr);
  worker_.get()->BindHidService(url::Origin::Create(origin),
                                std::move(receiver));
}
#endif  // !BUILDFLAG(IS_ANDROID)

EmbeddedWorkerInstanceTestHarness::EmbeddedWorkerInstanceTestHarness(
    std::unique_ptr<BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {}

}  // namespace content
