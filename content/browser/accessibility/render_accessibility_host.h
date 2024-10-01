// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_RENDER_ACCESSIBILITY_HOST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_RENDER_ACCESSIBILITY_HOST_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/mojom/ax_updates_and_events.mojom.h"

namespace content {

class RenderFrameHostImpl;

// Handles accessibility messages sent from the renderer to the browser possibly
// off the main UI thread and then forwards them to the RenderFrameHostImpl on
// the browser process main thread, so that the potentially large
// deserialization does not block the main thread.
//
// This endpoint is bound on the ThreadPool for *performance reasons*, so its
// messages can be reordered w.r.t. navigation. To mitigate this:
//
// * The renderer explicitly destroys this endpoint when committing a new
//   document in the existing RenderFrame, so all messages send on behalf of a
//   new document will go through new document's BrowserInterfaceBroker.
//
// * Each message contains the per-document tree_id and the message will be
//   ignored if the tree_id doesn't match the current tree_id passed at
//   construction time. (It will also be checked again on the UI thread).
class RenderAccessibilityHost : public blink::mojom::RenderAccessibilityHost {
 public:
  RenderAccessibilityHost(
      base::WeakPtr<RenderFrameHostImpl> render_frame_host_impl,
      ui::AXTreeID tree_id);

  void Bind(
      mojo::PendingReceiver<blink::mojom::RenderAccessibilityHost> receiver) {
    receiver_.Add(this, std::move(receiver));
  }

  RenderAccessibilityHost(const RenderAccessibilityHost&) = delete;
  RenderAccessibilityHost& operator=(const RenderAccessibilityHost&) = delete;

  ~RenderAccessibilityHost() override;

  void HandleAXEvents(
      const ui::AXUpdatesAndEvents& updates_and_events,
      const ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
      uint32_t reset_token,
      HandleAXEventsCallback callback) override;
  void HandleAXEvents(
      ui::AXUpdatesAndEvents& updates_and_events,
      ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
      uint32_t reset_token,
      HandleAXEventsCallback callback) override;

  void HandleAXLocationChanges(const ui::AXLocationAndScrollUpdates& changes,
                               uint32_t reset_token) override;

  void HandleAXLocationChanges(ui::AXLocationAndScrollUpdates& changes,
                               uint32_t reset_token) override;

 private:
  base::WeakPtr<RenderFrameHostImpl> render_frame_host_impl_;
  // TODO(chrishtr): change this back to a Receiver once all render process
  /// callsites of this mojo interface have been migrated to Blink.
  mojo::ReceiverSet<blink::mojom::RenderAccessibilityHost> receiver_;
  const ui::AXTreeID tree_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_RENDER_ACCESSIBILITY_HOST_H_
