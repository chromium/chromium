// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_overscroll_controller_delegate_aura.h"

#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/test/test_utils.h"
#include "ui/display/screen.h"

namespace content {

MockOverscrollControllerDelegateAura::MockOverscrollControllerDelegateAura(
    RenderWidgetHostViewAura* rwhva)
    : rwhva_(rwhva),
      update_message_loop_runner_(new MessageLoopRunner),
      end_message_loop_runner_(new MessageLoopRunner),
      seen_update_(false),
      overscroll_ended_(false) {}

MockOverscrollControllerDelegateAura::~MockOverscrollControllerDelegateAura() {}

gfx::Size MockOverscrollControllerDelegateAura::GetDisplaySize() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(rwhva_->GetNativeView())
      .size();
}

std::optional<float>
MockOverscrollControllerDelegateAura::GetMaxOverscrollDelta() const {
  return std::nullopt;
}

bool MockOverscrollControllerDelegateAura::OnOverscrollUpdate(float, float) {
  seen_update_ = true;
  if (update_message_loop_runner_->loop_running())
    update_message_loop_runner_->Quit();
  return true;
}

void MockOverscrollControllerDelegateAura::OnOverscrollComplete(
    OverscrollMode) {
  OnOverscrollEnd();
}

void MockOverscrollControllerDelegateAura::OnOverscrollModeChange(
    OverscrollMode old_mode,
    OverscrollMode new_mode,
    OverscrollSource source,
    cc::OverscrollBehavior behavior) {
  if (new_mode == OVERSCROLL_NONE)
    OnOverscrollEnd();
}

void MockOverscrollControllerDelegateAura::WaitForUpdate() {
  if (!seen_update_)
    update_message_loop_runner_->Run();
}

void MockOverscrollControllerDelegateAura::WaitForEnd() {
  if (!overscroll_ended_)
    end_message_loop_runner_->Run();
}

void MockOverscrollControllerDelegateAura::Reset() {
  update_message_loop_runner_ = new MessageLoopRunner;
  end_message_loop_runner_ = new MessageLoopRunner;
  seen_update_ = false;
  overscroll_ended_ = false;
}

void MockOverscrollControllerDelegateAura::OnOverscrollEnd() {
  overscroll_ended_ = true;
  if (end_message_loop_runner_->loop_running())
    end_message_loop_runner_->Quit();
}

}  // namespace content
