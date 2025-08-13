// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_desktop_capturer.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// Helper class to allow the capturer to be safely used on both the creating
// and capture sequences.
class PipewireDesktopCapturer::Core : public base::RefCountedThreadSafe<Core>,
                                      public webrtc::DesktopCapturer::Callback {
 public:
  Core();
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  // Invalidate the capture sequence pointers. Must be called once before
  // the destructor is called.
  void InvalidateOnCaptureSequence();

  // Invalidate the creating sequence pointers and stops video capturing. Must
  // be called once before the destructor is called.
  void InvalidateOnCreatingSequence();

  void Init(base::WeakPtr<PipewireCaptureStream> stream);
  base::WeakPtr<Core> GetCreatingSequenceWeakPtr();
  bool SupportsFrameCallbacks();
  void Start(Callback* callback);
  void CaptureFrame();
  void SetMaxFrameRate(std::uint32_t max_frame_rate);

 private:
  friend class base::RefCountedThreadSafe<Core>;

  ~Core() override;

  // webrtc::DesktopCapturer::Callback implementation.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Runs `task` on the creating sequence. The task may be run on the current
  // stack frame.
  void RunTaskOnCreatingSequence(base::OnceClosure task);

  scoped_refptr<base::SequencedTaskRunner> creating_sequence_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SequencedTaskRunner> capture_sequence_;

  // Must only be tested for validity and dereferenced on the creating sequence.
  base::WeakPtr<PipewireCaptureStream> stream_;

  // Lock to allow checking the initialization state and queueing up pending
  // capturer tasks on either sequences.
  base::Lock init_lock_;

  bool initialized_ GUARDED_BY(init_lock_) = false;

  // Tasks to be run on the creating sequence after Init() is called.
  base::OnceClosureList pending_capturer_tasks_ GUARDED_BY(init_lock_);

  // Per the webrtc::DesktopCapturer interface, callback is required to remain
  // valid until this is destroyed.
  raw_ptr<Callback> callback_;

  base::WeakPtr<Core> capture_sequence_weak_ptr_;
  base::WeakPtr<Core> creating_sequence_weak_ptr_;

  // Will be bound to the capture sequence when Start() is called and used by
  // tasks posted by CallbackProxy.
  base::WeakPtrFactory<Core> capture_sequence_weak_ptr_factory_{this};

  // Bound to the creating sequence when the object is constructed.
  base::WeakPtrFactory<Core> creating_sequence_weak_ptr_factory_{this};
};

// PipewireDesktopCapturer::Core implementation.

PipewireDesktopCapturer::PipewireDesktopCapturer::Core::Core() {
  creating_sequence_weak_ptr_ =
      creating_sequence_weak_ptr_factory_.GetWeakPtr();
}

PipewireDesktopCapturer::Core::~Core() {
  DCHECK(!creating_sequence_weak_ptr_factory_.HasWeakPtrs());
  DCHECK(!capture_sequence_weak_ptr_factory_.HasWeakPtrs());
  DCHECK(!stream_);
}

void PipewireDesktopCapturer::Core::InvalidateOnCaptureSequence() {
  if (!capture_sequence_) {
    // Capturing has never been started.
    return;
  }
  if (!capture_sequence_->RunsTasksInCurrentSequence()) {
    // PostTask() adds reference to `this`, effectively extending its lifetime
    // until the core is invalidated on the capture sequence.
    capture_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PipewireDesktopCapturer::Core::InvalidateOnCaptureSequence, this));
  }

  capture_sequence_weak_ptr_.reset();
  capture_sequence_weak_ptr_factory_.InvalidateWeakPtrsAndDoom();
}

void PipewireDesktopCapturer::Core::InvalidateOnCreatingSequence() {
  if (!creating_sequence_->RunsTasksInCurrentSequence()) {
    // PostTask() adds reference to `this`, effectively extending its lifetime
    // until the core is invalidated on the creating sequence.
    creating_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PipewireDesktopCapturer::Core::InvalidateOnCreatingSequence,
            this));
    return;
  }

  creating_sequence_weak_ptr_.reset();
  creating_sequence_weak_ptr_factory_.InvalidateWeakPtrsAndDoom();

  if (stream_) {
    stream_->StopVideoCapture();
  }
  // We must always call reset() event if `stream_` is falsy, since otherwise
  // the destructor can't safely null-test it on the capturing sequence.
  stream_.reset();
}

void PipewireDesktopCapturer::Core::Init(
    base::WeakPtr<PipewireCaptureStream> stream) {
  base::AutoLock lock(init_lock_);
  stream_ = stream;
  initialized_ = true;
  if (!pending_capturer_tasks_.empty()) {
    pending_capturer_tasks_.Notify();
    pending_capturer_tasks_.Clear();
  }
}

base::WeakPtr<PipewireDesktopCapturer::Core>
PipewireDesktopCapturer::Core::GetCreatingSequenceWeakPtr() {
  return creating_sequence_weak_ptr_;
}

bool PipewireDesktopCapturer::Core::SupportsFrameCallbacks() {
  return true;
}

void PipewireDesktopCapturer::Core::Start(Callback* callback) {
  // The capturer is created by GnomeInteractionStrategy on its sequence, but
  // is potentially passed to and owned by a different sequence, which will be
  // the sequence that calls Start().
  capture_sequence_ = base::SequencedTaskRunner::GetCurrentDefault();
  capture_sequence_weak_ptr_ = capture_sequence_weak_ptr_factory_.GetWeakPtr();
  callback_ = callback;

  RunTaskOnCreatingSequence(base::BindOnce(&PipewireCaptureStream::SetCallback,
                                           stream_, capture_sequence_,
                                           capture_sequence_weak_ptr_));
}

void PipewireDesktopCapturer::Core::CaptureFrame() {
  // Capturer will push frames as they are ready.
  DCHECK(capture_sequence_->RunsTasksInCurrentSequence());
}

void PipewireDesktopCapturer::Core::SetMaxFrameRate(
    std::uint32_t max_frame_rate) {
  RunTaskOnCreatingSequence(base::BindOnce(
      &PipewireCaptureStream::SetMaxFrameRate, stream_, max_frame_rate));
}

void PipewireDesktopCapturer::Core::OnFrameCaptureStart() {
  DCHECK(capture_sequence_->RunsTasksInCurrentSequence());
  callback_->OnFrameCaptureStart();
}

void PipewireDesktopCapturer::Core::OnCaptureResult(
    Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(capture_sequence_->RunsTasksInCurrentSequence());
  callback_->OnCaptureResult(result, std::move(frame));
}

void PipewireDesktopCapturer::Core::RunTaskOnCreatingSequence(
    base::OnceClosure task) {
  {
    base::AutoLock lock(init_lock_);

    if (!initialized_) {
      pending_capturer_tasks_.AddUnsafe(std::move(task));
      return;
    }
  }

  if (creating_sequence_->RunsTasksInCurrentSequence()) {
    std::move(task).Run();
  } else {
    creating_sequence_->PostTask(FROM_HERE, std::move(task));
  }
}

// PipewireDesktopCapturer implementation.

PipewireDesktopCapturer::PipewireDesktopCapturer()
    : core_(base::MakeRefCounted<Core>()) {}

PipewireDesktopCapturer::~PipewireDesktopCapturer() {
  core_->InvalidateOnCaptureSequence();
  core_->InvalidateOnCreatingSequence();
}

base::OnceCallback<void(base::WeakPtr<PipewireCaptureStream>)>
PipewireDesktopCapturer::GetInitCallback() {
  // We can't safely provide WeakPtr<PipewireDesktopCapturer> bound to the
  // creating sequence, since the capturer might be destructed on the capturer
  // sequence after it is dereferenced on the creating sequence, causing `core_`
  // to be invalid. So, we have to bind and return a callback of Core::Init()
  // instead.
  return base::BindOnce(&PipewireDesktopCapturer::Core::Init,
                        core_->GetCreatingSequenceWeakPtr());
}

bool PipewireDesktopCapturer::SupportsFrameCallbacks() {
  return core_->SupportsFrameCallbacks();
}

void PipewireDesktopCapturer::Start(Callback* callback) {
  core_->Start(callback);
}

void PipewireDesktopCapturer::CaptureFrame() {
  core_->CaptureFrame();
}

void PipewireDesktopCapturer::SetMaxFrameRate(std::uint32_t max_frame_rate) {
  core_->SetMaxFrameRate(max_frame_rate);
}

bool PipewireDesktopCapturer::GetSourceList(SourceList* sources) {
  NOTREACHED();
}

bool PipewireDesktopCapturer::SelectSource(SourceId id) {
  NOTREACHED();
}

}  // namespace remoting
