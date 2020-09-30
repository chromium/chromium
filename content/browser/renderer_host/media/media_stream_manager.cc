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
#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/permissions/permission_controller_impl.h"
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
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
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
#include "media/mojo/mojom/display_media_information.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "content/browser/gpu/chromeos/video_capture_dependencies.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#endif

namespace content {

base::LazyInstance<base::ThreadLocalPointer<MediaStreamManager>>::Leaky
    g_media_stream_manager_tls_ptr = LAZY_INSTANCE_INITIALIZER;

using blink::MediaStreamDevice;
using blink::MediaStreamDevices;
using blink::MediaStreamRequestType;
using blink::StreamControls;
using blink::TrackControls;
using blink::mojom::MediaStreamRequestResult;
using blink::mojom::MediaStreamType;
using blink::mojom::StreamSelectionInfo;
using blink::mojom::StreamSelectionInfoPtr;
using blink::mojom::StreamSelectionStrategy;

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
    // Use |base::size(kAlphabet) - 1| to avoid |kAlphabet|s terminating '\0';
    c = kAlphabet[base::RandGenerator(base::size(kAlphabet) - 1)];
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
    // Only enable if a hotword device exists.
    if (chromeos::CrasAudioHandler::Get()->HasHotwordDevice())
      *effects |= media::AudioParameters::HOTWORD;
#endif
  }
}

bool GetDeviceIDFromHMAC(const std::string& salt,
                         const url::Origin& security_origin,
                         const std::string& hmac_device_id,
                         const blink::WebMediaDeviceInfoArray& devices,
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

MediaStreamType ConvertToMediaStreamType(blink::MediaDeviceType type) {
  switch (type) {
    case blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return MediaStreamType::DEVICE_AUDIO_CAPTURE;
    case blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return MediaStreamType::DEVICE_VIDEO_CAPTURE;
    default:
      NOTREACHED();
  }

  return MediaStreamType::NO_SERVICE;
}

blink::MediaDeviceType ConvertToMediaDeviceType(MediaStreamType stream_type) {
  switch (stream_type) {
    case MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT;
    case MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT;
    default:
      NOTREACHED();
  }

  return blink::NUM_MEDIA_DEVICE_TYPES;
}

const char* DeviceTypeToString(blink::MediaDeviceType type) {
  switch (type) {
    case blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return "DEVICE_AUDIO_INPUT";
    case blink::MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return "DEVICE_AUDIO_OUTPUT";
    case blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return "DEVICE_VIDEO_INPUT";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* RequestTypeToString(blink::MediaStreamRequestType type) {
  switch (type) {
    case blink::MEDIA_DEVICE_ACCESS:
      return "MEDIA_DEVICE_ACCESS";
    case blink::MEDIA_DEVICE_UPDATE:
      return "MEDIA_DEVICE_UPDATE";
    case blink::MEDIA_GENERATE_STREAM:
      return "MEDIA_GENERATE_STREAM";
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      return "MEDIA_OPEN_DEVICE_PEPPER_ONLY";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* StreamTypeToString(blink::mojom::MediaStreamType type) {
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return "DEVICE_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return "DEVICE_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
      return "GUM_TAB_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      return "GUM_TAB_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
      return "GUM_DESKTOP_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      return "GUM_DESKTOP_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
      return "DISPLAY_AUDIO_CAPTURE";
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      return "DISPLAY_VIDEO_CAPTURE";
    case blink::mojom::MediaStreamType::NO_SERVICE:
      return "NO_SERVICE";
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      return "NUM_MEDIA_TYPES";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* RequestStateToString(MediaRequestState state) {
  switch (state) {
    case MEDIA_REQUEST_STATE_NOT_REQUESTED:
      return "STATE_NOT_REQUESTED";
    case MEDIA_REQUEST_STATE_REQUESTED:
      return "STATE_REQUESTED";
    case MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      return "STATE_PENDING_APPROVAL";
    case MEDIA_REQUEST_STATE_OPENING:
      return "STATE_OPENING";
    case MEDIA_REQUEST_STATE_DONE:
      return "STATE_DONE";
    case MEDIA_REQUEST_STATE_CLOSING:
      return "STATE_CLOSING";
    case MEDIA_REQUEST_STATE_ERROR:
      return "STATE_ERROR";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

const char* RequestResultToString(
    blink::mojom::MediaStreamRequestResult result) {
  switch (result) {
    case blink::mojom::MediaStreamRequestResult::OK:
      return "OK";
    case blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED:
      return "PERMISSION_DISMISSED";
    case blink::mojom::MediaStreamRequestResult::INVALID_STATE:
      return "INVALID_STATE";
    case blink::mojom::MediaStreamRequestResult::NO_HARDWARE:
      return "NO_HARDWARE";
    case blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      return "INVALID_SECURITY_ORIGIN";
    case blink::mojom::MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      return "INVALID_STATE";
    case blink::mojom::MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      return "TAB_CAPTURE_FAILURE";
    case blink::mojom::MediaStreamRequestResult::CAPTURE_FAILURE:
      return "CAPTURE_FAILURE";
    case blink::mojom::MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      return "CONSTRAINT_NOT_SATISFIED";
    case blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      return "TRACK_START_FAILURE_AUDIO";
    case blink::mojom::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      return "TRACK_START_FAILURE_VIDEO";
    case blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      return "FAILED_DUE_TO_SHUTDOWN";
    case blink::mojom::MediaStreamRequestResult::KILL_SWITCH_ON:
      return "KILL_SWITCH_ON";
    case blink::mojom::MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      return "SYSTEM_PERMISSION_DENIED";
    case blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      return "NUM_MEDIA_REQUEST_RESULTS";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

std::string GetGenerateStreamLogString(int render_process_id,
                                       int render_frame_id,
                                       int requester_id,
                                       int page_request_id) {
  return base::StringPrintf(
      "GenerateStream({render_process_id=%d}, {render_frame_id=%d}, "
      "{requester_id=%d}, {page_request_id=%d})",
      render_process_id, render_frame_id, requester_id, page_request_id);
}

std::string GetOpenDeviceLogString(int render_process_id,
                                   int render_frame_id,
                                   int requester_id,
                                   int page_request_id,
                                   const std::string& device_id,
                                   MediaStreamType type) {
  return base::StringPrintf(
      "OpenDevice({render_process_id=%d}, {render_frame_id=%d}, "
      "{requester_id=%d}, {page_request_id=%d}, {device_id=%s}, {type=%s})",
      render_process_id, render_frame_id, requester_id, page_request_id,
      device_id.c_str(), StreamTypeToString(type));
}

std::string GetStopStreamDeviceLogString(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    const std::string& device_id,
    const base::UnguessableToken& session_id) {
  return base::StringPrintf(
      "StopStreamDevice({render_process_id=%d}, {render_frame_id=%d}, "
      "{requester_id=%d}, {device_id=%s}, {session_id=%s})",
      render_process_id, render_frame_id, requester_id, device_id.c_str(),
      session_id.ToString().c_str());
}

void SendLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("MSM::" + message);
}

void SendVideoCaptureLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("video capture: " + message);
}

// Returns MediaStreamDevices for getDisplayMedia() calls.
// Returns a video device built with DesktopMediaID with fake initializers if
// |kUseFakeDeviceForMediaStream| is set. Returns a video device with
// default DesktopMediaID otherwise.
// Returns an audio device with default device parameters.
// If |kUseFakeDeviceForMediaStream| specifies a browser window, use
// |render_process_id| and |render_frame_id| as the browser window identifier.
MediaStreamDevices DisplayMediaDevicesFromFakeDeviceConfig(
    bool request_audio,
    int render_process_id,
    int render_frame_id) {
  MediaStreamDevices devices;
  DesktopMediaID::Type desktop_media_type = DesktopMediaID::TYPE_SCREEN;
  DesktopMediaID::Id desktop_media_id_id = DesktopMediaID::kNullId;
  WebContentsMediaCaptureId web_contents_id;
  media::mojom::DisplayCaptureSurfaceType display_surface =
      media::mojom::DisplayCaptureSurfaceType::MONITOR;
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
    if (!config.empty()) {
      desktop_media_type = DesktopMediaID::TYPE_NONE;
      desktop_media_id_id = DesktopMediaID::kFakeId;
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
          web_contents_id = {render_process_id, render_frame_id};
          break;
      }
    }
  }
  DesktopMediaID media_id(desktop_media_type, desktop_media_id_id,
                          web_contents_id);
  MediaStreamDevice device(MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           media_id.ToString(), media_id.ToString());
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface, true, media::mojom::CursorCaptureType::NEVER);
  devices.push_back(device);
  if (!request_audio)
    return devices;

  devices.emplace_back(MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                       media::AudioDeviceDescription::kDefaultDeviceId,
                       "Fake audio");
  return devices;
}

void FinalizeGetMediaDeviceIDForHMAC(
    blink::MediaDeviceType type,
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& source_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback,
    const MediaDeviceEnumeration& enumeration) {
  for (const auto& device : enumeration[type]) {
    if (MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
            salt, security_origin, source_id, device.device_id)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), device.device_id));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), base::nullopt));
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
      int requester_id,
      int page_request_id,
      bool user_gesture,
      StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      MediaStreamRequestType request_type,
      const StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      DeviceStoppedCallback device_stopped_cb = DeviceStoppedCallback())
      : requesting_process_id(requesting_process_id),
        requesting_frame_id(requesting_frame_id),
        requester_id(requester_id),
        page_request_id(page_request_id),
        user_gesture(user_gesture),
        audio_stream_selection_info_ptr(
            std::move(audio_stream_selection_info_ptr)),
        controls(controls),
        salt_and_origin(std::move(salt_and_origin)),
        device_stopped_cb(std::move(device_stopped_cb)),
        state_(static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES),
               MEDIA_REQUEST_STATE_NOT_REQUESTED),
        request_type_(request_type),
        audio_type_(MediaStreamType::NO_SERVICE),
        video_type_(MediaStreamType::NO_SERVICE),
        target_process_id_(-1),
        target_frame_id_(-1) {
    SendLogMessage(base::StringPrintf(
        "DR::DeviceRequest({requesting_process_id=%d}, "
        "{requesting_frame_id=%d}, {requester_id=%d}, {request_type=%s})",
        requesting_process_id, requesting_frame_id, requester_id,
        RequestTypeToString(request_type)));
  }

  ~DeviceRequest() { RunMojoCallbacks(); }

  void set_request_type(MediaStreamRequestType type) { request_type_ = type; }
  MediaStreamRequestType request_type() const { return request_type_; }

  void SetAudioType(MediaStreamType audio_type) {
    DCHECK(blink::IsAudioInputMediaType(audio_type) ||
           audio_type == MediaStreamType::NO_SERVICE);
    SendLogMessage(base::StringPrintf(
        "DR::SetAudioType([requester_id=%d] {audio_type=%s})", requester_id,
        StreamTypeToString(audio_type)));
    audio_type_ = audio_type;
  }

  MediaStreamType audio_type() const { return audio_type_; }

  void SetVideoType(MediaStreamType video_type) {
    DCHECK(blink::IsVideoInputMediaType(video_type) ||
           video_type == MediaStreamType::NO_SERVICE);
    video_type_ = video_type;
  }

  MediaStreamType video_type() const { return video_type_; }

  // Creates a MediaStreamRequest object that is used by this request when UI
  // is asked for permission and device selection.
  void CreateUIRequest(const std::string& requested_audio_device_id,
                       const std::string& requested_video_device_id) {
    DCHECK(!ui_request_);
    SendLogMessage(base::StringPrintf(
        "DR::CreateUIRequest([requester_id=%d] {requested_audio_device_id=%s}, "
        "{requested_video_device_id=%s})",
        requester_id, requested_audio_device_id.c_str(),
        requested_video_device_id.c_str()));
    target_process_id_ = requesting_process_id;
    target_frame_id_ = requesting_frame_id;
    ui_request_.reset(new MediaStreamRequest(
        requesting_process_id, requesting_frame_id, page_request_id,
        salt_and_origin.origin.GetURL(), user_gesture, request_type_,
        requested_audio_device_id, requested_video_device_id, audio_type_,
        video_type_, controls.disable_local_echo,
        controls.request_pan_tilt_zoom_permission));
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
        salt_and_origin.origin.GetURL(), user_gesture, request_type_, "", "",
        audio_type_, video_type_, controls.disable_local_echo,
        /*request_pan_tilt_zoom_permission=*/false));
  }

  bool HasUIRequest() const { return ui_request_.get() != nullptr; }
  std::unique_ptr<MediaStreamRequest> DetachUIRequest() {
    return std::move(ui_request_);
  }

  // Update the request state and notify observers.
  void SetState(MediaStreamType stream_type, MediaRequestState new_state) {
    SendLogMessage(base::StringPrintf(
        "DR::SetState([requester_id=%d] {stream_type=%s}, {new_state=%s})",
        requester_id, StreamTypeToString(stream_type),
        RequestStateToString(new_state)));

    if (stream_type == MediaStreamType::NUM_MEDIA_TYPES) {
      for (int i = static_cast<int>(MediaStreamType::NO_SERVICE) + 1;
           i < static_cast<int>(MediaStreamType::NUM_MEDIA_TYPES); ++i) {
        state_[i] = new_state;
      }
    } else {
      state_[static_cast<int>(stream_type)] = new_state;
    }

    MediaObserver* media_observer =
        GetContentClient()->browser()->GetMediaObserver();
    if (!media_observer)
      return;

    if (stream_type == MediaStreamType::NUM_MEDIA_TYPES) {
      for (int i = static_cast<int>(MediaStreamType::NO_SERVICE) + 1;
           i < static_cast<int>(MediaStreamType::NUM_MEDIA_TYPES); ++i) {
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
    return state_[static_cast<int>(stream_type)];
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
          .Run(MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN, std::string(),
               MediaStreamDevices(), MediaStreamDevices(),
               /*pan_tilt_zoom_allowed=*/false);
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

  // The id of the object that requested this stream to be generated and that
  // will receive a handle to the MediaStream. This may be different from
  // MediaStreamRequest::requester_id which in the tab capture case
  // specifies the target renderer from which audio and video is captured.
  const int requester_id;

  // An ID the render frame provided to identify this request.
  const int page_request_id;

  const bool user_gesture;

  // Information as of how to select a stream for an audio device provided by
  // the caller.
  // NB: This information is invalid after the request has been processed.
  StreamSelectionInfoPtr audio_stream_selection_info_ptr;

  const StreamControls controls;

  const MediaDeviceSaltAndOrigin salt_and_origin;

  MediaStreamDevices devices;
  MediaStreamDevices old_devices;

  // Callback to the requester which audio/video devices have been selected.
  // It can be null if the requester has no interest to know the result.
  // Currently it is only used by |DEVICE_ACCESS| type.
  MediaAccessRequestCallback media_access_request_cb;

  GenerateStreamCallback generate_stream_cb;

  OpenDeviceCallback open_device_cb;

  DeviceStoppedCallback device_stopped_cb;

  DeviceChangedCallback device_changed_cb;

  std::unique_ptr<MediaStreamUIProxy> ui_proxy;

  std::string tab_capture_device_id;

  int audio_subscription_id = PermissionControllerImpl::kNoPendingOperation;

  int video_subscription_id = PermissionControllerImpl::kNoPendingOperation;

 private:
  std::vector<MediaRequestState> state_;
  std::unique_ptr<MediaStreamRequest> ui_request_;
  MediaStreamRequestType request_type_;
  MediaStreamType audio_type_;
  MediaStreamType video_type_;
  int target_process_id_;
  int target_frame_id_;
};

// static
void MediaStreamManager::SendMessageToNativeLog(const std::string& message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaStreamManager::SendMessageToNativeLog, message));
    return;
  }
  DVLOG(1) << message;

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
    : MediaStreamManager(audio_system, std::move(audio_task_runner), nullptr) {
  SendLogMessage(base::StringPrintf("MediaStreamManager([this=%p]))", this));
}

MediaStreamManager::MediaStreamManager(
    media::AudioSystem* audio_system,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    std::unique_ptr<VideoCaptureProvider> video_capture_provider)
    : audio_system_(audio_system) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)) {
    fake_ui_factory_ = base::BindRepeating([] {
      return std::make_unique<FakeMediaStreamUIProxy>(
          /*tests_use_fake_render_frame_hosts=*/false);
    });
  }
  if (base::FeatureList::IsEnabled(media::kUseFakeDeviceForMediaStream)) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
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
                  GetUIThreadTaskRunner({}))),
          std::move(device_task_runner),
          base::BindRepeating(&SendVideoCaptureLogMessage));
    }
  }
  InitializeMaybeAsync(std::move(video_capture_provider));

  audio_service_listener_ = std::make_unique<AudioServiceListener>();

  base::PowerMonitor::AddObserver(this);
}

MediaStreamManager::~MediaStreamManager() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO));
  DCHECK(requests_.empty());

  base::PowerMonitor::RemoveObserver(this);
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
    int requester_id,
    int page_request_id,
    const StreamControls& controls,
    const url::Origin& security_origin,
    MediaAccessRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StreamSelectionInfoPtr audio_stream_selection_info_ptr =
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          base::nullopt);
  auto request = std::make_unique<DeviceRequest>(
      render_process_id, render_frame_id, requester_id, page_request_id,
      false /* user gesture */, std::move(audio_stream_selection_info_ptr),
      blink::MEDIA_DEVICE_ACCESS, controls,
      MediaDeviceSaltAndOrigin{std::string() /* salt */,
                               std::string() /* group_id_salt */,
                               security_origin});

  request->media_access_request_cb = std::move(callback);
  const std::string& label = AddRequest(std::move(request));

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamManager::SetUpRequest,
                                base::Unretained(this), label));
  return label;
}

void MediaStreamManager::GenerateStream(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    int page_request_id,
    const StreamControls& controls,
    MediaDeviceSaltAndOrigin salt_and_origin,
    bool user_gesture,
    StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamCallback generate_stream_cb,
    DeviceStoppedCallback device_stopped_cb,
    DeviceChangedCallback device_changed_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(GetGenerateStreamLogString(render_process_id, render_frame_id,
                                            requester_id, page_request_id));

  DeviceRequest* request = new DeviceRequest(
      render_process_id, render_frame_id, requester_id, page_request_id,
      user_gesture, std::move(audio_stream_selection_info_ptr),
      blink::MEDIA_GENERATE_STREAM, controls, std::move(salt_and_origin),
      std::move(device_stopped_cb));
  request->device_changed_cb = std::move(device_changed_cb);

  const std::string& label = AddRequest(base::WrapUnique(request));

  request->generate_stream_cb = std::move(generate_stream_cb);

  if (generate_stream_test_callback_) {
    // The test callback is responsible to verify whether the |controls| is
    // as expected. Then we need to finish getUserMedia and let Javascript
    // access the result.
    if (std::move(generate_stream_test_callback_).Run(controls)) {
      FinalizeGenerateStream(label, request);
    } else {
      FinalizeRequestFailed(label, request,
                            MediaStreamRequestResult::INVALID_STATE);
    }
    return;
  }

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamManager::SetUpRequest,
                                base::Unretained(this), label));
}

void MediaStreamManager::CancelRequest(int render_process_id,
                                       int render_frame_id,
                                       int requester_id,
                                       int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second.get();
    if (request->requesting_process_id == render_process_id &&
        request->requesting_frame_id == render_frame_id &&
        request->requester_id == requester_id &&
        request->page_request_id == page_request_id) {
      CancelRequest(labeled_request.first);
      return;
    }
  }
}

void MediaStreamManager::CancelRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(
      base::StringPrintf("CancelRequest({label=%s})", label.c_str()));
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
    CloseDevice(device.type, device.session_id());
  }

  // Cancel the request if still pending at UI side.
  request->SetState(MediaStreamType::NUM_MEDIA_TYPES,
                    MEDIA_REQUEST_STATE_CLOSING);
  DeleteRequest(label);
}

void MediaStreamManager::CancelAllRequests(int render_process_id,
                                           int render_frame_id,
                                           int requester_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto request_it = requests_.begin();
  while (request_it != requests_.end()) {
    if (request_it->second->requesting_process_id != render_process_id ||
        request_it->second->requesting_frame_id != render_frame_id ||
        request_it->second->requester_id != requester_id) {
      ++request_it;
      continue;
    }
    const std::string label = request_it->first;
    ++request_it;
    CancelRequest(label);
  }
}

void MediaStreamManager::StopStreamDevice(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    const std::string& device_id,
    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(GetStopStreamDeviceLogString(
      render_process_id, render_frame_id, requester_id, device_id, session_id));

  // Find the first request for this |render_process_id| and |render_frame_id|
  // of type MEDIA_GENERATE_STREAM that has requested to use |device_id| and
  // stop it.
  for (const LabeledDeviceRequest& device_request : requests_) {
    DeviceRequest* const request = device_request.second.get();
    if (request->requesting_process_id != render_process_id ||
        request->requesting_frame_id != render_frame_id ||
        request->requester_id != requester_id ||
        (request->request_type() != blink::MEDIA_GENERATE_STREAM &&
         request->request_type() != blink::MEDIA_DEVICE_UPDATE)) {
      continue;
    }

    for (const MediaStreamDevice& device : request->devices) {
      if (device.id == device_id && device.session_id() == session_id) {
        StopDevice(device.type, device.session_id());
        return;
      }
    }
  }
}

base::UnguessableToken MediaStreamManager::VideoDeviceIdToSessionId(
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const LabeledDeviceRequest& device_request : requests_) {
    for (const MediaStreamDevice& device : device_request.second->devices) {
      if (device.id == device_id &&
          device.type == MediaStreamType::DEVICE_VIDEO_CAPTURE) {
        return device.session_id();
      }
    }
  }
  return base::UnguessableToken();
}

void MediaStreamManager::StopDevice(MediaStreamType type,
                                    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("StopDevice({type=%s}, {session_id=%s})",
                                    StreamTypeToString(type),
                                    session_id.ToString().c_str()));
  auto request_it = requests_.begin();
  while (request_it != requests_.end()) {
    DeviceRequest* request = request_it->second.get();
    MediaStreamDevices* devices = &request->devices;
    if (devices->empty()) {
      // There is no device in use yet by this request.
      ++request_it;
      continue;
    }
    auto device_it = devices->begin();
    while (device_it != devices->end()) {
      if (device_it->type != type || device_it->session_id() != session_id) {
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

void MediaStreamManager::CloseDevice(MediaStreamType type,
                                     const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("CloseDevice({type=%s}, {session_id=%s})",
                                    StreamTypeToString(type),
                                    session_id.ToString().c_str()));
  GetDeviceManager(type)->Close(session_id);

  for (const LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* const request = labeled_request.second.get();
    for (const MediaStreamDevice& device : request->devices) {
      if (device.session_id() == session_id && device.type == type) {
        // Notify observers that this device is being closed.
        // Note that only one device per type can be opened.
        request->SetState(type, MEDIA_REQUEST_STATE_CLOSING);
      }
    }
  }
}

void MediaStreamManager::OpenDevice(int render_process_id,
                                    int render_frame_id,
                                    int requester_id,
                                    int page_request_id,
                                    const std::string& device_id,
                                    MediaStreamType type,
                                    MediaDeviceSaltAndOrigin salt_and_origin,
                                    OpenDeviceCallback open_device_cb,
                                    DeviceStoppedCallback device_stopped_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  SendLogMessage(GetOpenDeviceLogString(render_process_id, render_frame_id,
                                        requester_id, page_request_id,
                                        device_id, type));
  StreamControls controls;
  if (blink::IsAudioInputMediaType(type)) {
    controls.audio.requested = true;
    controls.audio.stream_type = type;
    controls.audio.device_id = device_id;
  } else if (blink::IsVideoInputMediaType(type)) {
    controls.video.requested = true;
    controls.video.stream_type = type;
    controls.video.device_id = device_id;
  } else {
    NOTREACHED();
  }
  // For pepper, we default to searching for a device always based on device ID,
  // independently of whether the request is for an audio or a video device.
  StreamSelectionInfoPtr audio_stream_selection_info_ptr =
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          base::nullopt);
  auto request = std::make_unique<DeviceRequest>(
      render_process_id, render_frame_id, requester_id, page_request_id,
      false /* user gesture */, std::move(audio_stream_selection_info_ptr),
      blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY, controls,
      std::move(salt_and_origin), std::move(device_stopped_cb));

  request->open_device_cb = std::move(open_device_cb);
  const std::string& label = AddRequest(std::move(request));

  // Post a task and handle the request asynchronously. The reason is that the
  // requester won't have a label for the request until this function returns
  // and thus can not handle a response. Using base::Unretained is safe since
  // MediaStreamManager is deleted on the UI thread, after the IO thread has
  // been stopped.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamManager::SetUpRequest,
                                base::Unretained(this), label));
}

bool MediaStreamManager::TranslateSourceIdToDeviceId(
    MediaStreamType stream_type,
    const std::string& salt,
    const url::Origin& security_origin,
    const std::string& source_id,
    std::string* device_id) const {
  DCHECK(stream_type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         stream_type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  // The source_id can be empty if the constraint is set but empty.
  if (source_id.empty())
    return false;

  // TODO(guidou): Change to use MediaDevicesManager::EnumerateDevices.
  // See http://crbug.com/648155.
  blink::WebMediaDeviceInfoArray cached_devices =
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
    blink::MediaDeviceType type,
    const blink::WebMediaDeviceInfo& media_device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
         type == blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT);
  SendLogMessage(base::StringPrintf(
                     "StopRemovedDevice({type=%s}, {device=[id: %s, name: %s]}",
                     DeviceTypeToString(type),
                     media_device_info.device_id.c_str(),
                     media_device_info.label.c_str())
                     .c_str());

  MediaStreamType stream_type = ConvertToMediaStreamType(type);
  std::vector<base::UnguessableToken> session_ids;
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second.get();
    for (const MediaStreamDevice& device : request->devices) {
      const std::string source_id = GetHMACForMediaDeviceID(
          request->salt_and_origin.device_id_salt,
          request->salt_and_origin.origin, media_device_info.device_id);
      if (device.id == source_id && device.type == stream_type) {
        session_ids.push_back(device.session_id());
        if (request->device_stopped_cb) {
          request->device_stopped_cb.Run(labeled_request.first, device);
        }
      }
    }
  }
  for (const auto& session_id : session_ids)
    StopDevice(stream_type, session_id);
}

bool MediaStreamManager::PickDeviceId(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const TrackControls& controls,
    const blink::WebMediaDeviceInfoArray& devices,
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
    const blink::WebMediaDeviceInfoArray& devices,
    std::string* device_id) const {
  if (type == MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    return PickDeviceId(request->salt_and_origin, request->controls.audio,
                        devices, device_id);
  } else if (type == MediaStreamType::DEVICE_VIDEO_CAPTURE) {
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
  if (request->audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
      request->video_type() == MediaStreamType::DEVICE_VIDEO_CAPTURE) {
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
  SendLogMessage(
      base::StringPrintf("StartEnumeration({requester_id=%d}, {label=%s})",
                         request->requester_id, label.c_str()));

  // Start monitoring the devices when doing the first enumeration.
  media_devices_manager_->StartMonitoring();

  // Start enumeration for devices of all requested device types.
  bool request_audio_input =
      request->audio_type() != MediaStreamType::NO_SERVICE;
  if (request_audio_input)
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_REQUESTED);

  bool request_video_input =
      request->video_type() != MediaStreamType::NO_SERVICE;
  if (request_video_input)
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_REQUESTED);

  // base::Unretained is safe here because MediaStreamManager is deleted on the
  // UI thread, after the IO thread has been stopped.
  DCHECK(request_audio_input || request_video_input);
  MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT] =
      request_audio_input;
  devices_to_enumerate[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT] =
      request_video_input;
  media_devices_manager_->EnumerateDevices(
      devices_to_enumerate,
      base::BindOnce(&MediaStreamManager::DevicesEnumerated,
                     base::Unretained(this), request_audio_input,
                     request_video_input, label));
}

std::string MediaStreamManager::AddRequest(
    std::unique_ptr<DeviceRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create a label for this request and verify it is unique.
  std::string unique_label;
  do {
    unique_label = RandomLabel();
  } while (FindRequest(unique_label) != nullptr);

  SendLogMessage(
      base::StringPrintf("AddRequest([requester_id=%d]) => (label=%s)",
                         request->requester_id, unique_label.c_str()));
  requests_.push_back(std::make_pair(unique_label, std::move(request)));

  return unique_label;
}

MediaStreamManager::DeviceRequest* MediaStreamManager::FindRequest(
    const std::string& label) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    if (labeled_request.first == label)
      return labeled_request.second.get();
  }
  return nullptr;
}

void MediaStreamManager::DeleteRequest(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(
      base::StringPrintf("DeleteRequest([label=%s])", label.c_str()));
  for (auto request_it = requests_.begin(); request_it != requests_.end();
       ++request_it) {
    if (request_it->first == label) {
      // Clean up permission controller subscription.
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&MediaStreamManager::
                             UnsubscribeFromPermissionControllerOnUIThread,
                         request_it->second->requesting_process_id,
                         request_it->second->requesting_frame_id,
                         request_it->second->audio_subscription_id,
                         request_it->second->video_subscription_id));

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
  if (request->audio_type() == MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
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
  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;
  DCHECK(request->HasUIRequest());
  SendLogMessage(
      base::StringPrintf("PostRequestToUI({label=%s}, ", label.c_str()));

  const MediaStreamType audio_type = request->audio_type();
  const MediaStreamType video_type = request->video_type();

  // Post the request to UI and set the state.
  if (blink::IsAudioInputMediaType(audio_type))
    request->SetState(audio_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);
  if (blink::IsVideoInputMediaType(video_type))
    request->SetState(video_type, MEDIA_REQUEST_STATE_PENDING_APPROVAL);

  // If using the fake UI, it will just auto-select from the available devices.
  // The fake UI doesn't work for desktop sharing requests since we can't see
  // its devices from here; always use the real UI for such requests. The
  // processing below for MEDIA_GUM_DESKTOP_VIDEO_CAPTURE is for
  // media_stream_dispatcher_host_unittest only.
  if (fake_ui_factory_ &&
      (request->video_type() != MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
       !base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kUseFakeUIForMediaStream))) {
    MediaStreamDevices devices;
    if (request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE) {
      devices = DisplayMediaDevicesFromFakeDeviceConfig(
          request->audio_type() == MediaStreamType::DISPLAY_AUDIO_CAPTURE,
          request->requesting_process_id, request->requesting_frame_id);
    } else if (request->video_type() ==
               MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
      // Cache the |label| in the device name field, for unit test purpose only.
      devices.push_back(MediaStreamDevice(
          MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
          DesktopMediaID(DesktopMediaID::TYPE_SCREEN, DesktopMediaID::kNullId)
              .ToString(),
          label));
    } else {
      MediaStreamDevices audio_devices = ConvertToMediaStreamDevices(
          request->audio_type(),
          enumeration[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT]);
      MediaStreamDevices video_devices = ConvertToMediaStreamDevices(
          request->video_type(),
          enumeration[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT]);
      devices.reserve(audio_devices.size() + video_devices.size());
      devices.insert(devices.end(), audio_devices.begin(), audio_devices.end());
      devices.insert(devices.end(), video_devices.begin(), video_devices.end());
    }

    std::unique_ptr<FakeMediaStreamUIProxy> fake_ui = fake_ui_factory_.Run();
    fake_ui->SetAvailableDevices(devices);

    request->ui_proxy = std::move(fake_ui);
  } else if (!request->ui_proxy) {
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
  SendLogMessage(
      base::StringPrintf("SetUpRequest([requester_id=%d] {label=%s})",
                         request->requester_id, label.c_str()));

  request->SetAudioType(request->controls.audio.stream_type);
  request->SetVideoType(request->controls.video.stream_type);

  const bool is_display_capture =
      request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  if (is_display_capture && !SetUpDisplayCaptureRequest(request)) {
    FinalizeRequestFailed(label, request,
                          MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE);
    return;
  }

  const bool is_tab_capture =
      request->audio_type() == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
      request->video_type() == MediaStreamType::GUM_TAB_VIDEO_CAPTURE;
  if (is_tab_capture) {
    if (!SetUpTabCaptureRequest(request, label)) {
      FinalizeRequestFailed(label, request,
                            MediaStreamRequestResult::TAB_CAPTURE_FAILURE);
    }
    return;
  }

  const bool is_screen_capture =
      request->video_type() == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  if (is_screen_capture && !SetUpScreenCaptureRequest(request)) {
    FinalizeRequestFailed(label, request,
                          MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE);
    return;
  }

  if (!is_tab_capture && !is_screen_capture && !is_display_capture) {
    if (blink::IsDeviceMediaType(request->audio_type()) ||
        blink::IsDeviceMediaType(request->video_type())) {
      StartEnumeration(request, label);
      return;
    }
    // If no actual device capture is requested, set up the request with an
    // empty device list.
    if (!SetUpDeviceCaptureRequest(request, MediaDeviceEnumeration())) {
      FinalizeRequestFailed(label, request,
                            MediaStreamRequestResult::NO_HARDWARE);
      return;
    }
  }
  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
}

bool MediaStreamManager::SetUpDisplayCaptureRequest(DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->video_type() == MediaStreamType::DISPLAY_VIDEO_CAPTURE);

  // getDisplayMedia function does not permit the use of constraints for
  // selection of a source, see
  // https://w3c.github.io/mediacapture-screen-share/#constraints.
  if (!request->controls.video.requested ||
      !request->controls.video.device_id.empty() ||
      !request->controls.audio.device_id.empty()) {
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
  DCHECK((request->audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
          request->audio_type() == MediaStreamType::NO_SERVICE) &&
         (request->video_type() == MediaStreamType::DEVICE_VIDEO_CAPTURE ||
          request->video_type() == MediaStreamType::NO_SERVICE));
  SendLogMessage(base::StringPrintf(
      "SetUpDeviceCaptureRequest([requester_id=%d])", request->requester_id));
  std::string audio_device_id;
  if (request->controls.audio.requested &&
      !GetRequestedDeviceCaptureId(
          request, request->audio_type(),
          enumeration[blink::MEDIA_DEVICE_TYPE_AUDIO_INPUT],
          &audio_device_id)) {
    return false;
  }

  std::string video_device_id;
  if (request->controls.video.requested &&
      !GetRequestedDeviceCaptureId(
          request, request->video_type(),
          enumeration[blink::MEDIA_DEVICE_TYPE_VIDEO_INPUT],
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
  DCHECK(request->audio_type() == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
         request->video_type() == MediaStreamType::GUM_TAB_VIDEO_CAPTURE);

  std::string capture_device_id;
  if (!request->controls.audio.device_id.empty()) {
    capture_device_id = request->controls.audio.device_id;
  } else if (!request->controls.video.device_id.empty()) {
    capture_device_id = request->controls.video.device_id;
  } else {
    return false;
  }

  if ((request->audio_type() != MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
       request->audio_type() != MediaStreamType::NO_SERVICE) ||
      (request->video_type() != MediaStreamType::GUM_TAB_VIDEO_CAPTURE &&
       request->video_type() != MediaStreamType::NO_SERVICE)) {
    return false;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
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
      capture_device_id, requesting_process_id, requesting_frame_id,
      url::Origin::Create(origin), nullptr, kRegistryStreamTypeTab);
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
    FinalizeRequestFailed(label, request,
                          MediaStreamRequestResult::TAB_CAPTURE_FAILURE);
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
  DCHECK(request->audio_type() == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE ||
         request->video_type() == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);

  // For screen capture we only support two valid combinations:
  // (1) screen video capture only, or
  // (2) screen video capture with loopback audio capture.
  if (request->video_type() != MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
      (request->audio_type() != MediaStreamType::NO_SERVICE &&
       request->audio_type() != MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE)) {
    LOG(ERROR) << "Invalid screen capture request.";
    return false;
  }

  std::string video_device_id;
  if (request->video_type() == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE &&
      !request->controls.video.device_id.empty()) {
    video_device_id = request->controls.video.device_id;
  }

  const std::string audio_device_id =
      request->audio_type() == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE
          ? video_device_id
          : "";

  request->CreateUIRequest(audio_device_id, video_device_id);
  return true;
}

void MediaStreamManager::SetUpDesktopCaptureChangeSourceRequest(
    DeviceRequest* request,
    const std::string& label,
    const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsDesktopCaptureMediaType(request->video_type()));
  DCHECK(request->request_type() == blink::MEDIA_GENERATE_STREAM ||
         request->request_type() == blink::MEDIA_DEVICE_UPDATE);

  // Set up request type to bring up the picker again within a session.
  request->set_request_type(blink::MEDIA_DEVICE_UPDATE);

  request->CreateUIRequest(
      std::string() /* requested_audio_device_id */,
      media_id.is_null() ? std::string()
                         : media_id.ToString() /* requested_video_device_id */);

  ReadOutputParamsAndPostRequestToUI(label, request, MediaDeviceEnumeration());
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

  std::string hashed_source_id = GetHMACForMediaDeviceID(
      new_request.salt_and_origin.device_id_salt,
      new_request.salt_and_origin.origin, new_device.id);

  bool is_audio_capture =
      new_device.type == MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      new_request.audio_type() == MediaStreamType::DEVICE_AUDIO_CAPTURE;
  StreamSelectionStrategy strategy =
      new_request.audio_stream_selection_info_ptr->strategy;
  if (is_audio_capture &&
      strategy == blink::mojom::StreamSelectionStrategy::FORCE_NEW_STREAM) {
    return false;
  }

  base::Optional<base::UnguessableToken> requested_session_id =
      new_request.audio_stream_selection_info_ptr->session_id;
#if DCHECK_IS_ON()
  if (strategy == StreamSelectionStrategy::SEARCH_BY_SESSION_ID) {
    DCHECK(requested_session_id);
    DCHECK(!requested_session_id->is_empty());
  }
#endif
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const DeviceRequest* request = labeled_request.second.get();
    if (request->requesting_process_id == new_request.requesting_process_id &&
        request->requesting_frame_id == new_request.requesting_frame_id &&
        request->request_type() == new_request.request_type()) {
      for (const MediaStreamDevice& device : request->devices) {
        bool is_same_device =
            device.id == hashed_source_id && device.type == new_device.type;
        // If |strategy| is equal to SEARCH_BY_DEVICE_ID, the
        // search is performed only based on the |device.id|. If, however,
        // |strategy| is equal to SEARCH_BY_SESSION_ID, the
        // search also includes the session ID provided in the request.
        // NB: this only applies to audio. In case of media stream types that
        // are not an audio capture, the session id is always ignored.
        bool is_same_session =
            !is_audio_capture ||
            strategy == StreamSelectionStrategy::SEARCH_BY_DEVICE_ID ||
            (strategy == StreamSelectionStrategy::SEARCH_BY_SESSION_ID &&
             device.session_id() == *requested_session_id);

        if (is_same_device && is_same_session) {
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->generate_stream_cb);
  SendLogMessage(
      base::StringPrintf("FinalizeGenerateStream({label=%s}, {requester_id="
                         "%d}, {request_type=%s})",
                         label.c_str(), request->requester_id,
                         RequestTypeToString(request->request_type())));

  // Partition the array of devices into audio vs video.
  MediaStreamDevices audio_devices, video_devices;
  for (const MediaStreamDevice& device : request->devices) {
    if (blink::IsAudioInputMediaType(device.type))
      audio_devices.push_back(device);
    else if (blink::IsVideoInputMediaType(device.type))
      video_devices.push_back(device);
    else
      NOTREACHED();
  }

  // Subscribe to follow permission changes in order to close streams when the
  // user denies mic/camera.
  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamManager::SubscribeToPermissionControllerOnUIThread,
          base::Unretained(this), label, request->requesting_process_id,
          request->requesting_frame_id, request->requester_id,
          request->page_request_id, audio_devices.size() > 0,
          video_devices.size() > 0, request->salt_and_origin.origin.GetURL()));

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop and so outlives the IO thread.
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MediaDevicesPermissionChecker::
                         HasPanTiltZoomPermissionGrantedOnUIThread,
                     request->requesting_process_id,
                     request->requesting_frame_id),
      base::BindOnce(&MediaStreamManager::PanTiltZoomPermissionChecked,
                     base::Unretained(this), label, audio_devices,
                     video_devices));
}

void MediaStreamManager::PanTiltZoomPermissionChecked(
    const std::string& label,
    MediaStreamDevices audio_devices,
    MediaStreamDevices video_devices,
    bool pan_tilt_zoom_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;

  SendLogMessage(base::StringPrintf(
      "PanTiltZoomPermissionChecked({label=%s}, {requester_id="
      "%d}, {request_type=%s}, {pan_tilt_zoom_allowed=%d})",
      label.c_str(), request->requester_id,
      RequestTypeToString(request->request_type()), pan_tilt_zoom_allowed));

  std::move(request->generate_stream_cb)
      .Run(MediaStreamRequestResult::OK, label, audio_devices, video_devices,
           pan_tilt_zoom_allowed);
}

void MediaStreamManager::FinalizeRequestFailed(
    const std::string& label,
    DeviceRequest* request,
    MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf(
      "FinalizeRequestFailed({label=%s}, {requester_id=%d}, {result=%s})",
      label.c_str(), request->requester_id, RequestResultToString(result)));

  switch (request->request_type()) {
    case blink::MEDIA_GENERATE_STREAM: {
      DCHECK(request->generate_stream_cb);
      std::move(request->generate_stream_cb)
          .Run(result, std::string(), MediaStreamDevices(),
               MediaStreamDevices(), /*pan_tilt_zoom_allowed=*/false);
      break;
    }
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY: {
      if (request->open_device_cb) {
        std::move(request->open_device_cb)
            .Run(false /* success */, std::string(), MediaStreamDevice());
      }
      break;
    }
    case blink::MEDIA_DEVICE_ACCESS: {
      DCHECK(request->media_access_request_cb);
      std::move(request->media_access_request_cb)
          .Run(MediaStreamDevices(), std::move(request->ui_proxy));
      break;
    }
    case blink::MEDIA_DEVICE_UPDATE: {
      // Fail to change desktop capture source, keep everything unchanged and
      // bring the previous shared tab to the front.
      for (const auto& device : request->devices) {
        if (device.type == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE) {
          DesktopMediaID source = DesktopMediaID::Parse(device.id);
          DCHECK(source.type == DesktopMediaID::TYPE_WEB_CONTENTS);
          GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&MediaStreamManager::ActivateTabOnUIThread,
                             base::Unretained(this), source));
          break;
        }
      }
      return;
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
  SendLogMessage(
      base::StringPrintf("FinalizeOpenDevice({label=%s}, {requester_id="
                         "%d}, {request_type=%s})",
                         label.c_str(), request->requester_id,
                         RequestTypeToString(request->request_type())));
  if (request->open_device_cb) {
    std::move(request->open_device_cb)
        .Run(true /* success */, label, request->devices.front());
  }
}

void MediaStreamManager::FinalizeChangeDevice(const std::string& label,
                                              DeviceRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->device_changed_cb);
  SendLogMessage(
      base::StringPrintf("FinalizeChangeDevice({label=%s}, {requester_id="
                         "%d}, {request_type=%s})",
                         label.c_str(), request->requester_id,
                         RequestTypeToString(request->request_type())));

  std::vector<std::vector<MediaStreamDevice>> old_devices_by_type(
      static_cast<size_t>(MediaStreamType::NUM_MEDIA_TYPES));
  for (const auto& old_device : request->old_devices)
    old_devices_by_type[static_cast<size_t>(old_device.type)].push_back(
        old_device);

  for (const auto& new_device : request->devices) {
    MediaStreamDevice old_device;
    auto& old_devices = old_devices_by_type[static_cast<int>(new_device.type)];
    if (!old_devices.empty()) {
      old_device = old_devices.back();
      old_devices.pop_back();
    }

    request->device_changed_cb.Run(label, old_device, new_device);
  }

  for (const auto& old_devices : old_devices_by_type)
    for (const auto& old_device : old_devices)
      request->device_changed_cb.Run(label, old_device, MediaStreamDevice());
}

void MediaStreamManager::FinalizeMediaAccessRequest(
    const std::string& label,
    DeviceRequest* request,
    const MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request->media_access_request_cb);
  SendLogMessage(
      base::StringPrintf("FinalizeMediaAccessRequest({label=%s}, {requester_id="
                         "%d}, {request_type=%s})",
                         label.c_str(), request->requester_id,
                         RequestTypeToString(request->request_type())));
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
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&MediaStreamManager::InitializeMaybeAsync,
                                  base::Unretained(this),
                                  std::move(video_capture_provider)));
    return;
  }
  SendLogMessage(base::StringPrintf("InitializeMaybeAsync([this=%p])", this));

  // Store a pointer to |this| on the IO thread to avoid having to jump to
  // the UI thread to fetch a pointer to the MSM. In particular on Android,
  // it can be problematic to post to a UI thread from arbitrary worker
  // threads since attaching to the VM is required and we may have to access
  // the MSM from callback threads that we don't own and don't want to
  // attach.
  g_media_stream_manager_tls_ptr.Pointer()->Set(this);

  audio_input_device_manager_ = new AudioInputDeviceManager(audio_system_);
  audio_input_device_manager_->RegisterListener(this);

  // We want to be notified of IO message loop destruction to delete the thread
  // and the device managers.
  base::CurrentThread::Get()->AddDestructionObserver(this);

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

void MediaStreamManager::Opened(
    MediaStreamType stream_type,
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("Opened({stream_type=%s}, {session_id=%s})",
                                    StreamTypeToString(stream_type),
                                    capture_session_id.ToString().c_str()));

  // Find the request(s) containing this device and mark it as used.
  // It can be used in several requests since the same device can be
  // requested from the same web page.
  for (const LabeledDeviceRequest& labeled_request : requests_) {
    const std::string& label = labeled_request.first;
    DeviceRequest* request = labeled_request.second.get();
    for (MediaStreamDevice& device : request->devices) {
      if (device.type == stream_type &&
          device.session_id() == capture_session_id) {
        if (request->state(device.type) == MEDIA_REQUEST_STATE_DONE)
          continue;
        CHECK_EQ(request->state(device.type), MEDIA_REQUEST_STATE_OPENING);
        // We've found a matching request.
        request->SetState(device.type, MEDIA_REQUEST_STATE_DONE);

        if (blink::IsAudioInputMediaType(device.type)) {
          // Store the native audio parameters in the device struct.
          // TODO(xians): Handle the tab capture sample rate/channel layout
          // in AudioInputDeviceManager::Open().
          if (device.type != MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
            const MediaStreamDevice* opened_device =
                audio_input_device_manager_->GetOpenedDeviceById(
                    device.session_id());
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

  switch (request->request_type()) {
    case blink::MEDIA_OPEN_DEVICE_PEPPER_ONLY:
      FinalizeOpenDevice(label, request);
      OnStreamStarted(label);
      break;
    case blink::MEDIA_GENERATE_STREAM: {
      FinalizeGenerateStream(label, request);
      break;
    }
    case blink::MEDIA_DEVICE_UPDATE:
      FinalizeChangeDevice(label, request);
      OnStreamStarted(label);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void MediaStreamManager::Closed(
    MediaStreamType stream_type,
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("Closed({stream_type=%s}, {session_id=%s})",
                                    StreamTypeToString(stream_type),
                                    capture_session_id.ToString().c_str()));
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

  SendLogMessage(base::StringPrintf(
      "DevicesEnumerated({label=%s}, {requester_id=%d}, {request_type=%s})",
      label.c_str(), request->requester_id,
      RequestTypeToString(request->request_type())));

  bool requested[] = {requested_audio_input, requested_video_input};
  MediaStreamType stream_types[] = {MediaStreamType::DEVICE_AUDIO_CAPTURE,
                                    MediaStreamType::DEVICE_VIDEO_CAPTURE};
  for (size_t i = 0; i < base::size(requested); ++i) {
    if (!requested[i])
      continue;

    DCHECK(request->audio_type() == stream_types[i] ||
           request->video_type() == stream_types[i]);
    if (request->state(stream_types[i]) == MEDIA_REQUEST_STATE_REQUESTED) {
      request->SetState(stream_types[i], MEDIA_REQUEST_STATE_PENDING_APPROVAL);
    }
  }

  if (!SetUpDeviceCaptureRequest(request, enumeration))
    FinalizeRequestFailed(label, request,
                          MediaStreamRequestResult::NO_HARDWARE);
  else
    ReadOutputParamsAndPostRequestToUI(label, request, enumeration);
}

void MediaStreamManager::Aborted(
    MediaStreamType stream_type,
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf(
      "Aborted({stream_type=%s}, {session_id=%s})",
      StreamTypeToString(stream_type), capture_session_id.ToString().c_str()));
  StopDevice(stream_type, capture_session_id);
}

void MediaStreamManager::OnSuspend() {
  SendLogMessage(base::StringPrintf("OnSuspend([this=%p])", this));
}

void MediaStreamManager::OnResume() {
  SendLogMessage(base::StringPrintf("OnResume([this=%p])", this));
}

void MediaStreamManager::OnThermalStateChange(
    base::PowerObserver::DeviceThermalState new_state) {
  const char* state_name =
      base::PowerMonitorSource::DeviceThermalStateToString(new_state);
  SendLogMessage(base::StringPrintf(
      "OnThermalStateChange({this=%p}, {new_state=%s})", this, state_name));
}

void MediaStreamManager::UseFakeUIFactoryForTests(
    base::RepeatingCallback<std::unique_ptr<FakeMediaStreamUIProxy>(void)>
        fake_ui_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  fake_ui_factory_ = std::move(fake_ui_factory);
}

// static
void MediaStreamManager::RegisterNativeLogCallback(
    int renderer_host_id,
    base::RepeatingCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager* msm = g_media_stream_manager_tls_ptr.Pointer()->Get();
  if (!msm) {
    DLOG(ERROR) << "No MediaStreamManager on the IO thread.";
    return;
  }

  msm->DoNativeLogCallbackRegistration(renderer_host_id, std::move(callback));
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
  DeviceRequest* request = FindRequest(label);
  if (!request) {
    // The request has been canceled before the UI returned.
    return;
  }
  SendLogMessage(base::StringPrintf(
      "HandleAccessRequestResponse({label=%s}, {request=%s}, {result=%s})",
      label.c_str(), RequestTypeToString(request->request_type()),
      RequestResultToString(result)));

  if (request->request_type() == blink::MEDIA_DEVICE_ACCESS) {
    FinalizeMediaAccessRequest(label, request, devices);
    return;
  }

  // Handle the case when the request was denied.
  if (result != MediaStreamRequestResult::OK) {
    FinalizeRequestFailed(label, request, result);
    return;
  }
  DCHECK(!devices.empty());

  if (request->request_type() == blink::MEDIA_DEVICE_UPDATE) {
    HandleChangeSourceRequestResponse(label, request, devices);
    return;
  }

  // Process all newly-accepted devices for this request.
  bool found_audio = false;
  bool found_video = false;
  for (const MediaStreamDevice& media_stream_device : devices) {
    MediaStreamDevice device = media_stream_device;

    if (device.type == MediaStreamType::GUM_TAB_VIDEO_CAPTURE ||
        device.type == MediaStreamType::GUM_TAB_AUDIO_CAPTURE) {
      device.id = request->tab_capture_device_id;
    }

    // Initialize the sample_rate and channel_layout here since for audio
    // mirroring, we don't go through EnumerateDevices where these are usually
    // initialized.
    if (device.type == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
        device.type == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE) {
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
    if (request->request_type() == blink::MEDIA_GENERATE_STREAM) {
      MediaRequestState state;
      if (FindExistingRequestedDevice(*request, device, &device, &state)) {
        request->devices.push_back(device);
        request->SetState(device.type, state);
        SendLogMessage(base::StringPrintf(
            "HandleAccessRequestResponse([label=%s]) => "
            "(already opened device: [id: %s, session_id: %s])",
            label.c_str(), device.id.c_str(),
            device.session_id().ToString().c_str()));
        continue;
      }
    }
    device.set_session_id(GetDeviceManager(device.type)->Open(device));
    TranslateDeviceIdToSourceId(request, &device);
    request->devices.push_back(device);
    request->SetState(device.type, MEDIA_REQUEST_STATE_OPENING);
    SendLogMessage(
        base::StringPrintf("HandleAccessRequestResponse([label=%s]) => "
                           "(opening device: [id: %s, session_id: %s])",
                           label.c_str(), device.id.c_str(),
                           device.session_id().ToString().c_str()));
  }

  // Check whether we've received all stream types requested.
  if (!found_audio && blink::IsAudioInputMediaType(request->audio_type())) {
    request->SetState(request->audio_type(), MEDIA_REQUEST_STATE_ERROR);
    DVLOG(1) << "Set no audio found label " << label;
  }

  if (!found_video && blink::IsVideoInputMediaType(request->video_type()))
    request->SetState(request->video_type(), MEDIA_REQUEST_STATE_ERROR);

  if (RequestDone(*request))
    HandleRequestDone(label, request);
}

void MediaStreamManager::HandleChangeSourceRequestResponse(
    const std::string& label,
    DeviceRequest* request,
    const MediaStreamDevices& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "HandleChangeSourceRequestResponse("
           << ", {label = " << label << "})";

  request->old_devices.clear();
  request->old_devices.swap(request->devices);

  bool found_audio = false;
  for (const MediaStreamDevice& media_stream_device : devices) {
    MediaStreamDevice new_device = media_stream_device;
    found_audio |= blink::IsAudioInputMediaType(new_device.type);

    new_device.set_session_id(
        GetDeviceManager(new_device.type)->Open(new_device));
    request->SetState(new_device.type, MEDIA_REQUEST_STATE_OPENING);
    request->devices.push_back(new_device);
  }

  request->SetAudioType(found_audio ? request->controls.audio.stream_type
                                    : MediaStreamType::NO_SERVICE);
}

void MediaStreamManager::StopMediaStreamFromBrowser(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;

  SendLogMessage(base::StringPrintf("StopMediaStreamFromBrowser({label=%s})",
                                    label.c_str()));

  // Notify renderers that the devices in the stream will be stopped.
  if (request->device_stopped_cb) {
    for (const MediaStreamDevice& device : request->devices) {
      request->device_stopped_cb.Run(label, device);
    }
  }

  CancelRequest(label);
  IncrementDesktopCaptureCounter(DESKTOP_CAPTURE_NOTIFICATION_STOP);
}

void MediaStreamManager::ChangeMediaStreamSourceFromBrowser(
    const std::string& label,
    const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* request = FindRequest(label);
  if (!request)
    return;

  SendLogMessage(base::StringPrintf(
      "ChangeMediaStreamSourceFromBrowser({label=%s})", label.c_str()));

  SetUpDesktopCaptureChangeSourceRequest(request, label, media_id);
  IncrementDesktopCaptureCounter(DESKTOP_CAPTURE_NOTIFICATION_CHANGE_SOURCE);
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
    blink::MediaDeviceType device_type,
    const blink::WebMediaDeviceInfoArray& devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendLogMessage(base::StringPrintf("NotifyDevicesChanged({device_type=%s})",
                                    DeviceTypeToString(device_type)));

  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();

  MediaStreamType stream_type = ConvertToMediaStreamType(device_type);
  MediaStreamDevices new_devices =
      ConvertToMediaStreamDevices(stream_type, devices);

  if (blink::IsAudioInputMediaType(stream_type)) {
    MediaCaptureDevicesImpl::GetInstance()->OnAudioCaptureDevicesChanged(
        new_devices);
    if (media_observer)
      media_observer->OnAudioCaptureDevicesChanged();
  } else if (blink::IsVideoInputMediaType(stream_type)) {
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
  SendLogMessage(base::StringPrintf(
      "RequestDone({requester_id=%d}, {request_type=%s})", request.requester_id,
      RequestTypeToString(request.request_type())));

  const bool requested_audio =
      blink::IsAudioInputMediaType(request.audio_type());
  const bool requested_video =
      blink::IsVideoInputMediaType(request.video_type());

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
  if (blink::IsVideoInputMediaType(stream_type))
    return video_capture_manager();
  else if (blink::IsAudioInputMediaType(stream_type))
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

  if (!blink::IsVideoDesktopCaptureMediaType(video_type))
    return;

  // Pass along for desktop screen and window capturing when
  // DesktopCaptureDevice is used.
  for (const MediaStreamDevice& device : devices) {
    if (!blink::IsVideoDesktopCaptureMediaType(device.type))
      continue;

    DesktopMediaID media_id = DesktopMediaID::Parse(device.id);
    // WebContentsVideoCaptureDevice is used for tab/webcontents.
    if (media_id.type == DesktopMediaID::TYPE_WEB_CONTENTS)
      continue;
#if defined(USE_AURA)
    // DesktopCaptureDeviceAura is used when aura_id is valid.
    if (media_id.window_id > DesktopMediaID::kNullId)
      continue;
#endif
    video_capture_manager_->SetDesktopCaptureWindowId(device.session_id(),
                                                      window_id);
    break;
  }
}

void MediaStreamManager::DoNativeLogCallbackRegistration(
    int renderer_host_id,
    base::RepeatingCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Re-registering (overwriting) is allowed and happens in some tests.
  log_callbacks_[renderer_host_id] = std::move(callback);
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
void MediaStreamManager::GetMediaDeviceIDForHMAC(
    MediaStreamType stream_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stream_type == MediaStreamType::DEVICE_AUDIO_CAPTURE ||
         stream_type == MediaStreamType::DEVICE_VIDEO_CAPTURE);
  blink::MediaDeviceType device_type = ConvertToMediaDeviceType(stream_type);
  MediaStreamManager::GetMediaDeviceIDForHMAC(
      device_type, std::move(salt), std::move(security_origin),
      std::move(hmac_device_id), std::move(task_runner), std::move(callback));
}

void MediaStreamManager::GetMediaDeviceIDForHMAC(
    blink::MediaDeviceType device_type,
    std::string salt,
    url::Origin security_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(const base::Optional<std::string>&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager* msm = g_media_stream_manager_tls_ptr.Pointer()->Get();
  MediaDevicesManager::BoolDeviceTypes requested_types;
  requested_types[device_type] = true;
  msm->media_devices_manager()->EnumerateDevices(
      requested_types,
      base::BindOnce(&FinalizeGetMediaDeviceIDForHMAC, device_type,
                     std::move(salt), std::move(security_origin),
                     std::move(hmac_device_id), std::move(task_runner),
                     std::move(callback)));
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

void MediaStreamManager::SetCapturingLinkSecured(
    int render_process_id,
    const base::UnguessableToken& session_id,
    MediaStreamType type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (LabeledDeviceRequest& labeled_request : requests_) {
    DeviceRequest* request = labeled_request.second.get();
    if (request->requesting_process_id != render_process_id)
      continue;

    for (const MediaStreamDevice& device : request->devices) {
      if (device.session_id() == session_id && device.type == type) {
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
    const blink::WebMediaDeviceInfoArray& device_infos) {
  MediaStreamDevices devices;
  for (const auto& info : device_infos) {
    devices.emplace_back(stream_type, info.device_id, info.label,
                         info.video_control_support, info.video_facing,
                         info.group_id);
  }

  return devices;
}

void MediaStreamManager::ActivateTabOnUIThread(const DesktopMediaID source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh =
      RenderFrameHost::FromID(source.web_contents_id.render_process_id,
                              source.web_contents_id.main_render_frame_id);
  if (rfh)
    rfh->GetRenderViewHost()->GetDelegate()->Activate();
}

void MediaStreamManager::OnStreamStarted(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeviceRequest* const request = FindRequest(label);
  if (!request)
    return;
  SendLogMessage(base::StringPrintf(
      "OnStreamStarted({label=%s}, {requester_id=%d}, {request_type=%s})",
      label.c_str(), request->requester_id,
      RequestTypeToString(request->request_type())));

  // Show "Change source" button on notification bar only for tab sharing by
  // desktopCapture API.
  bool enable_change_source = std::any_of(
      request->devices.cbegin(), request->devices.cend(), [](auto device) {
        DesktopMediaID media_id = DesktopMediaID::Parse(device.id);
        return device.type == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE &&
               media_id.type == DesktopMediaID::TYPE_WEB_CONTENTS;
      });

  MediaStreamUI::SourceCallback device_changed_cb;
  if (enable_change_source &&
      base::FeatureList::IsEnabled(features::kDesktopCaptureChangeSource)) {
    device_changed_cb = base::BindRepeating(
        &MediaStreamManager::ChangeMediaStreamSourceFromBrowser,
        base::Unretained(this), label);
  }

  if (request->ui_proxy) {
    request->ui_proxy->OnStarted(
        base::BindOnce(&MediaStreamManager::StopMediaStreamFromBrowser,
                       base::Unretained(this), label),
        device_changed_cb,
        base::BindOnce(&MediaStreamManager::OnMediaStreamUIWindowId,
                       base::Unretained(this), request->video_type(),
                       request->devices));
  }
}

// static
PermissionControllerImpl* MediaStreamManager::GetPermissionController(
    int requesting_process_id,
    int requesting_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(requesting_process_id, requesting_frame_id);
  if (!rfh)
    return nullptr;

  return PermissionControllerImpl::FromBrowserContext(rfh->GetBrowserContext());
}

void MediaStreamManager::SubscribeToPermissionControllerOnUIThread(
    const std::string& label,
    int requesting_process_id,
    int requesting_frame_id,
    int requester_id,
    int page_request_id,
    bool is_audio_request,
    bool is_video_request,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionControllerImpl* controller =
      GetPermissionController(requesting_process_id, requesting_frame_id);
  if (!controller)
    return;

  int audio_subscription_id = PermissionControllerImpl::kNoPendingOperation;
  int video_subscription_id = PermissionControllerImpl::kNoPendingOperation;

  if (is_audio_request) {
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    audio_subscription_id = controller->SubscribePermissionStatusChange(
        PermissionType::AUDIO_CAPTURE,
        RenderFrameHost::FromID(requesting_process_id, requesting_frame_id),
        origin,
        base::BindRepeating(&MediaStreamManager::PermissionChangedCallback,
                            base::Unretained(this), requesting_process_id,
                            requesting_frame_id, requester_id,
                            page_request_id));
  }

  if (is_video_request) {
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    video_subscription_id = controller->SubscribePermissionStatusChange(
        PermissionType::VIDEO_CAPTURE,
        RenderFrameHost::FromID(requesting_process_id, requesting_frame_id),
        origin,
        base::BindRepeating(&MediaStreamManager::PermissionChangedCallback,
                            base::Unretained(this), requesting_process_id,
                            requesting_frame_id, requester_id,
                            page_request_id));
  }

  // It is safe to bind base::Unretained(this) because MediaStreamManager is
  // owned by BrowserMainLoop.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamManager::SetPermissionSubscriptionIDs,
                     base::Unretained(this), label, requesting_process_id,
                     requesting_frame_id, audio_subscription_id,
                     video_subscription_id));
}

void MediaStreamManager::SetPermissionSubscriptionIDs(
    const std::string& label,
    int requesting_process_id,
    int requesting_frame_id,
    int audio_subscription_id,
    int video_subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DeviceRequest* const request = FindRequest(label);
  if (!request) {
    // Something happened with the request while the permission subscription was
    // created, unsubscribe to clean up.
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaStreamManager::UnsubscribeFromPermissionControllerOnUIThread,
            requesting_process_id, requesting_frame_id, audio_subscription_id,
            video_subscription_id));

    return;
  }

  request->audio_subscription_id = audio_subscription_id;
  request->video_subscription_id = video_subscription_id;
}

// static
void MediaStreamManager::UnsubscribeFromPermissionControllerOnUIThread(
    int requesting_process_id,
    int requesting_frame_id,
    int audio_subscription_id,
    int video_subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionControllerImpl* controller =
      GetPermissionController(requesting_process_id, requesting_frame_id);
  if (!controller)
    return;

  controller->UnsubscribePermissionStatusChange(audio_subscription_id);
  controller->UnsubscribePermissionStatusChange(video_subscription_id);
}

void MediaStreamManager::PermissionChangedCallback(
    int requesting_process_id,
    int requesting_frame_id,
    int requester_id,
    int page_request_id,
    blink::mojom::PermissionStatus status) {
  if (status == blink::mojom::PermissionStatus::GRANTED)
    return;

  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // It is safe to bind base::Unretained(this) because MediaStreamManager is
    // owned by BrowserMainLoop.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaStreamManager::PermissionChangedCallback,
                       base::Unretained(this), requesting_process_id,
                       requesting_frame_id, requester_id, page_request_id,
                       status));

    return;
  }

  CancelRequest(requesting_process_id, requesting_frame_id, requester_id,
                page_request_id);
}

}  // namespace content
