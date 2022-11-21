// Copyright 2015 The Chromium Authors
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
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer_differ_wrapper.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/feature_list.h"
#include "remoting/host/chromeos/aura_desktop_capturer.h"
#include "remoting/host/chromeos/features.h"
#include "remoting/host/chromeos/frame_sink_desktop_capturer.h"
#endif

#if defined(REMOTING_USE_WAYLAND)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "remoting/host/linux/wayland_desktop_capturer.h"
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

  void Start(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner);
  void SetSharedMemoryFactory(
      std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory);
  void SelectSource(SourceId id);
  void CaptureFrame();
#if defined(WEBRTC_USE_GIO)
  void GetAndSetMetadata();
#endif

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  base::WeakPtr<DesktopCapturerProxy> proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;

  THREAD_CHECKER(thread_checker_);
};

DesktopCapturerProxy::Core::Core(base::WeakPtr<DesktopCapturerProxy> proxy)
    : proxy_(proxy) {
  DETACH_FROM_THREAD(thread_checker_);
}

DesktopCapturerProxy::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DesktopCapturerProxy::Core::CreateCapturer(
    const webrtc::DesktopCaptureOptions& options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!capturer_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(
          remoting::features::kEnableFrameSinkDesktopCapturerInCrd)) {
    capturer_ = std::make_unique<FrameSinkDesktopCapturer>();
  } else {
    capturer_ = std::make_unique<webrtc::DesktopCapturerDifferWrapper>(
        std::make_unique<AuraDesktopCapturer>());
  }
#elif defined(REMOTING_USE_WAYLAND)
  static base::nix::SessionType session_type = base::nix::SessionType::kUnset;
  if (session_type == base::nix::SessionType::kUnset) {
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    session_type = base::nix::GetSessionType(*env);
  }

  if (options.allow_pipewire() &&
      session_type == base::nix::SessionType::kWayland) {
    capturer_ = std::make_unique<WaylandDesktopCapturer>(options);
  } else {
    capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
  }
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!capturer_)
    LOG(ERROR) << "Failed to initialize screen capturer.";
}

void DesktopCapturerProxy::Core::Start(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!caller_task_runner_);

  caller_task_runner_ = caller_task_runner;
  if (capturer_)
    capturer_->Start(this);
}

void DesktopCapturerProxy::Core::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
  }
}

void DesktopCapturerProxy::Core::SelectSource(SourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    capturer_->SelectSource(id);
  }
}

void DesktopCapturerProxy::Core::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    capturer_->CaptureFrame();
  } else {
    OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_PERMANENT, nullptr);
  }
}

#if defined(WEBRTC_USE_GIO)
void DesktopCapturerProxy::Core::GetAndSetMetadata() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (capturer_) {
    webrtc::DesktopCaptureMetadata metadata = capturer_->GetMetadata();
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DesktopCapturerProxy::OnMetadata, proxy_,
                                  std::move(metadata)));
  }
}
#endif

void DesktopCapturerProxy::Core::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DesktopCapturerProxy::OnFrameCaptured, proxy_,
                                result, std::move(frame)));
}

DesktopCapturerProxy::DesktopCapturerProxy(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner)
    : capture_task_runner_(capture_task_runner) {
  DETACH_FROM_THREAD(thread_checker_);
  core_ = std::make_unique<Core>(weak_factory_.GetWeakPtr());
}

DesktopCapturerProxy::~DesktopCapturerProxy() {
  capture_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void DesktopCapturerProxy::CreateCapturer(
    const webrtc::DesktopCaptureOptions& options) {
  // CreateCapturer() must be called before Start().
  DCHECK(!callback_);

  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::CreateCapturer,
                                base::Unretained(core_.get()), options));
}

void DesktopCapturerProxy::set_capturer(
    std::unique_ptr<webrtc::DesktopCapturer> capturer) {
  // set_capturer() must be called before Start().
  DCHECK(!callback_);

  core_->set_capturer(std::move(capturer));
}

void DesktopCapturerProxy::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  callback_ = callback;

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Start, base::Unretained(core_.get()),
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
}

void DesktopCapturerProxy::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::SetSharedMemoryFactory,
                     base::Unretained(core_.get()),
                     base::WrapUnique(shared_memory_factory.release())));
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

  metadata_callback_ = std::move(callback);
  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::GetAndSetMetadata, base::Unretained(core_.get())));
}

void DesktopCapturerProxy::OnMetadata(webrtc::DesktopCaptureMetadata metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(metadata_callback_).Run(std::move(metadata));
}

#endif
}  // namespace remoting
