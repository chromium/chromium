// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/desktop_capturer_proxy.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

#if defined(WEBRTC_USE_GIO)
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#endif

namespace remoting {

class DesktopCapturerProxy::Core : public webrtc::DesktopCapturer::Callback {
 public:
  explicit Core(base::WeakPtr<DesktopCapturerProxy> proxy);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void SetCapturer(std::unique_ptr<DesktopCapturer> capturer);
  void CreateCapturer(
      base::OnceCallback<std::unique_ptr<DesktopCapturer>()> creator);

  void Start(scoped_refptr<base::SequencedTaskRunner> caller_task_runner);
  void SetSharedMemoryFactory(
      std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory);
  void SelectSource(SourceId id);
  void CaptureFrame();
  void SetMaxFrameRate(std::uint32_t max_frame_rate);
  void Pause(bool pause);
  void BoostCaptureRate(base::TimeDelta capture_interval,
                        base::TimeDelta duration);

#if defined(WEBRTC_USE_GIO)
  webrtc::DesktopCaptureMetadata GetMetadata();
#endif

  base::WeakPtr<Core> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnFrameCaptureStart() override;
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Either runs the capturer task right away if it is set, or queue it so that
  // it can be run after the underlying capturer is created. This is to allow
  // callers to set capture params before the underlying capturer has been
  // created.
  template <typename F, typename... Args>
    requires std::invocable<F, DesktopCapturer*, Args...>
  void RunCapturerTask(F&& method, Args&&... args);

  base::WeakPtr<DesktopCapturerProxy> proxy_;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;
  std::unique_ptr<DesktopCapturer> capturer_;

  // Tasks to be run after `capturer_` is set.
  base::queue<base::OnceCallback<void(DesktopCapturer*)>> pending_tasks_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<Core> weak_ptr_factory_{this};
};

DesktopCapturerProxy::Core::Core(base::WeakPtr<DesktopCapturerProxy> proxy)
    : proxy_(proxy) {
  DETACH_FROM_THREAD(thread_checker_);
}

DesktopCapturerProxy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DesktopCapturerProxy::Core::SetCapturer(
    std::unique_ptr<DesktopCapturer> capturer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!capturer_);

  capturer_ = std::move(capturer);
  if (!capturer_) {
    LOG(DFATAL) << "Trying to set the underlying capturer to null.";
    return;
  }
  while (!pending_tasks_.empty()) {
    if (capturer_) {
      std::move(pending_tasks_.front()).Run(capturer_.get());
    }
    pending_tasks_.pop();
  }
}

void DesktopCapturerProxy::Core::CreateCapturer(
    base::OnceCallback<std::unique_ptr<DesktopCapturer>()> creator) {
  auto capturer = std::move(creator).Run();

  if (!capturer) {
    LOG(ERROR) << "Failed to initialize screen capturer.";
  }
  SetCapturer(std::move(capturer));
}

void DesktopCapturerProxy::Core::Start(
    scoped_refptr<base::SequencedTaskRunner> caller_task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!caller_task_runner_);

  caller_task_runner_ = caller_task_runner;
  RunCapturerTask(&DesktopCapturer::Start, this);
}

void DesktopCapturerProxy::Core::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunCapturerTask(&DesktopCapturer::SetSharedMemoryFactory,
                  std::move(shared_memory_factory));
}

void DesktopCapturerProxy::Core::SelectSource(SourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunCapturerTask(&DesktopCapturer::SelectSource, std::move(id));
}

void DesktopCapturerProxy::Core::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunCapturerTask(&DesktopCapturer::CaptureFrame);
}

void DesktopCapturerProxy::Core::SetMaxFrameRate(std::uint32_t max_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunCapturerTask(&DesktopCapturer::SetMaxFrameRate, std::move(max_frame_rate));
}

void DesktopCapturerProxy::Core::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunCapturerTask(&DesktopCapturer::Pause, std::move(pause));
}

void DesktopCapturerProxy::Core::BoostCaptureRate(
    base::TimeDelta capture_interval,
    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RunCapturerTask(&DesktopCapturer::BoostCaptureRate,
                  std::move(capture_interval), std::move(duration));
}

#if defined(WEBRTC_USE_GIO)
webrtc::DesktopCaptureMetadata DesktopCapturerProxy::Core::GetMetadata() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return capturer_ ? capturer_->GetMetadata()
                   : webrtc::DesktopCaptureMetadata{};
}
#endif

void DesktopCapturerProxy::Core::OnFrameCaptureStart() {
  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DesktopCapturerProxy::OnFrameCaptureStarting, proxy_));
}

void DesktopCapturerProxy::Core::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DesktopCapturerProxy::OnFrameCaptured, proxy_,
                                result, std::move(frame)));
}

template <typename F, typename... Args>
  requires std::invocable<F, DesktopCapturer*, Args...>
void DesktopCapturerProxy::Core::RunCapturerTask(F&& method, Args&&... args) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    std::invoke(std::forward<F>(method), capturer_,
                std::forward<Args>(args)...);
    return;
  }
  pending_tasks_.push(base::BindOnce(
      [](F&& method, Args&&... args, DesktopCapturer* capturer) {
        std::invoke(std::forward<F>(method), capturer,
                    std::forward<Args>(args)...);
      },
      std::forward<F>(method), std::forward<Args>(args)...));
}

DesktopCapturerProxy::DesktopCapturerProxy(
    scoped_refptr<base::SequencedTaskRunner> capture_task_runner)
    : capture_task_runner_(std::move(capture_task_runner)) {
  DETACH_FROM_THREAD(thread_checker_);
  core_ = std::make_unique<Core>(weak_factory_.GetWeakPtr());
}

DesktopCapturerProxy::~DesktopCapturerProxy() {
  capture_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void DesktopCapturerProxy::CreateCapturer(
    base::OnceCallback<std::unique_ptr<DesktopCapturer>()> creator) {
  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::CreateCapturer, base::Unretained(core_.get()),
                     std::move(creator)));
}

base::WeakPtr<DesktopCapturerProxy> DesktopCapturerProxy::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DesktopCapturerProxy::set_capturer(
    std::unique_ptr<DesktopCapturer> capturer) {
  core_->SetCapturer(std::move(capturer));
}

base::OnceCallback<void(std::unique_ptr<DesktopCapturer>)>
DesktopCapturerProxy::GetSetCapturerCallback() {
  return base::BindPostTask(
      capture_task_runner_,
      base::BindOnce(&DesktopCapturerProxy::Core::SetCapturer,
                     core_->GetWeakPtr()));
}

void DesktopCapturerProxy::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  callback_ = callback;

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Start, base::Unretained(core_.get()),
                     base::SequencedTaskRunner::GetCurrentDefault()));
}

void DesktopCapturerProxy::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetSharedMemoryFactory,
                                base::Unretained(core_.get()),
                                std::move(shared_memory_factory)));
}

void DesktopCapturerProxy::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Start() must be called before CaptureFrame().
  DCHECK(callback_);

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::CaptureFrame, base::Unretained(core_.get())));
}

bool DesktopCapturerProxy::GetSourceList(SourceList* sources) {
  NOTIMPLEMENTED();
  return false;
}

bool DesktopCapturerProxy::SelectSource(SourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::SelectSource, base::Unretained(core_.get()), id));
  return false;
}

void DesktopCapturerProxy::OnFrameCaptureStarting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  callback_->OnFrameCaptureStart();
}

void DesktopCapturerProxy::OnFrameCaptured(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  callback_->OnCaptureResult(result, std::move(frame));
}

#if defined(WEBRTC_USE_GIO)
void DesktopCapturerProxy::GetMetadataAsync(
    base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&Core::GetMetadata, base::Unretained(core_.get())),
      base::BindOnce(&DesktopCapturerProxy::OnMetadata,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DesktopCapturerProxy::OnMetadata(
    base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> callback,
    webrtc::DesktopCaptureMetadata metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(callback).Run(std::move(metadata));
}
#endif

void DesktopCapturerProxy::SetMaxFrameRate(std::uint32_t max_frame_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetMaxFrameRate,
                                base::Unretained(core_.get()), max_frame_rate));
}

void DesktopCapturerProxy::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Pause, base::Unretained(core_.get()), pause));
}

void DesktopCapturerProxy::BoostCaptureRate(base::TimeDelta capture_interval,
                                            base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::BoostCaptureRate, base::Unretained(core_.get()),
                     capture_interval, duration));
}

}  // namespace remoting
