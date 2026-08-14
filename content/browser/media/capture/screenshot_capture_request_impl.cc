// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screenshot_capture_request_impl.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/media/capture/native_screen_capture_picker_mac.h"
#endif

namespace content::desktop_capture {

namespace {

#if BUILDFLAG(IS_MAC)
class MacScreenshotCaptureRequest : public ScreenshotCaptureRequest {
 public:
  explicit MacScreenshotCaptureRequest(
      base::OnceCallback<void(const SkBitmap&)> callback)
      : callback_(std::move(callback)) {}
  ~MacScreenshotCaptureRequest() override = default;

  void OnResult(const SkBitmap& bitmap) {
    if (callback_) {
      std::move(callback_).Run(bitmap);
    }
  }

  base::WeakPtr<MacScreenshotCaptureRequest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::OnceCallback<void(const SkBitmap&)> callback_;
  base::WeakPtrFactory<MacScreenshotCaptureRequest> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_MAC)

class ScreenshotCaptureRequestImpl : public ScreenshotCaptureRequest {
 public:
  ScreenshotCaptureRequestImpl(
      std::unique_ptr<webrtc::DesktopCapturer> capturer,
      base::OnceCallback<void(const SkBitmap&)> callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    worker_ = base::MakeRefCounted<CaptureWorker>(std::move(capturer),
                                                  std::move(callback));
    worker_->Start();
  }

  ~ScreenshotCaptureRequestImpl() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    worker_->Cancel();
  }

 private:
  class CaptureWorker : public base::RefCountedThreadSafe<CaptureWorker>,
                        public webrtc::DesktopCapturer::Callback {
   public:
    CaptureWorker(std::unique_ptr<webrtc::DesktopCapturer> capturer,
                  base::OnceCallback<void(const SkBitmap&)> callback)
        : capturer_(std::move(capturer)),
          callback_(std::move(callback)),
          caller_task_runner_(
              base::SingleThreadTaskRunner::GetCurrentDefault()),
          capturer_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

    void Start() {
      capturer_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CaptureWorker::StartAndCaptureOnBackgroundThread,
                         base::RetainedRef(this)));
    }

    void Cancel() {
      callback_.Reset();
      capturer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CaptureWorker::StopOnBackgroundThread,
                                    base::RetainedRef(this)));
    }

    // webrtc::DesktopCapturer::Callback (called on background thread):
    void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                         std::unique_ptr<webrtc::DesktopFrame> frame) override {
      DCHECK(capturer_task_runner_->RunsTasksInCurrentSequence());
      SkBitmap bitmap;
      if (result == webrtc::DesktopCapturer::Result::SUCCESS && frame) {
        bitmap.allocN32Pixels(frame->size().width(), frame->size().height());
        uint8_t* pixels_data = reinterpret_cast<uint8_t*>(bitmap.getPixels());
        if constexpr (kN32_SkColorType == kBGRA_8888_SkColorType) {
          libyuv::ARGBCopy(frame->data(), frame->stride(), pixels_data,
                           bitmap.rowBytes(), frame->size().width(),
                           frame->size().height());
        } else {
          libyuv::ABGRToARGB(frame->data(), frame->stride(), pixels_data,
                             bitmap.rowBytes(), frame->size().width(),
                             frame->size().height());
        }
      }

      caller_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CaptureWorker::RunCallbackOnCallerThread,
                         base::RetainedRef(this), std::move(bitmap)));
    }

   private:
    friend class base::RefCountedThreadSafe<CaptureWorker>;
    ~CaptureWorker() override = default;

    void StartAndCaptureOnBackgroundThread() {
      DCHECK(capturer_task_runner_->RunsTasksInCurrentSequence());
      if (!capturer_) {
        caller_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&CaptureWorker::RunCallbackOnCallerThread,
                                      base::RetainedRef(this), SkBitmap()));
        return;
      }
      capturer_->Start(this);
      capturer_->CaptureFrame();
    }

    void StopOnBackgroundThread() {
      DCHECK(capturer_task_runner_->RunsTasksInCurrentSequence());
      if (capturer_) {
        capturer_->Start(nullptr);
        capturer_.reset();
      }
    }

    void RunCallbackOnCallerThread(SkBitmap bitmap) {
      DCHECK(caller_task_runner_->RunsTasksInCurrentSequence());
      if (callback_) {
        std::move(callback_).Run(bitmap);
      }
    }

    std::unique_ptr<webrtc::DesktopCapturer> capturer_;
    base::OnceCallback<void(const SkBitmap&)> callback_;

    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
    scoped_refptr<base::SequencedTaskRunner> capturer_task_runner_;
  };

  THREAD_CHECKER(thread_checker_);
  scoped_refptr<CaptureWorker> worker_;
};

std::unique_ptr<webrtc::DesktopCapturer>& GetInjectedDesktopCapturer() {
  static base::NoDestructor<std::unique_ptr<webrtc::DesktopCapturer>> capturer;
  return *capturer;
}

}  // namespace

void SetDesktopCapturerForTesting(  // IN-TEST
    std::unique_ptr<webrtc::DesktopCapturer> capturer) {
  GetInjectedDesktopCapturer() = std::move(capturer);
}

std::unique_ptr<ScreenshotCaptureRequest> CreateScreenshotCaptureRequest(
    DesktopMediaID source,
    base::OnceCallback<void(const ::SkBitmap&)> callback) {
#if BUILDFLAG(IS_MAC)
  if (source.id_type == DesktopMediaID::IdType::kNativePickerSession) {
    auto request =
        std::make_unique<MacScreenshotCaptureRequest>(std::move(callback));
    CaptureScreenshotFromMacNativePicker(
        source.id, base::BindOnce(&MacScreenshotCaptureRequest::OnResult,
                                  request->GetWeakPtr()));
    return request;
  }
#endif

  std::unique_ptr<webrtc::DesktopCapturer> capturer =
      std::move(GetInjectedDesktopCapturer());
  if (!capturer) {
    auto options = CreateDesktopCaptureOptions();
    if (source.type == content::DesktopMediaID::TYPE_SCREEN) {
      capturer = CreateScreenCapturer(options, /*for_snapshot=*/true);
    } else if (source.type == content::DesktopMediaID::TYPE_WINDOW) {
      capturer = CreateWindowCapturer(options);
    }
  }

  if (!capturer || !capturer->SelectSource(source.id)) {
    return nullptr;
  }

  return std::make_unique<ScreenshotCaptureRequestImpl>(std::move(capturer),
                                                        std::move(callback));
}

}  // namespace content::desktop_capture
