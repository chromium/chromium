// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/test_host_event_dispatcher.h"

#include "ui/aura/window_tree_host.h"
#include "ui/events/event_sink.h"

namespace ws {

TestHostEventDispatcher::TestHostEventDispatcher(
    aura::WindowTreeHost* window_tree_host)
    : window_tree_host_(window_tree_host) {
  DCHECK(window_tree_host_);
}

TestHostEventDispatcher::~TestHostEventDispatcher() = default;

void TestHostEventDispatcher::DispatchEventFromQueue(ui::Event* event) {
  ignore_result(window_tree_host_->event_sink()->OnEventFromSource(event));
}

}  // namespace ws
