// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_manager.h"

#include <utility>

#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/common/features.h"

namespace content {

RenderAccessibilityManager::RenderAccessibilityManager(
    RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

RenderAccessibilityManager::~RenderAccessibilityManager() = default;

void RenderAccessibilityManager::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::RenderAccessibility>
        receiver) {
  // TODO(crbug.com/40842669): re-add   DCHECK(!receiver_.is_bound()),
  // once underlying issue is resolved.
  if (receiver_.is_bound())
    receiver_.reset();

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      [](RenderAccessibilityManager* impl) {
        impl->receiver_.reset();
        impl->SetMode(ui::AXMode::kNone, 0);
      },
      base::Unretained(this)));
}

RenderAccessibilityImpl*
RenderAccessibilityManager::GetRenderAccessibilityImpl() {
  return render_accessibility_.get();
}

ui::AXMode RenderAccessibilityManager::GetAccessibilityMode() const {
  if (!render_accessibility_)
    return ui::AXMode();
  return render_accessibility_->GetAccessibilityMode();
}

void RenderAccessibilityManager::SetMode(const ui::AXMode& new_mode,
                                         uint32_t reset_token) {
  ui::AXMode old_mode = GetAccessibilityMode();

  if (old_mode == new_mode) {
    if (render_accessibility_) {
      render_accessibility_->set_reset_token(reset_token);
    }
    return;
  }

  if (new_mode.has_mode(ui::AXMode::kWebContents) &&
      !old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_ =
        std::make_unique<RenderAccessibilityImpl>(this, render_frame_);
  } else if (!new_mode.has_mode(ui::AXMode::kWebContents) &&
             old_mode.has_mode(ui::AXMode::kWebContents)) {
    render_accessibility_.reset();
  }

  if (render_accessibility_) {
    CHECK(reset_token);
    render_accessibility_->set_reset_token(reset_token);
    render_accessibility_->NotifyAccessibilityModeChange(new_mode);
  }

  // Notify the RenderFrame when the accessibility mode is changes to ensure it
  // notifies the relevant observers (subclasses of RenderFrameObserver). This
  // does not include the RenderAccessibilityImpl instance owned by |this| which
  // already received the mode change above. It must go first because it sets up
  // or tears down Blink accessibility ensuring subsequent observers can reason
  // accurately about accessibility.
  render_frame_->NotifyAccessibilityModeChange(new_mode);
}

void RenderAccessibilityManager::FatalError() {
  NO_CODE_FOLDING();
  CHECK(false) << "Invalid accessibility tree.";
}

void RenderAccessibilityManager::HitTest(
    const gfx::Point& point,
    ax::mojom::Event event_to_fire,
    int request_id,
    blink::mojom::RenderAccessibility::HitTestCallback callback) {
  DCHECK(render_accessibility_);
  render_accessibility_->HitTest(point, event_to_fire, request_id,
                                 std::move(callback));
}

void RenderAccessibilityManager::PerformAction(const ui::AXActionData& data) {
  DCHECK(render_accessibility_);
  render_accessibility_->PerformAction(data);
}

void RenderAccessibilityManager::Reset(uint32_t reset_token) {
  DCHECK(render_accessibility_);
  render_accessibility_->Reset(reset_token);
}

void RenderAccessibilityManager::HandleAccessibilityEvents(
    ui::AXUpdatesAndEvents& updates_and_events,
    ui::AXLocationAndScrollUpdates& location_and_scroll_updates,
    uint32_t reset_token,
    blink::mojom::RenderAccessibilityHost::HandleAXEventsCallback callback) {
  CHECK(reset_token);
  GetOrCreateRemoteRenderAccessibilityHost()->HandleAXEvents(
      updates_and_events, location_and_scroll_updates, reset_token,
      std::move(callback));
}

mojo::Remote<blink::mojom::RenderAccessibilityHost>&
RenderAccessibilityManager::GetOrCreateRemoteRenderAccessibilityHost() {
  if (!render_accessibility_host_) {
    render_frame_->GetBrowserInterfaceBroker().GetInterface(
        render_accessibility_host_.BindNewPipeAndPassReceiver());
  }
  return render_accessibility_host_;
}

void RenderAccessibilityManager::CloseConnection() {
  if (render_accessibility_host_) {
    render_accessibility_host_.reset();
    if (render_accessibility_) {
      render_accessibility_->ConnectionClosed();
    }
  }
}

}  // namespace content
