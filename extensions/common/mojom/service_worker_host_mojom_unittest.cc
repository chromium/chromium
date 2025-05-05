// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Test implementation of mojom::EventDispatcher interface.
class TestEventDispatcherImpl : public mojom::EventDispatcher {
 public:
  TestEventDispatcherImpl() = default;
  TestEventDispatcherImpl(const TestEventDispatcherImpl&) = delete;
  TestEventDispatcherImpl& operator=(const TestEventDispatcherImpl&) = delete;

  // mojom::EventDispatcher overrides:
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     base::Value::List event_args,
                     DispatchEventCallback callback) override {}

  mojo::AssociatedReceiver<mojom::EventDispatcher>& receiver() {
    return receiver_;
  }

 private:
  mojo::AssociatedReceiver<mojom::EventDispatcher> receiver_{this};
};

// Test implementation of mojom::ServiceWorkerHost interface.
class TestServiceWorkerHostImpl : public mojom::ServiceWorkerHost {
 public:
  explicit TestServiceWorkerHostImpl(
      mojo::PendingReceiver<mojom::ServiceWorkerHost> receiver)
      : receiver_(this, std::move(receiver)) {}
  TestServiceWorkerHostImpl(const TestServiceWorkerHostImpl&) = delete;
  TestServiceWorkerHostImpl& operator=(const TestServiceWorkerHostImpl&) =
      delete;

  // mojom::ServiceWorkerHost:
  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id,
      int64_t service_worker_version_id,
      int worker_thread_id,
      const blink::ServiceWorkerToken& service_worker_token,
      mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher)
      override {}
  void DidStartServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) override {}
  void DidStopServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) override {}
  void RequestWorker(mojom::RequestParamsPtr params,
                     RequestWorkerCallback callback) override {}
  void WorkerResponseAck(const base::Uuid& request_uuid) override {}
  void OpenChannelToExtension(
      mojom::ExternalConnectionInfoPtr info,
      mojom::ChannelType channel_type,
      const std::string& channel_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host)
      override {}
  void OpenChannelToNativeApp(
      const std::string& native_app_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> port_host)
      override {}
  void OpenChannelToTab(int32_t tab_id,
                        int32_t frame_id,
                        const std::optional<std::string>& document_id,
                        mojom::ChannelType channel_type,
                        const std::string& channel_name,
                        const PortId& port_id,
                        mojo::PendingAssociatedRemote<mojom::MessagePort> port,
                        mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
                            port_host) override {}

 private:
  mojo::Receiver<mojom::ServiceWorkerHost> receiver_;
};

class ServiceWorkerHostMojomExtensionIdTest : public testing::Test {
 public:
  ServiceWorkerHostMojomExtensionIdTest() {
    service_worker_host_impl_ = std::make_unique<TestServiceWorkerHostImpl>(
        service_worker_host_remote_.BindNewPipeAndPassReceiver());
    event_dispatcher_impl_ = std::make_unique<TestEventDispatcherImpl>();
  }

  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id,
      int64_t service_worker_version_id,
      int worker_thread_id,
      const blink::ServiceWorkerToken& service_worker_token,
      mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher) {
    service_worker_host_remote_->DidInitializeServiceWorkerContext(
        extension_id, service_worker_version_id, worker_thread_id,
        service_worker_token, std::move(event_dispatcher));
    service_worker_host_remote_.FlushForTesting();
  }

  void DidStartServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) {
    service_worker_host_remote_->DidStartServiceWorkerContext(
        extension_id, activation_token, service_worker_scope,
        service_worker_version_id, worker_thread_id);
    service_worker_host_remote_.FlushForTesting();
  }

  void DidStopServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) {
    service_worker_host_remote_->DidStopServiceWorkerContext(
        extension_id, activation_token, service_worker_scope,
        service_worker_version_id, worker_thread_id);
    service_worker_host_remote_.FlushForTesting();
  }

  // TODO(crbug.com/404568026): Once RequestParams is converted (in
  // crbug.com/404555668), then test RequestWorker() as well.

  bool PipeConnected() { return service_worker_host_remote_.is_connected(); }

  void RebindServiceWorkerHost() {
    service_worker_host_impl_.reset();
    service_worker_host_remote_.reset();
    service_worker_host_impl_ = std::make_unique<TestServiceWorkerHostImpl>(
        service_worker_host_remote_.BindNewPipeAndPassReceiver());
  }

  TestEventDispatcherImpl* test_event_dispatcher_impl() {
    return event_dispatcher_impl_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::Remote<mojom::ServiceWorkerHost> service_worker_host_remote_;
  std::unique_ptr<TestServiceWorkerHostImpl> service_worker_host_impl_;
  std::unique_ptr<TestEventDispatcherImpl> event_dispatcher_impl_;
};

// Tests that sending valid extension IDs to mojom::ServiceWorkerHost
// implementations pass message validation and keep the mojom pipe connected.
TEST_F(ServiceWorkerHostMojomExtensionIdTest, ValidExtensionId) {
  // Create a valid ExtensionId.
  ExtensionId valid_extension_id(32, 'a');

  DidInitializeServiceWorkerContext(
      valid_extension_id, /*service_worker_version_id=*/0,
      /*worker_thread_id=*/0,
      /*service_worker_token=*/blink::ServiceWorkerToken(),
      test_event_dispatcher_impl()->receiver().BindNewEndpointAndPassRemote());
  EXPECT_TRUE(PipeConnected());

  ASSERT_TRUE(PipeConnected());
  DidStartServiceWorkerContext(
      valid_extension_id,
      /*activation_token=*/base::UnguessableToken::Create(),
      /*service_worker_scope=*/GURL("test_scope"),
      /*service_worker_version_id=*/0,
      /*worker_thread_id=*/0);
  EXPECT_TRUE(PipeConnected());

  ASSERT_TRUE(PipeConnected());
  DidStopServiceWorkerContext(
      valid_extension_id,
      /*activation_token=*/base::UnguessableToken::Create(),
      /*service_worker_scope=*/GURL("test_scope"),
      /*service_worker_version_id=*/0,
      /*worker_thread_id=*/0);
  EXPECT_TRUE(PipeConnected());
}

// Tests that sending invalid extension IDs to mojom::ServiceWorkerHost
// implementations fail message validation and disconnect the mojom pipe.
TEST_F(ServiceWorkerHostMojomExtensionIdTest, InvalidExtensionId) {
  // Create an invalid ExtensionId.
  ExtensionId invalid_extension_id = "invalid_id";

  DidInitializeServiceWorkerContext(
      invalid_extension_id, /*service_worker_version_id=*/0,
      /*worker_thread_id=*/0,
      /*service_worker_token=*/blink::ServiceWorkerToken(),
      test_event_dispatcher_impl()->receiver().BindNewEndpointAndPassRemote());
  EXPECT_FALSE(PipeConnected());
  RebindServiceWorkerHost();

  ASSERT_TRUE(PipeConnected());
  DidStartServiceWorkerContext(
      invalid_extension_id,
      /*activation_token=*/base::UnguessableToken::Create(),
      /*service_worker_scope=*/GURL("test_scope"),
      /*service_worker_version_id=*/0,
      /*worker_thread_id=*/0);
  EXPECT_FALSE(PipeConnected());
  RebindServiceWorkerHost();

  ASSERT_TRUE(PipeConnected());
  DidStopServiceWorkerContext(
      invalid_extension_id,
      /*activation_token=*/base::UnguessableToken::Create(),
      /*service_worker_scope=*/GURL("test_scope"),
      /*service_worker_version_id=*/0,
      /*worker_thread_id=*/0);
  EXPECT_FALSE(PipeConnected());
}

}  // namespace
}  // namespace extensions
