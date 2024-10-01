// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/browser/renderer_host/media/in_process_launched_video_capture_device.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/common/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if defined(USE_AURA)
#include "content/browser/media/capture/aura_window_video_capture_device.h"
#endif  // defined(USE_AURA)
#include "content/browser/media/capture/desktop_capture_device.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_MAC)
#include "content/browser/media/capture/desktop_capture_device_mac.h"
#include "content/browser/media/capture/screen_capture_kit_device_utils_mac.h"
#include "content/browser/media/capture/views_widget_video_capture_device_mac.h"
#endif
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/browser/gpu/chromeos/video_capture_dependencies.h"
#include "media/capture/video/chromeos/scoped_video_capture_jpeg_decoder.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"
#elif BUILDFLAG(IS_WIN)
#include "media/capture/video/win/video_capture_device_factory_win.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    media::VideoCaptureJpegDecoder::DecodeDoneCB decode_done_cb,
    base::RepeatingCallback<void(const std::string&)> send_log_message_cb) {
  auto io_task_runner = GetIOThreadTaskRunner({});
  return std::make_unique<media::ScopedVideoCaptureJpegDecoder>(
      std::make_unique<media::VideoCaptureJpegDecoderImpl>(
          base::BindRepeating(
              &VideoCaptureDependencies::CreateJpegDecodeAccelerator),
          io_task_runner, std::move(decode_done_cb),
          std::move(send_log_message_cb)),
      io_task_runner);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)

// The maximum number of video frame buffers in-flight at any one time. This
// value should be based on the logical capacity of the capture pipeline, and
// not on hardware performance.
const int kMaxNumberOfBuffers = media::kVideoCaptureDefaultMaxBufferPoolSize;

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kScreenCaptureKitMac,
             "ScreenCaptureKitMac",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, ScreenCaptureKit will be used for window
// capturing even if kScreenCaptureKitMac is disabled. Please note that this
// feature has no effect if kScreenCaptureKitMac is enabled.
BASE_FEATURE(kScreenCaptureKitMacWindow,
             "ScreenCaptureKitMacWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If this feature is enabled, ScreenCaptureKit will be used for screen
// capturing even if kScreenCaptureKitMac is disabled. Please note that this
// feature has no effect if kScreenCaptureKitMac is enabled.
BASE_FEATURE(kScreenCaptureKitMacScreen,
             "ScreenCaptureKitMacScreen",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

void IncrementDesktopCaptureCounters(const DesktopMediaID& device_id) {
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
    case DesktopMediaID::TYPE_WEB_CONTENTS:
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED);
      IncrementDesktopCaptureCounter(
          device_id.audio_share ? TAB_VIDEO_CAPTURER_CREATED_WITH_AUDIO
                                : TAB_VIDEO_CAPTURER_CREATED_WITHOUT_AUDIO);
      break;
    case DesktopMediaID::TYPE_NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum DesktopCaptureImplementation {
  kNoImplementation = 0,
  kScreenCaptureDeviceAndroid = 1,
  kScreenCaptureKitDeviceMac = 2,
  kDesktopCaptureDeviceMac = 3,
  kLegacyDesktopCaptureDevice = 4,
  kImplementationCount = 5,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum DesktopCaptureImplementationAndType {
  kNoImplementationTypeNone = 0,
  kNoImplementationTypeScreen = 1,
  kNoImplementationTypeWindow = 2,
  kNoImplementationTypeWebContents = 3,
  kScreenCaptureDeviceAndroidTypeNone = 4,
  kScreenCaptureDeviceAndroidTypeScreen = 5,
  kScreenCaptureDeviceAndroidTypeWindow = 6,
  kScreenCaptureDeviceAndroidTypeWebContents = 7,
  kScreenCaptureKitDeviceMacTypeNone = 8,
  kScreenCaptureKitDeviceMacTypeScreen = 9,
  kScreenCaptureKitDeviceMacTypeWindow = 10,
  kScreenCaptureKitDeviceMacTypeWebContents = 11,
  kDesktopCaptureDeviceMacTypeNone = 12,
  kDesktopCaptureDeviceMacTypeScreen = 13,
  kDesktopCaptureDeviceMacTypeWindow = 14,
  kDesktopCaptureDeviceMacTypeWebContents = 15,
  kLegacyDesktopCaptureDeviceTypeNone = 16,
  kLegacyDesktopCaptureDeviceTypeScreen = 17,
  kLegacyDesktopCaptureDeviceTypeWindow = 18,
  kLegacyDesktopCaptureDeviceTypeWebContents = 19,
  kMaxValue = kLegacyDesktopCaptureDeviceTypeWebContents,
};

void ReportDesktopCaptureImplementationAndType(
    DesktopCaptureImplementation implementation,
    DesktopMediaID::Type type) {
  constexpr int kDesktopIdTypeCount = 4;
  static_assert(kDesktopIdTypeCount * kImplementationCount ==
                DesktopCaptureImplementationAndType::kMaxValue + 1);
  DCHECK_LT(type, kDesktopIdTypeCount);
  auto implementation_and_type =
      static_cast<DesktopCaptureImplementationAndType>(
          implementation * kDesktopIdTypeCount + type);
  base::UmaHistogramEnumeration(
      "Media.VideoCaptureManager.DesktopCaptureImplementationAndType",
      implementation_and_type);
}

DesktopCaptureImplementation CreatePlatformDependentVideoCaptureDevice(
    NativeScreenCapturePicker* picker,
    const DesktopMediaID& desktop_id,
    std::unique_ptr<media::VideoCaptureDevice>& device_out) {
  DCHECK_EQ(device_out.get(), nullptr);
#if BUILDFLAG(IS_MAC)
  // Use ScreenCaptureKit with picker if specified. `desktop_id` for the picker
  // is not compatible with the other implementations.
  if (picker) {
    device_out = picker->CreateDevice(desktop_id);
    if (device_out) {
      return kScreenCaptureKitDeviceMac;
    }
    return kNoImplementation;
  }

  // Prefer using ScreenCaptureKit. After that try DesktopCaptureDeviceMac, and
  // if both fail, use the generic DesktopCaptureDevice.
  if (base::FeatureList::IsEnabled(kScreenCaptureKitMac) ||
      (desktop_id.type == DesktopMediaID::TYPE_WINDOW &&
       base::FeatureList::IsEnabled(kScreenCaptureKitMacWindow)) ||
      (desktop_id.type == DesktopMediaID::TYPE_SCREEN &&
       base::FeatureList::IsEnabled(kScreenCaptureKitMacScreen))) {
    device_out = CreateScreenCaptureKitDeviceMac(desktop_id);
    if (device_out) {
      return kScreenCaptureKitDeviceMac;
    }
  }
  if ((device_out = CreateDesktopCaptureDeviceMac(desktop_id))) {
    return kDesktopCaptureDeviceMac;
  }
#endif  // BUILDFLAG(IS_MAC)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if ((device_out = DesktopCaptureDevice::Create(desktop_id))) {
    return kLegacyDesktopCaptureDevice;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  return kNoImplementation;
}
#endif  // BUILDFLAG(ENABLE_SCREEN_CAPTURE)
}  // anonymous namespace

InProcessVideoCaptureDeviceLauncher::InProcessVideoCaptureDeviceLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    NativeScreenCapturePicker* picker)
    : device_task_runner_(std::move(device_task_runner)),
      state_(State::READY_TO_LAUNCH),
      native_screen_capture_picker_(picker) {}

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
    base::OnceClosure done_cb,
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
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
      receiver_on_io_thread, GetIOThreadTaskRunner({}));

  base::OnceClosure start_capture_closure;
  // Use of Unretained |this| is safe, because |done_cb| guarantees that |this|
  // stays alive.
  ReceiveDeviceCallback after_start_capture_callback =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &InProcessVideoCaptureDeviceLauncher::OnDeviceStarted,
          base::Unretained(this), callbacks, std::move(done_cb)));

  switch (stream_type) {
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      // Only the Service-based device launcher is supported for device capture
      // from cameras etc.
      NOTREACHED();
#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      start_capture_closure = base::BindOnce(
          &InProcessVideoCaptureDeviceLauncher::DoStartTabCaptureOnDeviceThread,
          base::Unretained(this), device_id, params, std::move(receiver),
          std::move(after_start_capture_callback));
      break;

    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET: {
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

      if (desktop_id.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
        after_start_capture_callback = base::BindOnce(
            [](const DesktopMediaID& device_id, ReceiveDeviceCallback callback,
               std::unique_ptr<media::VideoCaptureDevice> device) {
              // Special case: Only call IncrementDesktopCaptureCounters()
              // for WebContents capture if it was started from a desktop
              // capture API.
              if (device) {
                IncrementDesktopCaptureCounters(device_id);
              }
              std::move(callback).Run(std::move(device));
            },
            desktop_id, std::move(after_start_capture_callback));
        start_capture_closure = base::BindOnce(
            &InProcessVideoCaptureDeviceLauncher::
                DoStartTabCaptureOnDeviceThread,
            base::Unretained(this), device_id, params, std::move(receiver),
            std::move(after_start_capture_callback));
        break;
      }

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
      if (desktop_id.window_id != DesktopMediaID::kNullId) {
        // For the other capturers, when a bug reports the type of capture it's
        // easy enough to determine which capturer was used, but it's a little
        // fuzzier with window capture.
        TRACE_EVENT_INSTANT0(
            TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
            "UsingVizFrameSinkCapturer", TRACE_EVENT_SCOPE_THREAD);
        start_capture_closure = base::BindOnce(
            &InProcessVideoCaptureDeviceLauncher::
                DoStartVizFrameSinkWindowCaptureOnDeviceThread,
            base::Unretained(this), desktop_id, params, std::move(receiver),
            std::move(after_start_capture_callback));
        break;
      }
#endif  // defined(USE_AURA) || BUILDFLAG(IS_MAC)

      // All cases other than tab capture or Aura desktop/window capture.
      TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                           "UsingDesktopCapturer", TRACE_EVENT_SCOPE_THREAD);
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

    default:
      NOTREACHED_IN_MIGRATION() << "unsupported stream type=" << stream_type;
      start_capture_closure =
          base::BindOnce(std::move(after_start_capture_callback), nullptr);
  }

  state_ = State::DEVICE_START_IN_PROGRESS;
  device_task_runner_->PostTask(FROM_HERE, std::move(start_capture_closure));
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

#if BUILDFLAG(IS_WIN)
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool =
      base::MakeRefCounted<media::VideoCaptureBufferPoolImpl>(
          requested_buffer_type, buffer_pool_max_buffer_count,
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(
              /*dxgi_device_manager=*/nullptr));
#else
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool =
      base::MakeRefCounted<media::VideoCaptureBufferPoolImpl>(
          requested_buffer_type, buffer_pool_max_buffer_count);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<media::VideoCaptureDeviceClient>(
      std::move(receiver), std::move(buffer_pool),
      base::BindRepeating(
          &CreateGpuJpegDecoder,
          base::BindRepeating(&media::VideoFrameReceiver::OnFrameReadyInBuffer,
                              receiver_on_io_thread),
          base::BindRepeating(&media::VideoFrameReceiver::OnLog,
                              receiver_on_io_thread)));
#else
  return std::make_unique<media::VideoCaptureDeviceClient>(
      std::move(receiver), std::move(buffer_pool),
      media::VideoEffectsContext({}));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
  }
}

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE)

void InProcessVideoCaptureDeviceLauncher::DoStartTabCaptureOnDeviceThread(
    const std::string& device_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver,
    ReceiveDeviceCallback result_callback) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<WebContentsVideoCaptureDevice> video_capture_device =
      WebContentsVideoCaptureDevice::Create(device_id);
  if (video_capture_device) {
    video_capture_device->AllocateAndStartWithReceiver(params,
                                                       std::move(receiver));
  }
  std::move(result_callback).Run(std::move(video_capture_device));
}

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
void InProcessVideoCaptureDeviceLauncher::
    DoStartVizFrameSinkWindowCaptureOnDeviceThread(
        const DesktopMediaID& device_id,
        const media::VideoCaptureParams& params,
        std::unique_ptr<media::VideoFrameReceiver> receiver,
        ReceiveDeviceCallback result_callback) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<FrameSinkVideoCaptureDevice> video_capture_device;
#if defined(USE_AURA)
  video_capture_device =
      std::make_unique<AuraWindowVideoCaptureDevice>(device_id);
#elif BUILDFLAG(IS_MAC)
  video_capture_device =
      std::make_unique<ViewsWidgetVideoCaptureDeviceMac>(device_id);
#endif
  if (video_capture_device) {
    video_capture_device->AllocateAndStartWithReceiver(params,
                                                       std::move(receiver));
    IncrementDesktopCaptureCounters(device_id);
  }
  std::move(result_callback).Run(std::move(video_capture_device));
}
#endif  // defined(USE_AURA) || BUILDFLAG(IS_MAC)

void InProcessVideoCaptureDeviceLauncher::DoStartDesktopCaptureOnDeviceThread(
    const DesktopMediaID& desktop_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());
  DCHECK(!desktop_id.is_null());

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device;
  DesktopCaptureImplementation implementation =
      CreatePlatformDependentVideoCaptureDevice(
          native_screen_capture_picker_, desktop_id, video_capture_device);
  DVLOG(1) << __func__ << " implementation " << implementation << " type "
           << desktop_id.type;
  ReportDesktopCaptureImplementationAndType(implementation, desktop_id.type);
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

  fake_device_factory_ =
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
    fake_device_factory_->SetToCustomDevicesConfig(config);
  }

  // base::Unretained() is safe because |this| owns |fake_device_factory_|.
  fake_device_factory_->GetDevicesInfo(base::BindOnce(
      &InProcessVideoCaptureDeviceLauncher::OnFakeDevicesEnumerated,
      base::Unretained(this), params, std::move(device_client),
      std::move(result_callback)));
}

void InProcessVideoCaptureDeviceLauncher::OnFakeDevicesEnumerated(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback,
    std::vector<media::VideoCaptureDeviceInfo> devices_info) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  if (devices_info.empty()) {
    LOG(ERROR) << "Cannot start with no fake device config";
    std::move(result_callback).Run(nullptr);
    return;
  }
  auto video_capture_device =
      fake_device_factory_->CreateDevice(devices_info.front().descriptor)
          .ReleaseDevice();
  video_capture_device->AllocateAndStart(params, std::move(device_client));
  std::move(result_callback).Run(std::move(video_capture_device));
}

}  // namespace content
