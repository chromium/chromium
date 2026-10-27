// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/service_worker/service_worker_host.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/process/kill.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/event_args.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

namespace {

class TestEventDispatcher : public mojom::EventDispatcher {
 public:
  TestEventDispatcher() = default;
  ~TestEventDispatcher() override = default;

  // mojom::EventDispatcher:
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     const scoped_refptr<const EventArgs>& event_args,
                     DispatchEventCallback callback) override {}
};

}  // namespace

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

TEST_F(ServiceWorkerHostTest, BindReceiverIgnoresDeadRenderProcessHost) {
  render_process_host_->Init();
  render_process_host_->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, /*exit_code=*/0);
  ASSERT_FALSE(render_process_host_->IsInitializedAndNotDead());

  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  ServiceWorkerHost::BindReceiver(
      render_process_host_->GetID(),
      remote.BindNewEndpointAndPassDedicatedReceiver());

  EXPECT_TRUE(
      ServiceWorkerHost::GetServiceWorkerHostList(render_process_host_.get())
          .empty());
}

TEST_F(ServiceWorkerHostTest,
       DidInitializeServiceWorkerContextIgnoresDeadRenderProcessHost) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test")
          .SetBackgroundContext(
              ExtensionBuilder::BackgroundContext::SERVICE_WORKER)
          .Build();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  ProcessMap::Get(browser_context())
      ->Insert(extension->id(), render_process_host_->GetID());

  render_process_host_->Init();
  render_process_host_->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, /*exit_code=*/0);
  ASSERT_FALSE(render_process_host_->IsInitializedAndNotDead());

  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  auto host = std::make_unique<ServiceWorkerHost>(
      render_process_host_.get(),
      remote.BindNewEndpointAndPassDedicatedReceiver());

  TestEventDispatcher test_dispatcher;
  mojo::AssociatedReceiver<mojom::EventDispatcher> dispatcher_receiver(
      &test_dispatcher);
  mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher =
      dispatcher_receiver.BindNewEndpointAndPassDedicatedRemote();
  base::RunLoop disconnect_loop;
  dispatcher_receiver.set_disconnect_handler(disconnect_loop.QuitClosure());

  const blink::ServiceWorkerToken token;
  host->DidInitializeServiceWorkerContext(
      extension->id(), base::UnguessableToken::Create(),
      /*service_worker_version_id=*/1, /*worker_thread_id=*/1, token,
      std::move(event_dispatcher));

  // Early return drops `event_dispatcher` without binding it in EventRouter or
  // tracking the worker in ProcessManager.
  disconnect_loop.Run();
  const WorkerId worker_id{extension->id(), render_process_host_->GetID(),
                           /*version_id=*/1, /*thread_id=*/1, token};
  EXPECT_FALSE(
      ProcessManager::Get(browser_context())->HasServiceWorker(worker_id));
}

TEST_F(ServiceWorkerHostTest, DestroyedOnRenderProcessHostDestroyed) {
  render_process_host_->Init();
  ASSERT_TRUE(render_process_host_->IsInitializedAndNotDead());

  mojo::AssociatedRemote<mojom::ServiceWorkerHost> remote;
  ServiceWorkerHost::BindReceiver(
      render_process_host_->GetID(),
      remote.BindNewEndpointAndPassDedicatedReceiver());
  EXPECT_EQ(1u, ServiceWorkerHost::GetServiceWorkerHostList(
                    render_process_host_.get())
                    .size());

  base::RunLoop disconnect_loop;
  remote.set_disconnect_handler(disconnect_loop.QuitClosure());

  // Destroying MockRenderProcessHost directly fires
  // RenderProcessHostDestroyed() without RenderProcessExited().
  render_process_host_.reset();
  disconnect_loop.Run();
  EXPECT_FALSE(remote.is_connected());
}

}  // namespace extensions
