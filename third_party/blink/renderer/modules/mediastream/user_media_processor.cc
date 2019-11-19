// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_processor.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_parameters.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/mediastream/local_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/video_capture/local_video_capturer_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/gfx/geometry/size.h"

namespace WTF {

template <>
struct CrossThreadCopier<blink::WebMediaStream>
    : public CrossThreadCopierPassThrough<blink::WebMediaStream> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<blink::WebUserMediaRequest>
    : public CrossThreadCopierPassThrough<blink::WebUserMediaRequest> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

using blink::mojom::MediaStreamRequestResult;
using blink::mojom::MediaStreamType;
using blink::mojom::StreamSelectionStrategy;
using EchoCancellationType =
    blink::AudioProcessingProperties::EchoCancellationType;

namespace {

void InitializeAudioTrackControls(const blink::WebUserMediaRequest& web_request,
                                  TrackControls* track_controls) {
  if (web_request.MediaRequestType() ==
      blink::WebUserMediaRequest::MediaType::kDisplayMedia) {
    track_controls->requested = true;
    track_controls->stream_type = MediaStreamType::DISPLAY_AUDIO_CAPTURE;
    return;
  }

  DCHECK_EQ(blink::WebUserMediaRequest::MediaType::kUserMedia,
            web_request.MediaRequestType());
  const blink::WebMediaConstraints& constraints =
      web_request.AudioConstraints();
  DCHECK(!constraints.IsNull());
  track_controls->requested = true;

  MediaStreamType* stream_type = &track_controls->stream_type;
  *stream_type = MediaStreamType::NO_SERVICE;

  String source_constraint =
      constraints.Basic().media_stream_source.Exact().empty()
          ? String()
          : String(constraints.Basic().media_stream_source.Exact()[0]);
  if (!source_constraint.IsEmpty()) {
    if (source_constraint == blink::kMediaStreamSourceTab) {
      *stream_type = MediaStreamType::GUM_TAB_AUDIO_CAPTURE;
    } else if (source_constraint == blink::kMediaStreamSourceDesktop ||
               source_constraint == blink::kMediaStreamSourceSystem) {
      *stream_type = MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;
    }
  } else {
    *stream_type = MediaStreamType::DEVICE_AUDIO_CAPTURE;
  }
}

void InitializeVideoTrackControls(const blink::WebUserMediaRequest& web_request,
                                  TrackControls* track_controls) {
  if (web_request.MediaRequestType() ==
      blink::WebUserMediaRequest::MediaType::kDisplayMedia) {
    track_controls->requested = true;
    track_controls->stream_type = MediaStreamType::DISPLAY_VIDEO_CAPTURE;
    return;
  }

  DCHECK_EQ(blink::WebUserMediaRequest::MediaType::kUserMedia,
            web_request.MediaRequestType());
  const blink::WebMediaConstraints& constraints =
      web_request.VideoConstraints();
  DCHECK(!constraints.IsNull());
  track_controls->requested = true;

  MediaStreamType* stream_type = &track_controls->stream_type;
  *stream_type = MediaStreamType::NO_SERVICE;

  String source_constraint =
      constraints.Basic().media_stream_source.Exact().empty()
          ? String()
          : String(constraints.Basic().media_stream_source.Exact()[0]);
  if (!source_constraint.IsEmpty()) {
    if (source_constraint == blink::kMediaStreamSourceTab) {
      *stream_type = MediaStreamType::GUM_TAB_VIDEO_CAPTURE;
    } else if (source_constraint == blink::kMediaStreamSourceDesktop ||
               source_constraint == blink::kMediaStreamSourceScreen) {
      *stream_type = MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
    }
  } else {
    *stream_type = MediaStreamType::DEVICE_VIDEO_CAPTURE;
  }
}

bool IsSameDevice(const MediaStreamDevice& device,
                  const MediaStreamDevice& other_device) {
  return device.id == other_device.id && device.type == other_device.type &&
         device.session_id() == other_device.session_id();
}

bool IsSameSource(const blink::WebMediaStreamSource& source,
                  const blink::WebMediaStreamSource& other_source) {
  blink::WebPlatformMediaStreamSource* const source_extra_data =
      source.GetPlatformSource();
  const MediaStreamDevice& device = source_extra_data->device();

  blink::WebPlatformMediaStreamSource* const other_source_extra_data =
      other_source.GetPlatformSource();
  const MediaStreamDevice& other_device = other_source_extra_data->device();

  return IsSameDevice(device, other_device);
}

void SurfaceAudioProcessingSettings(blink::WebMediaStreamSource* source) {
  blink::MediaStreamAudioSource* source_impl =
      static_cast<blink::MediaStreamAudioSource*>(source->GetPlatformSource());

  // If the source is a processed source, get the properties from it.
  if (auto* processed_source =
          blink::ProcessedLocalAudioSource::From(source_impl)) {
    blink::AudioProcessingProperties properties =
        processed_source->audio_processing_properties();
    WebMediaStreamSource::EchoCancellationMode echo_cancellation_mode;

    switch (properties.echo_cancellation_type) {
      case EchoCancellationType::kEchoCancellationDisabled:
        echo_cancellation_mode =
            WebMediaStreamSource::EchoCancellationMode::kDisabled;
        break;
      case EchoCancellationType::kEchoCancellationAec3:
        echo_cancellation_mode =
            WebMediaStreamSource::EchoCancellationMode::kBrowser;
        break;
      case EchoCancellationType::kEchoCancellationSystem:
        echo_cancellation_mode =
            WebMediaStreamSource::EchoCancellationMode::kSystem;
        break;
    }

    source->SetAudioProcessingProperties(echo_cancellation_mode,
                                         properties.goog_auto_gain_control,
                                         properties.goog_noise_suppression);
  } else {
    // If the source is not a processed source, it could still support system
    // echo cancellation. Surface that if it does.
    media::AudioParameters params = source_impl->GetAudioParameters();
    const WebMediaStreamSource::EchoCancellationMode echo_cancellation_mode =
        params.IsValid() &&
                (params.effects() & media::AudioParameters::ECHO_CANCELLER)
            ? WebMediaStreamSource::EchoCancellationMode::kSystem
            : WebMediaStreamSource::EchoCancellationMode::kDisabled;

    source->SetAudioProcessingProperties(echo_cancellation_mode, false, false);
  }
}

// TODO(crbug.com/704136): Check all places where this helper is used.
// Change their types from using std::vector to WTF::Vector, so this
// extra conversion round is not needed.
template <typename T>
std::vector<T> ToStdVector(const Vector<T>& format_vector) {
  std::vector<T> formats;
  std::copy(format_vector.begin(), format_vector.end(),
            std::back_inserter(formats));
  return formats;
}

media::VideoFacingMode ToMediaVideoFacingMode(
    mojom::blink::FacingMode facing_mode) {
  switch (facing_mode) {
    case mojom::FacingMode::NONE:
      return media::MEDIA_VIDEO_FACING_NONE;
    case mojom::FacingMode::USER:
      return media::MEDIA_VIDEO_FACING_USER;
    case mojom::FacingMode::ENVIRONMENT:
      return media::MEDIA_VIDEO_FACING_ENVIRONMENT;
    case mojom::FacingMode::LEFT:
    case mojom::FacingMode::RIGHT:
      NOTREACHED();
  }
  return media::MEDIA_VIDEO_FACING_NONE;
}

Vector<blink::VideoInputDeviceCapabilities> ToVideoInputDeviceCapabilities(
    const Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr>&
        input_capabilities) {
  Vector<blink::VideoInputDeviceCapabilities> capabilities;
  for (const auto& capability : input_capabilities) {
    // TODO(crbug.com/704136): Make the conversion from mojom::blink::FacingMode
    // to be handled automatically, eg by making media_devices.typemap work in
    // blink/renderer/platform/mojo/blink_typemaps.gni.
    capabilities.emplace_back(capability->device_id, capability->group_id,
                              capability->formats,
                              ToMediaVideoFacingMode(capability->facing_mode));
  }

  return capabilities;
}

}  // namespace

UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    const blink::WebUserMediaRequest& web_request,
    bool is_processing_user_gesture)
    : request_id(request_id),
      web_request(web_request),
      is_processing_user_gesture(is_processing_user_gesture) {}

// Class for storing state of the the processing of getUserMedia requests.
class UserMediaProcessor::RequestInfo final
    : public GarbageCollected<UserMediaProcessor::RequestInfo> {
 public:
  using ResourcesReady =
      base::OnceCallback<void(RequestInfo* request_info,
                              MediaStreamRequestResult result,
                              const String& result_name)>;
  enum class State {
    NOT_SENT_FOR_GENERATION,
    SENT_FOR_GENERATION,
    GENERATED,
  };

  explicit RequestInfo(std::unique_ptr<UserMediaRequestInfo> request);

  void StartAudioTrack(const blink::WebMediaStreamTrack& track,
                       bool is_pending);
  blink::WebMediaStreamTrack CreateAndStartVideoTrack(
      const blink::WebMediaStreamSource& source);

  // Triggers |callback| when all sources used in this request have either
  // successfully started, or a source has failed to start.
  void CallbackOnTracksStarted(ResourcesReady callback);

  // Called when a local audio source has finished (or failed) initializing.
  void OnAudioSourceStarted(blink::WebPlatformMediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const String& result_name);

  UserMediaRequestInfo* request() { return request_.get(); }
  int request_id() const { return request_->request_id; }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

  const blink::AudioCaptureSettings& audio_capture_settings() const {
    return audio_capture_settings_;
  }
  void SetAudioCaptureSettings(const blink::AudioCaptureSettings& settings,
                               bool is_content_capture) {
    DCHECK(settings.HasValue());
    is_audio_content_capture_ = is_content_capture;
    audio_capture_settings_ = settings;
  }
  const blink::VideoCaptureSettings& video_capture_settings() const {
    return video_capture_settings_;
  }
  bool is_video_content_capture() const {
    return video_capture_settings_.HasValue() && is_video_content_capture_;
  }
  bool is_video_device_capture() const {
    return video_capture_settings_.HasValue() && !is_video_content_capture_;
  }
  void SetVideoCaptureSettings(const blink::VideoCaptureSettings& settings,
                               bool is_content_capture) {
    DCHECK(settings.HasValue());
    is_video_content_capture_ = is_content_capture;
    video_capture_settings_ = settings;
  }

  void SetDevices(Vector<MediaStreamDevice> audio_devices,
                  Vector<MediaStreamDevice> video_devices) {
    audio_devices_ = std::move(audio_devices);
    video_devices_ = std::move(video_devices);
  }

  void AddNativeVideoFormats(const String& device_id,
                             Vector<media::VideoCaptureFormat> formats) {
    video_formats_map_.insert(device_id, std::move(formats));
  }

  // Do not store or delete the returned pointer.
  Vector<media::VideoCaptureFormat>* GetNativeVideoFormats(
      const String& device_id) {
    auto it = video_formats_map_.find(device_id);
    CHECK(it != video_formats_map_.end());
    return &it->value;
  }

  const Vector<MediaStreamDevice>& audio_devices() const {
    return audio_devices_;
  }
  const Vector<MediaStreamDevice>& video_devices() const {
    return video_devices_;
  }

  bool CanStartTracks() const {
    return video_formats_map_.size() == video_devices_.size();
  }

  blink::WebMediaStream* web_stream() { return &web_stream_; }

  const blink::WebUserMediaRequest& web_request() const {
    return request_->web_request;
  }

  StreamControls* stream_controls() { return &stream_controls_; }

  bool is_processing_user_gesture() const {
    return request_->is_processing_user_gesture;
  }

  void Trace(Visitor* visitor) {}

 private:
  void OnTrackStarted(blink::WebPlatformMediaStreamSource* source,
                      MediaStreamRequestResult result,
                      const blink::WebString& result_name);

  // Checks if the sources for all tracks have been started and if so,
  // invoke the |ready_callback_|.  Note that the caller should expect
  // that |this| might be deleted when the function returns.
  void CheckAllTracksStarted();

  std::unique_ptr<UserMediaRequestInfo> request_;
  State state_ = State::NOT_SENT_FOR_GENERATION;
  blink::AudioCaptureSettings audio_capture_settings_;
  bool is_audio_content_capture_ = false;
  blink::VideoCaptureSettings video_capture_settings_;
  bool is_video_content_capture_ = false;
  blink::WebMediaStream web_stream_;
  StreamControls stream_controls_;
  ResourcesReady ready_callback_;
  MediaStreamRequestResult request_result_ = MediaStreamRequestResult::OK;
  String request_result_name_;
  // Sources used in this request.
  Vector<blink::WebMediaStreamSource> sources_;
  Vector<blink::WebPlatformMediaStreamSource*> sources_waiting_for_callback_;
  HashMap<String, Vector<media::VideoCaptureFormat>> video_formats_map_;
  Vector<MediaStreamDevice> audio_devices_;
  Vector<MediaStreamDevice> video_devices_;
};

// TODO(guidou): Initialize request_result_name_ as a null WTF::String.
// https://crbug.com/764293
UserMediaProcessor::RequestInfo::RequestInfo(
    std::unique_ptr<UserMediaRequestInfo> request)
    : request_(std::move(request)), request_result_name_("") {}

void UserMediaProcessor::RequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    bool is_pending) {
  DCHECK(track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio);
  DCHECK(web_request().Audio());
#if DCHECK_IS_ON()
  DCHECK(audio_capture_settings_.HasValue());
#endif
  blink::MediaStreamAudioSource* native_source =
      blink::MediaStreamAudioSource::From(track.Source());
  // Add the source as pending since OnTrackStarted will expect it to be there.
  sources_waiting_for_callback_.push_back(native_source);

  sources_.push_back(track.Source());
  bool connected = native_source->ConnectToTrack(track);
  if (!is_pending) {
    OnTrackStarted(native_source,
                   connected
                       ? MediaStreamRequestResult::OK
                       : MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO,
                   "");
  }
}

blink::WebMediaStreamTrack
UserMediaProcessor::RequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source) {
  DCHECK(source.GetType() == blink::WebMediaStreamSource::kTypeVideo);
  DCHECK(web_request().Video());
  DCHECK(video_capture_settings_.HasValue());
  blink::MediaStreamVideoSource* native_source =
      blink::MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return blink::MediaStreamVideoTrack::CreateVideoTrack(
      native_source, video_capture_settings_.track_adapter_settings(),
      video_capture_settings_.noise_reduction(), is_video_content_capture_,
      video_capture_settings_.min_frame_rate(),
      WTF::BindRepeating(&UserMediaProcessor::RequestInfo::OnTrackStarted,
                         WrapWeakPersistent(this)),
      true);
}

void UserMediaProcessor::RequestInfo::CallbackOnTracksStarted(
    ResourcesReady callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = std::move(callback);
  CheckAllTracksStarted();
}

void UserMediaProcessor::RequestInfo::OnTrackStarted(
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(1) << "OnTrackStarted result " << result;
  auto** it = std::find(sources_waiting_for_callback_.begin(),
                        sources_waiting_for_callback_.end(), source);
  DCHECK(it != sources_waiting_for_callback_.end());
  sources_waiting_for_callback_.erase(it);
  // All tracks must be started successfully. Otherwise the request is a
  // failure.
  if (result != MediaStreamRequestResult::OK) {
    request_result_ = result;
    request_result_name_ = result_name;
  }

  CheckAllTracksStarted();
}

void UserMediaProcessor::RequestInfo::CheckAllTracksStarted() {
  if (ready_callback_ && sources_waiting_for_callback_.IsEmpty()) {
    std::move(ready_callback_).Run(this, request_result_, request_result_name_);
    // NOTE: |this| might now be deleted.
  }
}

void UserMediaProcessor::RequestInfo::OnAudioSourceStarted(
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result,
    const String& result_name) {
  // Check if we're waiting to be notified of this source.  If not, then we'll
  // ignore the notification.
  if (base::Contains(sources_waiting_for_callback_, source))
    OnTrackStarted(source, result, result_name);
}

UserMediaProcessor::UserMediaProcessor(
    LocalFrame* frame,
    MediaDevicesDispatcherCallback media_devices_dispatcher_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : media_devices_dispatcher_cb_(std::move(media_devices_dispatcher_cb)),
      frame_(frame),
      task_runner_(std::move(task_runner)) {}

UserMediaProcessor::~UserMediaProcessor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Ensure StopAllProcessing() has been called by UserMediaClient.
  DCHECK(!current_request_info_ && !request_completed_cb_ &&
         !local_sources_.size());
}

UserMediaRequestInfo* UserMediaProcessor::CurrentRequest() {
  return current_request_info_ ? current_request_info_->request() : nullptr;
}

void UserMediaProcessor::ProcessRequest(
    std::unique_ptr<UserMediaRequestInfo> request,
    base::OnceClosure callback) {
  DCHECK(!request_completed_cb_);
  DCHECK(!current_request_info_);
  request_completed_cb_ = std::move(callback);
  current_request_info_ = MakeGarbageCollected<RequestInfo>(std::move(request));
  blink::WebRtcLogMessage(base::StringPrintf(
      "UMP::ProcessRequest. request_id=%d. Has audio=%d. Has video=%d.",
      current_request_info_->request_id(),
      current_request_info_->request()->web_request.Audio(),
      current_request_info_->request()->web_request.Video()));
  // TODO(guidou): Set up audio and video in parallel.
  if (current_request_info_->web_request().Audio()) {
    SetupAudioInput();
    return;
  }
  SetupVideoInput();
}

void UserMediaProcessor::SetupAudioInput() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->web_request().Audio());
  blink::WebRtcLogMessage(base::StringPrintf(
      "UMP::SetupAudioInput. request_id=%d, audio constraints=%s",
      current_request_info_->request_id(),
      current_request_info_->request()
          ->web_request.AudioConstraints()
          .ToString()
          .Utf8()
          .c_str()));

  auto& audio_controls = current_request_info_->stream_controls()->audio;
  InitializeAudioTrackControls(current_request_info_->web_request(),
                               &audio_controls);

  if (audio_controls.stream_type == MediaStreamType::DISPLAY_AUDIO_CAPTURE) {
    SelectAudioSettings(current_request_info_->web_request(),
                        {blink::AudioDeviceCaptureCapability()});
    return;
  }

  if (blink::IsDeviceMediaType(audio_controls.stream_type)) {
    blink::WebRtcLogMessage(
        base::StringPrintf("UMP::SetupAudioInput. request_id=%d, "
                           "Requesting device capabilities",
                           current_request_info_->request_id()));
    GetMediaDevicesDispatcher()->GetAudioInputCapabilities(WTF::Bind(
        &UserMediaProcessor::SelectAudioDeviceSettings,
        WrapWeakPersistent(this), current_request_info_->web_request()));
  } else {
    if (!blink::IsAudioInputMediaType(audio_controls.stream_type)) {
      String failed_constraint_name =
          String(current_request_info_->web_request()
                     .AudioConstraints()
                     .Basic()
                     .media_stream_source.GetName());
      MediaStreamRequestResult result =
          MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    SelectAudioSettings(current_request_info_->web_request(),
                        {blink::AudioDeviceCaptureCapability()});
  }
}

void UserMediaProcessor::SelectAudioDeviceSettings(
    const blink::WebUserMediaRequest& web_request,
    Vector<blink::mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  blink::AudioDeviceCaptureCapabilities capabilities;
  for (const auto& device : audio_input_capabilities) {
    // Find the first occurrence of blink::MediaStreamAudioSource that matches
    // the same device ID as |device|. If more than one exists, any such source
    // will contain the same non-reconfigurable settings that limit the
    // associated capabilities.
    blink::MediaStreamAudioSource* audio_source = nullptr;
    auto* it =
        std::find_if(local_sources_.begin(), local_sources_.end(),
                     [&device](const blink::WebMediaStreamSource& web_source) {
                       DCHECK(!web_source.IsNull());
                       return WTF::String(web_source.Id()) == device->device_id;
                     });
    if (it != local_sources_.end()) {
      blink::WebPlatformMediaStreamSource* const source =
          it->GetPlatformSource();
      if (source->device().type == MediaStreamType::DEVICE_AUDIO_CAPTURE)
        audio_source = static_cast<blink::MediaStreamAudioSource*>(source);
    }
    if (audio_source) {
      capabilities.emplace_back(audio_source);
    } else {
      capabilities.emplace_back(device->device_id, device->group_id,
                                device->parameters);
    }
  }

  SelectAudioSettings(web_request, capabilities);
}

void UserMediaProcessor::SelectAudioSettings(
    const blink::WebUserMediaRequest& web_request,
    const blink::AudioDeviceCaptureCapabilities& capabilities) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The frame might reload or |web_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(web_request))
    return;

  DCHECK(current_request_info_->stream_controls()->audio.requested);
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::SelectAudioSettings. request_id=%d.",
                         current_request_info_->request_id()));
  auto settings = SelectSettingsAudioCapture(
      capabilities, web_request.AudioConstraints(),
      web_request.ShouldDisableHardwareNoiseSuppression(),
      true /* is_reconfiguration_allowed */);
  if (!settings.HasValue()) {
    String failed_constraint_name = String(settings.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.IsEmpty()
            ? MediaStreamRequestResult::NO_HARDWARE
            : MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
    GetUserMediaRequestFailed(result, failed_constraint_name);
    return;
  }
  if (current_request_info_->stream_controls()->audio.stream_type !=
      MediaStreamType::DISPLAY_AUDIO_CAPTURE) {
    current_request_info_->stream_controls()->audio.device_id =
        settings.device_id();
    current_request_info_->stream_controls()->disable_local_echo =
        settings.disable_local_echo();
  }
  current_request_info_->SetAudioCaptureSettings(
      settings,
      !blink::IsDeviceMediaType(
          current_request_info_->stream_controls()->audio.stream_type));

  // No further audio setup required. Continue with video.
  SetupVideoInput();
}

base::Optional<base::UnguessableToken>
UserMediaProcessor::DetermineExistingAudioSessionId() {
  DCHECK(current_request_info_->web_request().Audio());

  auto settings = current_request_info_->audio_capture_settings();
  auto device_id = settings.device_id();

  // Create a copy of the blink::WebMediaStreamSource objects that are
  // associated to the same audio device capture based on its device ID.
  Vector<blink::WebMediaStreamSource> matching_sources;
  std::copy_if(local_sources_.begin(), local_sources_.end(),
               std::back_inserter(matching_sources),
               [&device_id](const blink::WebMediaStreamSource& web_source) {
                 DCHECK(!web_source.IsNull());
                 return web_source.Id().Utf8() == device_id;
               });

  // Return the session ID associated to the source that has the same settings
  // that have been previously selected, if one exists.
  if (!matching_sources.IsEmpty()) {
    for (auto& matching_source : matching_sources) {
      blink::MediaStreamAudioSource* audio_source =
          static_cast<blink::MediaStreamAudioSource*>(
              matching_source.GetPlatformSource());
      if (audio_source->HasSameReconfigurableSettings(
              settings.audio_processing_properties())) {
        return audio_source->device().session_id();
      }
    }
  }

  return base::nullopt;
}

void UserMediaProcessor::SetupVideoInput() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);

  if (!current_request_info_->web_request().Video()) {
    base::Optional<base::UnguessableToken> audio_session_id =
        DetermineExistingAudioSessionId();
    GenerateStreamForCurrentRequestInfo(
        audio_session_id, audio_session_id.has_value()
                              ? StreamSelectionStrategy::SEARCH_BY_SESSION_ID
                              : StreamSelectionStrategy::FORCE_NEW_STREAM);
    return;
  }
  blink::WebRtcLogMessage(base::StringPrintf(
      "UMP::SetupVideoInput. request_id=%d, video constraints=%s",
      current_request_info_->request_id(),
      current_request_info_->request()
          ->web_request.VideoConstraints()
          .ToString()
          .Utf8()
          .c_str()));

  auto& video_controls = current_request_info_->stream_controls()->video;
  InitializeVideoTrackControls(current_request_info_->web_request(),
                               &video_controls);
  if (blink::IsDeviceMediaType(video_controls.stream_type)) {
    GetMediaDevicesDispatcher()->GetVideoInputCapabilities(WTF::Bind(
        &UserMediaProcessor::SelectVideoDeviceSettings,
        WrapWeakPersistent(this), current_request_info_->web_request()));
  } else {
    if (!blink::IsVideoInputMediaType(video_controls.stream_type)) {
      String failed_constraint_name =
          String(current_request_info_->web_request()
                     .VideoConstraints()
                     .Basic()
                     .media_stream_source.GetName());
      MediaStreamRequestResult result =
          MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    SelectVideoContentSettings();
  }
}

void UserMediaProcessor::SelectVideoDeviceSettings(
    const blink::WebUserMediaRequest& web_request,
    Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The frame might reload or |web_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(web_request))
    return;

  DCHECK(current_request_info_->stream_controls()->video.requested);
  DCHECK(blink::IsDeviceMediaType(
      current_request_info_->stream_controls()->video.stream_type));
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::SelectVideoDeviceSettings. request_id=%d.",
                         current_request_info_->request_id()));

  blink::VideoDeviceCaptureCapabilities capabilities;
  capabilities.device_capabilities =
      ToVideoInputDeviceCapabilities(video_input_capabilities);
  capabilities.noise_reduction_capabilities = {base::Optional<bool>(),
                                               base::Optional<bool>(true),
                                               base::Optional<bool>(false)};
  blink::VideoCaptureSettings settings = SelectSettingsVideoDeviceCapture(
      std::move(capabilities), web_request.VideoConstraints(),
      blink::MediaStreamVideoSource::kDefaultWidth,
      blink::MediaStreamVideoSource::kDefaultHeight,
      blink::MediaStreamVideoSource::kDefaultFrameRate);
  if (!settings.HasValue()) {
    String failed_constraint_name = String(settings.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.IsEmpty()
            ? MediaStreamRequestResult::NO_HARDWARE
            : MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
    GetUserMediaRequestFailed(result, failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->video.device_id =
      settings.device_id();
  current_request_info_->SetVideoCaptureSettings(
      settings, false /* is_content_capture */);

  if (current_request_info_->web_request().Audio()) {
    base::Optional<base::UnguessableToken> audio_session_id =
        DetermineExistingAudioSessionId();
    GenerateStreamForCurrentRequestInfo(
        audio_session_id, audio_session_id.has_value()
                              ? StreamSelectionStrategy::SEARCH_BY_SESSION_ID
                              : StreamSelectionStrategy::FORCE_NEW_STREAM);
  } else {
    GenerateStreamForCurrentRequestInfo();
  }
}

void UserMediaProcessor::SelectVideoContentSettings() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::SelectVideoContentSettings. request_id=%d.",
                         current_request_info_->request_id()));
  gfx::Size screen_size = GetScreenSize();
  blink::VideoCaptureSettings settings =
      blink::SelectSettingsVideoContentCapture(
          current_request_info_->web_request().VideoConstraints(),
          current_request_info_->stream_controls()->video.stream_type,
          screen_size.width(), screen_size.height());
  if (!settings.HasValue()) {
    String failed_constraint_name = String(settings.failed_constraint_name());
    DCHECK(!failed_constraint_name.IsEmpty());
    GetUserMediaRequestFailed(
        MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED,
        failed_constraint_name);
    return;
  }
  if (current_request_info_->stream_controls()->video.stream_type !=
      MediaStreamType::DISPLAY_VIDEO_CAPTURE) {
    current_request_info_->stream_controls()->video.device_id =
        settings.device_id();
  }

  current_request_info_->SetVideoCaptureSettings(settings,
                                                 true /* is_content_capture */);
  GenerateStreamForCurrentRequestInfo();
}

void UserMediaProcessor::GenerateStreamForCurrentRequestInfo(
    base::Optional<base::UnguessableToken> requested_audio_capture_session_id,
    blink::mojom::StreamSelectionStrategy strategy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  blink::WebRtcLogMessage(base::StringPrintf(
      "UMP::GenerateStreamForCurrentRequestInfo. request_id=%d, "
      "audio device id=\"%s\", video device id=\"%s\"",
      current_request_info_->request_id(),
      current_request_info_->stream_controls()->audio.device_id.c_str(),
      current_request_info_->stream_controls()->video.device_id.c_str()));
  current_request_info_->set_state(RequestInfo::State::SENT_FOR_GENERATION);

  // The browser replies to this request by invoking OnStreamGenerated().
  GetMediaStreamDispatcherHost()->GenerateStream(
      current_request_info_->request_id(),
      *current_request_info_->stream_controls(),
      current_request_info_->is_processing_user_gesture(),
      blink::mojom::blink::StreamSelectionInfo::New(
          strategy, requested_audio_capture_session_id),
      WTF::Bind(&UserMediaProcessor::OnStreamGenerated,
                WrapWeakPersistent(this), current_request_info_->request_id()));
}

WebMediaStreamDeviceObserver*
UserMediaProcessor::GetMediaStreamDeviceObserver() {
  auto* media_stream_device_observer =
      media_stream_device_observer_for_testing_;
  if (frame_) {  // Can be null for tests.
    auto* web_frame = static_cast<WebLocalFrame*>(WebFrame::FromFrame(frame_));
    if (!web_frame || !web_frame->Client())
      return nullptr;

    // TODO(704136): Move ownership of |WebMediaStreamDeviceObserver| out of
    // RenderFrameImpl, back to UserMediaClient.
    media_stream_device_observer =
        web_frame->Client()->MediaStreamDeviceObserver();
    DCHECK(media_stream_device_observer);
  }

  return media_stream_device_observer;
}

void UserMediaProcessor::OnStreamGenerated(
    int request_id,
    MediaStreamRequestResult result,
    const String& label,
    const Vector<MediaStreamDevice>& audio_devices,
    const Vector<MediaStreamDevice>& video_devices) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (result != MediaStreamRequestResult::OK) {
    OnStreamGenerationFailed(request_id, result);
    return;
  }

  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcherHost is processing the request.
    DVLOG(1) << "Request ID not found";
    OnStreamGeneratedForCancelledRequest(audio_devices, video_devices);
    return;
  }
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::OnStreamGenerated. request_id=%d.",
                         current_request_info_->request_id()));

  current_request_info_->set_state(RequestInfo::State::GENERATED);

  for (const auto* devices : {&audio_devices, &video_devices}) {
    for (const auto& device : *devices) {
      blink::WebRtcLogMessage(base::StringPrintf(
          "UMP::OnStreamGenerated. request_id=%d, device id=\"%s\", "
          "device name=\"%s\"",
          request_id, device.id.c_str(), device.name.c_str()));
    }
  }

  current_request_info_->SetDevices(audio_devices, video_devices);

  if (video_devices.IsEmpty()) {
    StartTracks(label);
    return;
  }

  if (current_request_info_->is_video_content_capture()) {
    media::VideoCaptureFormat format =
        current_request_info_->video_capture_settings().Format();
    for (const auto& video_device : video_devices) {
      String video_device_id(video_device.id.data());
      current_request_info_->AddNativeVideoFormats(
          video_device_id,
          {media::VideoCaptureFormat(GetScreenSize(), format.frame_rate,
                                     format.pixel_format)});
    }
    StartTracks(label);
    return;
  }

  for (const auto& video_device : video_devices) {
    String video_device_id(video_device.id.data());
    GetMediaDevicesDispatcher()->GetAllVideoInputDeviceFormats(
        video_device_id,
        WTF::Bind(&UserMediaProcessor::GotAllVideoInputFormatsForDevice,
                  WrapWeakPersistent(this),
                  current_request_info_->web_request(), label,
                  video_device_id));
  }
}

void UserMediaProcessor::GotAllVideoInputFormatsForDevice(
    const blink::WebUserMediaRequest& web_request,
    const String& label,
    const String& device_id,
    const Vector<media::VideoCaptureFormat>& formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The frame might reload or |web_request| might be cancelled while video
  // formats are queried. Do nothing if a different request is being processed
  // at this point.
  if (!IsCurrentRequestInfo(web_request))
    return;

  current_request_info_->AddNativeVideoFormats(device_id, formats);
  if (current_request_info_->CanStartTracks())
    StartTracks(label);
}

gfx::Size UserMediaProcessor::GetScreenSize() {
  gfx::Size screen_size(blink::kDefaultScreenCastWidth,
                        blink::kDefaultScreenCastHeight);
  if (frame_) {  // Can be null in tests.
    blink::WebScreenInfo info =
        frame_->GetChromeClient().GetScreenInfo(*frame_);
    screen_size = gfx::Size(info.rect.width, info.rect.height);
  }
  return screen_size;
}

void UserMediaProcessor::OnStreamGeneratedForCancelledRequest(
    const Vector<MediaStreamDevice>& audio_devices,
    const Vector<MediaStreamDevice>& video_devices) {
  blink::WebRtcLogMessage("UMP::OnStreamGeneratedForCancelledRequest.");
  // Only stop the device if the device is not used in another MediaStream.
  for (auto* it = audio_devices.begin(); it != audio_devices.end(); ++it) {
    if (!FindLocalSource(*it)) {
      String id(it->id.data());
      GetMediaStreamDispatcherHost()->StopStreamDevice(
          id, it->serializable_session_id());
    }
  }

  for (auto* it = video_devices.begin(); it != video_devices.end(); ++it) {
    if (!FindLocalSource(*it)) {
      String id(it->id.data());
      GetMediaStreamDispatcherHost()->StopStreamDevice(
          id, it->serializable_session_id());
    }
  }
}

// static
void UserMediaProcessor::OnAudioSourceStartedOnAudioThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    UserMediaProcessor* weak_ptr,
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  PostCrossThreadTask(
      *task_runner.get(), FROM_HERE,
      CrossThreadBindOnce(&UserMediaProcessor::OnAudioSourceStarted,
                          WrapCrossThreadWeakPersistent(weak_ptr),
                          CrossThreadUnretained(source), result,
                          String(result_name)));
}

void UserMediaProcessor::OnAudioSourceStarted(
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result,
    const String& result_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto* it = pending_local_sources_.begin();
       it != pending_local_sources_.end(); ++it) {
    blink::WebPlatformMediaStreamSource* const source_extra_data =
        it->GetPlatformSource();
    if (source_extra_data != source)
      continue;
    if (result == MediaStreamRequestResult::OK)
      local_sources_.push_back((*it));
    pending_local_sources_.erase(it);

    NotifyCurrentRequestInfoOfAudioSourceStarted(source, result, result_name);
    return;
  }
}

void UserMediaProcessor::NotifyCurrentRequestInfoOfAudioSourceStarted(
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result,
    const String& result_name) {
  // The only request possibly being processed is |current_request_info_|.
  if (current_request_info_)
    current_request_info_->OnAudioSourceStarted(source, result, result_name);
}

void UserMediaProcessor::OnStreamGenerationFailed(
    int request_id,
    MediaStreamRequestResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is cancelled or the frame reloads while
    // MediaStreamDispatcherHost is processing the request.
    return;
  }
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::OnStreamGenerationFailed. request_id=%d.",
                         current_request_info_->request_id()));

  GetUserMediaRequestFailed(result);
  DeleteWebRequest(current_request_info_->web_request());
}

void UserMediaProcessor::OnDeviceStopped(const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "UserMediaProcessor::OnDeviceStopped("
           << "{device_id = " << device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happens to stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }
  // By creating |source| it is guaranteed that the blink::WebMediaStreamSource
  // object is valid during the cleanup.
  blink::WebMediaStreamSource source(*source_ptr);
  StopLocalSource(source, false);
  RemoveLocalSource(source);
}

void UserMediaProcessor::OnDeviceChanged(const MediaStreamDevice& old_device,
                                         const MediaStreamDevice& new_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "UserMediaProcessor::OnDeviceChange("
           << "{old_device_id = " << old_device.id
           << ", session id = " << old_device.session_id()
           << ", type = " << old_device.type << "}"
           << "{new_device_id = " << new_device.id
           << ", session id = " << new_device.session_id()
           << ", type = " << new_device.type << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(old_device);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happens to stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    DVLOG(1) << "failed to find existing source with device " << old_device.id;
    return;
  }

  if (old_device.type != MediaStreamType::NO_SERVICE &&
      new_device.type == MediaStreamType::NO_SERVICE) {
    // At present, this will only happen to the case that a new desktop capture
    // source without audio share is selected, then the previous audio capture
    // device should be stopped if existing.
    DCHECK(blink::IsAudioInputMediaType(old_device.type));
    OnDeviceStopped(old_device);
    return;
  }

  blink::WebPlatformMediaStreamSource* const source_impl =
      source_ptr->GetPlatformSource();
  source_impl->ChangeSource(new_device);
}

void UserMediaProcessor::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(current_request_info_);
}

blink::WebMediaStreamSource UserMediaProcessor::InitializeVideoSourceObject(
    const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (!source.GetPlatformSource()) {
    source.SetPlatformSource(CreateVideoSource(
        device, WTF::Bind(&UserMediaProcessor::OnLocalSourceStopped,
                          WrapWeakPersistent(this))));
    String device_id(device.id.data());
    source.SetCapabilities(ComputeCapabilitiesForVideoSource(
        // TODO(crbug.com/704136): Change ComputeCapabilitiesForVideoSource to
        // operate over WTF::Vector and WTF::String.
        blink::WebString::FromUTF8(device.id),
        ToStdVector(*current_request_info_->GetNativeVideoFormats(device_id)),
        device.video_facing, current_request_info_->is_video_device_capture(),
        device.group_id));
    local_sources_.push_back(source);
  }
  return source;
}

blink::WebMediaStreamSource UserMediaProcessor::InitializeAudioSourceObject(
    const MediaStreamDevice& device,
    bool* is_pending) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);

  *is_pending = true;

  // See if the source is already being initialized.
  auto* pending = FindPendingLocalSource(device);
  if (pending)
    return *pending;

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (source.GetPlatformSource()) {
    // The only return point for non-pending sources.
    *is_pending = false;
    return source;
  }

  // While sources are being initialized, keep them in a separate array.
  // Once they've finished initialized, they'll be moved over to local_sources_.
  // See OnAudioSourceStarted for more details.
  pending_local_sources_.push_back(source);

  blink::WebPlatformMediaStreamSource::ConstraintsRepeatingCallback
      source_ready = ConvertToBaseCallback(CrossThreadBindRepeating(
          &UserMediaProcessor::OnAudioSourceStartedOnAudioThread, task_runner_,
          WrapCrossThreadWeakPersistent(this)));

  std::unique_ptr<blink::MediaStreamAudioSource> audio_source =
      CreateAudioSource(device, std::move(source_ready));
  audio_source->SetStopCallback(WTF::Bind(
      &UserMediaProcessor::OnLocalSourceStopped, WrapWeakPersistent(this)));

#if DCHECK_IS_ON()
  for (const auto& local_source : local_sources_) {
    blink::WebPlatformMediaStreamSource* platform_source =
        static_cast<blink::WebPlatformMediaStreamSource*>(
            local_source.GetPlatformSource());
    DCHECK(platform_source);
    if (platform_source->device().id == audio_source->device().id) {
      blink::MediaStreamAudioSource* audio_platform_source =
          static_cast<blink::MediaStreamAudioSource*>(platform_source);
      DCHECK(audio_source->HasSameNonReconfigurableSettings(
          audio_platform_source));
    }
  }
#endif  // DCHECK_IS_ON()

  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.echo_cancellation = {true, false};
  capabilities.echo_cancellation_type.reserve(3);
  capabilities.echo_cancellation_type.emplace_back(
      blink::WebString::FromASCII(blink::kEchoCancellationTypeBrowser));
  capabilities.echo_cancellation_type.emplace_back(
      blink::WebString::FromASCII(blink::kEchoCancellationTypeAec3));
  if (device.input.effects() &
      (media::AudioParameters::ECHO_CANCELLER |
       media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER)) {
    capabilities.echo_cancellation_type.emplace_back(
        blink::WebString::FromASCII(blink::kEchoCancellationTypeSystem));
  }
  capabilities.auto_gain_control = {true, false};
  capabilities.noise_suppression = {true, false};
  capabilities.sample_size = {
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
  };
  auto device_parameters = audio_source->device().input;
  if (device_parameters.IsValid()) {
    capabilities.channel_count = {1, device_parameters.channels()};
    capabilities.sample_rate = {std::min(blink::kAudioProcessingSampleRate,
                                         device_parameters.sample_rate()),
                                std::max(blink::kAudioProcessingSampleRate,
                                         device_parameters.sample_rate())};
    double fallback_latency =
        static_cast<double>(blink::kFallbackAudioLatencyMs) / 1000;
    double min_latency, max_latency;
    std::tie(min_latency, max_latency) =
        blink::GetMinMaxLatenciesForAudioParameters(device_parameters);
    capabilities.latency = {std::min(fallback_latency, min_latency),
                            std::max(fallback_latency, max_latency)};
  }

  capabilities.device_id = blink::WebString::FromUTF8(device.id);
  if (device.group_id)
    capabilities.group_id = blink::WebString::FromUTF8(*device.group_id);

  source.SetPlatformSource(std::move(audio_source));  // Takes ownership.
  source.SetCapabilities(capabilities);
  return source;
}

std::unique_ptr<blink::MediaStreamAudioSource>
UserMediaProcessor::CreateAudioSource(
    const MediaStreamDevice& device,
    blink::WebPlatformMediaStreamSource::ConstraintsRepeatingCallback
        source_ready) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);

  StreamControls* stream_controls = current_request_info_->stream_controls();
  // If the audio device is a loopback device (for screen capture), or if the
  // constraints/effects parameters indicate no audio processing is needed,
  // create an efficient, direct-path MediaStreamAudioSource instance.
  blink::AudioProcessingProperties audio_processing_properties =
      current_request_info_->audio_capture_settings()
          .audio_processing_properties();
  if (blink::IsScreenCaptureMediaType(device.type) ||
      !blink::MediaStreamAudioProcessor::WouldModifyAudio(
          audio_processing_properties)) {
    return std::make_unique<blink::LocalMediaStreamAudioSource>(
        frame_, device,
        base::OptionalOrNullptr(current_request_info_->audio_capture_settings()
                                    .requested_buffer_size()),
        stream_controls->disable_local_echo, std::move(source_ready),
        task_runner_);
  }

  // The audio device is not associated with screen capture and also requires
  // processing.
  //
  // TODO(crbug.com/c/704136): Convert ProcessedLocalAudioSource ctor to
  // operate over LocalFrame instead of WebLocalFrame.
  WebLocalFrame* web_frame =
      frame_ ? static_cast<WebLocalFrame*>(WebFrame::FromFrame(frame_))
             : nullptr;
  return std::make_unique<blink::ProcessedLocalAudioSource>(
      web_frame, device, stream_controls->disable_local_echo,
      audio_processing_properties, std::move(source_ready), task_runner_);
}

std::unique_ptr<blink::MediaStreamVideoSource>
UserMediaProcessor::CreateVideoSource(
    const MediaStreamDevice& device,
    blink::WebPlatformMediaStreamSource::SourceStoppedCallback stop_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->video_capture_settings().HasValue());

  return std::make_unique<blink::MediaStreamVideoCapturerSource>(
      frame_, std::move(stop_callback), device,
      current_request_info_->video_capture_settings().capture_params(),
      WTF::BindRepeating(
          &blink::LocalVideoCapturerSource::Create,
          frame_->GetTaskRunner(blink::TaskType::kInternalMedia)));
}

void UserMediaProcessor::StartTracks(const String& label) {
  DCHECK(!current_request_info_->web_request().IsNull());
  if (auto* media_stream_device_observer = GetMediaStreamDeviceObserver()) {
    media_stream_device_observer->AddStream(
        blink::WebString(label),
        ToStdVector(current_request_info_->audio_devices()),
        ToStdVector(current_request_info_->video_devices()),
        WTF::BindRepeating(&UserMediaProcessor::OnDeviceStopped,
                           WrapWeakPersistent(this)),
        WTF::BindRepeating(&UserMediaProcessor::OnDeviceChanged,
                           WrapWeakPersistent(this)));
  }

  Vector<blink::WebMediaStreamTrack> audio_tracks(
      current_request_info_->audio_devices().size());
  CreateAudioTracks(current_request_info_->audio_devices(), &audio_tracks);

  Vector<blink::WebMediaStreamTrack> video_tracks(
      current_request_info_->video_devices().size());
  CreateVideoTracks(current_request_info_->video_devices(), &video_tracks);

  String blink_id = label;
  current_request_info_->web_stream()->Initialize(
      blink_id,
      WebVector<WebMediaStreamTrack>(audio_tracks.data(), audio_tracks.size()),
      WebVector<WebMediaStreamTrack>(video_tracks.data(), video_tracks.size()));

  // Wait for the tracks to be started successfully or to fail.
  current_request_info_->CallbackOnTracksStarted(
      WTF::Bind(&UserMediaProcessor::OnCreateNativeTracksCompleted,
                WrapWeakPersistent(this), label));
}

void UserMediaProcessor::CreateVideoTracks(
    const Vector<MediaStreamDevice>& devices,
    Vector<blink::WebMediaStreamTrack>* webkit_tracks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (WTF::wtf_size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource source =
        InitializeVideoSourceObject(devices[i]);
    (*webkit_tracks)[i] =
        current_request_info_->CreateAndStartVideoTrack(source);
  }
}

void UserMediaProcessor::CreateAudioTracks(
    const Vector<MediaStreamDevice>& devices,
    Vector<blink::WebMediaStreamTrack>* webkit_tracks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  Vector<MediaStreamDevice> overridden_audio_devices = devices;
  bool render_to_associated_sink =
      current_request_info_->audio_capture_settings().HasValue() &&
      current_request_info_->audio_capture_settings()
          .render_to_associated_sink();
  if (!render_to_associated_sink) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device id must
    // be removed.
    for (auto& device : overridden_audio_devices)
      device.matched_output_device_id.reset();
  }

  for (WTF::wtf_size_t i = 0; i < overridden_audio_devices.size(); ++i) {
    bool is_pending = false;
    blink::WebMediaStreamSource source =
        InitializeAudioSourceObject(overridden_audio_devices[i], &is_pending);
    (*webkit_tracks)[i].Initialize(source);
    current_request_info_->StartAudioTrack((*webkit_tracks)[i], is_pending);
    // At this point the source has started, and its audio parameters have been
    // set. Thus, all audio processing properties are known and can be surfaced
    // to |source|.
    SurfaceAudioProcessingSettings(&source);
  }
}

void UserMediaProcessor::OnCreateNativeTracksCompleted(
    const String& label,
    RequestInfo* request_info,
    MediaStreamRequestResult result,
    const String& constraint_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (result == MediaStreamRequestResult::OK) {
    GetUserMediaRequestSucceeded(*request_info->web_stream(),
                                 request_info->web_request());
    GetMediaStreamDispatcherHost()->OnStreamStarted(label);
  } else {
    GetUserMediaRequestFailed(result, constraint_name);

    for (auto& web_track : request_info->web_stream()->AudioTracks()) {
      blink::WebPlatformMediaStreamTrack* track =
          blink::WebPlatformMediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }

    for (auto& web_track : request_info->web_stream()->VideoTracks()) {
      blink::WebPlatformMediaStreamTrack* track =
          blink::WebPlatformMediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
  }

  DeleteWebRequest(request_info->web_request());
}

void UserMediaProcessor::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest web_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsCurrentRequestInfo(web_request));
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::GetUserMediaRequestSucceeded. request_id=%d",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClient/UserMediaProcessor are destroyed if the JavaScript
  // code request the frame to be destroyed within the scope of the callback.
  // Therefore, post a task to complete the request with a clean stack.
  task_runner_->PostTask(
      FROM_HERE,
      WTF::Bind(&UserMediaProcessor::DelayedGetUserMediaRequestSucceeded,
                WrapWeakPersistent(this), stream, web_request));
}

void UserMediaProcessor::DelayedGetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest web_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "UserMediaProcessor::DelayedGetUserMediaRequestSucceeded";
  blink::LogUserMediaRequestResult(MediaStreamRequestResult::OK);
  DeleteWebRequest(web_request);
  web_request.RequestSucceeded(stream);
}

void UserMediaProcessor::GetUserMediaRequestFailed(
    MediaStreamRequestResult result,
    const String& constraint_name) {
  DCHECK(current_request_info_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  blink::WebRtcLogMessage(
      base::StringPrintf("UMP::GetUserMediaRequestFailed. request_id=%d",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClient/UserMediaProcessor are destroyed if the JavaScript
  // code request the frame to be destroyed within the scope of the callback.
  // Therefore, post a task to complete the request with a clean stack.
  task_runner_->PostTask(
      FROM_HERE,
      WTF::Bind(&UserMediaProcessor::DelayedGetUserMediaRequestFailed,
                WrapWeakPersistent(this), current_request_info_->web_request(),
                result, constraint_name));
}

void UserMediaProcessor::DelayedGetUserMediaRequestFailed(
    blink::WebUserMediaRequest web_request,
    MediaStreamRequestResult result,
    const String& constraint_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  blink::LogUserMediaRequestResult(result);
  DeleteWebRequest(web_request);
  switch (result) {
    case MediaStreamRequestResult::OK:
    case MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      NOTREACHED();
      return;
    case MediaStreamRequestResult::PERMISSION_DENIED:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kPermissionDenied,
          "Permission denied");
      return;
    case MediaStreamRequestResult::PERMISSION_DISMISSED:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kPermissionDismissed,
          "Permission dismissed");
      return;
    case MediaStreamRequestResult::INVALID_STATE:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kInvalidState, "Invalid state");
      return;
    case MediaStreamRequestResult::NO_HARDWARE:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kDevicesNotFound,
          "Requested device not found");
      return;
    case MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kSecurityError,
          "Invalid security origin");
      return;
    case MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      web_request.RequestFailed(blink::WebUserMediaRequest::Error::kTabCapture,
                                "Error starting tab capture");
      return;
    case MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kScreenCapture,
          "Error starting screen capture");
      return;
    case MediaStreamRequestResult::CAPTURE_FAILURE:
      web_request.RequestFailed(blink::WebUserMediaRequest::Error::kCapture,
                                "Error starting capture");
      return;
    case MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      web_request.RequestFailedConstraint(constraint_name);
      return;
    case MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      web_request.RequestFailed(blink::WebUserMediaRequest::Error::kTrackStart,
                                "Could not start audio source");
      return;
    case MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      web_request.RequestFailed(blink::WebUserMediaRequest::Error::kTrackStart,
                                "Could not start video source");
      return;
    case MediaStreamRequestResult::NOT_SUPPORTED:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kNotSupported, "Not supported");
      return;
    case MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kFailedDueToShutdown,
          "Failed due to shutdown");
      return;
    case MediaStreamRequestResult::KILL_SWITCH_ON:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kKillSwitchOn);
      return;
    case MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      web_request.RequestFailed(
          blink::WebUserMediaRequest::Error::kSystemPermissionDenied,
          "Permission denied by system");
      return;
  }
  NOTREACHED();
  web_request.RequestFailed(
      blink::WebUserMediaRequest::Error::kPermissionDenied);
}

const blink::WebMediaStreamSource* UserMediaProcessor::FindLocalSource(
    const LocalStreamSources& sources,
    const MediaStreamDevice& device) const {
  for (const auto& local_source : sources) {
    blink::WebPlatformMediaStreamSource* const source =
        local_source.GetPlatformSource();
    const MediaStreamDevice& active_device = source->device();
    if (IsSameDevice(active_device, device))
      return &local_source;
  }
  return nullptr;
}

blink::WebMediaStreamSource UserMediaProcessor::FindOrInitializeSourceObject(
    const MediaStreamDevice& device) {
  const blink::WebMediaStreamSource* existing_source = FindLocalSource(device);
  if (existing_source) {
    DVLOG(1) << "Source already exists. Reusing source with id "
             << existing_source->Id().Utf8();
    return *existing_source;
  }

  blink::WebMediaStreamSource::Type type =
      blink::IsAudioInputMediaType(device.type)
          ? blink::WebMediaStreamSource::kTypeAudio
          : blink::WebMediaStreamSource::kTypeVideo;

  blink::WebMediaStreamSource source;
  source.Initialize(blink::WebString::FromUTF8(device.id), type,
                    blink::WebString::FromUTF8(device.name),
                    false /* remote */);
  if (device.group_id)
    source.SetGroupId(blink::WebString::FromUTF8(*device.group_id));

  DVLOG(1) << "Initialize source object :"
           << "id = " << source.Id().Utf8()
           << ", name = " << source.GetName().Utf8();
  return source;
}

bool UserMediaProcessor::RemoveLocalSource(
    const blink::WebMediaStreamSource& source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto* device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (IsSameSource(*device_it, source)) {
      local_sources_.erase(device_it);
      return true;
    }
  }

  // Check if the source was pending.
  for (auto* device_it = pending_local_sources_.begin();
       device_it != pending_local_sources_.end(); ++device_it) {
    if (IsSameSource(*device_it, source)) {
      blink::WebPlatformMediaStreamSource* const source_extra_data =
          source.GetPlatformSource();
      const bool is_audio_source =
          source.GetType() == blink::WebMediaStreamSource::kTypeAudio;
      NotifyCurrentRequestInfoOfAudioSourceStarted(
          source_extra_data,
          is_audio_source ? MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO
                          : MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO,
          String::FromUTF8(is_audio_source
                               ? "Failed to access audio capture device"
                               : "Failed to access video capture device"));
      pending_local_sources_.erase(device_it);
      return true;
    }
  }

  return false;
}

bool UserMediaProcessor::IsCurrentRequestInfo(int request_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_request_info_ &&
         current_request_info_->request_id() == request_id;
}

bool UserMediaProcessor::IsCurrentRequestInfo(
    const blink::WebUserMediaRequest& web_request) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_request_info_ &&
         current_request_info_->web_request() == web_request;
}

bool UserMediaProcessor::DeleteWebRequest(
    const blink::WebUserMediaRequest& web_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (current_request_info_ &&
      current_request_info_->web_request() == web_request) {
    current_request_info_ = nullptr;
    std::move(request_completed_cb_).Run();
    return true;
  }
  return false;
}

void UserMediaProcessor::StopAllProcessing() {
  if (current_request_info_) {
    switch (current_request_info_->state()) {
      case RequestInfo::State::SENT_FOR_GENERATION:
        // Let the browser process know that the previously sent request must be
        // canceled.
        GetMediaStreamDispatcherHost()->CancelRequest(
            current_request_info_->request_id());
        FALLTHROUGH;

      case RequestInfo::State::NOT_SENT_FOR_GENERATION:
        LogUserMediaRequestWithNoResult(
            blink::MEDIA_STREAM_REQUEST_NOT_GENERATED);
        break;

      case RequestInfo::State::GENERATED:
        LogUserMediaRequestWithNoResult(
            blink::MEDIA_STREAM_REQUEST_PENDING_MEDIA_TRACKS);
        break;
    }
    current_request_info_ = nullptr;
  }
  request_completed_cb_.Reset();

  // Loop through all current local sources and stop the sources.
  auto* it = local_sources_.begin();
  while (it != local_sources_.end()) {
    StopLocalSource(*it, true);
    it = local_sources_.erase(it);
  }
}

void UserMediaProcessor::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << "UserMediaProcessor::OnLocalSourceStopped";

  const bool some_source_removed = RemoveLocalSource(source);
  CHECK(some_source_removed);

  blink::WebPlatformMediaStreamSource* source_impl = source.GetPlatformSource();
  if (auto* media_stream_device_observer = GetMediaStreamDeviceObserver())
    media_stream_device_observer->RemoveStreamDevice(source_impl->device());

  String device_id(source_impl->device().id.data());
  GetMediaStreamDispatcherHost()->StopStreamDevice(
      device_id, source_impl->device().serializable_session_id());
}

void UserMediaProcessor::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  blink::WebPlatformMediaStreamSource* source_impl = source.GetPlatformSource();
  DVLOG(1) << "UserMediaProcessor::StopLocalSource("
           << "{device_id = " << source_impl->device().id << "})";

  if (notify_dispatcher) {
    if (auto* media_stream_device_observer = GetMediaStreamDeviceObserver())
      media_stream_device_observer->RemoveStreamDevice(source_impl->device());

    String device_id(source_impl->device().id.data());
    GetMediaStreamDispatcherHost()->StopStreamDevice(
        device_id, source_impl->device().serializable_session_id());
  }

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

bool UserMediaProcessor::HasActiveSources() const {
  return !local_sources_.IsEmpty();
}

blink::mojom::blink::MediaStreamDispatcherHost*
UserMediaProcessor::GetMediaStreamDispatcherHost() {
  if (!dispatcher_host_) {
    frame_->GetBrowserInterfaceBroker().GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver());
  }
  return dispatcher_host_.get();
}

blink::mojom::blink::MediaDevicesDispatcherHost*
UserMediaProcessor::GetMediaDevicesDispatcher() {
  return media_devices_dispatcher_cb_.Run();
}

const blink::AudioCaptureSettings&
UserMediaProcessor::AudioCaptureSettingsForTesting() const {
  DCHECK(current_request_info_);
  return current_request_info_->audio_capture_settings();
}

const blink::VideoCaptureSettings&
UserMediaProcessor::VideoCaptureSettingsForTesting() const {
  DCHECK(current_request_info_);
  return current_request_info_->video_capture_settings();
}

void UserMediaProcessor::SetMediaStreamDeviceObserverForTesting(
    WebMediaStreamDeviceObserver* media_stream_device_observer) {
  DCHECK(!GetMediaStreamDeviceObserver());
  DCHECK(media_stream_device_observer);
  media_stream_device_observer_for_testing_ = media_stream_device_observer;
}

}  // namespace blink
