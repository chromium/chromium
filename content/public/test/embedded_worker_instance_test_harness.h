// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_EMBEDDED_WORKER_INSTANCE_TEST_HARNESS_H_
#define CONTENT_PUBLIC_TEST_EMBEDDED_WORKER_INSTANCE_TEST_HARNESS_H_

#include <stdint.h>

#include "browser_task_environment.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"

namespace content {

class EmbeddedWorkerInstance;
class EmbeddedWorkerTestHelper;
class ServiceWorkerVersion;

// EmbeddedWorkerInstanceTestHarness provides helper functions to set up a test
// environment with an EmbeddedWorkerInstance, and allow a test to test the
// worker with bound services.
//
// Example:
//
//  class MyWorkerTest : public EmbeddedWorkerInstanceTestHarness {
//    std::unique_ptr<BrowserContext> CreateBrowserContext() override {
//      // Create the BrowserContext and store the raw pointer
//      // to the created BrowserContext.
//      ...
//    }
//  };
//
//  CreateAndStartWorker(); //  Create and start a worker!
//
//  mojo::Remote<Xyz> xyz_service;
//  // Note a new binding function might need to be added to this harness file
//  // if that is not available.
//  BindXyzServiceToWorker(origin, xyz_service.BindNewPipeAndPassReceiver()));
//  xyz_service->DoSomething();
//
//  StopAndResetWorker();  // Stop and delete the worker!

class EmbeddedWorkerInstanceTestHarness : public testing::Test {
 public:
  // Constructs a EmbeddedWorkerInstanceTestHarness which uses |traits| to
  // initialize its BrowserTaskEnvironment.
  template <typename... TaskEnvironmentTraits>
  explicit EmbeddedWorkerInstanceTestHarness(TaskEnvironmentTraits&&... traits)
      : EmbeddedWorkerInstanceTestHarness(
            std::make_unique<BrowserTaskEnvironment>(
                std::forward<TaskEnvironmentTraits>(traits)...)) {}

  EmbeddedWorkerInstanceTestHarness(const EmbeddedWorkerInstanceTestHarness&) =
      delete;
  EmbeddedWorkerInstanceTestHarness& operator=(
      const EmbeddedWorkerInstanceTestHarness&) = delete;

  ~EmbeddedWorkerInstanceTestHarness() override;

  void SetUp() override;

  void TearDown() override;

  virtual std::unique_ptr<BrowserContext> CreateBrowserContext();

  void CreateAndStartWorker(const GURL& origin, const GURL& worker_url);

  void StopAndResetWorker();

#if !BUILDFLAG(IS_ANDROID)
  void BindHidServiceToWorker(
      const GURL& origin,
      mojo::PendingReceiver<blink::mojom::HidService> receiver);
#endif

  void BindUsbServiceToWorker(
      const GURL& origin,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

 protected:
  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line.
  explicit EmbeddedWorkerInstanceTestHarness(
      std::unique_ptr<BrowserTaskEnvironment> task_environment);
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

 private:
  std::unique_ptr<BrowserTaskEnvironment> task_environment_;
  scoped_refptr<content::ServiceWorkerVersion> worker_version_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_EMBEDDED_WORKER_INSTANCE_TEST_HARNESS_H_
