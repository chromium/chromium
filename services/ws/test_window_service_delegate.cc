// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/test_window_service_delegate.h"

#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"

namespace ws {

TestWindowServiceDelegate::TestWindowServiceDelegate(
    aura::Window* top_level_parent)
    : top_level_parent_(top_level_parent) {}

TestWindowServiceDelegate::~TestWindowServiceDelegate() = default;

WindowServiceDelegate::DoneCallback
TestWindowServiceDelegate::TakeMoveLoopCallback() {
  return std::move(move_loop_callback_);
}

WindowServiceDelegate::DragDropCompletedCallback
TestWindowServiceDelegate::TakeDragLoopCallback() {
  return std::move(drag_loop_callback_);
}

std::unique_ptr<aura::Window> TestWindowServiceDelegate::NewTopLevel(
    aura::PropertyConverter* property_converter,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(delegate_for_next_top_level_);
  delegate_for_next_top_level_ = nullptr;
  window->Init(ui::LAYER_NOT_DRAWN);
  if (top_level_parent_)
    top_level_parent_->AddChild(window.get());
  for (auto property : properties) {
    property_converter->SetPropertyFromTransportValue(
        window.get(), property.first, &property.second);
  }
  return window;
}

void TestWindowServiceDelegate::OnUnhandledKeyEvent(
    const ui::KeyEvent& key_event) {
  unhandled_key_events_.push_back(key_event);
}

void TestWindowServiceDelegate::RunWindowMoveLoop(aura::Window* window,
                                                  mojom::MoveLoopSource source,
                                                  const gfx::Point& cursor,
                                                  DoneCallback callback) {
  move_loop_callback_ = std::move(callback);
}

void TestWindowServiceDelegate::CancelWindowMoveLoop() {
  cancel_window_move_loop_called_ = true;
}

void TestWindowServiceDelegate::RunDragLoop(
    aura::Window* window,
    const ui::OSExchangeData& data,
    const gfx::Point& screen_location,
    uint32_t drag_operation,
    ui::DragDropTypes::DragEventSource source,
    DragDropCompletedCallback callback) {
  drag_loop_callback_ = std::move(callback);
}

void TestWindowServiceDelegate::CancelDragLoop(aura::Window* window) {
  cancel_drag_loop_called_ = true;
}

aura::Window* TestWindowServiceDelegate::GetTopmostWindowAtPoint(
    const gfx::Point& location_in_screen,
    const std::set<aura::Window*>& ignore,
    aura::Window** real_topmost) {
  if (real_topmost)
    *real_topmost = real_topmost_;
  return topmost_;
}

}  // namespace ws
