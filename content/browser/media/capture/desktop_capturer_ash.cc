// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_ash.h"

#include "ash/shell.h"
#include "content/browser/media/capture/desktop_frame_skia.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot_aura.h"

namespace content {

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
  aura::Window* window = ash::Shell::GetRootWindowForNewWindows();
  if (display_id_) {
    window = ash::Shell::GetRootWindowForDisplayId(*display_id_);
  }
  if (!window) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  const gfx::Rect bounds(window->bounds().size());
  ui::GrabWindowSnapshot(
      window, bounds,
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
