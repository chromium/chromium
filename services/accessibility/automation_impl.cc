// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/automation_impl.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ax {

AutomationImpl::AutomationImpl() = default;
AutomationImpl::~AutomationImpl() = default;

void AutomationImpl::Bind(
    mojo::PendingRemote<mojom::AutomationClient> automation_client_remote,
    mojo::PendingReceiver<mojom::Automation> automation_receiver) {
  mojo::Remote<mojom::AutomationClient> remote(
      std::move(automation_client_remote));
  automation_client_remotes_.Add(std::move(remote));
  automation_receivers_.Add(this, std::move(automation_receiver));
}

void AutomationImpl::DispatchTreeDestroyedEvent(const ui::AXTreeID& tree_id) {
  // TODO(crbug.com/1355633): Send tree destroyed event to accessibility
  // features.
  // When implementing this method, cc an IPC security reviewer.
}

void AutomationImpl::DispatchActionResult(const ui::AXActionData& data,
                                          bool result) {
  // TODO(crbug.com/1355633): Send action result to accessibility features.
  // When implementing this method, cc an IPC security reviewer.
}

void AutomationImpl::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  // TODO(crbug.com/1355633): Send events to accessibility features.
  // When implementing this method, cc an IPC security reviewer.
}

void AutomationImpl::DispatchAccessibilityLocationChange(
    const ui::AXTreeID& tree_id,
    int node_id,
    const ui::AXRelativeBounds& bounds) {
  // TODO(crbug.com/1355633): Send location change to accessibility features.
  // When implementing this method, cc an IPC security reviewer.
}

}  // namespace ax
