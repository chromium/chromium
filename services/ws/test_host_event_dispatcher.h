// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TEST_HOST_EVENT_DISPATCHER_H_
#define SERVICES_WS_TEST_HOST_EVENT_DISPATCHER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "services/ws/host_event_dispatcher.h"

namespace aura {
class WindowTreeHost;
}

namespace ws {

class TestHostEventDispatcher : public HostEventDispatcher {
 public:
  explicit TestHostEventDispatcher(aura::WindowTreeHost* window_tree_host);
  ~TestHostEventDispatcher() override;

  // HostEventDispatcher:
  void DispatchEventFromQueue(ui::Event* event) override;

 private:
  aura::WindowTreeHost* window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(TestHostEventDispatcher);
};

}  // namespace ws

#endif  // SERVICES_WS_TEST_HOST_EVENT_DISPATCHER_H_
