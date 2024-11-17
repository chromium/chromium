// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_ash.h"

#include "content/browser/media/capture/desktop_frame_skia.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot_aura.h"

namespace content {
namespace {

aura::Window* FindRootWindowForDisplayId(int64_t display_id) {
  for (auto& window_tree_host : aura::Env::GetInstance()->window_tree_hosts()) {
    auto* root_window = window_tree_host->window();
    auto display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
    if (display.id() == display_id) {
      return root_window;
    }
  }
  return nullptr;
}

}  // namespace

DesktopCapturerAsh::DesktopCapturerAsh() = default;

DesktopCapturerAsh::~DesktopCapturerAsh() = default;

bool DesktopCapturerAsh::GetSourceList(SourceList* result) {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    result->emplace_back(display.id(), std::string(), display.id());
  }
  return true;
}

bool DesktopCapturerAsh::SelectSource(SourceId id) {
  display_id_ = id;
  return true;
}

bool DesktopCapturerAsh::FocusOnSelectedSource() {
  return true;
}

void DesktopCapturerAsh::Start(Callback* callback) {
  callback_ = callback;
}

void DesktopCapturerAsh::CaptureFrame() {
  aura::Window* root_window = nullptr;
  if (display_id_) {
    root_window = FindRootWindowForDisplayId(*display_id_);
  }
  if (!root_window) {
    root_window = FindRootWindowForDisplayId(
        display::Screen::GetScreen()->GetDisplayForNewWindows().id());
  }
  if (!root_window) {
    // No root window to capture was found.
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }
  const gfx::Rect bounds(root_window->bounds().size());
  ui::GrabWindowSnapshot(
      root_window, bounds,
      base::BindOnce(&DesktopCapturerAsh::OnGrabWindowSnapsot,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool DesktopCapturerAsh::IsOccluded(const webrtc::DesktopVector& pos) {
  return false;
}

void DesktopCapturerAsh::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerAsh::SetExcludedWindow(webrtc::WindowId window) {}

void DesktopCapturerAsh::OnGrabWindowSnapsot(gfx::Image snapshot) {
  callback_->OnCaptureResult(
      Result::SUCCESS,
      std::make_unique<content::DesktopFrameSkia>(snapshot.AsBitmap()));
}

}  // namespace content
