// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_SERVICE_TEST_SETUP_H_
#define SERVICES_WS_WINDOW_SERVICE_TEST_SETUP_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "services/ws/test_window_service_delegate.h"
#include "services/ws/test_window_tree_client.h"
#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/wm/core/focus_controller.h"

namespace wm {
class ScopedCaptureClient;
}

namespace ws {

class HostEventQueue;
class TestHostEventDispatcher;
class WindowService;
class WindowTree;
class WindowTreeTestHelper;

struct EmbeddingHelper;

// Helper to setup state needed for WindowService tests.
class WindowServiceTestSetup {
 public:
  WindowServiceTestSetup();
  ~WindowServiceTestSetup();

  // |flags| mirrors that from mojom::WindowTree::Embed(), see it for
  // details.
  std::unique_ptr<EmbeddingHelper> CreateEmbedding(aura::Window* embed_root,
                                                   uint32_t flags = 0);

  void set_ack_events_immediately(bool value) {
    ack_events_immediately_ = value;
  }
  bool ack_events_immediately() const { return ack_events_immediately_; }

  aura::Window* root() { return aura_test_helper_.root_window(); }
  TestWindowServiceDelegate* delegate() { return &delegate_; }
  TestWindowTreeClient* window_tree_client() { return &window_tree_client_; }
  WindowTreeTestHelper* window_tree_test_helper() {
    return window_tree_test_helper_.get();
  }
  wm::FocusController* focus_controller() { return &focus_controller_; }

  std::vector<Change>* changes() {
    return window_tree_client_.tracker()->changes();
  }

  WindowTree* window_tree() { return window_tree_.get(); }

  WindowService* service() { return service_.get(); }

  aura::test::AuraTestHelper* aura_test_helper() { return &aura_test_helper_; }

  HostEventQueue* host_event_queue() { return host_event_queue_.get(); }

 private:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};
  wm::FocusController focus_controller_;
  aura::test::AuraTestHelper aura_test_helper_;
  std::unique_ptr<wm::ScopedCaptureClient> scoped_capture_client_;
  TestWindowServiceDelegate delegate_;
  std::unique_ptr<WindowService> service_;
  std::unique_ptr<TestHostEventDispatcher> host_event_dispatcher_;
  std::unique_ptr<HostEventQueue> host_event_queue_;
  TestWindowTreeClient window_tree_client_;
  std::unique_ptr<WindowTree> window_tree_;
  std::unique_ptr<WindowTreeTestHelper> window_tree_test_helper_;

  // If true, any events that are sent to clients are acked immediately.
  bool ack_events_immediately_ = true;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceTestSetup);
};

// EmbeddingHelper contains the object necessary for an embedding. This is
// created by way of WindowServiceTestSetup::CreateEmbedding().
struct EmbeddingHelper {
  EmbeddingHelper();
  ~EmbeddingHelper();

  std::vector<Change>* changes() {
    return window_tree_client.tracker()->changes();
  }

  // The Embedding. This is owned by the window the embedding was created on.
  Embedding* embedding = nullptr;

  TestWindowTreeClient window_tree_client;

  // The client Embed() was called on.
  WindowTree* parent_window_tree = nullptr;

  // NOTE: this is owned by |parent_window_tree|.
  WindowTree* window_tree = nullptr;

  std::unique_ptr<WindowTreeTestHelper> window_tree_test_helper;
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_SERVICE_TEST_SETUP_H_
