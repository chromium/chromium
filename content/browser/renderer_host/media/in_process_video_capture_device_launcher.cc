// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/in_process_launched_video_capture_device.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/common/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#if defined(OS_ANDROID)
#include "content/browser/media/capture/screen_capture_device_android.h"
#else
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#if defined(USE_AURA)
#include "content/browser/media/capture/aura_window_video_capture_device.h"
#endif
#include "content/browser/media/capture/desktop_capture_device.h"
#endif  // defined(OS_ANDROID)
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

#if defined(OS_CHROMEOS)
#include "content/browser/gpu/chromeos/video_capture_dependencies.h"
#include "media/capture/video/chromeos/scoped_video_capture_jpeg_decoder.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)
std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    media::VideoCaptureJpegDecoder::DecodeDoneCB decode_done_cb,
    base::Callback<void(const std::string&)> send_log_message_cb) {
  auto io_task_runner =
      base::CreateSingleThreadTaskRunner({content::BrowserThread::IO});
  return std::make_unique<media::ScopedVideoCaptureJpegDecoder>(
      std::make_unique<media::VideoCaptureJpegDecoderImpl>(
          base::BindRepeating(
              &content::VideoCaptureDependencies::CreateJpegDecodeAccelerator),
          io_task_runner, std::move(decode_done_cb),
          std::move(send_log_message_cb)),
      io_task_runner);
}
#endif  // defined(OS_CHROMEOS)

// The maximum number of video frame buffers in-flight at any one time. This
// value should be based on the logical capacity of the capture pipeline, and
// not on hardware performance.
const int kMaxNumberOfBuffers = 3;

}  // anonymous namespace

namespace content {

InProcessVideoCaptureDeviceLauncher::InProcessVideoCaptureDeviceLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    media::VideoCaptureSystem* video_capture_system)
    : device_task_runner_(std::move(device_task_runner)),
      video_capture_system_(video_capture_system),
      state_(State::READY_TO_LAUNCH) {}

InProcessVideoCaptureDeviceLauncher::~InProcessVideoCaptureDeviceLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == State::READY_TO_LAUNCH);
}

void InProcessVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver_on_io_thread,
    base::OnceClosure /* connection_lost_cb */,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == State::READY_TO_LAUNCH);

  if (receiver_on_io_thread) {
    std::ostringstream string_stream;
    string_stream
        << "InProcessVideoCaptureDeviceLauncher::LaunchDeviceAsync: Posting "
           "start request to device thread for device_id = "
        << device_id;
    receiver_on_io_thread->OnLog(string_stream.str());
  }

  // Wrap the receiver, to trampoline all its method calls from the device
  // to the IO thread.
  auto receiver = std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
      receiver_on_io_thread,
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}));

  base::OnceClosure start_capture_closure;
  // Use of Unretained |this| is safe, because |done_cb| guarantees that |this|
  // stays alive.
  ReceiveDeviceCallback after_start_capture_callback = media::BindToCurrentLoop(
      base::BindOnce(&InProcessVideoCaptureDeviceLauncher::OnDeviceStarted,
                     base::Unretained(this), callbacks, std::move(done_cb)));

  switch (stream_type) {
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE: {
      if (!video_capture_system_) {
        // Clients who create an instance of |this| without providing a
        // VideoCaptureSystem instance are expected to know that
        // MEDIA_DEVICE_VIDEO_CAPTURE is not supported in this case.
        NOTREACHED();
        return;
      }
      start_capture_closure = base::BindOnce(
          &InProcessVideoCaptureDeviceLauncher::
              DoStartDeviceCaptureOnDeviceThread,
          base::Unretained(this), device_id, params,
          CreateDeviceClient(media::VideoCaptureBufferType::kSharedMemory,
                             kMaxNumberOfBuffers, std::move(receiver),
                             std::move(receiver_on_io_thread)),
          std::move(after_start_capture_callback));
      break;
    }

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
#if !defined(OS_ANDROID)
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      start_capture_closure = base::BindOnce(
          &InProcessVideoCaptureDeviceLauncher::DoStartTabCaptureOnDeviceThread,
          base::Unretained(this), device_id, params, std::move(receiver),
          std::move(after_start_capture_callback));
      break;
#endif  // !defined(OS_ANDROID)

    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      FALLTHROUGH;
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE: {
      const DesktopMediaID desktop_id = DesktopMediaID::Parse(device_id);
      if (desktop_id.is_null()) {
        DLOG(ERROR) << "Desktop media ID is null";
        start_capture_closure =
            base::BindOnce(std::move(after_start_capture_callback), nullptr);
        break;
      }

      if (desktop_id.id == DesktopMediaID::kFakeId) {
        start_capture_closure = base::BindOnce(
            &InProcessVideoCaptureDeviceLauncher::
                DoStartFakeDisplayCaptureOnDeviceThread,
            base::Unretained(this), desktop_id, params,
            CreateDeviceClient(media::VideoCaptureBufferType::kSharedMemory,
                               kMaxNumberOfBuffers, std::move(receiver),
                               std::move(receiver_on_io_thread)),
            std::move(after_start_capture_callback));
        break;
      }

#if !defined(OS_ANDROID)
      if (desktop_id.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
        after_start_capture_callback = base::BindOnce(
            [](bool with_audio, ReceiveDeviceCallback callback,
               std::unique_ptr<media::VideoCaptureDevice> device) {
              // Special case: Only call IncrementDesktopCaptureCounter()
              // for WebContents capture if it was started from a desktop
              // capture API.
              if (device) {
                IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED);
                IncrementDesktopCaptureCounter(
                    with_audio ? TAB_VIDEO_CAPTURER_CREATED_WITH_AUDIO
                               : TAB_VIDEO_CAPTURER_CREATED_WITHOUT_AUDIO);
              }
              std::move(callback).Run(std::move(device));
            },
            desktop_id.audio_share, std::move(after_start_capture_callback));
        start_capture_closure = base::BindOnce(
            &InProcessVideoCaptureDeviceLauncher::
                DoStartTabCaptureOnDeviceThread,
            base::Unretained(this), device_id, params, std::move(receiver),
            std::move(after_start_capture_callback));
        break;
      }
#endif  // !defined(OS_ANDROID)

#if defined(USE_AURA)
      if (desktop_id.window_id != DesktopMediaID::kNullId) {
        start_capture_closure = base::BindOnce(
            &InProcessVideoCaptureDeviceLauncher::
                DoStartAuraWindowCaptureOnDeviceThread,
            base::Unretained(this), desktop_id, params, std::move(receiver),
            std::move(after_start_capture_callback));
        break;
      }
#endif  // defined(USE_AURA)

      // All cases other than tab capture or Aura desktop/window capture.
      start_capture_closure = base::BindOnce(
          &InProcessVideoCaptureDeviceLauncher::
              DoStartDesktopCaptureOnDeviceThread,
          base::Unretained(this), desktop_id, params,
          CreateDeviceClient(media::VideoCaptureBufferType::kSharedMemory,
                             kMaxNumberOfBuffers, std::move(receiver),
                             std::move(receiver_on_io_thread)),
          std::move(after_start_capture_callback));
      break;
    }
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

    default: {
      NOTIMPLEMENTED();
      std::move(after_start_capture_callback).Run(nullptr);
      return;
    }
  }

  device_task_runner_->PostTask(FROM_HERE, std::move(start_capture_closure));
  state_ = State::DEVICE_START_IN_PROGRESS;
}

void InProcessVideoCaptureDeviceLauncher::AbortLaunch() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == State::DEVICE_START_IN_PROGRESS)
    state_ = State::DEVICE_START_ABORTING;
}

std::unique_ptr<media::VideoCaptureDeviceClient>
InProcessVideoCaptureDeviceLauncher::CreateDeviceClient(
    media::VideoCaptureBufferType requested_buffer_type,
    int buffer_pool_max_buffer_count,
    std::unique_ptr<media::VideoFrameReceiver> receiver,
    base::WeakPtr<media::VideoFrameReceiver> receiver_on_io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool =
      new media::VideoCaptureBufferPoolImpl(
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(),
          requested_buffer_type, buffer_pool_max_buffer_count);

#if defined(OS_CHROMEOS)
  return std::make_unique<media::VideoCaptureDeviceClient>(
      requested_buffer_type, std::move(receiver), std::move(buffer_pool),
      base::BindRepeating(
          &CreateGpuJpegDecoder,
          base::BindRepeating(&media::VideoFrameReceiver::OnFrameReadyInBuffer,
                              receiver_on_io_thread),
          base::BindRepeating(&media::VideoFrameReceiver::OnLog,
                              receiver_on_io_thread)));
#else
  return std::make_unique<media::VideoCaptureDeviceClient>(
      requested_buffer_type, std::move(receiver), std::move(buffer_pool));
#endif  // defined(OS_CHROMEOS)
}

void InProcessVideoCaptureDeviceLauncher::OnDeviceStarted(
    Callbacks* callbacks,
    base::OnceClosure done_cb,
    std::unique_ptr<media::VideoCaptureDevice> device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  State state_copy = state_;
  state_ = State::READY_TO_LAUNCH;
  if (!device) {
    switch (state_copy) {
      case State::DEVICE_START_IN_PROGRESS:
        callbacks->OnDeviceLaunchFailed(
            media::VideoCaptureError::
                kInProcessDeviceLauncherFailedToCreateDeviceInstance);
        std::move(done_cb).Run();
        return;
      case State::DEVICE_START_ABORTING:
        callbacks->OnDeviceLaunchAborted();
        std::move(done_cb).Run();
        return;
      case State::READY_TO_LAUNCH:
        NOTREACHED();
        return;
    }
  }

  auto launched_device = std::make_unique<InProcessLaunchedVideoCaptureDevice>(
      std::move(device), device_task_runner_);

  switch (state_copy) {
    case State::DEVICE_START_IN_PROGRESS:
      callbacks->OnDeviceLaunched(std::move(launched_device));
      std::move(done_cb).Run();
      return;
    case State::DEVICE_START_ABORTING:
      launched_device.reset();
      callbacks->OnDeviceLaunchAborted();
      std::move(done_cb).Run();
      return;
    case State::READY_TO_LAUNCH:
      NOTREACHED();
      return;
  }
}

void InProcessVideoCaptureDeviceLauncher::DoStartDeviceCaptureOnDeviceThread(
    const std::string& device_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());
  DCHECK(video_capture_system_);

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device =
      video_capture_system_->CreateDevice(device_id);

  if (video_capture_device)
    video_capture_device->AllocateAndStart(params, std::move(device_client));
  std::move(result_callback).Run(std::move(video_capture_device));
}

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)

#if !defined(OS_ANDROID)
void InProcessVideoCaptureDeviceLauncher::DoStartTabCaptureOnDeviceThread(
    const std::string& device_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<WebContentsVideoCaptureDevice> video_capture_device =
      WebContentsVideoCaptureDevice::Create(device_id);
  if (video_capture_device) {
    video_capture_device->AllocateAndStartWithReceiver(params,
                                                       std::move(receiver));
  }
  std::move(result_callback).Run(std::move(video_capture_device));
}
#endif  // !defined(OS_ANDROID)

#if defined(USE_AURA)
void InProcessVideoCaptureDeviceLauncher::
    DoStartAuraWindowCaptureOnDeviceThread(
        const DesktopMediaID& device_id,
        const media::VideoCaptureParams& params,
        std::unique_ptr<media::VideoFrameReceiver> receiver,
        ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  auto video_capture_device =
      std::make_unique<AuraWindowVideoCaptureDevice>(device_id);
  if (video_capture_device) {
    video_capture_device->AllocateAndStartWithReceiver(params,
                                                       std::move(receiver));
    switch (device_id.type) {
      case DesktopMediaID::TYPE_SCREEN:
        IncrementDesktopCaptureCounter(SCREEN_CAPTURER_CREATED);
        IncrementDesktopCaptureCounter(
            device_id.audio_share ? SCREEN_CAPTURER_CREATED_WITH_AUDIO
                                  : SCREEN_CAPTURER_CREATED_WITHOUT_AUDIO);
        break;
      case DesktopMediaID::TYPE_WINDOW:
        IncrementDesktopCaptureCounter(WINDOW_CAPTURER_CREATED);
        break;
      case DesktopMediaID::TYPE_NONE:
      case DesktopMediaID::TYPE_WEB_CONTENTS:
        NOTREACHED();
        break;
    }
  }
  std::move(result_callback).Run(std::move(video_capture_device));
}
#endif  // defined(USE_AURA)

void InProcessVideoCaptureDeviceLauncher::DoStartDesktopCaptureOnDeviceThread(
    const DesktopMediaID& desktop_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_id.is_null());

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device;
#if defined(OS_ANDROID)
  video_capture_device = std::make_unique<ScreenCaptureDeviceAndroid>();
#else
  if (!video_capture_device)
    video_capture_device = DesktopCaptureDevice::Create(desktop_id);
#endif  // defined (OS_ANDROID)

  if (video_capture_device)
    video_capture_device->AllocateAndStart(params, std::move(device_client));
  std::move(result_callback).Run(std::move(video_capture_device));
}

#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

void InProcessVideoCaptureDeviceLauncher::
    DoStartFakeDisplayCaptureOnDeviceThread(
        const DesktopMediaID& desktop_id,
        const media::VideoCaptureParams& params,
        std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
        ReceiveDeviceCallback result_callback) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(DesktopMediaID::kFakeId, desktop_id.id);

  auto fake_device_factory =
      std::make_unique<media::FakeVideoCaptureDeviceFactory>();
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream)) {
    std::vector<media::FakeVideoCaptureDeviceSettings> config;
    media::FakeVideoCaptureDeviceFactory::
        ParseFakeDevicesConfigFromOptionsString(
            command_line->GetSwitchValueASCII(
                switches::kUseFakeDeviceForMediaStream),
            &config);
    fake_device_factory->SetToCustomDevicesConfig(config);
  }
  media::VideoCaptureDeviceDescriptors device_descriptors;
  fake_device_factory->GetDeviceDescriptors(&device_descriptors);
  if (device_descriptors.empty()) {
    LOG(ERROR) << "Cannot start with no fake device config";
    std::move(result_callback).Run(nullptr);
    return;
  }
  auto video_capture_device =
      fake_device_factory->CreateDevice(device_descriptors.front());
  video_capture_device->AllocateAndStart(params, std::move(device_client));
  std::move(result_callback).Run(std::move(video_capture_device));
}

}  // namespace content
