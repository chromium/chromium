// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_router.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Test implementation of mojom::EventRouter.
class TestEventRouterImpl : public mojom::EventRouter {
 public:
  explicit TestEventRouterImpl(
      mojo::PendingReceiver<mojom::EventRouter> receiver)
      : receiver_(this, std::move(receiver)) {}
  TestEventRouterImpl(const TestEventRouterImpl&) = delete;
  TestEventRouterImpl& operator=(const TestEventRouterImpl&) = delete;

  // mojom::EventRouter:
  void AddListenerForMainThread(
      mojom::EventListenerPtr event_listener) override {}
  void AddListenerForServiceWorker(
      mojom::EventListenerPtr event_listener) override {}
  void AddLazyListenerForMainThread(const ExtensionId& extension_id,
                                    const std::string& name) override {}
  void AddLazyListenerForServiceWorker(const ExtensionId& extension_id,
                                       const GURL& worker_scope_url,
                                       const std::string& name) override {}
  void AddFilteredListenerForMainThread(
      mojom::EventListenerOwnerPtr listener_owner,
      const std::string& name,
      base::Value::Dict filter,
      bool add_lazy_listener) override {}
  void AddFilteredListenerForServiceWorker(
      const ExtensionId& extension_id,
      const std::string& name,
      mojom::ServiceWorkerContextPtr service_worker_context,
      base::Value::Dict filter,
      bool add_lazy_listener) override {}
  void RemoveListenerForMainThread(
      mojom::EventListenerPtr event_listener) override {}
  void RemoveListenerForServiceWorker(
      mojom::EventListenerPtr event_listener) override {}
  void RemoveLazyListenerForMainThread(const ExtensionId& extension_id,
                                       const std::string& name) override {}
  void RemoveLazyListenerForServiceWorker(const ExtensionId& extension_id,
                                          const GURL& worker_scope_url,
                                          const std::string& name) override {}
  void RemoveFilteredListenerForMainThread(
      mojom::EventListenerOwnerPtr listener_owner,
      const std::string& name,
      base::Value::Dict filter,
      bool remove_lazy_listener) override {}
  void RemoveFilteredListenerForServiceWorker(
      const ExtensionId& extension_id,
      const std::string& name,
      mojom::ServiceWorkerContextPtr service_worker_context,
      base::Value::Dict filter,
      bool remove_lazy_listener) override {}

 private:
  mojo::Receiver<mojom::EventRouter> receiver_;
};

class EventRouterMojomExtensionIdTest : public testing::Test {
 public:
  EventRouterMojomExtensionIdTest() {
    event_router_impl_ = std::make_unique<TestEventRouterImpl>(
        event_router_remote_.BindNewPipeAndPassReceiver());
  }

  void AddListenerForMainThread(const ExtensionId& extension_id) {
    auto event_listener = CreateEventListener(extension_id);
    event_router_remote_->AddListenerForMainThread(std::move(event_listener));
    event_router_remote_.FlushForTesting();
  }

  void AddListenerForServiceWorker(const ExtensionId& extension_id) {
    auto event_listener = CreateEventListener(extension_id);
    event_router_remote_->AddListenerForServiceWorker(
        std::move(event_listener));
    event_router_remote_.FlushForTesting();
  }

  void AddLazyListenerForMainThread(const ExtensionId& extension_id) {
    event_router_remote_->AddLazyListenerForMainThread(extension_id,
                                                       "test_listener_name");
    event_router_remote_.FlushForTesting();
  }

  void AddLazyListenerForServiceWorker(const ExtensionId& extension_id) {
    event_router_remote_->AddLazyListenerForServiceWorker(
        extension_id, GURL("test_worker_scope"), "test_event_name");
    event_router_remote_.FlushForTesting();
  }

  void AddFilteredListenerForMainThread(const ExtensionId& extension_id) {
    auto event_listener = CreateEventListener(extension_id);
    event_router_remote_->AddFilteredListenerForMainThread(
        std::move(event_listener->listener_owner), "test_event_name",
        /*filter=*/base::Value::Dict(), /*add_lazy_listener=*/true);
    event_router_remote_.FlushForTesting();
  }

  void RemoveListenerForMainThread(const ExtensionId& extension_id) {
    auto event_listener = CreateEventListener(extension_id);
    event_router_remote_->RemoveListenerForMainThread(
        std::move(event_listener));
    event_router_remote_.FlushForTesting();
  }

  void RemoveListenerForServiceWorker(const ExtensionId& extension_id) {
    auto event_listener = CreateEventListener(extension_id);
    event_router_remote_->RemoveListenerForServiceWorker(
        std::move(event_listener));
    event_router_remote_.FlushForTesting();
  }

  void RemoveLazyListenerForMainThread(const ExtensionId& extension_id) {
    event_router_remote_->RemoveLazyListenerForMainThread(extension_id,
                                                          "test_listener_name");
    event_router_remote_.FlushForTesting();
  }

  void RemoveLazyListenerForServiceWorker(const ExtensionId& extension_id) {
    event_router_remote_->RemoveLazyListenerForServiceWorker(
        extension_id, GURL("test_worker_scope"), "test_event_name");
    event_router_remote_.FlushForTesting();
  }

  void RemoveFilteredListenerForMainThread(const ExtensionId& extension_id) {
    auto event_listener = CreateEventListener(extension_id);
    event_router_remote_->RemoveFilteredListenerForMainThread(
        std::move(event_listener->listener_owner), "test_event_name",
        /*filter=*/base::Value::Dict(), /*remove_lazy_listener=*/true);
    event_router_remote_.FlushForTesting();
  }

  bool PipeConnected() { return event_router_remote_.is_connected(); }

  void RebindReceiver() {
    event_router_impl_.reset();
    event_router_remote_.reset();
    event_router_impl_ = std::make_unique<TestEventRouterImpl>(
        event_router_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  mojom::EventListenerPtr CreateEventListener(const ExtensionId& extension_id) {
    return mojom::EventListenerPtr(mojom::EventListener::New(
        mojom::EventListenerOwner::NewExtensionId(extension_id),
        "test_event_name",
        mojom::ServiceWorkerContext::New(GURL("test_worker_scope"),
                                         /*version_id=*/0, /*thread_id=*/0),
        /*event_filter=*/std::nullopt));
  }

  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::EventRouter> event_router_remote_;
  std::unique_ptr<TestEventRouterImpl> event_router_impl_;
};

// Tests that passing valid extension IDs to mojom::EventRouter implementations
// pass message validation and keep the mojom pipe connected.
TEST_F(EventRouterMojomExtensionIdTest, ValidExtensionId) {
  // Create a valid ExtensionId.
  ExtensionId valid_extension_id(32, 'a');

  AddListenerForMainThread(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  AddListenerForServiceWorker(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  AddLazyListenerForMainThread(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  AddLazyListenerForServiceWorker(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  AddFilteredListenerForMainThread(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  RemoveListenerForMainThread(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  RemoveListenerForServiceWorker(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  RemoveLazyListenerForMainThread(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  RemoveLazyListenerForServiceWorker(valid_extension_id);
  ASSERT_TRUE(PipeConnected());

  RemoveFilteredListenerForMainThread(valid_extension_id);
  ASSERT_TRUE(PipeConnected());
}

// Tests that passing invalid extension IDs to mojom::EventRouter
// implementations fail message validation and close the mojom pipe.
TEST_F(EventRouterMojomExtensionIdTest, InvalidExtensionId) {
  // Create an invalid ExtensionId.
  ExtensionId invalid_extension_id = "invalid_id";

  AddListenerForMainThread(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddListenerForServiceWorker(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddLazyListenerForMainThread(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddLazyListenerForServiceWorker(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  AddFilteredListenerForMainThread(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  RemoveListenerForMainThread(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  RemoveListenerForServiceWorker(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  RemoveLazyListenerForMainThread(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  RemoveLazyListenerForServiceWorker(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());

  RebindReceiver();

  RemoveFilteredListenerForMainThread(invalid_extension_id);
  ASSERT_FALSE(PipeConnected());
}

}  // namespace
}  // namespace extensions
