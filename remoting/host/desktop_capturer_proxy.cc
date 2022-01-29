// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_capturer_proxy.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer_differ_wrapper.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "remoting/host/chromeos/aura_desktop_capturer.h"
#endif

namespace remoting {

class DesktopCapturerProxy::Core : public webrtc::DesktopCapturer::Callback {
 public:
  explicit Core(base::WeakPtr<DesktopCapturerProxy> proxy);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void set_capturer(std::unique_ptr<webrtc::DesktopCapturer> capturer) {
    DCHECK(!capturer_);
    capturer_ = std::move(capturer);
  }
  void CreateCapturer(const webrtc::DesktopCaptureOptions& options);

  void Start();
  void SetSharedMemoryFactory(
      std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory);
  void SelectSource(SourceId id);
  void CaptureFrame();

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<DesktopCapturerProxy> proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
};

DesktopCapturerProxy::Core::Core(base::WeakPtr<DesktopCapturerProxy> proxy)
    : proxy_(proxy), caller_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  thread_checker_.DetachFromThread();
}

DesktopCapturerProxy::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DesktopCapturerProxy::Core::CreateCapturer(
    const webrtc::DesktopCaptureOptions& options) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!capturer_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  capturer_ = std::make_unique<webrtc::DesktopCapturerDifferWrapper>(
      std::make_unique<AuraDesktopCapturer>());
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!capturer_)
    LOG(ERROR) << "Failed to initialize screen capturer.";
}

void DesktopCapturerProxy::Core::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (capturer_)
    capturer_->Start(this);
}

void DesktopCapturerProxy::Core::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (capturer_) {
    capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
  }
}

void DesktopCapturerProxy::Core::SelectSource(SourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (capturer_) {
    capturer_->SelectSource(id);
  }
}

void DesktopCapturerProxy::Core::CaptureFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (capturer_) {
    capturer_->CaptureFrame();
  } else {
    OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_PERMANENT, nullptr);
  }
}

void DesktopCapturerProxy::Core::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DesktopCapturerProxy::OnFrameCaptured, proxy_,
                                result, std::move(frame)));
}

DesktopCapturerProxy::DesktopCapturerProxy(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : capture_task_runner_(capture_task_runner) {
  core_ = std::make_unique<Core>(weak_factory_.GetWeakPtr());
}

DesktopCapturerProxy::~DesktopCapturerProxy() {
  capture_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void DesktopCapturerProxy::set_desktop_display_info_monitor(
    std::unique_ptr<DesktopDisplayInfoMonitor> monitor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  desktop_display_info_monitor_ = std::move(monitor);
}

void DesktopCapturerProxy::CreateCapturer(
    const webrtc::DesktopCaptureOptions& options) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::CreateCapturer,
                                base::Unretained(core_.get()), options));
}

void DesktopCapturerProxy::set_capturer(
    std::unique_ptr<webrtc::DesktopCapturer> capturer) {
  core_->set_capturer(std::move(capturer));
}

void DesktopCapturerProxy::Start(Callback* callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_ = callback;

  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

void DesktopCapturerProxy::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::SetSharedMemoryFactory,
                     base::Unretained(core_.get()),
                     base::WrapUnique(shared_memory_factory.release())));
}

void DesktopCapturerProxy::CaptureFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Start() must be called before Capture().
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
  DCHECK(thread_checker_.CalledOnValidThread());

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::SelectSource, base::Unretained(core_.get()), id));
  return false;
}

void DesktopCapturerProxy::OnFrameCaptured(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_->OnCaptureResult(result, std::move(frame));

  if (desktop_display_info_monitor_) {
    desktop_display_info_monitor_->QueryDisplayInfo();
  }
}

}  // namespace remoting
