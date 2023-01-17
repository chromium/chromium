// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_processor.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "media/capture/video_capture_types.h"
#include "media/webrtc/constants.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_source.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/mediastream/local_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/local_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints_consts.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

using blink::mojom::MediaStreamRequestResult;
using blink::mojom::MediaStreamType;
using blink::mojom::StreamSelectionStrategy;
using EchoCancellationType =
    blink::AudioProcessingProperties::EchoCancellationType;
using AudioSourceErrorCode = media::AudioCapturerSource::ErrorCode;

namespace {

const char* MediaStreamRequestResultToString(MediaStreamRequestResult value) {
  switch (value) {
    case MediaStreamRequestResult::OK:
      return "OK";
    case MediaStreamRequestResult::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case MediaStreamRequestResult::PERMISSION_DISMISSED:
      return "PERMISSION_DISMISSED";
    case MediaStreamRequestResult::INVALID_STATE:
      return "INVALID_STATE";
    case MediaStreamRequestResult::NO_HARDWARE:
      return "NO_HARDWARE";
    case MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      return "INVALID_SECURITY_ORIGIN";
    case MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      return "TAB_CAPTURE_FAILURE";
    case MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      return "SCREEN_CAPTURE_FAILURE";
    case MediaStreamRequestResult::CAPTURE_FAILURE:
      return "CAPTURE_FAILURE";
    case MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      return "CONSTRAINT_NOT_SATISFIED";
    case MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      return "TRACK_START_FAILURE_AUDIO";
    case MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      return "TRACK_START_FAILURE_VIDEO";
    case MediaStreamRequestResult::NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      return "FAILED_DUE_TO_SHUTDOWN";
    case MediaStreamRequestResult::KILL_SWITCH_ON:
      return "KILL_SWITCH_ON";
    case MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      return "SYSTEM_PERMISSION_DENIED";
    case MediaStreamRequestResult::DEVICE_IN_USE:
      return "DEVICE_IN_USE";
    case MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      return "NUM_MEDIA_REQUEST_RESULTS";
    default:
      NOTREACHED();
  }
  return "INVALID";
}

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("UMP::" + message);
}

void MaybeLogStreamDevice(const int32_t& request_id,
                          const String& label,
                          const absl::optional<MediaStreamDevice>& device) {
  if (!device.has_value()) {
    return;
  }

  SendLogMessage(base::StringPrintf(
      "OnStreamsGenerated({request_id=%d}, {label=%s}, {device=[id: %s, "
      "name: "
      "%s]})",
      request_id, label.Utf8().c_str(), device->id.c_str(),
      device->name.c_str()));
}

std::string GetTrackLogString(MediaStreamComponent* component,
                              bool is_pending) {
  String str = String::Format(
      "StartAudioTrack({track=[id: %s, enabled: %d]}, "
      "{is_pending=%d})",
      component->Id().Utf8().c_str(), component->Enabled(), is_pending);
  return str.Utf8();
}

std::string GetTrackSourceLogString(blink::MediaStreamAudioSource* source) {
  const MediaStreamDevice& device = source->device();
  StringBuilder builder;
  builder.AppendFormat("StartAudioTrack(source: {session_id=%s}, ",
                       device.session_id().ToString().c_str());
  builder.AppendFormat("{is_local_source=%d}, ", source->is_local_source());
  builder.AppendFormat("{device=[id: %s", device.id.c_str());
  if (device.group_id.has_value()) {
    builder.AppendFormat(", group_id: %s", device.group_id.value().c_str());
  }
  builder.AppendFormat(", name: %s", device.name.c_str());
  builder.Append(String("]})"));
  return builder.ToString().Utf8();
}

std::string GetOnTrackStartedLogString(
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result) {
  const MediaStreamDevice& device = source->device();
  String str = String::Format("OnTrackStarted({session_id=%s}, {result=%s})",
                              device.session_id().ToString().c_str(),
                              MediaStreamRequestResultToString(result));
  return str.Utf8();
}

bool IsSameDevice(const MediaStreamDevice& device,
                  const MediaStreamDevice& other_device) {
  return device.id == other_device.id && device.type == other_device.type &&
         device.session_id() == other_device.session_id();
}

bool IsSameSource(MediaStreamSource* source, MediaStreamSource* other_source) {
  WebPlatformMediaStreamSource* const source_extra_data =
      source->GetPlatformSource();
  const MediaStreamDevice& device = source_extra_data->device();

  WebPlatformMediaStreamSource* const other_source_extra_data =
      other_source->GetPlatformSource();
  const MediaStreamDevice& other_device = other_source_extra_data->device();

  return IsSameDevice(device, other_device);
}

void SurfaceAudioProcessingSettings(MediaStreamSource* source) {
  auto* source_impl =
      static_cast<blink::MediaStreamAudioSource*>(source->GetPlatformSource());

  // If the source is a processed source, get the properties from it.
  if (auto* processed_source =
          blink::ProcessedLocalAudioSource::From(source_impl)) {
    blink::AudioProcessingProperties properties =
        processed_source->audio_processing_properties();
    MediaStreamSource::EchoCancellationMode echo_cancellation_mode;

    switch (properties.echo_cancellation_type) {
      case EchoCancellationType::kEchoCancellationDisabled:
        echo_cancellation_mode =
            MediaStreamSource::EchoCancellationMode::kDisabled;
        break;
      case EchoCancellationType::kEchoCancellationAec3:
        echo_cancellation_mode =
            MediaStreamSource::EchoCancellationMode::kBrowser;
        break;
      case EchoCancellationType::kEchoCancellationSystem:
        echo_cancellation_mode =
            MediaStreamSource::EchoCancellationMode::kSystem;
        break;
    }

    source->SetAudioProcessingProperties(echo_cancellation_mode,
                                         properties.goog_auto_gain_control,
                                         properties.goog_noise_suppression);
  } else {
    // If the source is not a processed source, it could still support system
    // echo cancellation. Surface that if it does.
    media::AudioParameters params = source_impl->GetAudioParameters();
    const MediaStreamSource::EchoCancellationMode echo_cancellation_mode =
        params.IsValid() &&
                (params.effects() & media::AudioParameters::ECHO_CANCELLER)
            ? MediaStreamSource::EchoCancellationMode::kSystem
            : MediaStreamSource::EchoCancellationMode::kDisabled;

    source->SetAudioProcessingProperties(echo_cancellation_mode, false, false);
  }
}

// TODO(crbug.com/704136): Check all places where this helper is used.
// Change their types from using std::vector to WTF::Vector, so this
// extra conversion round is not needed.
template <typename T>
std::vector<T> ToStdVector(const Vector<T>& format_vector) {
  std::vector<T> formats;
  base::ranges::copy(format_vector, std::back_inserter(formats));
  return formats;
}

Vector<blink::VideoInputDeviceCapabilities> ToVideoInputDeviceCapabilities(
    const Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>&
        input_capabilities) {
  Vector<blink::VideoInputDeviceCapabilities> capabilities;
  for (const auto& capability : input_capabilities) {
    capabilities.emplace_back(capability->device_id, capability->group_id,
                              capability->control_support, capability->formats,
                              capability->facing_mode);
  }

  return capabilities;
}

String ErrorCodeToString(MediaStreamRequestResult result) {
  switch (result) {
    case MediaStreamRequestResult::PERMISSION_DENIED:
      return "Permission denied";
    case MediaStreamRequestResult::PERMISSION_DISMISSED:
      return "Permission dismissed";
    case MediaStreamRequestResult::INVALID_STATE:
      return "Invalid state";
    case MediaStreamRequestResult::NO_HARDWARE:
      return "Requested device not found";
    case MediaStreamRequestResult::INVALID_SECURITY_ORIGIN:
      return "Invalid security origin";
    case MediaStreamRequestResult::TAB_CAPTURE_FAILURE:
      return "Error starting tab capture";
    case MediaStreamRequestResult::SCREEN_CAPTURE_FAILURE:
      return "Error starting screen capture";
    case MediaStreamRequestResult::CAPTURE_FAILURE:
      return "Error starting capture";
    case MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO:
      return "Could not start audio source";
    case MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO:
      return "Could not start video source";
    case MediaStreamRequestResult::NOT_SUPPORTED:
      return "Not supported";
    case MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN:
      return "Failed due to shutdown";
    case MediaStreamRequestResult::KILL_SWITCH_ON:
      return "";
    case MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED:
      return "Permission denied by system";
    case MediaStreamRequestResult::DEVICE_IN_USE:
      return "Device in use";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

// Class for storing state of the the processing of getUserMedia requests.
class UserMediaProcessor::RequestInfo final
    : public GarbageCollected<UserMediaProcessor::RequestInfo> {
 public:
  using ResourcesReady =
      base::OnceCallback<void(RequestInfo* request_info,
                              MediaStreamRequestResult result,
                              const String& result_name)>;
  enum class State {
    kNotSentForGeneration,
    kSentForGeneration,
    kGenerated,
  };

  explicit RequestInfo(UserMediaRequest* request);

  void StartAudioTrack(MediaStreamComponent* component, bool is_pending);
  MediaStreamComponent* CreateAndStartVideoTrack(MediaStreamSource* source);

  // Triggers |callback| when all sources used in this request have either
  // successfully started, or a source has failed to start.
  void CallbackOnTracksStarted(ResourcesReady callback);

  // Called when a local audio source has finished (or failed) initializing.
  void OnAudioSourceStarted(blink::WebPlatformMediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const String& result_name);

  UserMediaRequest* request() { return request_; }
  int32_t request_id() const { return request_->request_id(); }

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

  void SetDevices(mojom::blink::StreamDevicesSetPtr stream_devices_set) {
    stream_devices_set_.stream_devices =
        std::move(stream_devices_set->stream_devices);
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

  void InitializeWebStreams(
      const String& label,
      const MediaStreamsComponentsVector& streams_components) {
    DCHECK(!streams_components.empty());

    // TODO(crbug.com/1313021): Refactor descriptors to make the assumption of
    // at most one audio and video track explicit.
    descriptors_ = MakeGarbageCollected<MediaStreamDescriptorVector>();
    for (const MediaStreamComponents* tracks : streams_components) {
      descriptors_->push_back(MakeGarbageCollected<MediaStreamDescriptor>(
          label,
          !tracks->audio_track_
              ? MediaStreamComponentVector()
              : MediaStreamComponentVector{tracks->audio_track_},
          !tracks->video_track_
              ? MediaStreamComponentVector()
              : MediaStreamComponentVector{tracks->video_track_}));
    }
  }

  bool CanStartTracks() const {
    return video_formats_map_.size() == count_video_devices();
  }

  MediaStreamDescriptorVector* descriptors() {
    DCHECK(descriptors_);
    return descriptors_;
  }

  const mojom::blink::StreamDevicesSet& devices_set() const {
    return stream_devices_set_;
  }

  StreamControls* stream_controls() { return &stream_controls_; }

  bool is_processing_user_gesture() const {
    return request_->has_transient_user_activation();
  }

  bool pan_tilt_zoom_allowed() const { return pan_tilt_zoom_allowed_; }
  void set_pan_tilt_zoom_allowed(bool pan_tilt_zoom_allowed) {
    pan_tilt_zoom_allowed_ = pan_tilt_zoom_allowed;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(request_);
    visitor->Trace(descriptors_);
    visitor->Trace(sources_);
  }

 private:
  void OnTrackStarted(blink::WebPlatformMediaStreamSource* source,
                      MediaStreamRequestResult result,
                      const blink::WebString& result_name);

  // Checks if the sources for all tracks have been started and if so,
  // invoke the |ready_callback_|.  Note that the caller should expect
  // that |this| might be deleted when the function returns.
  void CheckAllTracksStarted();

  size_t count_video_devices() const;

  Member<UserMediaRequest> request_;
  State state_ = State::kNotSentForGeneration;
  blink::AudioCaptureSettings audio_capture_settings_;
  bool is_audio_content_capture_ = false;
  blink::VideoCaptureSettings video_capture_settings_;
  bool is_video_content_capture_ = false;
  Member<MediaStreamDescriptorVector> descriptors_;
  StreamControls stream_controls_;
  ResourcesReady ready_callback_;
  MediaStreamRequestResult request_result_ = MediaStreamRequestResult::OK;
  String request_result_name_;
  // Sources used in this request.
  HeapVector<Member<MediaStreamSource>> sources_;
  Vector<blink::WebPlatformMediaStreamSource*> sources_waiting_for_callback_;
  HashMap<String, Vector<media::VideoCaptureFormat>> video_formats_map_;
  mojom::blink::StreamDevicesSet stream_devices_set_;
  bool pan_tilt_zoom_allowed_ = false;
};

// TODO(guidou): Initialize request_result_name_ as a null WTF::String.
// https://crbug.com/764293
UserMediaProcessor::RequestInfo::RequestInfo(UserMediaRequest* request)
    : request_(request), request_result_name_("") {}

void UserMediaProcessor::RequestInfo::StartAudioTrack(
    MediaStreamComponent* component,
    bool is_pending) {
  DCHECK(component->GetSourceType() == MediaStreamSource::kTypeAudio);
  DCHECK(request()->Audio());
#if DCHECK_IS_ON()
  DCHECK(audio_capture_settings_.HasValue());
#endif
  SendLogMessage(GetTrackLogString(component, is_pending));
  auto* native_source = MediaStreamAudioSource::From(component->Source());
  SendLogMessage(GetTrackSourceLogString(native_source));
  // Add the source as pending since OnTrackStarted will expect it to be there.
  sources_waiting_for_callback_.push_back(native_source);

  sources_.push_back(component->Source());
  bool connected = native_source->ConnectToInitializedTrack(component);
  if (!is_pending) {
    OnTrackStarted(native_source,
                   connected
                       ? MediaStreamRequestResult::OK
                       : MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO,
                   "");
  }
}

MediaStreamComponent* UserMediaProcessor::RequestInfo::CreateAndStartVideoTrack(
    MediaStreamSource* source) {
  DCHECK(source->GetType() == MediaStreamSource::kTypeVideo);
  DCHECK(request()->Video());
  DCHECK(video_capture_settings_.HasValue());
  SendLogMessage(base::StringPrintf(
      "UMP::RI::CreateAndStartVideoTrack({request_id=%d})", request_id()));

  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, video_capture_settings_.track_adapter_settings(),
      video_capture_settings_.noise_reduction(), is_video_content_capture_,
      video_capture_settings_.min_frame_rate(), video_capture_settings_.pan(),
      video_capture_settings_.tilt(), video_capture_settings_.zoom(),
      pan_tilt_zoom_allowed(),
      WTF::BindOnce(&UserMediaProcessor::RequestInfo::OnTrackStarted,
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
  SendLogMessage(GetOnTrackStartedLogString(source, result));
  auto** it = base::ranges::find(sources_waiting_for_callback_, source);
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
  if (ready_callback_ && sources_waiting_for_callback_.empty()) {
    std::move(ready_callback_).Run(this, request_result_, request_result_name_);
    // NOTE: |this| might now be deleted.
  }
}

size_t UserMediaProcessor::RequestInfo::count_video_devices() const {
  return base::ranges::count_if(
      stream_devices_set_.stream_devices.begin(),
      stream_devices_set_.stream_devices.end(),
      [](const mojom::blink::StreamDevicesPtr& stream_devices) {
        return stream_devices->video_device.has_value();
      });
}

void UserMediaProcessor::RequestInfo::OnAudioSourceStarted(
    blink::WebPlatformMediaStreamSource* source,
    MediaStreamRequestResult result,
    const String& result_name) {
  // Check if we're waiting to be notified of this source.  If not, then we'll
  // ignore the notification.
  if (base::Contains(sources_waiting_for_callback_, source)) {
    OnTrackStarted(source, result, result_name);
  }
}

UserMediaProcessor::UserMediaProcessor(
    LocalFrame* frame,
    MediaDevicesDispatcherCallback media_devices_dispatcher_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : dispatcher_host_(frame->DomWindow()),
      media_devices_dispatcher_cb_(std::move(media_devices_dispatcher_cb)),
      frame_(frame),
      task_runner_(std::move(task_runner)) {}

UserMediaProcessor::~UserMediaProcessor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Ensure StopAllProcessing() has been called by UserMediaClient.
  DCHECK(!current_request_info_ && !request_completed_cb_ &&
         !local_sources_.size());
}

UserMediaRequest* UserMediaProcessor::CurrentRequest() {
  return current_request_info_ ? current_request_info_->request() : nullptr;
}

void UserMediaProcessor::ProcessRequest(UserMediaRequest* request,
                                        base::OnceClosure callback) {
  DCHECK(!request_completed_cb_);
  DCHECK(!current_request_info_);
  request_completed_cb_ = std::move(callback);
  current_request_info_ = MakeGarbageCollected<RequestInfo>(request);
  SendLogMessage(
      base::StringPrintf("ProcessRequest({request_id=%d}, {audio=%d}, "
                         "{video=%d})",
                         current_request_info_->request_id(),
                         current_request_info_->request()->Audio(),
                         current_request_info_->request()->Video()));
  // TODO(guidou): Set up audio and video in parallel.
  if (current_request_info_->request()->Audio()) {
    SetupAudioInput();
    return;
  }
  SetupVideoInput();
}

void UserMediaProcessor::SetupAudioInput() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->request()->Audio());

  UserMediaRequest* const request = current_request_info_->request();

  SendLogMessage(base::StringPrintf(
      "SetupAudioInput({request_id=%d}, {constraints=%s})",
      current_request_info_->request_id(),
      request->AudioConstraints().ToString().Utf8().c_str()));

  StreamControls* const stream_controls =
      current_request_info_->stream_controls();
  stream_controls->exclude_system_audio = request->exclude_system_audio();

  stream_controls->suppress_local_audio_playback =
      request->suppress_local_audio_playback();

  TrackControls& audio_controls = stream_controls->audio;
  audio_controls.stream_type =
      (request->MediaRequestType() == UserMediaRequestType::kDisplayMediaSet)
          ? MediaStreamType::NO_SERVICE
          : request->AudioMediaStreamType();

  if (audio_controls.stream_type == MediaStreamType::DISPLAY_AUDIO_CAPTURE) {
    SelectAudioSettings(request, {blink::AudioDeviceCaptureCapability()});
    return;
  }

  if (blink::IsDeviceMediaType(audio_controls.stream_type)) {
    SendLogMessage(
        base::StringPrintf("SetupAudioInput({request_id=%d}) => "
                           "(Requesting device capabilities)",
                           current_request_info_->request_id()));
    GetMediaDevicesDispatcher()->GetAudioInputCapabilities(
        WTF::BindOnce(&UserMediaProcessor::SelectAudioDeviceSettings,
                      WrapWeakPersistent(this), WrapPersistent(request)));
  } else {
    if (!blink::IsAudioInputMediaType(audio_controls.stream_type)) {
      String failed_constraint_name = String(
          request->AudioConstraints().Basic().media_stream_source.GetName());
      MediaStreamRequestResult result =
          MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    SelectAudioSettings(request, {blink::AudioDeviceCaptureCapability()});
  }
}

void UserMediaProcessor::SelectAudioDeviceSettings(
    UserMediaRequest* user_media_request,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  blink::AudioDeviceCaptureCapabilities capabilities;
  for (const auto& device : audio_input_capabilities) {
    // Find the first occurrence of blink::ProcessedLocalAudioSource that
    // matches the same device ID as |device|. If more than one exists, any
    // such source will contain the same non-reconfigurable settings that limit
    // the associated capabilities.
    blink::MediaStreamAudioSource* audio_source = nullptr;
    auto* it = base::ranges::find_if(
        local_sources_, [&device](MediaStreamSource* source) {
          DCHECK(source);
          MediaStreamAudioSource* platform_source =
              MediaStreamAudioSource::From(source);
          ProcessedLocalAudioSource* processed_source =
              ProcessedLocalAudioSource::From(platform_source);
          return processed_source && source->Id() == device->device_id;
        });
    if (it != local_sources_.end()) {
      WebPlatformMediaStreamSource* const source = (*it)->GetPlatformSource();
      if (source->device().type == MediaStreamType::DEVICE_AUDIO_CAPTURE) {
        audio_source = static_cast<MediaStreamAudioSource*>(source);
      }
    }
    if (audio_source) {
      capabilities.emplace_back(audio_source);
    } else {
      capabilities.emplace_back(device->device_id, device->group_id,
                                device->parameters);
    }
  }

  SelectAudioSettings(user_media_request, capabilities);
}

void UserMediaProcessor::SelectAudioSettings(
    UserMediaRequest* user_media_request,
    const blink::AudioDeviceCaptureCapabilities& capabilities) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The frame might reload or |user_media_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(user_media_request)) {
    return;
  }

  DCHECK(current_request_info_->stream_controls()->audio.requested());
  SendLogMessage(base::StringPrintf("SelectAudioSettings({request_id=%d})",
                                    current_request_info_->request_id()));
  auto settings = SelectSettingsAudioCapture(
      capabilities, user_media_request->AudioConstraints(),
      current_request_info_->stream_controls()->audio.stream_type,
      user_media_request->ShouldDisableHardwareNoiseSuppression(),
      /*is_reconfiguration_allowed=*/true);
  if (!settings.HasValue()) {
    String failed_constraint_name = String(settings.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.empty()
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

absl::optional<base::UnguessableToken>
UserMediaProcessor::DetermineExistingAudioSessionId() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_->request()->Audio());

  auto settings = current_request_info_->audio_capture_settings();
  auto device_id = settings.device_id();

  // Create a copy of the MediaStreamSource objects that are
  // associated to the same audio device capture based on its device ID.
  HeapVector<Member<MediaStreamSource>> matching_sources;

  // Take a defensive copy, as local_sources_ can be modified during
  // destructions in GC runs triggered by the push_back allocation in this loop.
  // crbug.com/1238209
  HeapVector<Member<MediaStreamSource>> local_sources_copy = local_sources_;
  for (const auto& source : local_sources_copy) {
    MediaStreamSource* source_copy = source;
    if (source_copy->GetType() == MediaStreamSource::kTypeAudio &&
        source_copy->Id().Utf8() == device_id) {
      matching_sources.push_back(source_copy);
    }
  }

  // Return the session ID associated to the source that has the same settings
  // that have been previously selected, if one exists.
  if (!matching_sources.empty()) {
    for (auto& matching_source : matching_sources) {
      auto* audio_source = static_cast<MediaStreamAudioSource*>(
          matching_source->GetPlatformSource());
      if (audio_source->HasSameReconfigurableSettings(
              settings.audio_processing_properties())) {
        return audio_source->device().session_id();
      }
    }
  }

  return absl::nullopt;
}

void UserMediaProcessor::SetupVideoInput() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);

  UserMediaRequest* const request = current_request_info_->request();

  if (!request->Video()) {
    absl::optional<base::UnguessableToken> audio_session_id =
        DetermineExistingAudioSessionId();
    GenerateStreamForCurrentRequestInfo(
        audio_session_id, audio_session_id.has_value()
                              ? StreamSelectionStrategy::SEARCH_BY_SESSION_ID
                              : StreamSelectionStrategy::FORCE_NEW_STREAM);
    return;
  }
  SendLogMessage(base::StringPrintf(
      "SetupVideoInput. request_id=%d, video constraints=%s",
      current_request_info_->request_id(),
      request->VideoConstraints().ToString().Utf8().c_str()));

  auto& video_controls = current_request_info_->stream_controls()->video;
  video_controls.stream_type = request->VideoMediaStreamType();

  StreamControls* const stream_controls =
      current_request_info_->stream_controls();

  stream_controls->request_pan_tilt_zoom_permission =
      IsPanTiltZoomPermissionRequested(request->VideoConstraints());

  // TODO(crbug.com/1337788): Clean up naming inconsistency with
  // auto_select_all_screens.
  stream_controls->request_all_screens = request->auto_select_all_screens();

  stream_controls->exclude_self_browser_surface =
      request->exclude_self_browser_surface();

  stream_controls->preferred_display_surface =
      request->preferred_display_surface();

  stream_controls->dynamic_surface_switching_requested =
      request->dynamic_surface_switching_requested();

  if (blink::IsDeviceMediaType(video_controls.stream_type)) {
    GetMediaDevicesDispatcher()->GetVideoInputCapabilities(
        WTF::BindOnce(&UserMediaProcessor::SelectVideoDeviceSettings,
                      WrapWeakPersistent(this), WrapPersistent(request)));
  } else {
    if (!blink::IsVideoInputMediaType(video_controls.stream_type)) {
      String failed_constraint_name = String(
          request->VideoConstraints().Basic().media_stream_source.GetName());
      MediaStreamRequestResult result =
          MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
      GetUserMediaRequestFailed(result, failed_constraint_name);
      return;
    }
    SelectVideoContentSettings();
  }
}

// static
bool UserMediaProcessor::IsPanTiltZoomPermissionRequested(
    const MediaConstraints& constraints) {
  if (constraints.Basic().pan.IsPresent() ||
      constraints.Basic().tilt.IsPresent() ||
      constraints.Basic().zoom.IsPresent()) {
    return true;
  }

  for (const auto& advanced_set : constraints.Advanced()) {
    if (advanced_set.pan.IsPresent() || advanced_set.tilt.IsPresent() ||
        advanced_set.zoom.IsPresent()) {
      return true;
    }
  }

  return false;
}

void UserMediaProcessor::SelectVideoDeviceSettings(
    UserMediaRequest* user_media_request,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The frame might reload or |user_media_request| might be cancelled while
  // capabilities are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(user_media_request)) {
    return;
  }

  DCHECK(current_request_info_->stream_controls()->video.requested());
  DCHECK(blink::IsDeviceMediaType(
      current_request_info_->stream_controls()->video.stream_type));
  SendLogMessage(base::StringPrintf("SelectVideoDeviceSettings. request_id=%d.",
                                    current_request_info_->request_id()));

  blink::VideoDeviceCaptureCapabilities capabilities;
  capabilities.device_capabilities =
      ToVideoInputDeviceCapabilities(video_input_capabilities);
  capabilities.noise_reduction_capabilities = {absl::optional<bool>(),
                                               absl::optional<bool>(true),
                                               absl::optional<bool>(false)};
  blink::VideoCaptureSettings settings = SelectSettingsVideoDeviceCapture(
      std::move(capabilities), user_media_request->VideoConstraints(),
      blink::MediaStreamVideoSource::kDefaultWidth,
      blink::MediaStreamVideoSource::kDefaultHeight,
      blink::MediaStreamVideoSource::kDefaultFrameRate);
  if (!settings.HasValue()) {
    String failed_constraint_name = String(settings.failed_constraint_name());
    MediaStreamRequestResult result =
        failed_constraint_name.empty()
            ? MediaStreamRequestResult::NO_HARDWARE
            : MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED;
    GetUserMediaRequestFailed(result, failed_constraint_name);
    return;
  }
  current_request_info_->stream_controls()->video.device_id =
      settings.device_id();
  current_request_info_->SetVideoCaptureSettings(
      settings, false /* is_content_capture */);

  if (current_request_info_->request()->Audio()) {
    absl::optional<base::UnguessableToken> audio_session_id =
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
  SendLogMessage(
      base::StringPrintf("SelectVideoContentSettings. request_id=%d.",
                         current_request_info_->request_id()));
  gfx::Size screen_size = GetScreenSize();
  blink::VideoCaptureSettings settings =
      blink::SelectSettingsVideoContentCapture(
          current_request_info_->request()->VideoConstraints(),
          current_request_info_->stream_controls()->video.stream_type,
          screen_size.width(), screen_size.height());
  if (!settings.HasValue()) {
    String failed_constraint_name = String(settings.failed_constraint_name());
    DCHECK(!failed_constraint_name.empty());

    GetUserMediaRequestFailed(
        MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED,
        failed_constraint_name);
    return;
  }

  const MediaStreamType stream_type =
      current_request_info_->stream_controls()->video.stream_type;
  if (stream_type != MediaStreamType::DISPLAY_VIDEO_CAPTURE &&
      stream_type != MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB &&
      stream_type != MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET) {
    current_request_info_->stream_controls()->video.device_id =
        settings.device_id();
  }

  current_request_info_->SetVideoCaptureSettings(settings,
                                                 true /* is_content_capture */);
  GenerateStreamForCurrentRequestInfo();
}

void UserMediaProcessor::GenerateStreamForCurrentRequestInfo(
    absl::optional<base::UnguessableToken> requested_audio_capture_session_id,
    blink::mojom::StreamSelectionStrategy strategy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  SendLogMessage(base::StringPrintf(
      "GenerateStreamForCurrentRequestInfo({request_id=%d}, "
      "{audio.device_id=%s}, {video.device_id=%s})",
      current_request_info_->request_id(),
      current_request_info_->stream_controls()->audio.device_id.c_str(),
      current_request_info_->stream_controls()->video.device_id.c_str()));
  current_request_info_->set_state(RequestInfo::State::kSentForGeneration);

  // If SessionId is set, this request is for a transferred MediaStreamTrack and
  // GetOpenDevice() should be called.
  if (current_request_info_->request() &&
      current_request_info_->request()->IsTransferredTrackRequest()) {
    MediaStreamRequestResult result = MediaStreamRequestResult::INVALID_STATE;
    blink::mojom::blink::GetOpenDeviceResponsePtr response;
    GetMediaStreamDispatcherHost()->GetOpenDevice(
        current_request_info_->request_id(),
        *current_request_info_->request()->GetSessionId(),
        *current_request_info_->request()->GetTransferId(), &result, &response);
    GotOpenDevice(current_request_info_->request_id(), result,
                  std::move(response));
  } else {
    // The browser replies to this request by invoking OnStreamsGenerated().
    GetMediaStreamDispatcherHost()->GenerateStreams(
        current_request_info_->request_id(),
        *current_request_info_->stream_controls(),
        current_request_info_->is_processing_user_gesture(),
        mojom::blink::StreamSelectionInfo::New(
            strategy, requested_audio_capture_session_id),
        WTF::BindOnce(&UserMediaProcessor::OnStreamsGenerated,
                      WrapWeakPersistent(this),
                      current_request_info_->request_id()));
  }
}

WebMediaStreamDeviceObserver*
UserMediaProcessor::GetMediaStreamDeviceObserver() {
  auto* media_stream_device_observer =
      media_stream_device_observer_for_testing_;
  if (frame_) {  // Can be null for tests.
    auto* web_frame =
        static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(frame_));
    if (!web_frame || !web_frame->Client()) {
      return nullptr;
    }

    // TODO(704136): Move ownership of |WebMediaStreamDeviceObserver| out of
    // RenderFrameImpl, back to UserMediaClient.
    media_stream_device_observer =
        web_frame->Client()->MediaStreamDeviceObserver();
    DCHECK(media_stream_device_observer);
  }

  return media_stream_device_observer;
}

void UserMediaProcessor::GotOpenDevice(
    int32_t request_id,
    mojom::blink::MediaStreamRequestResult result,
    mojom::blink::GetOpenDeviceResponsePtr response) {
  if (result != MediaStreamRequestResult::OK) {
    OnStreamGenerationFailed(request_id, result);
    return;
  }

  mojom::blink::StreamDevicesPtr devices = mojom::blink::StreamDevices::New();
  if (IsAudioInputMediaType(response->device.type)) {
    devices->audio_device = response->device;
  } else if (IsVideoInputMediaType(response->device.type)) {
    devices->video_device = response->device;
  } else {
    NOTREACHED();
  }

  mojom::blink::StreamDevicesSetPtr stream_devices_set =
      mojom::blink::StreamDevicesSet::New();
  stream_devices_set->stream_devices.emplace_back(std::move(devices));
  OnStreamsGenerated(request_id, result, response->label,
                     std::move(stream_devices_set),
                     response->pan_tilt_zoom_allowed);
  current_request_info_->request()->FinalizeTransferredTrackInitialization(
      *current_request_info_->descriptors());
}

void UserMediaProcessor::OnStreamsGenerated(
    int32_t request_id,
    MediaStreamRequestResult result,
    const String& label,
    mojom::blink::StreamDevicesSetPtr stream_devices_set,
    bool pan_tilt_zoom_allowed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (result != MediaStreamRequestResult::OK) {
    DCHECK(!stream_devices_set);
    OnStreamGenerationFailed(request_id, result);
    return;
  }

  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcherHost is processing the request.
    SendLogMessage(base::StringPrintf(
        "OnStreamsGenerated([request_id=%d]) => (ERROR: invalid request ID)",
        request_id));
    for (const mojom::blink::StreamDevicesPtr& stream_devices :
         stream_devices_set->stream_devices) {
      OnStreamGeneratedForCancelledRequest(*stream_devices);
    }
    return;
  }

  current_request_info_->set_state(RequestInfo::State::kGenerated);
  current_request_info_->set_pan_tilt_zoom_allowed(pan_tilt_zoom_allowed);

  for (const mojom::blink::StreamDevicesPtr& stream_devices :
       stream_devices_set->stream_devices) {
    MaybeLogStreamDevice(request_id, label, stream_devices->audio_device);
    MaybeLogStreamDevice(request_id, label, stream_devices->video_device);
  }

  current_request_info_->SetDevices(stream_devices_set->Clone());

  if (base::ranges::none_of(
          stream_devices_set->stream_devices,
          [](const mojom::blink::StreamDevicesPtr& stream_devices) {
            return stream_devices->video_device.has_value();
          })) {
    StartTracks(label);
    return;
  }

  if (current_request_info_->is_video_content_capture()) {
    media::VideoCaptureFormat format =
        current_request_info_->video_capture_settings().Format();
    for (const mojom::blink::StreamDevicesPtr& stream_devices :
         stream_devices_set->stream_devices) {
      if (stream_devices->video_device.has_value()) {
        String video_device_id(stream_devices->video_device.value().id.data());
        current_request_info_->AddNativeVideoFormats(
            video_device_id,
            {media::VideoCaptureFormat(GetScreenSize(), format.frame_rate,
                                       format.pixel_format)});
      }
    }
    StartTracks(label);
    return;
  }

  for (const blink::mojom::blink::StreamDevicesPtr& stream_devices_ptr :
       stream_devices_set->stream_devices) {
    if (stream_devices_ptr->video_device.has_value()) {
      const MediaStreamDevice& video_device =
          stream_devices_ptr->video_device.value();

      Vector<String> video_device_ids;
      for (const mojom::blink::StreamDevicesPtr& stream_devices :
           stream_devices_set->stream_devices) {
        if (stream_devices->video_device.has_value()) {
          video_device_ids.push_back(
              stream_devices->video_device.value().id.data());
        }
      }

      SendLogMessage(base::StringPrintf(
          "OnStreamsGenerated({request_id=%d}, {label=%s}, {device=[id: %s, "
          "name: %s]}) => (Requesting video device formats)",
          request_id, label.Utf8().c_str(), video_device.id.c_str(),
          video_device.name.c_str()));
      String video_device_id(video_device.id.data());
      GetMediaDevicesDispatcher()->GetAllVideoInputDeviceFormats(
          video_device_id,
          WTF::BindOnce(&UserMediaProcessor::GotAllVideoInputFormatsForDevice,
                        WrapWeakPersistent(this),
                        WrapPersistent(current_request_info_->request()), label,
                        video_device_ids));
    }
  }
}

void UserMediaProcessor::GotAllVideoInputFormatsForDevice(
    UserMediaRequest* user_media_request,
    const String& label,
    const Vector<String>& device_ids,
    const Vector<media::VideoCaptureFormat>& formats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // The frame might reload or |user_media_request| might be cancelled while
  // video formats are queried. Do nothing if a different request is being
  // processed at this point.
  if (!IsCurrentRequestInfo(user_media_request)) {
    return;
  }

  // TODO(crbug.com/1336564): Remove the assumption that all devices support
  // the same video formats.
  for (const String& device_id : device_ids) {
    SendLogMessage(
        base::StringPrintf("GotAllVideoInputFormatsForDevice({request_id=%d}, "
                           "{label=%s}, {device=[id: %s]})",
                           current_request_info_->request_id(),
                           label.Utf8().c_str(), device_id.Utf8().c_str()));
    current_request_info_->AddNativeVideoFormats(device_id, formats);
  }
  if (current_request_info_->CanStartTracks()) {
    StartTracks(label);
  }
}

gfx::Size UserMediaProcessor::GetScreenSize() {
  gfx::Size screen_size(blink::kDefaultScreenCastWidth,
                        blink::kDefaultScreenCastHeight);
  if (frame_) {  // Can be null in tests.
    const display::ScreenInfo& info =
        frame_->GetChromeClient().GetScreenInfo(*frame_);
    screen_size = info.rect.size();
  }
  return screen_size;
}

void UserMediaProcessor::OnStreamGeneratedForCancelledRequest(
    const mojom::blink::StreamDevices& stream_devices) {
  SendLogMessage("OnStreamGeneratedForCancelledRequest()");
  // Only stop the device if the device is not used in another MediaStream.
  if (stream_devices.audio_device.has_value()) {
    const blink::MediaStreamDevice& audio_device =
        stream_devices.audio_device.value();
    if (!FindLocalSource(audio_device)) {
      GetMediaStreamDispatcherHost()->StopStreamDevice(
          String(audio_device.id.data()),
          audio_device.serializable_session_id());
    }
  }

  if (stream_devices.video_device.has_value()) {
    const blink::MediaStreamDevice& video_device =
        stream_devices.video_device.value();
    if (!FindLocalSource(video_device)) {
      GetMediaStreamDispatcherHost()->StopStreamDevice(
          String(video_device.id.data()),
          video_device.serializable_session_id());
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
        (*it)->GetPlatformSource();
    if (source_extra_data != source) {
      continue;
    }
    if (result == MediaStreamRequestResult::OK) {
      local_sources_.push_back((*it));
    }
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
  if (current_request_info_) {
    current_request_info_->OnAudioSourceStarted(source, result, result_name);
  }
}

void UserMediaProcessor::OnStreamGenerationFailed(
    int32_t request_id,
    MediaStreamRequestResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsCurrentRequestInfo(request_id)) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcherHost is processing the request.
    return;
  }
  SendLogMessage(base::StringPrintf("OnStreamGenerationFailed({request_id=%d})",
                                    current_request_info_->request_id()));

  GetUserMediaRequestFailed(result);
  DeleteUserMediaRequest(current_request_info_->request());
}

void UserMediaProcessor::OnDeviceStopped(const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf(
      "OnDeviceStopped({session_id=%s}, {device_id=%s})",
      device.session_id().ToString().c_str(), device.id.c_str()));

  MediaStreamSource* source = FindLocalSource(device);
  if (!source) {
    // This happens if the same device is used in several gUM requests or
    // if a user happens to stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }

  StopLocalSource(source, false);
  RemoveLocalSource(source);
}

void UserMediaProcessor::OnDeviceChanged(const MediaStreamDevice& old_device,
                                         const MediaStreamDevice& new_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(https://crbug.com/1017219): possibly useful in native logs as well.
  DVLOG(1) << "UserMediaProcessor::OnDeviceChange("
           << "{old_device_id = " << old_device.id
           << ", session id = " << old_device.session_id()
           << ", type = " << old_device.type << "}"
           << "{new_device_id = " << new_device.id
           << ", session id = " << new_device.session_id()
           << ", type = " << new_device.type << "})";

  MediaStreamSource* source = FindLocalSource(old_device);
  if (!source) {
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

  WebPlatformMediaStreamSource* const source_impl = source->GetPlatformSource();
  source_impl->ChangeSource(new_device);
}

void UserMediaProcessor::OnDeviceRequestStateChange(
    const MediaStreamDevice& device,
    const mojom::blink::MediaStreamStateChange new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf(
      "OnDeviceRequestStateChange({session_id=%s}, {device_id=%s}, "
      "{new_state=%s})",
      device.session_id().ToString().c_str(), device.id.c_str(),
      (new_state == mojom::blink::MediaStreamStateChange::PAUSE ? "PAUSE"
                                                                : "PLAY")));

  MediaStreamSource* source = FindLocalSource(device);
  if (!source) {
    // This happens if the same device is used in several guM requests or
    // if a user happens to stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }

  WebPlatformMediaStreamSource* const source_impl = source->GetPlatformSource();
  source_impl->SetSourceMuted(new_state ==
                              mojom::blink::MediaStreamStateChange::PAUSE);
  MediaStreamVideoSource* video_source =
      static_cast<blink::MediaStreamVideoSource*>(source_impl);
  if (!video_source) {
    return;
  }
  if (new_state == mojom::blink::MediaStreamStateChange::PAUSE) {
    if (video_source->IsRunning()) {
      video_source->StopForRestart(base::DoNothing(),
                                   /*send_black_frame=*/true);
    }
  } else if (new_state == mojom::blink::MediaStreamStateChange::PLAY) {
    if (video_source->IsStoppedForRestart()) {
      video_source->Restart(*video_source->GetCurrentFormat(),
                            base::DoNothing());
    }
  } else {
    NOTREACHED();
  }
}

void UserMediaProcessor::OnDeviceCaptureHandleChange(
    const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf(
      "OnDeviceCaptureHandleChange({session_id=%s}, {device_id=%s})",
      device.session_id().ToString().c_str(), device.id.c_str()));

  MediaStreamSource* const source = FindLocalSource(device);
  if (!source) {
    // This happens if the same device is used in several guM requests or
    // if a user happens to stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }

  source->OnDeviceCaptureHandleChange(device);
}

void UserMediaProcessor::Trace(Visitor* visitor) const {
  visitor->Trace(dispatcher_host_);
  visitor->Trace(frame_);
  visitor->Trace(current_request_info_);
  visitor->Trace(local_sources_);
  visitor->Trace(pending_local_sources_);
}

MediaStreamSource* UserMediaProcessor::InitializeVideoSourceObject(
    const MediaStreamDevice& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  SendLogMessage(base::StringPrintf(
      "UMP::InitializeVideoSourceObject({request_id=%d}, {device=[id: %s, "
      "name: %s]})",
      current_request_info_->request_id(), device.id.c_str(),
      device.name.c_str()));
  MediaStreamSource* existing_source = FindLocalSource(device);
  if (existing_source) {
    DVLOG(1) << "Source already exists. Reusing source with id "
             << existing_source->Id().Utf8();
    return existing_source;
  }

  auto video_source = CreateVideoSource(
      device, WTF::BindOnce(&UserMediaProcessor::OnLocalSourceStopped,
                            WrapWeakPersistent(this)));
  MediaStreamSource* source =
      InitializeSourceObject(device, std::move(video_source));

  String device_id(device.id.data());
  source->SetCapabilities(ComputeCapabilitiesForVideoSource(
      // TODO(crbug.com/704136): Change ComputeCapabilitiesForVideoSource to
      // operate over WTF::Vector.
      String::FromUTF8(device.id),
      ToStdVector(*current_request_info_->GetNativeVideoFormats(device_id)),
      static_cast<mojom::blink::FacingMode>(device.video_facing),
      current_request_info_->is_video_device_capture(), device.group_id));
  local_sources_.push_back(source);
  return source;
}

MediaStreamSource* UserMediaProcessor::InitializeAudioSourceObject(
    const MediaStreamDevice& device,
    bool* is_pending) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  SendLogMessage(
      base::StringPrintf("InitializeAudioSourceObject({session_id=%s})",
                         device.session_id().ToString().c_str()));

  *is_pending = true;

  // See if the source is already being initialized.
  auto* pending = FindPendingLocalSource(device);
  if (pending) {
    return pending;
  }

  MediaStreamSource* existing_source = FindLocalSource(device);
  if (existing_source) {
    DVLOG(1) << "Source already exists. Reusing source with id "
             << existing_source->Id().Utf8();
    // The only return point for non-pending sources.
    *is_pending = false;
    return existing_source;
  }

  blink::WebPlatformMediaStreamSource::ConstraintsRepeatingCallback
      source_ready = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &UserMediaProcessor::OnAudioSourceStartedOnAudioThread, task_runner_,
          WrapCrossThreadWeakPersistent(this)));

  std::unique_ptr<blink::MediaStreamAudioSource> audio_source =
      CreateAudioSource(device, std::move(source_ready));
  audio_source->SetStopCallback(WTF::BindOnce(
      &UserMediaProcessor::OnLocalSourceStopped, WrapWeakPersistent(this)));

#if DCHECK_IS_ON()
  for (auto local_source : local_sources_) {
    auto* platform_source = static_cast<WebPlatformMediaStreamSource*>(
        local_source->GetPlatformSource());
    DCHECK(platform_source);
    if (platform_source->device().id == audio_source->device().id &&
        IsAudioInputMediaType(platform_source->device().type)) {
      auto* audio_platform_source =
          static_cast<MediaStreamAudioSource*>(platform_source);
      auto* processed_existing_source =
          ProcessedLocalAudioSource::From(audio_platform_source);
      auto* processed_new_source =
          ProcessedLocalAudioSource::From(audio_source.get());
      if (processed_new_source && processed_existing_source) {
        DCHECK(audio_source->HasSameNonReconfigurableSettings(
            audio_platform_source));
      }
    }
  }
#endif  // DCHECK_IS_ON()

  MediaStreamSource::Capabilities capabilities;
  capabilities.echo_cancellation = {true, false};
  capabilities.echo_cancellation_type.reserve(3);
  capabilities.echo_cancellation_type.emplace_back(
      String::FromUTF8(kEchoCancellationTypeBrowser));
  capabilities.echo_cancellation_type.emplace_back(
      String::FromUTF8(kEchoCancellationTypeAec3));
  if (device.input.effects() &
      (media::AudioParameters::ECHO_CANCELLER |
       media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER)) {
    capabilities.echo_cancellation_type.emplace_back(
        String::FromUTF8(kEchoCancellationTypeSystem));
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
    capabilities.sample_rate = {std::min(media::kAudioProcessingSampleRateHz,
                                         device_parameters.sample_rate()),
                                std::max(media::kAudioProcessingSampleRateHz,
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
  if (device.group_id) {
    capabilities.group_id = blink::WebString::FromUTF8(*device.group_id);
  }

  MediaStreamSource* source =
      InitializeSourceObject(device, std::move(audio_source));
  source->SetCapabilities(capabilities);

  // While sources are being initialized, keep them in a separate array.
  // Once they've finished initialized, they'll be moved over to local_sources_.
  // See OnAudioSourceStarted for more details.
  pending_local_sources_.push_back(source);

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
    SendLogMessage(
        base::StringPrintf("%s => (no audiprocessing is used)", __func__));
    return std::make_unique<blink::LocalMediaStreamAudioSource>(
        frame_, device,
        base::OptionalToPtr(current_request_info_->audio_capture_settings()
                                .requested_buffer_size()),
        stream_controls->disable_local_echo, std::move(source_ready),
        task_runner_);
  }

  // The audio device is not associated with screen capture and also requires
  // processing.
  SendLogMessage(
      base::StringPrintf("%s => (audiprocessing is required)", __func__));
  return std::make_unique<blink::ProcessedLocalAudioSource>(
      *frame_, device, stream_controls->disable_local_echo,
      audio_processing_properties,
      current_request_info_->audio_capture_settings().num_channels(),
      std::move(source_ready), task_runner_);
}

std::unique_ptr<blink::MediaStreamVideoSource>
UserMediaProcessor::CreateVideoSource(
    const MediaStreamDevice& device,
    blink::WebPlatformMediaStreamSource::SourceStoppedCallback stop_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  DCHECK(current_request_info_->video_capture_settings().HasValue());

  return std::make_unique<blink::MediaStreamVideoCapturerSource>(
      frame_->GetTaskRunner(TaskType::kInternalMediaRealTime), frame_,
      std::move(stop_callback), device,
      current_request_info_->video_capture_settings().capture_params(),
      WTF::BindRepeating(&blink::LocalVideoCapturerSource::Create,
                         frame_->GetTaskRunner(blink::TaskType::kInternalMedia),
                         WrapWeakPersistent(frame_.Get())));
}

void UserMediaProcessor::StartTracks(const String& label) {
  DCHECK(current_request_info_->request());
  SendLogMessage(base::StringPrintf("StartTracks({request_id=%d}, {label=%s})",
                                    current_request_info_->request_id(),
                                    label.Utf8().c_str()));

  WebMediaStreamDeviceObserver* media_stream_device_observer =
      GetMediaStreamDeviceObserver();

  if (media_stream_device_observer &&
      !current_request_info_->devices_set().stream_devices.empty()) {
    // TODO(crbug.com/1327960): Introduce interface to replace the four
    // separate callbacks.
    media_stream_device_observer->AddStreams(
        WebString(label), current_request_info_->devices_set(),
        WTF::BindRepeating(&UserMediaProcessor::OnDeviceStopped,
                           WrapWeakPersistent(this)),
        WTF::BindRepeating(&UserMediaProcessor::OnDeviceChanged,
                           WrapWeakPersistent(this)),
        WTF::BindRepeating(&UserMediaProcessor::OnDeviceRequestStateChange,
                           WrapWeakPersistent(this)),
        WTF::BindRepeating(&UserMediaProcessor::OnDeviceCaptureHandleChange,
                           WrapWeakPersistent(this)));
  }

  MediaStreamsComponentsVector stream_components_set;
  for (const mojom::blink::StreamDevicesPtr& stream_devices :
       current_request_info_->devices_set().stream_devices) {
    stream_components_set.push_back(MakeGarbageCollected<MediaStreamComponents>(
        CreateAudioTrack(stream_devices->audio_device),
        CreateVideoTrack(stream_devices->video_device)));
  }

  String blink_id = label;
  current_request_info_->InitializeWebStreams(blink_id, stream_components_set);
  // Wait for the tracks to be started successfully or to fail.
  current_request_info_->CallbackOnTracksStarted(
      WTF::BindOnce(&UserMediaProcessor::OnCreateNativeTracksCompleted,
                    WrapWeakPersistent(this), label));
}

MediaStreamComponent* UserMediaProcessor::CreateVideoTrack(
    const absl::optional<MediaStreamDevice>& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  if (!device) {
    return nullptr;
  }
  MediaStreamSource* source = InitializeVideoSourceObject(*device);
  MediaStreamComponent* component =
      current_request_info_->CreateAndStartVideoTrack(source);
  if (current_request_info_->request()->IsTransferredTrackRequest()) {
    current_request_info_->request()->SetTransferredTrackComponent(component);
  }
  return component;
}

MediaStreamComponent* UserMediaProcessor::CreateAudioTrack(
    const absl::optional<MediaStreamDevice>& device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_request_info_);
  if (!device) {
    return nullptr;
  }
  MediaStreamDevice overriden_audio_device = *device;
  bool render_to_associated_sink =
      current_request_info_->audio_capture_settings().HasValue() &&
      current_request_info_->audio_capture_settings()
          .render_to_associated_sink();

  SendLogMessage(
      base::StringPrintf("CreateAudioTrack({render_to_associated_sink=%d})",
                         render_to_associated_sink));

  if (!render_to_associated_sink) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device id must
    // be removed.
    overriden_audio_device.matched_output_device_id.reset();
  }

  bool is_pending = false;
  MediaStreamSource* source =
      InitializeAudioSourceObject(overriden_audio_device, &is_pending);
  Member<MediaStreamComponent> component =
      MakeGarbageCollected<MediaStreamComponentImpl>(
          source,
          std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
  if (current_request_info_->request()->IsTransferredTrackRequest()) {
    current_request_info_->request()->SetTransferredTrackComponent(component);
  }
  current_request_info_->StartAudioTrack(component, is_pending);

  // At this point the source has started, and its audio parameters have been
  // set. Thus, all audio processing properties are known and can be surfaced
  // to |source|.
  SurfaceAudioProcessingSettings(source);
  return component;
}

void UserMediaProcessor::OnCreateNativeTracksCompleted(
    const String& label,
    RequestInfo* request_info,
    MediaStreamRequestResult result,
    const String& constraint_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf(
      "UMP::OnCreateNativeTracksCompleted({request_id=%d}, {label=%s})",
      request_info->request_id(), label.Utf8().c_str()));
  if (result == MediaStreamRequestResult::OK) {
    GetUserMediaRequestSucceeded(request_info->descriptors(),
                                 request_info->request());
    if (!base::FeatureList::IsEnabled(
            blink::features::kStartMediaStreamCaptureIndicatorInBrowser)) {
      GetMediaStreamDispatcherHost()->OnStreamStarted(label);
    }
  } else {
    GetUserMediaRequestFailed(result, constraint_name);

    for (const MediaStreamDescriptor* descriptor :
         *request_info->descriptors()) {
      for (auto web_track : descriptor->AudioComponents()) {
        MediaStreamTrackPlatform* track =
            MediaStreamTrackPlatform::GetTrack(WebMediaStreamTrack(web_track));
        if (track) {
          track->Stop();
        }
      }

      for (auto web_track : descriptor->VideoComponents()) {
        MediaStreamTrackPlatform* track =
            MediaStreamTrackPlatform::GetTrack(WebMediaStreamTrack(web_track));
        if (track) {
          track->Stop();
        }
      }
    }
  }

  DeleteUserMediaRequest(request_info->request());
}

void UserMediaProcessor::GetUserMediaRequestSucceeded(
    MediaStreamDescriptorVector* descriptors,
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsCurrentRequestInfo(user_media_request));
  SendLogMessage(
      base::StringPrintf("GetUserMediaRequestSucceeded({request_id=%d})",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClient/UserMediaProcessor are destroyed if the JavaScript
  // code request the frame to be destroyed within the scope of the callback.
  // Therefore, post a task to complete the request with a clean stack.
  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(
          &UserMediaProcessor::DelayedGetUserMediaRequestSucceeded,
          WrapWeakPersistent(this), current_request_info_->request_id(),
          WrapPersistent(descriptors), WrapPersistent(user_media_request)));
}

void UserMediaProcessor::DelayedGetUserMediaRequestSucceeded(
    int32_t request_id,
    MediaStreamDescriptorVector* components,
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf(
      "DelayedGetUserMediaRequestSucceeded({request_id=%d}, {result=%s})",
      request_id,
      MediaStreamRequestResultToString(MediaStreamRequestResult::OK)));
  blink::LogUserMediaRequestResult(MediaStreamRequestResult::OK);
  DeleteUserMediaRequest(user_media_request);
  if (!user_media_request->IsTransferredTrackRequest()) {
    // For transferred tracks, user_media_request has already been resolved in
    // FinalizeTransferredTrackInitialization.
    user_media_request->Succeed(*components);
  }
}

void UserMediaProcessor::GetUserMediaRequestFailed(
    MediaStreamRequestResult result,
    const String& constraint_name) {
  DCHECK(current_request_info_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(
      base::StringPrintf("GetUserMediaRequestFailed({request_id=%d})",
                         current_request_info_->request_id()));

  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClient/UserMediaProcessor are destroyed if the JavaScript
  // code request the frame to be destroyed within the scope of the callback.
  // Therefore, post a task to complete the request with a clean stack.
  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&UserMediaProcessor::DelayedGetUserMediaRequestFailed,
                    WrapWeakPersistent(this),
                    current_request_info_->request_id(),
                    WrapPersistent(current_request_info_->request()), result,
                    constraint_name));
}

void UserMediaProcessor::DelayedGetUserMediaRequestFailed(
    int32_t request_id,
    UserMediaRequest* user_media_request,
    MediaStreamRequestResult result,
    const String& constraint_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  blink::LogUserMediaRequestResult(result);
  SendLogMessage(base::StringPrintf(
      "DelayedGetUserMediaRequestFailed({request_id=%d}, {result=%s})",
      request_id, MediaStreamRequestResultToString(result)));
  DeleteUserMediaRequest(user_media_request);
  switch (result) {
    case MediaStreamRequestResult::OK:
    case MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS:
      NOTREACHED();
      return;
    case MediaStreamRequestResult::CONSTRAINT_NOT_SATISFIED:
      user_media_request->FailConstraint(constraint_name, "");
      return;
    default:
      user_media_request->Fail(result, ErrorCodeToString(result));
      return;
  }
}

MediaStreamSource* UserMediaProcessor::FindLocalSource(
    const LocalStreamSources& sources,
    const MediaStreamDevice& device) const {
  for (auto local_source : sources) {
    WebPlatformMediaStreamSource* const source =
        local_source->GetPlatformSource();
    const MediaStreamDevice& active_device = source->device();
    if (IsSameDevice(active_device, device)) {
      return local_source;
    }
  }
  return nullptr;
}

MediaStreamSource* UserMediaProcessor::InitializeSourceObject(
    const MediaStreamDevice& device,
    std::unique_ptr<WebPlatformMediaStreamSource> platform_source) {
  MediaStreamSource::StreamType type = IsAudioInputMediaType(device.type)
                                           ? MediaStreamSource::kTypeAudio
                                           : MediaStreamSource::kTypeVideo;

  auto* source = MakeGarbageCollected<MediaStreamSource>(
      String::FromUTF8(device.id), device.display_id, type,
      String::FromUTF8(device.name), false /* remote */,
      std::move(platform_source));
  if (device.group_id) {
    source->SetGroupId(String::FromUTF8(*device.group_id));
  }
  return source;
}

bool UserMediaProcessor::RemoveLocalSource(MediaStreamSource* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SendLogMessage(base::StringPrintf(
      "RemoveLocalSource({id=%s}, {name=%s}, {group_id=%s})",
      source->Id().Utf8().c_str(), source->GetName().Utf8().c_str(),
      source->GroupId().Utf8().c_str()));

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
      WebPlatformMediaStreamSource* const platform_source =
          source->GetPlatformSource();
      MediaStreamRequestResult result;
      String message;
      if (source->GetType() == MediaStreamSource::kTypeAudio) {
        auto error = MediaStreamAudioSource::From(source)->ErrorCode();
        switch (error.value_or(AudioSourceErrorCode::kUnknown)) {
          case AudioSourceErrorCode::kSystemPermissions:
            result = MediaStreamRequestResult::SYSTEM_PERMISSION_DENIED;
            message =
                "System Permssions prevented access to audio capture device";
            break;
          case AudioSourceErrorCode::kDeviceInUse:
            result = MediaStreamRequestResult::DEVICE_IN_USE;
            message = "Audio capture device already in use";
            break;
          default:
            result = MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO;
            message = "Failed to access audio capture device";
        }
      } else {
        result = MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO;
        message = "Failed to access video capture device";
      }
      NotifyCurrentRequestInfoOfAudioSourceStarted(platform_source, result,
                                                   message);
      pending_local_sources_.erase(device_it);
      return true;
    }
  }

  return false;
}

bool UserMediaProcessor::IsCurrentRequestInfo(int32_t request_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_request_info_ &&
         current_request_info_->request_id() == request_id;
}

bool UserMediaProcessor::IsCurrentRequestInfo(
    UserMediaRequest* user_media_request) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return current_request_info_ &&
         current_request_info_->request() == user_media_request;
}

bool UserMediaProcessor::DeleteUserMediaRequest(
    UserMediaRequest* user_media_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (current_request_info_ &&
      current_request_info_->request() == user_media_request) {
    current_request_info_ = nullptr;
    std::move(request_completed_cb_).Run();
    return true;
  }
  return false;
}

void UserMediaProcessor::StopAllProcessing() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (current_request_info_) {
    switch (current_request_info_->state()) {
      case RequestInfo::State::kSentForGeneration:
        // Let the browser process know that the previously sent request must be
        // canceled.
        GetMediaStreamDispatcherHost()->CancelRequest(
            current_request_info_->request_id());
        [[fallthrough]];

      case RequestInfo::State::kNotSentForGeneration:
        LogUserMediaRequestWithNoResult(
            blink::MEDIA_STREAM_REQUEST_NOT_GENERATED);
        break;

      case RequestInfo::State::kGenerated:
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
  // The client can be null if the frame is already detached.
  // If it's already detached, dispatcher_host_ shouldn't be bound again.
  // (ref: crbug.com/1105842)
  if (!frame_->Client()) {
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  blink::WebPlatformMediaStreamSource* source_impl = source.GetPlatformSource();
  SendLogMessage(base::StringPrintf(
      "OnLocalSourceStopped({session_id=%s})",
      source_impl->device().session_id().ToString().c_str()));

  const bool some_source_removed = RemoveLocalSource(source);
  CHECK(some_source_removed);

  if (auto* media_stream_device_observer = GetMediaStreamDeviceObserver()) {
    media_stream_device_observer->RemoveStreamDevice(source_impl->device());
  }

  String device_id(source_impl->device().id.data());
  GetMediaStreamDispatcherHost()->StopStreamDevice(
      device_id, source_impl->device().serializable_session_id());
}

void UserMediaProcessor::StopLocalSource(MediaStreamSource* source,
                                         bool notify_dispatcher) {
  WebPlatformMediaStreamSource* source_impl = source->GetPlatformSource();
  SendLogMessage(base::StringPrintf(
      "StopLocalSource({session_id=%s})",
      source_impl->device().session_id().ToString().c_str()));

  if (notify_dispatcher) {
    if (auto* media_stream_device_observer = GetMediaStreamDeviceObserver()) {
      media_stream_device_observer->RemoveStreamDevice(source_impl->device());
    }

    String device_id(source_impl->device().id.data());
    GetMediaStreamDispatcherHost()->StopStreamDevice(
        device_id, source_impl->device().serializable_session_id());
  }

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

bool UserMediaProcessor::HasActiveSources() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !local_sources_.empty();
}

#if !BUILDFLAG(IS_ANDROID)
void UserMediaProcessor::FocusCapturedSurface(const String& label, bool focus) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetMediaStreamDispatcherHost()->FocusCapturedSurface(label, focus);
}
#endif

mojom::blink::MediaStreamDispatcherHost*
UserMediaProcessor::GetMediaStreamDispatcherHost() {
  if (!dispatcher_host_.is_bound()) {
    frame_->GetBrowserInterfaceBroker().GetInterface(
        dispatcher_host_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return dispatcher_host_.get();
}

mojom::blink::MediaDevicesDispatcherHost*
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

void UserMediaProcessor::KeepDeviceAliveForTransfer(
    base::UnguessableToken session_id,
    base::UnguessableToken transfer_id,
    KeepDeviceAliveForTransferCallback keep_alive_cb) {
  GetMediaStreamDispatcherHost()->KeepDeviceAliveForTransfer(
      session_id, transfer_id, std::move(keep_alive_cb));
}

}  // namespace blink
