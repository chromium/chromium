// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_lacros.h"

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#include "chromeos/crosapi/cpp/bitmap.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace content {
namespace {

// An SkBitmap backed subclass of DesktopFrame. This enables the webrtc system
// to retain the SkBitmap buffer without having to copy the pixels out until
// they are needed (e.g., for encoding).
class DesktopFrameSkia : public webrtc::DesktopFrame {
 public:
  explicit DesktopFrameSkia(const SkBitmap& bitmap)
      : webrtc::DesktopFrame(
            webrtc::DesktopSize(bitmap.width(), bitmap.height()),
            bitmap.rowBytes(),
            static_cast<uint8_t*>(bitmap.getPixels()),
            nullptr),
        bitmap_(bitmap) {}
  ~DesktopFrameSkia() override = default;

 private:
  DesktopFrameSkia(const DesktopFrameSkia&) = delete;
  DesktopFrameSkia& operator=(const DesktopFrameSkia&) = delete;

  SkBitmap bitmap_;
};

}  // namespace

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

bool DesktopCapturerLacros::GetSourceList(SourceList* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<crosapi::mojom::SnapshotSourcePtr> sources;

  if (snapshot_capturer_) {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    snapshot_capturer_->ListSources(&sources);
  } else {
    if (capture_type_ == kScreen) {
      // TODO(https://crbug.com/1094460): Implement this source list
      // appropriately.
      Source w;
      w.id = 1;
      result->push_back(w);
      return true;
    }
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    screen_manager_->DeprecatedListWindows(&sources);
  }

  for (auto& source : sources) {
    Source s;
    s.id = source->id;
    s.title = source->title;
    result->push_back(s);
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

  // The lacros chrome service exists at all times except during early start-up
  // and late shut-down. This class should never be used in those two times.
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  DCHECK(lacros_chrome_service);
  lacros_chrome_service->BindScreenManagerReceiver(
      screen_manager_.BindNewPipeAndPassReceiver());

  // Do a round-trip to make sure we know what version of the screen manager we
  // are talking to. It is safe to run a nested loop here because we are on a
  // dedicated thread (see DesktopCaptureDevice) with nothing else happening.
  // TODO(crbug/1094460): Avoid this nested event loop.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  screen_manager_.QueryVersion(
      base::BindOnce([](base::OnceClosure quit_closure,
                        uint32_t version) { std::move(quit_closure).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();

  if (screen_manager_.version() >= 1) {
    if (capture_type_ == kScreen) {
      screen_manager_->GetScreenCapturer(
          snapshot_capturer_.BindNewPipeAndPassReceiver());
    } else {
      screen_manager_->GetWindowCapturer(
          snapshot_capturer_.BindNewPipeAndPassReceiver());
    }
  }
}

void DesktopCapturerLacros::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!capturing_frame_);
  capturing_frame_ = true;
#endif

  if (snapshot_capturer_) {
    snapshot_capturer_->TakeSnapshot(
        selected_source_,
        base::BindOnce(&DesktopCapturerLacros::DidTakeSnapshot,
                       weak_factory_.GetWeakPtr()));
  } else {
    if (capture_type_ == kScreen) {
      screen_manager_->DeprecatedTakeScreenSnapshot(
          base::BindOnce(&DesktopCapturerLacros::DeprecatedDidTakeSnapshot,
                         weak_factory_.GetWeakPtr(), /*success*/ true));
    } else {
      screen_manager_->DeprecatedTakeWindowSnapshot(
          selected_source_,
          base::BindOnce(&DesktopCapturerLacros::DeprecatedDidTakeSnapshot,
                         weak_factory_.GetWeakPtr()));
    }
  }
}

bool DesktopCapturerLacros::IsOccluded(const webrtc::DesktopVector& pos) {
  return false;
}

void DesktopCapturerLacros::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerLacros::SetExcludedWindow(webrtc::WindowId window) {}

void DesktopCapturerLacros::DidTakeSnapshot(bool success,
                                            const SkBitmap& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  capturing_frame_ = false;
#endif

  if (!success) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT,
                               std::unique_ptr<webrtc::DesktopFrame>());
    return;
  }

  callback_->OnCaptureResult(Result::SUCCESS,
                             std::make_unique<DesktopFrameSkia>(snapshot));
}

void DesktopCapturerLacros::DeprecatedDidTakeSnapshot(
    bool success,
    crosapi::Bitmap snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  capturing_frame_ = false;
#endif

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
