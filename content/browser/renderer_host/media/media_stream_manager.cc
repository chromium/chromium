// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cctype>
#include <list>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/video_capture_dependencies.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_service_listener.h"
#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_capture_devices_impl.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/service_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/media/video_capture_provider_switcher.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "crypto/hmac.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "media/mojo/interfaces/display_media_information.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#endif

namespace content {

base::LazyInstance<base::ThreadLocalPointer<MediaStreamManager>>::Leaky
    g_media_stream_manager_tls_ptr = LAZY_INSTANCE_INITIALIZER;

namespace {
// Creates a random label used to identify requests.
std::string RandomLabel() {
  // An earlier PeerConnection spec [1] defined MediaStream::label alphabet as
  // an uuid with characters from range: U+0021, U+0023 to U+0027, U+002A to
  // U+002B, U+002D to U+002E, U+0030 to U+0039, U+0041 to U+005A, U+005E to
  // U+007E. That causes problems with searching for labels in bots, so we use a
  // safe alphanumeric subset |kAlphabet|.
  // [1] http://dev.w3.org/2011/webrtc/editor/webrtc.html
  static const char kAlphabet[] =
      "0123456789"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  static const size_t kRfc4122LengthLabel = 36u;
  std::string label(kRfc4122LengthLabel, ' ');
  for (char& c : label) {
    // Use |arraysize(kAlphabet) - 1| to avoid |kAlphabet|s terminating '\0';
    c = kAlphabet[base::RandGenerator(arraysize(kAlphabet) - 1)];
    DCHECK(std::isalnum(c)) << c;
  }
  return label;
}

// Turns off available audio effects (removes the flag) if the options
// explicitly turn them off.
void FilterAudioEffects(const StreamControls& controls, int* effects) {
  DCHECK(effects);
  // TODO(ajm): Should we handle ECHO_CANCELLER here?
}

// Unlike other effects, hotword is off by default, so turn it on if it's
// requested and available.
void EnableHotwordEffect(const StreamControls& controls, int* effects) {
  DCHECK(effects);
  if (controls.hotword_enabled) {
#if defined(OS_CHROMEOS)
    chromeos::AudioDeviceList devices;
    chromeos::CrasAudioHandler::Get()->GetAudioDevices(&devices);
    // Only enable if a hotword device exists.
    for (const chromeos::AudioDevice& device : devices) {
      if (device.type == chromeos::AUDIO_TYPE_HOTWORD) {
        DCHECK(device.is_input);
        *effects |= media::AudioParameters::HOTWORD;
      }
    }
#endif
  }
}

bool GetDeviceIDFromHMAC(const std::string& salt,
                         const url::Origin& security_origin,
                         const std::string& hmac_device_id,
                         const MediaDeviceInfoArray& devices,
                         std::string* device_id) {
  // The source_id can be empty if the constraint is set but empty.
  if (hmac_device_id.empty())
    return false;

  for (const auto& device_info : devices) {
    if (MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
            salt, security_origin, hmac_device_id, device_info.device_id)) {
      *device_id = device_info.device_id;
      return true;
    }
  }
  return false;
}

MediaStreamType ConvertToMediaStreamType(MediaDeviceType type) {
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return MEDIA_DEVICE_AUDIO_CAPTURE;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return MEDIA_DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
  }

  return MEDIA_NO_SERVICE;
}

MediaDeviceType ConvertToMediaDeviceType(MediaStreamType stream_type) {
  switch (stream_type) {
    case MEDIA_DEVICE_AUDIO_CAPTURE:
      return MEDIA_DEVICE_TYPE_AUDIO_INPUT;
    case MEDIA_DEVICE_VIDEO_CAPTURE:
      return MEDIA_DEVICE_TYPE_VIDEO_INPUT;
    default:
      NOTREACHED();
  }

  return NUM_MEDIA_DEVICE_TYPES;
}

void SendVideoCaptureLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("video capture: " + message);
}

MediaStreamType AdjustAudioStreamTypeBasedOnCommandLineSwitches(
    MediaStreamType stream_type) {
  if (stream_type != MEDIA_GUM_DESKTOP_AUDIO_CAPTURE)
    return stream_type;
  const bool audio_support_flag_for_desktop_share =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioSupportForDesktopShare);
  return audio_support_flag_for_desktop_share ? MEDIA_GUM_DESKTOP_AUDIO_CAPTURE
                                              : MEDIA_NO_SERVICE;
}

// Returns MediaStreamDevice built with DesktopMediaID with fake
// initializers if |kUseFakeDeviceForMediaStream| is set. Returns a
// MediaStreamDevice with default DesktopMediaID otherwise.
MediaStreamDevice MediaStreamDeviceFromFakeDeviceConfig() {
  // TODO(emircan): When getDisplayMedia() accepts constraints, pick
  // the corresponding type.
  DesktopMediaID media_id(DesktopMediaID::TYPE_SCREEN, DesktopMediaID::kNullId);

  MediaStreamDevice device(MEDIA_DISPLAY_VIDEO_CAPTURE, media_id.ToString(),
                           media_id.ToString());
  media::mojom::DisplayCaptureSurfaceType display_surface =
      media::mojom::DisplayCaptureSurfaceType::MONITOR;
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface, true, media::mojom::CursorCaptureType::NEVER);

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
    if (config.empty())
      return device;

    DesktopMediaID::Type desktop_media_type = DesktopMediaID::TYPE_NONE;
    switch (config[0].display_media_type) {
      case media::FakeVideoCaptureDevice::DisplayMediaType::ANY:
      case media::FakeVideoCaptureDevice::DisplayMediaType::MONITOR:
        desktop_media_type = DesktopMediaID::TYPE_SCREEN;
        display_surface = media::mojom::DisplayCaptureSurfaceType::MONITOR;
        break;
      case media::FakeVideoCaptureDevice::DisplayMediaType::WINDOW:
        desktop_media_type = DesktopMediaID::TYPE_WINDOW;
        display_surface = media::mojom::DisplayCaptureSurfaceType::WINDOW;
        break;
      case media::FakeVideoCaptureDevice::DisplayMediaType::BROWSER:
        desktop_media_type = DesktopMediaID::TYPE_WEB_CONTENTS;
        display_surface = media::mojom::DisplayCaptureSurfaceType::BROWSER;
        break;
    }
    media_id = DesktopMediaID(desktop_media_type, DesktopMediaID::kFakeId);
  }
  device = MediaStreamDevice(MEDIA_DISPLAY_VIDEO_CAPTURE, media_id.ToString(),
                             media_id.ToString());
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface, true, media::mojom::CursorCaptureType::NEVER);
  return device;
}

}  // namespace

// MediaStreamManager::DeviceRequest represents a request to either enumerate
// available devices or open one or more devices.
// TODO(perkj): MediaStreamManager still needs refactoring. I propose we create
// several subclasses of DeviceRequest and move some of the responsibility of
// the MediaStreamManager to the subclasses to get rid of the way too many if
// statements in MediaStreamManager.
class MediaStreamManager::DeviceRequest {
 public:
  DeviceRequest(
      int requesting_process_id,
      int requesting_frame_id,
      int page_request_id,
      bool user_gesture,
      MediaStreamRequestType request_type,
      const StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      DeviceStoppedCallback device_stopped_cb = DeviceStoppedCallback())
      : requesting_process_id(requesting_process_id),
        requesting_frame_id(requesting_frame_id),
        page_request_id(page_request_id),
        user_gesture(user_gesture),
        request_type(request_type),
        controls(controls),
        salt_and_origin(std::move(salt_and_origin)),
        device_stopped_cb(std::move(device_stopped_cb)),
        state_(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_NOT_REQUESTED),
        audio_type_(MEDIA_NO_SERVICE),
        video_type_(MEDIA_NO_SERVICE),
        target_process_id_(-1),
        target_frame_id_(-1) {}

  ~DeviceRequest() { RunMojoCallbacks(); }

  void SetAudioType(MediaStreamType audio_type) {
    DCHECK(IsAudioInputMediaType(audio_type) || audio_type == MEDIA_NO_SERVICE);
    audio_type_ = audio_type;
  }

  MediaStreamType audio_type() const { return audio_type_; }

  void SetVideoType(MediaStreamType video_type) {
    DCHECK(IsVideoInputMediaType(video_type) || video_type == MEDIA_NO_SERVICE);
    video_type_ = video_type;
  }

  MediaStreamType video_type() const { return video_type_; }

  // Creates a MediaStreamRequest object that is used by this request when UI
  // is asked for permission and device selection.
  void CreateUIRequest(const std::string& requested_audio_device_id,
                       const std::string& requested_video_device_id) {
    DCHECK(!ui_request_);
    target_process_id_ = requesting_process_id;
    target_frame_id_ = requesting_frame_id;
    ui_request_.reset(new MediaStreamRequest(
        requesting_process_id, requesting_frame_id, page_request_id,
        salt_and_origin.origin.GetURL(), user_gesture, request_type,
        requested_audio_device_id, requested_video_device_id, audio_type_,
        video_type_, controls.disable_local_echo));
  }

  // Creates a tab capture specific MediaStreamRequest object that is used by
  // this request when UI is asked for permission and device selection.
  void CreateTabCaptureUIRequest(int target_render_process_id,
                                 int target_render_frame_id) {
    DCHECK(!ui_request_);
    target_process_id_ = target_render_process_id;
    target_frame_id_ = target_render_frame_id;
    ui_request_.reset(new MediaStreamRequest(
        target_render_process_id, target_render_frame_id, page_request_id,
        salt_and_origin.origin.GetURL(), user_gesture, request_type, "", "",
        audio_type_, video_type_, controls.disable_local_echo));
  }

  bool HasUIRequest() const { return ui_request_.get() != nullptr; }
  std::unique_ptr<MediaStreamRequest> DetachUIRequest() {
    return std::move(ui_request_);
  }

  // Update the request state and notify observers.
  void SetState(MediaStreamType stream_type, MediaRequestState new_state) {
    if (stream_type == NUM_MEDIA_TYPES) {
      for (int i = MEDIA_NO_SERVICE + 1; i < NUM_MEDIA_TYPES; ++i) {
        state_[static_cast<MediaStreamType>(i)] = new_state;
      }
    } else {
      state_[stream_type] = new_state;
    }

    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer)
      return;

    if (stream_type == NUM_MEDIA_TYPES) {
      for (int i = MEDIA_NO_SERVICE + 1; i < NUM_MEDIA_TYPES; ++i) {
        media_observer->OnMediaRequestStateChanged(
            target_process_id_, target_frame_id_, page_request_id,
            salt_and_origin.origin.GetURL(), static_cast<MediaStreamType>(i),
            new_state);
      }
    } else {
      media_observer->OnMediaRequestStateChanged(
          target_process_id_, target_frame_id_, page_request_id,
          salt_and_origin.origin.GetURL(), stream_type, new_state);
    }
  }

  MediaRequestState state(MediaStreamType stream_type) const {
    return state_[stream_type];
  }

  void SetCapturingLinkSecured(bool is_secure) {
    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer)
      return;

    media_observer->OnSetCapturingLinkSecured(target_process_id_,
                                              target_frame_id_, page_request_id,
                                              video_type_, is_secure);
  }

  void RunMojoCallbacks() {
    if (generate_stream_cb) {
      std::move(generate_stream_cb)
          .Run(MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN, std::string(),
               MediaStreamDevices(), MediaStreamDevices());
    }

    if (open_device_cb) {
      std::move(open_device_cb)
          .Run(false /* success */, std::string(), MediaStreamDevice());
    }
  }

  // The render process id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_process_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_process_id;

  // The render frame id that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::render_frame_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requesting_frame_id;

  // An ID the render frame provided to identify this request.
  const int page_request_id;

  const bool user_gesture;

  const MediaStreamRequestType request_type;

  const StreamControls controls;

  const MediaDeviceSaltAndOrigin salt_and_origin;

  MediaStreamDevices devices;

  // Callback to the requester which audio/video devices have been selected.
  // It can be null if the requester has no interest to know the result.
  // Currently it is only used by |DEVICE_ACCESS| type.
  MediaAccessRequestCallback media_access_request_cb;

  GenerateStreamCallback generate_stream_cb;

  OpenDeviceCallback open_device_cb;

  DeviceStoppedCallback device_stopped_cb;

  std::unique_ptr<MediaStreamUIProxy> ui_proxy;

  std::string tab_capture_device_id;

 private:
  std::vector<MediaRequestState> state_;
  std::unique_ptr<MediaStreamRequest> ui_request_;
  MediaStreamType audio_type_;
  MediaStreamType video_type_;
  int target_process_id_;
  int target_frame_id_;
};

// static
void MediaStreamManager::SendMessageToNativeLog(const std::string& message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MediaStreamManager::SendMessageToNativeLog, message));
    return;
  }

  MediaStreamManager* msm = g_media_stream_manager_tls_ptr.Pointer()->Get();
  if (!msm) {
    // MediaStreamManager hasn't been initialized. This is allowed in tests.
    return;
  }

  msm->AddLogMessageOnIOThread(message);
}

MediaStreamManager::MediaStreamManager(
    media::AudioSystem* audio_system,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner)
    : MediaStreamManager(audio_system, std::move(audio_task_runner), nullptr) {}

MediaStreamManager::MediaStreamManager(
    media::AudioSystem* audio_system,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    std::unique_ptr<VideoCaptureProvider> video_capture_provider)
    : audio_system_(audio_system),
      fake_ui_factory_() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    fake_ui_factory_ = base::Bind([] {
      return std::make_unique<FakeMediaStreamUIProxy>(
          /*tests_use_fake_render_frame_hosts=*/false);
    });
  }

  DCHECK(audio_system_);

  if (!video_capture_provider) {
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner =
#if defined(OS_WIN)
        // Windows unconditionally requires its own thread (see below).
        nullptr;
#else
        // Share the provided |audio_task_runner| if it's non-null.
        std::move(audio_task_runner);
#endif

    if (!device_task_runner) {
      video_capture_thread_.emplace("VideoCaptureThread");
#if defined(OS_WIN)
      // Use an STA Video Capture Thread to try to avoid crashes on enumeration
      // of buggy third party Direct Show modules, http://crbug.com/428958.
      video_capture_thread_->init_com_with_mta(false);
#endif
      CHECK(video_capture_thread_->Start());
      device_task_runner = video_capture_thread_->task_runner();
    }

#if defined(OS_CHROMEOS)
    if (media::ShouldUseCrosCameraService()) {
      media::VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
          GpuMemoryBufferManagerSingleton::GetInstance());
      media::CameraHalDispatcherImpl::GetInstance()->Start(
          base::BindRepeating(
              &VideoCaptureDependencies::CreateJpegDecodeAccelerator),
          base::BindRepeating(
              &VideoCaptureDependencies::CreateJpegEncodeAccelerator));
    }
#endif

    if (base::FeatureList::IsEnabled(features::kMojoVideoCapture)) {
      video_capture_provider = std::make_unique<VideoCaptureProviderSwitcher>(
          std::make_unique<ServiceVideoCaptureProvider>(
              base::BindRepeating(&SendVideoCaptureLogMessage)),
          InProcessVideoCaptureProvider::CreateInstanceForNonDeviceCapture(
              std::move(device_task_runner),
              base::BindRepeating(&SendVideoCaptureLogMessage)));
    } else {
      video_capture::uma::LogVideoCaptureServiceEvent(
          video_capture::uma::BROWSER_USING_LEGACY_CAPTURE);
      video_capture_provider = InProcessVideoCaptureProvider::CreateInstance(
          std::make_unique<media::VideoCaptureSystemImpl>(
              media::CreateVideoCaptureDeviceFactory(
                  base::CreateSingleThreadTaskRunnerWithTraits(
                      {BrowserThread::UI}))),
          std::move(device_task_runner),
          base::BindRepeating(&SendVideoCaptureLogMessage));
    }
  }
  InitializeMaybeAsync(std::move(video_capture_provider));

  // May be null in tests.
  if (ServiceManagerConnection::GetForProcess()) {
    audio_service_listener_ = std::make_unique<AudioServiceListener>(
        ServiceManagerConnection::GetForProcess()->GetConnector()->Clone());
  }

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  // BrowserMainLoop always creates the PowerMonitor instance before creating
  // MediaStreamManager, but power_monitor may be NULL in unit tests.
  if (power_monitor)
    power_monitor->AddObserver(this);
}

MediaStreamManager::~MediaStreamManager() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO));
  DVLOG(1) << "~MediaStreamManager";
  DCHECK(requests_.empty());

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  // The PowerMonitor instance owned by BrowserMainLoops always outlives the
  // MediaStreamManager, but it may be NULL in unit tests.
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

VideoCaptureManager* MediaStreamManager::video_capture_manager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(video_capture_manager_.get());
  return video_capture_manager_.get();
}

AudioInputDeviceManager* MediaStreamManager::audio_input_device_manager() {
  // May be called on any thread, provided that we are not in shutdown.
  DCHECK(audio_input_device_manager_.get());
  return audio_input_device_manager_.get();
}

AudioServiceListener* MediaStreamManager::audio_service_listener() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_service_listener_.get();
}

MediaDevicesManager* MediaStreamManager::media_devices_manager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // nullptr might be returned during shutdown.
  return media_devices_manager_.get();
}

media::AudioSystem* MediaStreamManager::audio_system() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return audio_system_;
}

void MediaStreamManager::AddVideoCaptureObserver(
    media::VideoCaptureObserver* capture_observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (video_capture_manager_) {
    video_capture_manager_->AddVideoCaptureObserver(capture_observer);
  }
}

void MediaStreamManager::RemoveAllVideoCaptureObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (video_capture_manager_) {
    video_capture_manager_->RemoveAllVideoCaptureObservers();
  }
}

std::string MediaStreamManager::MakeMediaAccessRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const StreamControls& controls,
    const url::Origin& security_origin,
    MediaAccessRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = new DeviceRequest(
      render_process_id, render_frame_id, page_request_id,
      false /* user gesture */, MEDIA_DEVICE_ACCESS, controls,
      MediaDeviceSaltAndOrigin{std::string() /* salt */,
                               std::string() /* group_id_salt */,
                               security_origin});

  const std::string& label = AddRequest(request);

  request->media_access_request_cb = std::move(callback);
  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&MediaStreamManager::SetUpRequest,
                                          base::Unretained(this), label));
  return label;
}

void MediaStreamManager::GenerateStream(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const StreamControls& controls,
    MediaDeviceSaltAndOrigin salt_and_origin,
    bool user_gesture,
    GenerateStreamCallback generate_stream_cb,
    DeviceStoppedCallback device_stopped_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "GenerateStream()";

  DeviceRequest* request = new DeviceRequest(
      render_process_id, render_frame_id, page_request_id, user_gesture,
      MEDIA_GENERATE_STREAM, controls, std::move(salt_and_origin),
      std::move(device_stopped_cb));

  const std::string& label = AddRequest(request);

  request->generate_stream_cb = std::move(generate_stream_cb);

  if (generate_stream_test_callback_) {
    // The test callback is responsible to verify whether the |controls| is
    // as expected. Then we need to finish getUserMedia and let Javascript
    // access the result.
    if (std::move(generate_stream_test_callback_).Run(controls)) {
      FinalizeGenerateStream(label, request);
    } else {
      FinalizeRequestFailed(label, request, MEDIA_DEVICE_INVALID_STATE);
    }
    return;
  }

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&MediaStreamManager::SetUpRequest,
                                          base::Unretained(this), label));
}

void MediaStreamManager::CancelRequest(int render_process_id,
                                       int render_frame_id,
                                       int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second;
    if (request->requesting_process_id == render_process_id &&
        request->requesting_frame_id == render_frame_id &&
        request->page_request_id == page_request_id) {
      CancelRequest(labeled_request.first);
      return;
    }
  }
}

void MediaStreamManager::CancelRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "CancelRequest({label = " << label << "})";
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    // The request does not exist.
    LOG(ERROR) << "The request with label = " << label << " does not exist.";
    return;
  }

  // This is a request for opening one or more devices.
  for (const MediaStreamDevice& device : request->devices) {
    const MediaRequestState state = request->state(device.type);
    // If we have not yet requested the device to be opened - just ignore it.
    if (state != MEDIA_REQUEST_STATE_OPENING &&
        state != MEDIA_REQUEST_STATE_DONE) {
      continue;
    }
    // Stop the opening/opened devices of the requests.
    CloseDevice(device.type, device.session_id);
  }

  // Cancel the request if still pending at UI side.
  request->SetState(NUM_MEDIA_TYPES, MEDIA_REQUEST_STATE_CLOSING);
  DeleteRequest(label);
}

void MediaStreamManager::CancelAllRequests(int render_process_id,
                                           int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto request_it = requests_.begin();
  while (request_it != requests_.end()) {
    if (request_it->second->requesting_process_id != render_process_id ||
        request_it->second->requesting_frame_id != render_frame_id) {
      ++request_it;
      continue;
    }
    const std::string label = request_it->first;
    ++request_it;
    CancelRequest(label);
  }
}

void MediaStreamManager::StopStreamDevice(int render_process_id,
                                          int render_frame_id,
                                          const std::string& device_id,
                                          int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "StopStreamDevice({render_frame_id = " << render_frame_id << "} "
           << ", {device_id = " << device_id << "}, session_id = " << session_id
           << "})";
  // Find the first request for this |render_process_id| and |render_frame_id|
  // of type MEDIA_GENERATE_STREAM that has requested to use |device_id| and
  // stop it.
  for (const LabeledDeviceRequest& device_request : requests_) {
    DeviceRequest* const request = device_request.second;
    if (request->requesting_process_id != render_process_id ||
        request->requesting_frame_id != render_frame_id ||
        request->request_type != MEDIA_GENERATE_STREAM) {
      continue;
    }

    for (const MediaStreamDevice& device : request->devices) {
      if (device.id == device_id && device.session_id == session_id) {
        StopDevice(device.type, device.session_id);
        return;
      }
    }
  }
}

int MediaStreamManager::VideoDeviceIdToSessionId(
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const LabeledDeviceRequest& device_request : requests_) {
    for (const MediaStreamDevice& device : device_request.second->devices) {
      if (device.id == device_id && device.type == MEDIA_DEVICE_VIDEO_CAPTURE) {
        return device.session_id;
      }
    }
  }
  return MediaStreamDevice::kNoId;
}

void MediaStreamManager::StopDevice(MediaStreamType type, int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "StopDevice"
           << "{type = " << type << "}"
           << "{session_id = " << session_id << "}";
  auto request_it = requests_.begin();
  while (request_it != requests_.end()) {
    DeviceRequest* request = request_it->second;
    MediaStreamDevices* devices = &request->devices;
    if (devices->empty()) {
      // There is no device in use yet by this request.
      ++request_it;
      continue;
    }
    auto device_it = devices->begin();
    while (device_it != devices->end()) {
      if (device_it->type != type || device_it->session_id != session_id) {
        ++device_it;
        continue;
      }

      if (request->state(type) == MEDIA_REQUEST_STATE_DONE)
        CloseDevice(type, session_id);
      device_it = devices->erase(device_it);
    }

    // If this request doesn't have any active devices after a device
    // has been stopped above, remove the request. Note that the request is
    // only deleted if a device as been removed from |devices|.
    if (devices->empty()) {
      std::string label = request_it->first;
      ++request_it;
      DeleteRequest(label);
    } else {
      ++request_it;
    }
  }
}

void MediaStreamManager::CloseDevice(MediaStreamType type, int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "CloseDevice("
           << "{type = " << type << "} "
           << "{session_id = " << session_id << "})";
  GetDeviceManager(type)->Close(session_id);

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second;
    for (const MediaStreamDevice& device : request->devices) {
      if (device.session_id == session_id && device.type == type) {
        // Notify observers that this device is being closed.
        // Note that only one device per type can be opened.
        request->SetState(type, MEDIA_REQUEST_STATE_CLOSING);
      }
    }
  }
}

void MediaStreamManager::OpenDevice(int render_process_id,
                                    int render_frame_id,
                                    int page_request_id,
                                    const std::string& device_id,
                                    MediaStreamType type,
                                    MediaDeviceSaltAndOrigin salt_and_origin,
                                    OpenDeviceCallback open_device_cb,
                                    DeviceStoppedCallback device_stopped_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);
  DVLOG(1) << "OpenDevice ({page_request_id = " << page_request_id << "})";
  StreamControls controls;
  if (IsAudioInputMediaType(type)) {
    controls.audio.requested = true;
    controls.audio.stream_type = type;
    controls.audio.device_id = device_id;
  } else if (IsVideoInputMediaType(type)) {
    controls.video.requested = true;
    controls.video.stream_type = type;
    controls.video.device_id = device_id;
  } else {
    NOTREACHED();
  }
  DeviceRequest* request = new DeviceRequest(
      render_process_id, render_frame_id, page_request_id,
      false /* user gesture */, MEDIA_OPEN_DEVICE_PEPPER_ONLY, controls,
      std::move(salt_and_origin), std::move(device_stopped_cb));

  const std::string& label = AddRequest(request);

  request->open_device_cb = std::move(open_device_cb);
  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&MediaStreamManager::SetUpRequest,
                                          base::Unretained(this), label));
}

bool MediaStreamManager::TranslateSourceIdToDeviceId(
    MediaStreamType stream_type,
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& source_id,
    std::string* device_id) const {
  DCHECK(stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         stream_type == MEDIA_DEVICE_VIDEO_CAPTURE);
  // The source_id can be empty if the constraint is set but empty.
  if (source_id.empty())
    return false;

  // TODO(guidou): Change to use MediaDevicesManager::EnumerateDevices.
  // See http://crbug.com/648155.
  MediaDeviceInfoArray cached_devices =
      media_devices_manager_->GetCachedDeviceInfo(
          ConvertToMediaDeviceType(stream_type));
  return GetDeviceIDFromHMAC(salt, security_origin, source_id, cached_devices,
                             device_id);
}

void MediaStreamManager::EnsureDeviceMonitorStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media_devices_manager_->StartMonitoring();
}

void MediaStreamManager::StopRemovedDevice(
    MediaDeviceType type,
    const MediaDeviceInfo& media_device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
         type == MEDIA_DEVICE_TYPE_VIDEO_INPUT);

  MediaStreamType stream_type = ConvertToMediaStreamType(type);
  std::vector<int> session_ids;
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second;
    for (const MediaStreamDevice& device : request->devices) {
      const std::string source_id = GetHMACForMediaDeviceID(
          request->salt_and_origin.device_id_salt,
          request->salt_and_origin.origin, media_device_info.device_id);
      if (device.id == source_id && device.type == stream_type) {
        session_ids.push_back(device.session_id);
        if (request->device_stopped_cb) {
          request->device_stopped_cb.Run(labeled_request.first, device);
        }
      }
    }
  }
  for (const int session_id : session_ids)
    StopDevice(stream_type, session_id);

  AddLogMessageOnIOThread(
      base::StringPrintf(
          "Media input device removed: type=%s, id=%s, name=%s ",
          (stream_type == MEDIA_DEVICE_AUDIO_CAPTURE ? "audio" : "video"),
          media_device_info.device_id.c_str(), media_device_info.label.c_str())
          .c_str());
}

bool MediaStreamManager::PickDeviceId(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const TrackControls& controls,
    const MediaDeviceInfoArray& devices,
    std::string* device_id) const {
  if (controls.device_id.empty())
    return true;

  if (!GetDeviceIDFromHMAC(salt_and_origin.device_id_salt,
                           salt_and_origin.origin, controls.device_id, devices,
                           device_id)) {
    LOG(WARNING) << "Invalid device ID = " << controls.device_id;
    return false;
  }
  return true;
}

bool MediaStreamManager::GetRequestedDeviceCaptureId(
    const DeviceRequest* request,
    MediaStreamType type,
    const MediaDeviceInfoArray& devices,
    std::string* device_id) const {
  if (type == MEDIA_DEVICE_AUDIO_CAPTURE) {
    return PickDeviceId(request->salt_and_origin, request->controls.audio,
                        devices, device_id);
  } else if (type == MEDIA_DEVICE_VIDEO_CAPTURE) {
    return PickDeviceId(request->salt_and_origin, request->controls.video,
                        devices, device_id);
  } else {
    NOTREACHED();
  }
  return false;
}

void MediaStreamManager::TranslateDeviceIdToSourceId(
    DeviceRequest* request,
    MediaStreamDevice* device) {
  if (request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
      request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE) {
    device->id =
        GetHMACForMediaDeviceID(request->salt_and_origin.device_id_salt,
                                request->salt_and_origin.origin, device->id);
    if (device->group_id) {
      device->group_id = GetHMACForMediaDeviceID(
          request->salt_and_origin.group_id_salt,
          request->salt_and_origin.origin, *device->group_id);
    }
  }
}

void MediaStreamManager::StartEnumeration(DeviceRequest* request,
                                          const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Start monitoring the devices when doing the first enumeration.
  media_devices_manager_->StartMonitoring();

  // Start enumeration for devices of all requested device types.
  bool request_audio_input = request->audio_type() != MEDIA_NO_SERVICE;
  if (request_audio_input)
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_REQUESTED);

  bool request_video_input = request->video_type() != MEDIA_NO_SERVICE;
  if (request_video_input)
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_REQUESTED);

  // base::Unretained is safe here because MediaStreamManager is deleted on the
  // UI thread, after the IO thread has been stopped.
  DCHECK(request_audio_input || request_video_input);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = request_audio_input;
  devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = request_video_input;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaStreamManager::DevicesEnumerated,
                     base::Unretained(this), request_audio_input,
                     request_video_input, label));
}

std::string MediaStreamManager::AddRequest(DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create a label for this request and verify it is unique.
  std::string unique_label;
  do {
    unique_label = RandomLabel();
  } while (FindRequest(unique_label) != nullptr);

  requests_.push_back(std::make_pair(unique_label, request));

  return unique_label;
}

MediaStreamManager::DeviceRequest* MediaStreamManager::FindRequest(
    const std::string& label) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    if (labeled_request.first == label)
      return labeled_request.second;
  }
  return nullptr;
}

void MediaStreamManager::DeleteRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "DeleteRequest({label= " << label << "})";
  for (auto request_it = requests_.begin(); request_it != requests_.end();
       ++request_it) {
    if (request_it->first == label) {
      std::unique_ptr<DeviceRequest> request(request_it->second);
      requests_.erase(request_it);
      return;
    }
  }
  NOTREACHED();
}

void MediaStreamManager::ReadOutputParamsAndPostRequestToUI(
    const std::string& label,
    DeviceRequest* request,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Actual audio parameters are required only for
  // MEDIA_GUM_TAB_AUDIO_CAPTURE.
  // TODO(guidou): MEDIA_GUM_TAB_AUDIO_CAPTURE should not be a special
  // case. See https://crbug.com/584287.
  if (request->audio_type() == MEDIA_GUM_TAB_AUDIO_CAPTURE) {
    // Using base::Unretained is safe: |audio_system_| will post
    // PostRequestToUI() to IO thread, and MediaStreamManager is deleted on the
    // UI thread, after the IO thread has been stopped.
    audio_system_->GetOutputStreamParameters(
        media::AudioDeviceDescription::kDefaultDeviceId,
        base::BindOnce(&MediaStreamManager::PostRequestToUI,
                       base::Unretained(this), label, enumeration));
  } else {
    PostRequestToUI(label, enumeration,
                    base::Optional<media::AudioParameters>());
  }
}

void MediaStreamManager::PostRequestToUI(
    const std::string& label,
    const MediaDeviceEnumeration& enumeration,
    const base::Optional<media::AudioParameters>& output_parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!output_parameters || output_parameters->IsValid());
  DVLOG(1) << "PostRequestToUI({label= " << label << "})";

  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;
  DCHECK(request->HasUIRequest());

  const MediaStreamType audio_type = request->audio_type();
  const MediaStreamType video_type = request->video_type();

  // Post the request to UI and set the state.
  if (IsAudioInputMediaType(audio_type))
    request->SetState(audio_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  if (IsVideoInputMediaType(video_type))
    request->SetState(video_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);

  // If using the fake UI, it will just auto-select from the available devices.
  // The fake UI doesn't work for desktop sharing requests since we can't see
  // its devices from here; always use the real UI for such requests.
  if (fake_ui_factory_ &&
      request->video_type() != MEDIA_GUM_DESKTOP_VIDEO_CAPTURE) {
    MediaStreamDevices devices = ConvertToMediaStreamDevices(
        request->audio_type(), enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT]);
    MediaStreamDevices video_devices = ConvertToMediaStreamDevices(
        request->video_type(), enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT]);
    devices.reserve(devices.size() + video_devices.size());
    devices.insert(devices.end(), video_devices.begin(), video_devices.end());

    if (request->video_type() == MEDIA_DISPLAY_VIDEO_CAPTURE) {
      DCHECK(devices.empty());
      devices.push_back(MediaStreamDeviceFromFakeDeviceConfig());
    }
    std::unique_ptr<FakeMediaStreamUIProxy> fake_ui = fake_ui_factory_.Run();
    fake_ui->SetAvailableDevices(devices);

    request->ui_proxy = std::move(fake_ui);
  } else {
    request->ui_proxy = MediaStreamUIProxy::Create();
  }

  request->ui_proxy->RequestAccess(
      request->DetachUIRequest(),
      base::BindOnce(&MediaStreamManager::HandleAccessRequestResponse,
                     base::Unretained(this), label,
                     output_parameters.value_or(
                         media::AudioParameters::UnavailableDeviceParams())));
}

void MediaStreamManager::SetUpRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    DVLOG(1) << "SetUpRequest label " << label << " doesn't exist!!";
    return;  // This can happen if the request has been canceled.
  }

  request->SetAudioType(AdjustAudioStreamTypeBasedOnCommandLineSwitches(
      request->controls.audio.stream_type));
  request->SetVideoType(request->controls.video.stream_type);

  const bool is_display_capture =
      request->video_type() == MEDIA_DISPLAY_VIDEO_CAPTURE;
  if (is_display_capture && !SetUpDisplayCaptureRequest(request)) {
    FinalizeRequestFailed(label, request, MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE);
    return;
  }

  const bool is_tab_capture =
      request->audio_type() == MEDIA_GUM_TAB_AUDIO_CAPTURE ||
      request->video_type() == MEDIA_GUM_TAB_VIDEO_CAPTURE;
  if (is_tab_capture) {
    if (!SetUpTabCaptureRequest(request, label)) {
      FinalizeRequestFailed(label, request, MEDIA_DEVICE_TAB_CAPTURE_FAILURE);
    }
    return;
  }

  const bool is_screen_capture =
      request->video_type() == MEDIA_GUM_DESKTOP_VIDEO_CAPTURE;
  if (is_screen_capture && !SetUpScreenCaptureRequest(request)) {
    FinalizeRequestFailed(label, request, MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE);
    return;
  }

  if (!is_tab_capture && !is_screen_capture && !is_display_capture) {
    if (IsDeviceMediaType(request->audio_type()) ||
        IsDeviceMediaType(request->video_type())) {
      StartEnumeration(request, label);
      return;
    }
    // If no actual device capture is requested, set up the request with an
    // empty device list.
    if (!SetUpDeviceCaptureRequest(request, MediaDeviceEnumeration())) {
      FinalizeRequestFailed(label, request, MEDIA_DEVICE_NO_HARDWARE);
      return;
    }
  }
  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
}

bool MediaStreamManager::SetUpDisplayCaptureRequest(DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->video_type() == MEDIA_DISPLAY_VIDEO_CAPTURE);

  // getDisplayMedia function does not permit the use of constraints for
  // selection of a source, see
  // https://w3c.github.io/mediacapture-screen-share/#constraints.
  if (!request->controls.video.requested ||
      !request->controls.video.device_id.empty()) {
    LOG(ERROR) << "Invalid display media request.";
    return false;
  }

  request->CreateUIRequest(std::string() /* requested_audio_device_id */,
                           std::string() /* requested_video_device_id */);
  DVLOG(3) << "Audio requested " << request->controls.audio.requested
           << " Video requested " << request->controls.video.requested;
  return true;
}

bool MediaStreamManager::SetUpDeviceCaptureRequest(
    DeviceRequest* request,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK((request->audio_type() == MEDIA_DEVICE_AUDIO_CAPTURE ||
          request->audio_type() == MEDIA_NO_SERVICE) &&
         (request->video_type() == MEDIA_DEVICE_VIDEO_CAPTURE ||
          request->video_type() == MEDIA_NO_SERVICE));
  std::string audio_device_id;
  if (request->controls.audio.requested &&
      !GetRequestedDeviceCaptureId(request, request->audio_type(),
                                   enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT],
                                   &audio_device_id)) {
    return false;
  }

  std::string video_device_id;
  if (request->controls.video.requested &&
      !GetRequestedDeviceCaptureId(request, request->video_type(),
                                   enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT],
                                   &video_device_id)) {
    return false;
  }
  request->CreateUIRequest(audio_device_id, video_device_id);
  DVLOG(3) << "Audio requested " << request->controls.audio.requested
           << " device id = " << audio_device_id << "Video requested "
           << request->controls.video.requested
           << " device id = " << video_device_id;
  return true;
}

bool MediaStreamManager::SetUpTabCaptureRequest(DeviceRequest* request,
                                                const std::string& label) {
  DCHECK(request->audio_type() == MEDIA_GUM_TAB_AUDIO_CAPTURE ||
         request->video_type() == MEDIA_GUM_TAB_VIDEO_CAPTURE);

  std::string capture_device_id;
  if (!request->controls.audio.device_id.empty()) {
    capture_device_id = request->controls.audio.device_id;
  } else if (!request->controls.video.device_id.empty()) {
    capture_device_id = request->controls.video.device_id;
  } else {
    return false;
  }

  if ((request->audio_type() != MEDIA_GUM_TAB_AUDIO_CAPTURE &&
       request->audio_type() != MEDIA_NO_SERVICE) ||
      (request->video_type() != MEDIA_GUM_TAB_VIDEO_CAPTURE &&
       request->video_type() != MEDIA_NO_SERVICE)) {
    return false;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MediaStreamManager::ResolveTabCaptureDeviceIdOnUIThread,
                     base::Unretained(this), capture_device_id,
                     request->requesting_process_id,
                     request->requesting_frame_id,
                     request->salt_and_origin.origin.GetURL()),
      base::BindOnce(
          &MediaStreamManager::FinishTabCaptureRequestSetupWithDeviceId,
          base::Unretained(this), label));
  return true;
}

DesktopMediaID MediaStreamManager::ResolveTabCaptureDeviceIdOnUIThread(
    const std::string& capture_device_id,
    int requesting_process_id,
    int requesting_frame_id,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Resolve DesktopMediaID for the specified device id.
  return DesktopStreamsRegistry::GetInstance()->RequestMediaForStreamId(
      capture_device_id, requesting_process_id, requesting_frame_id, origin,
      nullptr, kRegistryStreamTypeTab);
}

void MediaStreamManager::FinishTabCaptureRequestSetupWithDeviceId(
    const std::string& label,
    const DesktopMediaID& device_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request) {
    DVLOG(1) << "SetUpRequest label " << label << " doesn't exist!!";
    return;  // This can happen if the request has been canceled.
  }

  // Received invalid device id.
  if (device_id.type != content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    FinalizeRequestFailed(label, request, MEDIA_DEVICE_TAB_CAPTURE_FAILURE);
    return;
  }

  content::WebContentsMediaCaptureId web_id = device_id.web_contents_id;
  web_id.disable_local_echo = request->controls.disable_local_echo;

  request->tab_capture_device_id = web_id.ToString();

  request->CreateTabCaptureUIRequest(web_id.render_process_id,
                                     web_id.main_render_frame_id);

  DVLOG(3) << "SetUpTabCaptureRequest "
           << ", {capture_device_id = " << web_id.ToString() << "}"
           << ", {target_render_process_id = " << web_id.render_process_id
           << "}"
           << ", {target_render_frame_id = " << web_id.main_render_frame_id
           << "}";

  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
}

bool MediaStreamManager::SetUpScreenCaptureRequest(DeviceRequest* request) {
  DCHECK(request->audio_type() == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE ||
         request->video_type() == MEDIA_GUM_DESKTOP_VIDEO_CAPTURE);

  // For screen capture we only support two valid combinations:
  // (1) screen video capture only, or
  // (2) screen video capture with loopback audio capture.
  if (request->video_type() != MEDIA_GUM_DESKTOP_VIDEO_CAPTURE ||
      (request->audio_type() != MEDIA_NO_SERVICE &&
       request->audio_type() != MEDIA_GUM_DESKTOP_AUDIO_CAPTURE)) {
    LOG(ERROR) << "Invalid screen capture request.";
    return false;
  }

  std::string video_device_id;
  if (request->video_type() == MEDIA_GUM_DESKTOP_VIDEO_CAPTURE &&
      !request->controls.video.device_id.empty()) {
    video_device_id = request->controls.video.device_id;
  }

  const std::string audio_device_id =
      request->audio_type() == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE ? video_device_id
                                                               : "";

  request->CreateUIRequest(audio_device_id, video_device_id);
  return true;
}

MediaStreamDevices MediaStreamManager::GetDevicesOpenedByRequest(
    const std::string& label) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request)
    return MediaStreamDevices();
  return request->devices;
}

bool MediaStreamManager::FindExistingRequestedDevice(
    const DeviceRequest& new_request,
    const MediaStreamDevice& new_device,
    MediaStreamDevice* existing_device,
    MediaRequestState* existing_request_state) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(existing_device);
  DCHECK(existing_request_state);

  std::string source_id = GetHMACForMediaDeviceID(
      new_request.salt_and_origin.device_id_salt,
      new_request.salt_and_origin.origin, new_device.id);

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second;
    if (request->requesting_process_id == new_request.requesting_process_id &&
        request->requesting_frame_id == new_request.requesting_frame_id &&
        request->request_type == new_request.request_type) {
      for (const MediaStreamDevice& device : request->devices) {
        if (device.id == source_id && device.type == new_device.type) {
          *existing_device = device;
          // Make sure that the audio |effects| reflect what the request
          // is set to and not what the capabilities are.
          int effects = existing_device->input.effects();
          FilterAudioEffects(request->controls, &effects);
          EnableHotwordEffect(request->controls, &effects);
          existing_device->input.set_effects(effects);
          *existing_request_state = request->state(device.type);
          return true;
        }
      }
    }
  }
  return false;
}

void MediaStreamManager::FinalizeGenerateStream(const std::string& label,
                                                DeviceRequest* request) {
  DVLOG(1) << "FinalizeGenerateStream label " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->generate_stream_cb);

  // Partition the array of devices into audio vs video.
  MediaStreamDevices audio_devices, video_devices;
  for (const MediaStreamDevice& device : request->devices) {
    if (IsAudioInputMediaType(device.type))
      audio_devices.push_back(device);
    else if (IsVideoInputMediaType(device.type))
      video_devices.push_back(device);
    else
      NOTREACHED();
  }

  std::move(request->generate_stream_cb)
      .Run(MEDIA_DEVICE_OK, label, audio_devices, video_devices);
}

void MediaStreamManager::FinalizeRequestFailed(
    const std::string& label,
    DeviceRequest* request,
    MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  switch (request->request_type) {
    case MEDIA_GENERATE_STREAM: {
      DCHECK(request->generate_stream_cb);
      std::move(request->generate_stream_cb)
          .Run(result, std::string(), MediaStreamDevices(),
               MediaStreamDevices());
      break;
    }
    case MEDIA_OPEN_DEVICE_PEPPER_ONLY: {
      if (request->open_device_cb) {
        std::move(request->open_device_cb)
            .Run(false /* success */, std::string(), MediaStreamDevice());
      }
      break;
    }
    case MEDIA_DEVICE_ACCESS: {
      DCHECK(request->media_access_request_cb);
      std::move(request->media_access_request_cb)
          .Run(MediaStreamDevices(), std::move(request->ui_proxy));
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  DeleteRequest(label);
}

void MediaStreamManager::FinalizeOpenDevice(const std::string& label,
                                            DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (request->open_device_cb) {
    std::move(request->open_device_cb)
        .Run(true /* success */, label, request->devices.front());
  }
}

void MediaStreamManager::FinalizeMediaAccessRequest(
    const std::string& label,
    DeviceRequest* request,
    const MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->media_access_request_cb);
  std::move(request->media_access_request_cb)
      .Run(devices, std::move(request->ui_proxy));

  // Delete the request since it is done.
  DeleteRequest(label);
}

void MediaStreamManager::InitializeMaybeAsync(
    std::unique_ptr<VideoCaptureProvider> video_capture_provider) {
  // Some unit tests initialize the MSM in the IO thread and assume the
  // initialization is done synchronously. Other clients call this from a
  // different thread and expect initialization to run asynchronously.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&MediaStreamManager::InitializeMaybeAsync,
                       base::Unretained(this),
                       std::move(video_capture_provider)));
    return;
  }

  // Store a pointer to |this| on the IO thread to avoid having to jump to the
  // UI thread to fetch a pointer to the MSM. In particular on Android, it can
  // be problematic to post to a UI thread from arbitrary worker threads since
  // attaching to the VM is required and we may have to access the MSM from
  // callback threads that we don't own and don't want to attach.
  g_media_stream_manager_tls_ptr.Pointer()->Set(this);

  audio_input_device_manager_ = new AudioInputDeviceManager(audio_system_);
  audio_input_device_manager_->RegisterListener(this);

  // We want to be notified of IO message loop destruction to delete the thread
  // and the device managers.
  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);

  video_capture_manager_ =
      new VideoCaptureManager(std::move(video_capture_provider),
                              base::BindRepeating(&SendVideoCaptureLogMessage),
                              ScreenlockMonitor::Get());
  video_capture_manager_->RegisterListener(this);

  // Using base::Unretained(this) is safe because |this| owns and therefore
  // outlives |media_devices_manager_|.
  media_devices_manager_.reset(new MediaDevicesManager(
      audio_system_, video_capture_manager_,
      base::BindRepeating(&MediaStreamManager::StopRemovedDevice,
                          base::Unretained(this)),
      base::BindRepeating(&MediaStreamManager::NotifyDevicesChanged,
                          base::Unretained(this))));
}

void MediaStreamManager::Opened(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Opened({stream_type = " << stream_type << "} "
           << "{capture_session_id = " << capture_session_id << "})";

  // Find the request(s) containing this device and mark it as used.
  // It can be used in several requests since the same device can be
  // requested from the same web page.
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const std::string& label = labeled_request.first;
    DeviceRequest* request = labeled_request.second;
    for (MediaStreamDevice& device : request->devices) {
      if (device.type == stream_type &&
          device.session_id == capture_session_id) {
        CHECK_EQ(request->state(device.type), MEDIA_REQUEST_STATE_OPENING);
        // We've found a matching request.
        request->SetState(device.type, MEDIA_REQUEST_STATE_DONE);

        if (IsAudioInputMediaType(device.type)) {
          // Store the native audio parameters in the device struct.
          // TODO(xians): Handle the tab capture sample rate/channel layout
          // in AudioInputDeviceManager::Open().
          if (device.type != MEDIA_GUM_TAB_AUDIO_CAPTURE) {
            const MediaStreamDevice* opened_device =
                audio_input_device_manager_->GetOpenedDeviceById(
                    device.session_id);
            device.input = opened_device->input;

            // Since the audio input device manager will set the input
            // parameters to the default settings (including supported effects),
            // we need to adjust those settings here according to what the
            // request asks for.
            int effects = device.input.effects();
            FilterAudioEffects(request->controls, &effects);
            EnableHotwordEffect(request->controls, &effects);
            device.input.set_effects(effects);
          }
        }
        if (RequestDone(*request))
          HandleRequestDone(label, request);
        break;
      }
    }
  }
}

void MediaStreamManager::HandleRequestDone(const std::string& label,
                                           DeviceRequest* request) {
  DCHECK(RequestDone(*request));
  DVLOG(1) << "HandleRequestDone("
           << ", {label = " << label << "})";

  switch (request->request_type) {
    case MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      FinalizeOpenDevice(label, request);
      OnStreamStarted(label);
      break;
    case MEDIA_GENERATE_STREAM: {
      FinalizeGenerateStream(label, request);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void MediaStreamManager::Closed(MediaStreamType stream_type,
                                int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaStreamManager::DevicesEnumerated(
    bool requested_audio_input,
    bool requested_video_input,
    const std::string& label,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;

  bool requested[] = {requested_audio_input, requested_video_input};
  MediaStreamType stream_types[] = {MEDIA_DEVICE_AUDIO_CAPTURE,
                                    MEDIA_DEVICE_VIDEO_CAPTURE};
  for (size_t i = 0; i < arraysize(requested); ++i) {
    if (!requested[i])
      continue;

    DCHECK(request->audio_type() == stream_types[i] ||
           request->video_type() == stream_types[i]);
    if (request->state(stream_types[i]) == MEDIA_REQUEST_STATE_REQUESTED) {
      request->SetState(stream_types[i], MEDIA_REQUEST_STATE_PENDING_APPROVAL);
    }
  }

  if (!SetUpDeviceCaptureRequest(request, enumeration))
    FinalizeRequestFailed(label, request, MEDIA_DEVICE_NO_HARDWARE);
  else
    ReadOutputParamsAndPostRequestToUI(label, request, enumeration);
}

void MediaStreamManager::Aborted(MediaStreamType stream_type,
                                 int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Aborted({stream_type = " << stream_type << "} "
           << "{capture_session_id = " << capture_session_id << "})";
  StopDevice(stream_type, capture_session_id);
}

void MediaStreamManager::OnSuspend() {
  SendMessageToNativeLog("Power state suspended.");
}

void MediaStreamManager::OnResume() {
  SendMessageToNativeLog("Power state resumed.");
}

void MediaStreamManager::UseFakeUIFactoryForTests(
    base::Callback<std::unique_ptr<FakeMediaStreamUIProxy>(void)>
        fake_ui_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  fake_ui_factory_ = std::move(fake_ui_factory);
}

// static
void MediaStreamManager::RegisterNativeLogCallback(
    int renderer_host_id,
    const base::Callback<void(const std::string&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager* msm = g_media_stream_manager_tls_ptr.Pointer()->Get();
  if (!msm) {
    DLOG(ERROR) << "No MediaStreamManager on the IO thread.";
    return;
  }

  msm->DoNativeLogCallbackRegistration(renderer_host_id, callback);
}

// static
void MediaStreamManager::UnregisterNativeLogCallback(int renderer_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager* msm = g_media_stream_manager_tls_ptr.Pointer()->Get();
  if (!msm) {
    DLOG(ERROR) << "No MediaStreamManager on the IO thread.";
    return;
  }

  msm->DoNativeLogCallbackUnregistration(renderer_host_id);
}

void MediaStreamManager::AddLogMessageOnIOThread(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& callback : log_callbacks_)
    callback.second.Run(message);
}

void MediaStreamManager::HandleAccessRequestResponse(
    const std::string& label,
    const media::AudioParameters& output_parameters,
    const MediaStreamDevices& devices,
    MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "HandleAccessRequestResponse("
           << ", {label = " << label << "})";

  DeviceRequest* request = FindRequest(label);
  if (!request) {
    // The request has been canceled before the UI returned.
    return;
  }

  if (request->request_type == MEDIA_DEVICE_ACCESS) {
    FinalizeMediaAccessRequest(label, request, devices);
    return;
  }

  // Handle the case when the request was denied.
  if (result != MEDIA_DEVICE_OK) {
    FinalizeRequestFailed(label, request, result);
    return;
  }
  DCHECK(!devices.empty());

  // Process all newly-accepted devices for this request.
  bool found_audio = false;
  bool found_video = false;
  for (const MediaStreamDevice& media_stream_device : devices) {
    MediaStreamDevice device = media_stream_device;

    if (device.type == MEDIA_GUM_TAB_VIDEO_CAPTURE ||
        device.type == MEDIA_GUM_TAB_AUDIO_CAPTURE) {
      device.id = request->tab_capture_device_id;
    }

    // Initialize the sample_rate and channel_layout here since for audio
    // mirroring, we don't go through EnumerateDevices where these are usually
    // initialized.
    if (device.type == MEDIA_GUM_TAB_AUDIO_CAPTURE ||
        device.type == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE) {
      int sample_rate = output_parameters.sample_rate();
      // If we weren't able to get the native sampling rate or the sample_rate
      // is outside the valid range for input devices set reasonable defaults.
      if (sample_rate <= 0 || sample_rate > 96000)
        sample_rate = 44100;

      media::AudioParameters params(device.input.format(),
                                    media::CHANNEL_LAYOUT_STEREO, sample_rate,
                                    device.input.frames_per_buffer());
      params.set_effects(device.input.effects());
      params.set_mic_positions(device.input.mic_positions());
      DCHECK(params.IsValid());
      device.input = params;
    }

    if (device.type == request->audio_type())
      found_audio = true;
    else if (device.type == request->video_type())
      found_video = true;

    // If this is request for a new MediaStream, a device is only opened once
    // per render frame. This is so that the permission to use a device can be
    // revoked by a single call to StopStreamDevice regardless of how many
    // MediaStreams it is being used in.
    if (request->request_type == MEDIA_GENERATE_STREAM) {
      MediaRequestState state;
      if (FindExistingRequestedDevice(*request, device, &device, &state)) {
        request->devices.push_back(device);
        request->SetState(device.type, state);
        DVLOG(1) << "HandleAccessRequestResponse - device already opened "
                 << ", {label = " << label << "}"
                 << ", device_id = " << device.id << "}";
        continue;
      }
    }
    device.session_id = GetDeviceManager(device.type)->Open(device);
    TranslateDeviceIdToSourceId(request, &device);
    request->devices.push_back(device);

    request->SetState(device.type, MEDIA_REQUEST_STATE_OPENING);
    DVLOG(1) << "HandleAccessRequestResponse - opening device "
             << ", {label = " << label << "}"
             << ", {device_id = " << device.id << "}"
             << ", {session_id = " << device.session_id << "}";
  }

  // Check whether we've received all stream types requested.
  if (!found_audio && IsAudioInputMediaType(request->audio_type())) {
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_ERROR);
    DVLOG(1) << "Set no audio found label " << label;
  }

  if (!found_video && IsVideoInputMediaType(request->video_type()))
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_ERROR);

  if (RequestDone(*request))
    HandleRequestDone(label, request);
}

void MediaStreamManager::StopMediaStreamFromBrowser(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;

  // Notify renderers that the devices in the stream will be stopped.
  if (request->device_stopped_cb) {
    for (const MediaStreamDevice& device : request->devices) {
      request->device_stopped_cb.Run(label, device);
    }
  }

  CancelRequest(label);
}

void MediaStreamManager::WillDestroyCurrentMessageLoop() {
  DVLOG(3) << "MediaStreamManager::WillDestroyCurrentMessageLoop()";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::IO));
  if (media_devices_manager_)
    media_devices_manager_->StopMonitoring();
  if (video_capture_manager_)
    video_capture_manager_->UnregisterListener(this);
  if (audio_input_device_manager_)
    audio_input_device_manager_->UnregisterListener(this);

  audio_input_device_manager_ = nullptr;
  video_capture_manager_ = nullptr;
  media_devices_manager_ = nullptr;
  g_media_stream_manager_tls_ptr.Pointer()->Set(nullptr);
  requests_.clear();
}

void MediaStreamManager::NotifyDevicesChanged(
    MediaDeviceType device_type,
    const MediaDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  MediaStreamType stream_type = ConvertToMediaStreamType(device_type);
  MediaStreamDevices new_devices =
      ConvertToMediaStreamDevices(stream_type, devices);

  if (IsAudioInputMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnAudioCaptureDevicesChanged(
        new_devices);
    if (media_observer)
      media_observer->OnAudioCaptureDevicesChanged();
  } else if (IsVideoInputMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnVideoCaptureDevicesChanged(
        new_devices);
    if (media_observer)
      media_observer->OnVideoCaptureDevicesChanged();
  } else {
    NOTREACHED();
  }
}

bool MediaStreamManager::RequestDone(const DeviceRequest& request) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const bool requested_audio = IsAudioInputMediaType(request.audio_type());
  const bool requested_video = IsVideoInputMediaType(request.video_type());

  const bool audio_done =
      !requested_audio ||
      request.state(request.audio_type()) == MEDIA_REQUEST_STATE_DONE ||
      request.state(request.audio_type()) == MEDIA_REQUEST_STATE_ERROR;
  if (!audio_done)
    return false;

  const bool video_done =
      !requested_video ||
      request.state(request.video_type()) == MEDIA_REQUEST_STATE_DONE ||
      request.state(request.video_type()) == MEDIA_REQUEST_STATE_ERROR;
  if (!video_done)
    return false;

  return true;
}

MediaStreamProvider* MediaStreamManager::GetDeviceManager(
    MediaStreamType stream_type) {
  if (IsVideoInputMediaType(stream_type))
    return video_capture_manager();
  else if (IsAudioInputMediaType(stream_type))
    return audio_input_device_manager();
  NOTREACHED();
  return nullptr;
}

void MediaStreamManager::OnMediaStreamUIWindowId(
    MediaStreamType video_type,
    const MediaStreamDevices& devices,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id)
    return;

  if (video_type != MEDIA_GUM_DESKTOP_VIDEO_CAPTURE)
    return;

  // Pass along for desktop screen and window capturing when
  // DesktopCaptureDevice is used.
  for (const MediaStreamDevice& device : devices) {
    if (device.type != MEDIA_GUM_DESKTOP_VIDEO_CAPTURE)
      continue;

    DesktopMediaID media_id = DesktopMediaID::Parse(device.id);
    // WebContentsVideoCaptureDevice is used for tab/webcontents.
    if (media_id.type == DesktopMediaID::TYPE_WEB_CONTENTS)
      continue;
#if defined(USE_AURA)
    // DesktopCaptureDevicAura is used when aura_id is valid.
    if (media_id.aura_id > DesktopMediaID::kNullId)
      continue;
#endif
    video_capture_manager_->SetDesktopCaptureWindowId(device.session_id,
                                                      window_id);
    break;
  }
}

void MediaStreamManager::DoNativeLogCallbackRegistration(
    int renderer_host_id,
    const base::Callback<void(const std::string&)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Re-registering (overwriting) is allowed and happens in some tests.
  log_callbacks_[renderer_host_id] = callback;
}

void MediaStreamManager::DoNativeLogCallbackUnregistration(
    int renderer_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  log_callbacks_.erase(renderer_host_id);
}

// static
std::string MediaStreamManager::GetHMACForMediaDeviceID(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& raw_unique_id) {
  DCHECK(!raw_unique_id.empty());
  if (raw_unique_id == media::AudioDeviceDescription::kDefaultDeviceId ||
      raw_unique_id == media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return raw_unique_id;
  }

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8_t> digest(digest_length);
  bool result = hmac.Init(security_origin.Serialize()) &&
                hmac.Sign(raw_unique_id + salt, &digest[0], digest.size());
  DCHECK(result);
  return base::ToLowerASCII(base::HexEncode(&digest[0], digest.size()));
}

// static
bool MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& device_guid,
    const std::string& raw_unique_id) {
  DCHECK(!raw_unique_id.empty());
  std::string guid_from_raw_device_id =
      GetHMACForMediaDeviceID(salt, security_origin, raw_unique_id);
  return guid_from_raw_device_id == device_guid;
}

// static
bool MediaStreamManager::IsOriginAllowed(int render_process_id,
                                         const url::Origin& origin) {
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          render_process_id, origin.GetURL())) {
    LOG(ERROR) << "MSM: Renderer requested a URL it's not allowed to use: "
               << origin.Serialize();
    return false;
  }

  return true;
}

void MediaStreamManager::SetCapturingLinkSecured(int render_process_id,
                                                 int session_id,
                                                 MediaStreamType type,
                                                 bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* request = labeled_request.second;
    if (request->requesting_process_id != render_process_id)
      continue;

    for (const MediaStreamDevice& device : request->devices) {
      if (device.session_id == session_id && device.type == type) {
        request->SetCapturingLinkSecured(is_secure);
        return;
      }
    }
  }
}

void MediaStreamManager::SetGenerateStreamCallbackForTesting(
    GenerateStreamTestCallback test_callback) {
  generate_stream_test_callback_ = std::move(test_callback);
}

MediaStreamDevices MediaStreamManager::ConvertToMediaStreamDevices(
    MediaStreamType stream_type,
    const MediaDeviceInfoArray& device_infos) {
  MediaStreamDevices devices;
  for (const auto& info : device_infos) {
    devices.emplace_back(stream_type, info.device_id, info.label,
                         info.video_facing, info.group_id);
  }

  if (stream_type != MEDIA_DEVICE_VIDEO_CAPTURE)
    return devices;

  for (auto& device : devices) {
    device.camera_calibration =
        video_capture_manager()->GetCameraCalibration(device.id);
  }
  return devices;
}

void MediaStreamManager::OnStreamStarted(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* const request = FindRequest(label);
  if (!request)
    return;

  if (request->ui_proxy) {
    request->ui_proxy->OnStarted(
        base::BindOnce(&MediaStreamManager::StopMediaStreamFromBrowser,
                       base::Unretained(this), label),
        base::BindOnce(&MediaStreamManager::OnMediaStreamUIWindowId,
                       base::Unretained(this), request->video_type(),
                       request->devices));
  }
}

}  // namespace content
