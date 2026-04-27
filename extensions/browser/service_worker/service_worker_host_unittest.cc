// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_host.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ServiceWorkerHostTest : public ExtensionsTest {
 public:
  ServiceWorkerHostTest() = default;
  ~ServiceWorkerHostTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
};

TEST_F(ServiceWorkerHostTest, DidStartServiceWorkerContext_NonExtensionScope) {
  // Setup: Add extension to process map so we don't return early.
  ExtensionId extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  ProcessMap::Get(browser_context())
      ->Insert(extension_id, render_process_host_->GetID());

  // Create ServiceWorkerHost.
  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  auto host = std::make_unique<ServiceWorkerHost>(
      render_process_host_.get(),
      remote.BindNewEndpointAndPassDedicatedReceiver());

  // Call method with invalid scope (not extension scheme).
  GURL invalid_scope("http://example.com");

  EXPECT_EQ(0, render_process_host_->bad_msg_count());
  host->DidStartServiceWorkerContext(
      extension_id, base::UnguessableToken::Create(), invalid_scope,
      /*service_worker_version_id=*/1, /*worker_thread_id=*/1,
      blink::ServiceWorkerToken());

  // Verify bad message was reported.
  EXPECT_EQ(1, render_process_host_->bad_msg_count());
}

TEST_F(ServiceWorkerHostTest, DidStartServiceWorkerContext_WrongExtensionHost) {
  ExtensionId extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  ProcessMap::Get(browser_context())
      ->Insert(extension_id, render_process_host_->GetID());

  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  auto host = std::make_unique<ServiceWorkerHost>(
      render_process_host_.get(),
      remote.BindNewEndpointAndPassDedicatedReceiver());

  // Scope is extension scheme but wrong host.
  GURL invalid_scope("chrome-extension://bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/");

  EXPECT_EQ(0, render_process_host_->bad_msg_count());
  host->DidStartServiceWorkerContext(
      extension_id, base::UnguessableToken::Create(), invalid_scope,
      /*service_worker_version_id=*/1, /*worker_thread_id=*/1,
      blink::ServiceWorkerToken());

  EXPECT_EQ(1, render_process_host_->bad_msg_count());
}

TEST_F(ServiceWorkerHostTest, DidStopServiceWorkerContext_NonExtensionScope) {
  ExtensionId extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  ProcessMap::Get(browser_context())
      ->Insert(extension_id, render_process_host_->GetID());

  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  auto host = std::make_unique<ServiceWorkerHost>(
      render_process_host_.get(),
      remote.BindNewEndpointAndPassDedicatedReceiver());

  GURL invalid_scope("http://example.com");

  EXPECT_EQ(0, render_process_host_->bad_msg_count());
  host->DidStopServiceWorkerContext(
      extension_id, base::UnguessableToken::Create(), invalid_scope,
      /*service_worker_version_id=*/1, /*worker_thread_id=*/1,
      blink::ServiceWorkerToken());

  EXPECT_EQ(1, render_process_host_->bad_msg_count());
}

TEST_F(ServiceWorkerHostTest, DidStopServiceWorkerContext_WrongExtensionHost) {
  ExtensionId extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  ProcessMap::Get(browser_context())
      ->Insert(extension_id, render_process_host_->GetID());

  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  auto host = std::make_unique<ServiceWorkerHost>(
      render_process_host_.get(),
      remote.BindNewEndpointAndPassDedicatedReceiver());

  GURL invalid_scope("chrome-extension://bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/");

  EXPECT_EQ(0, render_process_host_->bad_msg_count());
  host->DidStopServiceWorkerContext(
      extension_id, base::UnguessableToken::Create(), invalid_scope,
      /*service_worker_version_id=*/1, /*worker_thread_id=*/1,
      blink::ServiceWorkerToken());

  EXPECT_EQ(1, render_process_host_->bad_msg_count());
}

}  // namespace extensions
