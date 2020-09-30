// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_lacros.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#include "chromeos/crosapi/cpp/bitmap.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace content {

DesktopCapturerLacros::DesktopCapturerLacros(
    CaptureType capture_type,
    const webrtc::DesktopCaptureOptions& options)
    : capture_type_(capture_type), options_(options) {
  // Allow this class to be constructed on any sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DesktopCapturerLacros::~DesktopCapturerLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool DesktopCapturerLacros::GetSourceList(SourceList* sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capture_type_ == kScreen) {
    // TODO(https://crbug.com/1094460): Implement this source list
    // appropriately.
    Source w;
    w.id = 1;
    sources->push_back(w);
    return true;
  }

  EnsureScreenManager();

  std::vector<crosapi::mojom::WindowDetailsPtr> windows;
  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    screen_manager_->ListWindows(&windows);
  }

  for (auto& window : windows) {
    Source w;
    w.id = window->id;
    w.title = window->title;
    sources->push_back(w);
  }
  return true;
}

bool DesktopCapturerLacros::SelectSource(SourceId id) {
#if DCHECK_IS_ON()
  // OK to modify on any thread prior to calling Start.
  DCHECK(!callback_ || sequence_checker_.CalledOnValidSequence());
#endif

  selected_source_ = id;
  return true;
}

bool DesktopCapturerLacros::FocusOnSelectedSource() {
  return true;
}

void DesktopCapturerLacros::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
}

void DesktopCapturerLacros::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EnsureScreenManager();

  if (capture_type_ == kScreen) {
    crosapi::Bitmap snapshot;
    {
      // lacros-chrome is allowed to make sync calls to ash-chrome.
      mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
      screen_manager_->TakeScreenSnapshot(&snapshot);
    }
    DidTakeSnapshot(/*success=*/true, snapshot);
  } else {
    bool success;
    crosapi::Bitmap snapshot;
    {
      // lacros-chrome is allowed to make sync calls to ash-chrome.
      mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
      screen_manager_->TakeWindowSnapshot(selected_source_, &success,
                                          &snapshot);
    }
    DidTakeSnapshot(success, snapshot);
  }
}

bool DesktopCapturerLacros::IsOccluded(const webrtc::DesktopVector& pos) {
  return false;
}

void DesktopCapturerLacros::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerLacros::SetExcludedWindow(webrtc::WindowId window) {}

void DesktopCapturerLacros::EnsureScreenManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (screen_manager_)
    return;

  // The lacros chrome service exists at all times except during early start-up
  // and late shut-down. This class should never be used in those two times.
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  DCHECK(lacros_chrome_service);
  lacros_chrome_service->BindScreenManagerReceiver(
      screen_manager_.BindNewPipeAndPassReceiver());
}

void DesktopCapturerLacros::DidTakeSnapshot(bool success,
                                            const crosapi::Bitmap& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT,
                               std::unique_ptr<webrtc::DesktopFrame>());
    return;
  }

  std::unique_ptr<webrtc::DesktopFrame> frame =
      std::make_unique<webrtc::BasicDesktopFrame>(
          webrtc::DesktopSize(snapshot.width, snapshot.height));

  // This code assumes that the stride is 4 * width. This relies on the
  // assumption that there's no padding and each pixel is 4 bytes.
  frame->CopyPixelsFrom(
      snapshot.pixels.data(), 4 * snapshot.width,
      webrtc::DesktopRect::MakeWH(snapshot.width, snapshot.height));

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

}  // namespace content
