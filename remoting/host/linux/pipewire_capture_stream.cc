// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_capture_stream.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

// SharedScreenCastStream runs the pipewire loop, and invokes frame callbacks,
// on a separate thread. This class is responsible for bouncing them back to
// the corresponding methods of `parent_` on `callback_sequence`.
class PipewireCaptureStream::CallbackProxy
    : public webrtc::DesktopCapturer::Callback,
      public webrtc::SharedScreenCastStream::Observer {
 public:
  explicit CallbackProxy(base::WeakPtr<PipewireCaptureStream> parent);
  ~CallbackProxy() override;

  void Start();
  void Stop();

  // Callback interface
  void OnFrameCaptureStart() override;
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // webrtc::SharedScreenCastStream::Observer implementation.
  void OnCursorPositionChanged() override;
  void OnCursorShapeChanged() override;
  void OnDesktopFrameChanged() override;
  void OnFailedToProcessBuffer() override;
  void OnBufferCorruptedMetadata() override;
  void OnBufferCorruptedData() override;
  void OnEmptyBuffer() override;
  void OnStreamConfigured() override;
  void OnFrameRateChanged(uint32_t frame_rate) override;

 private:
  // Lock is needed since Initialize() and the callback methods are called
  // from different threads. It also ensures that the initial frame is
  // delivered before any frames received from the SharedScreenCastStream.
  base::Lock lock_;
  bool started_ GUARDED_BY(lock_) = false;
  scoped_refptr<base::SequencedTaskRunner> callback_sequence_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::WeakPtr<PipewireCaptureStream> parent_;
};

PipewireCaptureStream::CallbackProxy::CallbackProxy(
    base::WeakPtr<PipewireCaptureStream> parent)
    : parent_(parent) {}

PipewireCaptureStream::CallbackProxy::~CallbackProxy() = default;

void PipewireCaptureStream::CallbackProxy::Start() {
  base::AutoLock lock(lock_);
  started_ = true;
}

void PipewireCaptureStream::CallbackProxy::Stop() {
  base::AutoLock lock(lock_);
  started_ = false;
}

void PipewireCaptureStream::CallbackProxy::OnFrameCaptureStart() {
  base::AutoLock lock(lock_);
  if (!started_) {
    return;
  }
  callback_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipewireCaptureStream::OnFrameCaptureStart, parent_));
}

void PipewireCaptureStream::CallbackProxy::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  base::AutoLock lock(lock_);
  if (!started_) {
    return;
  }
  callback_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&PipewireCaptureStream::OnCaptureResult,
                                parent_, result, std::move(frame)));
}

void PipewireCaptureStream::CallbackProxy::OnCursorPositionChanged() {
  base::AutoLock lock(lock_);
  if (!started_) {
    return;
  }
  callback_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipewireCaptureStream::OnCursorPositionChanged, parent_));
}

void PipewireCaptureStream::CallbackProxy::OnCursorShapeChanged() {
  base::AutoLock lock(lock_);
  if (!started_) {
    return;
  }
  callback_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&PipewireCaptureStream::OnCursorShapeChanged, parent_));
}

void PipewireCaptureStream::CallbackProxy::OnDesktopFrameChanged() {}
void PipewireCaptureStream::CallbackProxy::OnFailedToProcessBuffer() {}
void PipewireCaptureStream::CallbackProxy::OnBufferCorruptedMetadata() {}
void PipewireCaptureStream::CallbackProxy::OnBufferCorruptedData() {}
void PipewireCaptureStream::CallbackProxy::OnEmptyBuffer() {}
void PipewireCaptureStream::CallbackProxy::OnStreamConfigured() {}
void PipewireCaptureStream::CallbackProxy::OnFrameRateChanged(
    uint32_t frame_rate) {}

PipewireCaptureStream::PipewireCaptureStream() {
  callback_proxy_ =
      std::make_unique<CallbackProxy>(weak_ptr_factory_.GetWeakPtr());
  stream_->SetObserver(callback_proxy_.get());
}

PipewireCaptureStream::~PipewireCaptureStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PipewireCaptureStream::SetPipeWireStream(
    std::uint32_t pipewire_node,
    const webrtc::DesktopSize& initial_resolution,
    std::string_view mapping_id,
    int pipewire_fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pipewire_node_ = pipewire_node;
  resolution_ = initial_resolution;
  mapping_id_ = mapping_id;
  pipewire_fd_ = pipewire_fd;
}

void PipewireCaptureStream::StartVideoCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->StartScreenCastStream(pipewire_node_, pipewire_fd_,
                                 resolution_.width(), resolution_.height(),
                                 false, callback_proxy_.get());
}

void PipewireCaptureStream::SetCallback(
    base::WeakPtr<webrtc::DesktopCapturer::Callback> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  if (!callback_) {
    // The current lifecycle of the pipewire stream and its virtual monitor is:
    //
    // 1. Call the org_gnome_Mutter_ScreenCast_Stream::Start API, which creates
    //    the pipewire stream but doesn't actually create the virtual monitor.
    // 2. Call stream_->StartScreenCastStream(), which creates the virtual
    //    monitor.
    // 3. Call stream_->StopScreenCastStream(), which stops the stream but
    //    doesn't destroy the virtual monitor.
    // 4. Call the org_gnome_Mutter_ScreenCast_Stream::Stop API, which destroys
    //    the virtual monitor.
    //
    // Based on this, we could call StopScreenCastStream() here and call
    // StartScreenCastStream() again when the callback is set to a non-null
    // value. However, the lifecycle is not documented anywhere, and it's
    // asymmetrical which doesn't sound right, so we don't do it in case the
    // behavior gets changed in the future.
    callback_proxy_->Stop();
    is_capturing_frame_ = false;
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  // RecaptureLatestFrameAsDirty() must be called before
  // callback_proxy_.Initialize(), since calling the latter will immediately
  // start pumping frames to `PipewireCaptureStream` and can potentially cause
  // race conditions (an old frame is delivered after the current frame).
  RecaptureLatestFrameAsDirty();
  // While unlikely, RecaptureLatestFrameAsDirty() runs `callback_` in the
  // current stack frame and could potentially delete `this`, so we should only
  // access class members if the weak pointer remains valid.
  if (self) {
    callback_proxy_->Start();
  }
}

void PipewireCaptureStream::SetUseDamageRegion(bool use_damage_region) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->SetUseDamageRegion(use_damage_region);
  RecaptureLatestFrameAsDirty();
}

void PipewireCaptureStream::SetResolution(
    const webrtc::DesktopSize& new_resolution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resolution_ = new_resolution;
  stream_->UpdateScreenCastStreamResolution(resolution_.width(),
                                            resolution_.height());
}

void PipewireCaptureStream::SetMaxFrameRate(std::uint32_t frame_rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_->UpdateScreenCastStreamFrameRate(frame_rate);
}

std::unique_ptr<webrtc::MouseCursor> PipewireCaptureStream::CaptureCursor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->CaptureCursor();
}

std::optional<webrtc::DesktopVector>
PipewireCaptureStream::CaptureCursorPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_->CaptureCursorPosition();
}

CaptureStream::CursorObserver::Subscription
PipewireCaptureStream::AddCursorObserver(CursorObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_observers_.AddObserver(observer);
  return base::ScopedClosureRunner(
      base::BindOnce(&PipewireCaptureStream::RemoveCursorObserver,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

std::string_view PipewireCaptureStream::mapping_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mapping_id_;
}

const webrtc::DesktopSize& PipewireCaptureStream::resolution() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resolution_;
}

void PipewireCaptureStream::set_screen_id(webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screen_id_ = screen_id;
}

webrtc::ScreenId PipewireCaptureStream::screen_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return screen_id_;
}

base::WeakPtr<CaptureStream> PipewireCaptureStream::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void PipewireCaptureStream::RecaptureLatestFrameAsDirty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_capturing_frame_) {
    should_mark_current_frame_dirty_ = true;
    return;
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  OnFrameCaptureStart();
  // While unlikely, OnFrameCaptureStart() runs `callback_` in the current stack
  // frame and could potentially delete `this`, so we should only access class
  // members if the weak pointer remains valid.
  if (!self) {
    return;
  }
  // Note: CaptureFrame() does not really capture a new frame. It just returns
  // the latest available frame, or null if it's unavailable.
  auto frame = stream_->CaptureFrame();
  if (frame) {
    // Mark the entire frame as dirty.
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeSize(frame->size()));
    OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS, std::move(frame));
  } else {
    OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
  }
}

void PipewireCaptureStream::RemoveCursorObserver(CursorObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_observers_.RemoveObserver(observer);
}

void PipewireCaptureStream::OnFrameCaptureStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_capturing_frame_ = true;
  if (callback_) {
    callback_->OnFrameCaptureStart();
  }
}

void PipewireCaptureStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_capturing_frame_ = false;

  if (frame) {
    if (!should_mark_current_frame_dirty_) {
      // Check to see if the updated region is invalid, which may happen if the
      // frame with an invalid updated region is received before
      // SetUseDamageRegion(false) is called. If this happens, we mark the
      // entire frame dirty. Note that the updated region could still be invalid
      // even if the check passes, e.g., the monitor offset changes slightly so
      // the updated rectangles still remain in the desktop rectangle.
      // SetUseDamageRegion() will call RecaptureLatestFrameAsDirty() to cover
      // that.
      auto updated_region_it =
          webrtc::DesktopRegion::Iterator(frame->updated_region());
      while (!updated_region_it.IsAtEnd()) {
        if (updated_region_it.rect().left() < 0 ||
            updated_region_it.rect().top() < 0 ||
            updated_region_it.rect().right() > frame->size().width() ||
            updated_region_it.rect().bottom() > frame->size().height()) {
          should_mark_current_frame_dirty_ = true;
          break;
        }
        updated_region_it.Advance();
      }
    }
    if (should_mark_current_frame_dirty_) {
      frame->mutable_updated_region()->SetRect(
          webrtc::DesktopRect::MakeSize(frame->size()));
    }
  }

  should_mark_current_frame_dirty_ = false;
  if (callback_) {
    callback_->OnCaptureResult(result, std::move(frame));
  }
}

void PipewireCaptureStream::OnCursorPositionChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_observers_.Notify(&CursorObserver::OnCursorPositionChanged, this);
}

void PipewireCaptureStream::OnCursorShapeChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cursor_observers_.Notify(&CursorObserver::OnCursorShapeChanged, this);
}

}  // namespace remoting
