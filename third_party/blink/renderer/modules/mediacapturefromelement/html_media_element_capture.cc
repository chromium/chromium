// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_media_element_capture.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/modules/encryptedmedia/html_media_element_encrypted_media.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/html_audio_element_capturer_source.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/html_video_element_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

// This method creates a MediaStreamSource with the provided video
// capturer source. A new MediaStreamComponent + MediaStreamTrack pair is
// created, connected to the source and is plugged into the
// MediaStreamDescriptor (|descriptor|).
// |is_remote| should be true if the source of the data is not a local device.
// |is_readonly| should be true if the format of the data cannot be changed by
// MediaTrackConstraints.
bool AddVideoTrackToMediaStream(
    LocalFrame* frame,
    std::unique_ptr<VideoCapturerSource> video_source,
    bool is_remote,
    MediaStreamDescriptor* descriptor) {
  DCHECK(video_source.get());
  if (!descriptor) {
    DLOG(ERROR) << "MediaStreamDescriptor is null";
    return false;
  }

  media::VideoCaptureFormats preferred_formats =
      video_source->GetPreferredFormats();
  auto media_stream_video_source =
      std::make_unique<MediaStreamVideoCapturerSource>(
          frame->GetTaskRunner(TaskType::kInternalMediaRealTime), frame,
          WebPlatformMediaStreamSource::SourceStoppedCallback(),
          std::move(video_source));
  auto* media_stream_video_source_ptr = media_stream_video_source.get();
  const String track_id(WTF::CreateCanonicalUUIDString());
  auto* media_stream_source = MakeGarbageCollected<MediaStreamSource>(
      track_id, MediaStreamSource::kTypeVideo, track_id, is_remote,
      std::move(media_stream_video_source));
  media_stream_source->SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats, mojom::blink::FacingMode::kNone,
      false /* is_device_capture */));
  descriptor->AddRemoteTrack(MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_video_source_ptr,
      MediaStreamVideoSource::ConstraintsOnceCallback(), true));
  return true;
}

// Fills in the MediaStreamDescriptor to capture from the WebMediaPlayer
// identified by the second parameter.
void CreateHTMLVideoElementCapturer(
    LocalFrame* frame,
    MediaStreamDescriptor* descriptor,
    WebMediaPlayer* web_media_player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(descriptor);
  DCHECK(web_media_player);
  AddVideoTrackToMediaStream(
      frame,
      HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player, Platform::Current()->GetIOTaskRunner(),
          std::move(task_runner)),
      false,  // is_remote
      descriptor);
}

// Fills in the MediaStreamDescriptor to capture from the WebMediaPlayer
// identified by the second parameter.
void CreateHTMLAudioElementCapturer(
    LocalFrame*,
    MediaStreamDescriptor* descriptor,
    WebMediaPlayer* web_media_player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(descriptor);
  DCHECK(web_media_player);

  const String track_id = WTF::CreateCanonicalUUIDString();

  MediaStreamAudioSource* const media_stream_audio_source =
      HtmlAudioElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player, std::move(task_runner));

  // |media_stream_source| takes ownership of |media_stream_audio_source|.
  auto* media_stream_source = MakeGarbageCollected<MediaStreamSource>(
      track_id, MediaStreamSource::StreamType::kTypeAudio, track_id,
      false /* is_remote */, base::WrapUnique(media_stream_audio_source));
  auto* media_stream_component = MakeGarbageCollected<MediaStreamComponentImpl>(
      media_stream_source,
      std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/true));

  MediaStreamSource::Capabilities capabilities;
  capabilities.device_id = track_id;
  capabilities.echo_cancellation.emplace_back(false);
  capabilities.auto_gain_control.emplace_back(false);
  capabilities.noise_suppression.emplace_back(false);
  capabilities.voice_isolation.emplace_back(false);
  capabilities.sample_size = {
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
  };
  media_stream_source->SetCapabilities(capabilities);

  media_stream_audio_source->ConnectToInitializedTrack(media_stream_component);
  descriptor->AddRemoteTrack(media_stream_component);
}

// Class to register to the events of |m_mediaElement|, acting accordingly on
// the tracks of |m_mediaStream|.
class MediaElementEventListener final : public NativeEventListener {
 public:
  MediaElementEventListener(HTMLMediaElement*, MediaStream*);
  void UpdateSources(ExecutionContext*);

  void Trace(Visitor*) const override;

  // EventListener implementation.
  void Invoke(ExecutionContext*, Event*) override;

 private:
  Member<HTMLMediaElement> media_element_;
  Member<MediaStream> media_stream_;
  HeapHashSet<WeakMember<MediaStreamSource>> sources_;
};

MediaElementEventListener::MediaElementEventListener(HTMLMediaElement* element,
                                                     MediaStream* stream)
    : NativeEventListener(), media_element_(element), media_stream_(stream) {
  UpdateSources(element->GetExecutionContext());
}

void MediaElementEventListener::Invoke(ExecutionContext* context,
                                       Event* event) {
  DVLOG(2) << __func__ << " " << event->type();
  DCHECK(media_stream_);

  if (event->type() == event_type_names::kEnded) {
    const MediaStreamTrackVector tracks = media_stream_->getTracks();
    // Stop all tracks before removing them. This ensures multi-track stream
    // consumers like the MediaRecorder sees all tracks ended before they're
    // removed from the stream, which is interpreted as an error if happening
    // earlier, see for example
    // https://www.w3.org/TR/mediastream-recording/#dom-mediarecorder-start
    // step 14.4.
    for (const auto& track : tracks) {
      track->stopTrack(context);
    }
    for (const auto& track : tracks) {
      media_stream_->RemoveTrackByComponentAndFireEvents(
          track->Component(),
          MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
    }

    media_stream_->StreamEnded();
    return;
  }
  if (event->type() != event_type_names::kLoadedmetadata)
    return;

  // If |media_element_| is a MediaStream, clone the new tracks.
  if (media_element_->GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    const MediaStreamTrackVector tracks = media_stream_->getTracks();
    for (const auto& track : tracks) {
      track->stopTrack(context);
      media_stream_->RemoveTrackByComponentAndFireEvents(
          track->Component(),
          MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
    }
    auto variant = media_element_->GetSrcObjectVariant();
    // The load type check above, should prevent this from failing:
    DCHECK(absl::holds_alternative<MediaStreamDescriptor*>(variant));
    MediaStreamDescriptor* const descriptor =
        absl::get<MediaStreamDescriptor*>(variant);
    DCHECK(descriptor);
    for (unsigned i = 0; i < descriptor->NumberOfAudioComponents(); i++) {
      media_stream_->AddTrackByComponentAndFireEvents(
          descriptor->AudioComponent(i),
          MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
    }
    for (unsigned i = 0; i < descriptor->NumberOfVideoComponents(); i++) {
      media_stream_->AddTrackByComponentAndFireEvents(
          descriptor->VideoComponent(i),
          MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
    }
    UpdateSources(context);
    return;
  }

  auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
      WTF::CreateCanonicalUUIDString(), MediaStreamComponentVector(),
      MediaStreamComponentVector());

  if (media_element_->HasVideo()) {
    CreateHTMLVideoElementCapturer(
        To<LocalDOMWindow>(context)->GetFrame(), descriptor,
        media_element_->GetWebMediaPlayer(),
        media_element_->GetExecutionContext()->GetTaskRunner(
            TaskType::kInternalMediaRealTime));
  }
  if (media_element_->HasAudio()) {
    CreateHTMLAudioElementCapturer(
        To<LocalDOMWindow>(context)->GetFrame(), descriptor,
        media_element_->GetWebMediaPlayer(),
        media_element_->GetExecutionContext()->GetTaskRunner(
            TaskType::kInternalMediaRealTime));
  }

  MediaStreamComponentVector video_components = descriptor->VideoComponents();
  for (auto component : video_components) {
    media_stream_->AddTrackByComponentAndFireEvents(
        component,
        MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
  }

  MediaStreamComponentVector audio_components = descriptor->AudioComponents();
  for (auto component : audio_components) {
    media_stream_->AddTrackByComponentAndFireEvents(
        component,
        MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
  }

  DVLOG(2) << "#videotracks: " << video_components.size()
           << " #audiotracks: " << audio_components.size();

  UpdateSources(context);
}

void DidStopMediaStreamSource(MediaStreamSource* source) {
  if (!source)
    return;
  WebPlatformMediaStreamSource* const platform_source =
      source->GetPlatformSource();
  DCHECK(platform_source);
  platform_source->SetSourceMuted(true);
  platform_source->StopSource();
}

void MediaElementEventListener::UpdateSources(ExecutionContext* context) {
  for (auto track : media_stream_->getTracks())
    sources_.insert(track->Component()->Source());

  // Handling of the ended event in JS triggered by DidStopMediaStreamSource()
  // may cause a reentrant call to this function, which can modify |sources_|.
  // Iterate over a copy of |sources_| to avoid invalidation of the iterator
  // when a reentrant call occurs.
  auto sources_copy = sources_;
  if (!media_element_->currentSrc().IsEmpty() &&
      !media_element_->IsMediaDataCorsSameOrigin()) {
    for (auto source : sources_copy)
      DidStopMediaStreamSource(source.Get());
  }
}

void MediaElementEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
  visitor->Trace(media_stream_);
  visitor->Trace(sources_);
  EventListener::Trace(visitor);
}

}  // anonymous namespace

// static
MediaStream* HTMLMediaElementCapture::captureStream(
    ScriptState* script_state,
    HTMLMediaElement& element,
    ExceptionState& exception_state) {
  // Avoid capturing from EME-protected Media Elements.
  if (HTMLMediaElementEncryptedMedia::mediaKeys(element)) {
    // This exception is not defined in the spec, see
    // https://github.com/w3c/mediacapture-fromelement/issues/20.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Stream capture not supported with EME");
    return nullptr;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The context has been destroyed");
    return nullptr;
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!element.currentSrc().IsEmpty() && !element.IsMediaDataCorsSameOrigin()) {
    exception_state.ThrowSecurityError(
        "Cannot capture from element with cross-origin data");
    return nullptr;
  }

  auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
      WTF::CreateCanonicalUUIDString(), MediaStreamComponentVector(),
      MediaStreamComponentVector());

  // Create() duplicates the MediaStreamTracks inside |descriptor|.
  MediaStream* stream = MediaStream::Create(context, descriptor);

  MediaElementEventListener* listener =
      MakeGarbageCollected<MediaElementEventListener>(&element, stream);
  element.addEventListener(event_type_names::kLoadedmetadata, listener, false);
  element.addEventListener(event_type_names::kEnded, listener, false);

  // If |element| is actually playing a MediaStream, just clone it.
  if (element.GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    auto variant = element.GetSrcObjectVariant();
    // The load type check above, should prevent this from failing:
    DCHECK(absl::holds_alternative<MediaStreamDescriptor*>(variant));
    MediaStreamDescriptor* const element_descriptor =
        absl::get<MediaStreamDescriptor*>(variant);
    DCHECK(element_descriptor);
    return MediaStream::Create(context, element_descriptor);
  }

  LocalFrame* frame = ToLocalFrameIfNotDetached(script_state->GetContext());
  DCHECK(frame);
  if (element.HasVideo()) {
    CreateHTMLVideoElementCapturer(frame, descriptor,
                                   element.GetWebMediaPlayer(),
                                   element.GetExecutionContext()->GetTaskRunner(
                                       TaskType::kInternalMediaRealTime));
  }
  if (element.HasAudio()) {
    CreateHTMLAudioElementCapturer(frame, descriptor,
                                   element.GetWebMediaPlayer(),
                                   element.GetExecutionContext()->GetTaskRunner(
                                       TaskType::kInternalMediaRealTime));
  }
  listener->UpdateSources(context);

  // If element.currentSrc().isNull() then |stream| will have no tracks, those
  // will be added eventually afterwards via MediaElementEventListener.
  return stream;
}

}  // namespace blink
