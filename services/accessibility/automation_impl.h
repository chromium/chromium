// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_AUTOMATION_IMPL_H_
#define SERVICES_ACCESSIBILITY_AUTOMATION_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/automation.mojom.h"
#include "services/accessibility/public/mojom/automation_client.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ax {

// Implementation of Automation in the Accessibility service process for Chrome.
class AutomationImpl : public mojom::Automation {
 public:
  AutomationImpl();
  ~AutomationImpl() override;
  AutomationImpl(const AutomationImpl&) = delete;
  AutomationImpl& operator=(const AutomationImpl&) = delete;

  void Bind(
      mojo::PendingRemote<mojom::AutomationClient> automation_client_remote,
      mojo::PendingReceiver<mojom::Automation> automation_receiver);

 private:
  // mojom::Automation:
  void DispatchTreeDestroyedEvent(const ui::AXTreeID& tree_id) override;
  void DispatchActionResult(const ui::AXActionData& data, bool result) override;
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      int node_id,
      const ui::AXRelativeBounds& bounds) override;

  // We may have multiple automation sources, so use a set to store their
  // receivers.
  mojo::ReceiverSet<mojom::Automation> automation_receivers_;

  // We can send automation info back to the OS with the automation client
  // interface. There may be multiple AutomationClients, for example
  // in different processes and tree sources.
  mojo::RemoteSet<mojom::AutomationClient> automation_client_remotes_;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_AUTOMATION_IMPL_H_
