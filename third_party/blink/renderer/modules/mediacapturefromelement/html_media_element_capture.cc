// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_media_element_capture.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
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
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

// This method creates a WebMediaStreamSource + MediaStreamSource pair with the
// provided video capturer source. A new WebMediaStreamTrack +
// MediaStreamTrack pair is created, connected to the source and is plugged into
// the WebMediaStream (|web_media_stream|).
// |is_remote| should be true if the source of the data is not a local device.
// |is_readonly| should be true if the format of the data cannot be changed by
// MediaTrackConstraints.
bool AddVideoTrackToMediaStream(
    LocalFrame* frame,
    std::unique_ptr<media::VideoCapturerSource> video_source,
    bool is_remote,
    WebMediaStream* web_media_stream) {
  DCHECK(video_source.get());
  if (!web_media_stream || web_media_stream->IsNull()) {
    DLOG(ERROR) << "WebMediaStream is null";
    return false;
  }

  media::VideoCaptureFormats preferred_formats =
      video_source->GetPreferredFormats();
  MediaStreamVideoSource* const media_stream_source =
      new MediaStreamVideoCapturerSource(
          frame, WebPlatformMediaStreamSource::SourceStoppedCallback(),
          std::move(video_source));
  const WebString track_id(WTF::CreateCanonicalUUIDString());
  WebMediaStreamSource web_media_stream_source;
  web_media_stream_source.Initialize(track_id, WebMediaStreamSource::kTypeVideo,
                                     track_id, is_remote);
  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetPlatformSource(
      base::WrapUnique(media_stream_source));
  web_media_stream_source.SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats,
      media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
      false /* is_device_capture */));
  web_media_stream->AddTrack(MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_source, MediaStreamVideoSource::ConstraintsCallback(),
      true));
  return true;
}

// Fills in the WebMediaStream to capture from the WebMediaPlayer identified
// by the second parameter.
void CreateHTMLVideoElementCapturer(
    LocalFrame* frame,
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(web_media_stream);
  DCHECK(web_media_player);
  AddVideoTrackToMediaStream(
      frame,
      HtmlVideoElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player, Platform::Current()->GetIOTaskRunner(),
          std::move(task_runner)),
      false,  // is_remote
      web_media_stream);
}

// Fills in the WebMediaStream to capture from the WebMediaPlayer identified
// by the second parameter.
void CreateHTMLAudioElementCapturer(
    LocalFrame*,
    WebMediaStream* web_media_stream,
    WebMediaPlayer* web_media_player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(web_media_stream);
  DCHECK(web_media_player);

  blink::WebMediaStreamSource web_media_stream_source;
  blink::WebMediaStreamTrack web_media_stream_track;
  const WebString track_id(WTF::CreateCanonicalUUIDString());

  web_media_stream_source.Initialize(track_id,
                                     blink::WebMediaStreamSource::kTypeAudio,
                                     track_id, false /* is_remote */);
  web_media_stream_track.Initialize(web_media_stream_source);

  blink::MediaStreamAudioSource* const media_stream_source =
      HtmlAudioElementCapturerSource::CreateFromWebMediaPlayerImpl(
          web_media_player, std::move(task_runner));

  // Takes ownership of |media_stream_source|.
  web_media_stream_source.SetPlatformSource(
      base::WrapUnique(media_stream_source));

  blink::WebMediaStreamSource::Capabilities capabilities;
  capabilities.device_id = track_id;
  capabilities.echo_cancellation.emplace_back(false);
  capabilities.auto_gain_control.emplace_back(false);
  capabilities.noise_suppression.emplace_back(false);
  capabilities.sample_size = {
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
  };
  web_media_stream_source.SetCapabilities(capabilities);

  media_stream_source->ConnectToTrack(web_media_stream_track);
  web_media_stream->AddTrack(web_media_stream_track);
}

// Class to register to the events of |m_mediaElement|, acting accordingly on
// the tracks of |m_mediaStream|.
class MediaElementEventListener final : public NativeEventListener {
 public:
  MediaElementEventListener(HTMLMediaElement*, MediaStream*);
  void UpdateSources(ExecutionContext*);

  void Trace(blink::Visitor*) override;

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
    for (const auto& track : tracks) {
      track->stopTrack(context);
      media_stream_->RemoveTrackByComponentAndFireEvents(track->Component());
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
      media_stream_->RemoveTrackByComponentAndFireEvents(track->Component());
    }
    MediaStreamDescriptor* const descriptor = media_element_->GetSrcObject();
    DCHECK(descriptor);
    for (unsigned i = 0; i < descriptor->NumberOfAudioComponents(); i++) {
      media_stream_->AddTrackByComponentAndFireEvents(
          descriptor->AudioComponent(i));
    }
    for (unsigned i = 0; i < descriptor->NumberOfVideoComponents(); i++) {
      media_stream_->AddTrackByComponentAndFireEvents(
          descriptor->VideoComponent(i));
    }
    UpdateSources(context);
    return;
  }

  WebMediaStream web_stream;
  web_stream.Initialize(WebVector<WebMediaStreamTrack>(),
                        WebVector<WebMediaStreamTrack>());

  if (media_element_->HasVideo()) {
    CreateHTMLVideoElementCapturer(
        To<Document>(context)->GetFrame(), &web_stream,
        media_element_->GetWebMediaPlayer(),
        media_element_->GetExecutionContext()->GetTaskRunner(
            TaskType::kInternalMediaRealTime));
  }
  if (media_element_->HasAudio()) {
    CreateHTMLAudioElementCapturer(
        To<Document>(context)->GetFrame(), &web_stream,
        media_element_->GetWebMediaPlayer(),
        media_element_->GetExecutionContext()->GetTaskRunner(
            TaskType::kInternalMediaRealTime));
  }

  WebVector<WebMediaStreamTrack> video_tracks = web_stream.VideoTracks();
  for (const auto& track : video_tracks)
    media_stream_->AddTrackByComponentAndFireEvents(track);

  WebVector<WebMediaStreamTrack> audio_tracks = web_stream.AudioTracks();
  for (const auto& track : audio_tracks)
    media_stream_->AddTrackByComponentAndFireEvents(track);

  DVLOG(2) << "#videotracks: " << video_tracks.size()
           << " #audiotracks: " << audio_tracks.size();

  UpdateSources(context);
}

void DidStopMediaStreamSource(const WebMediaStreamSource& source) {
  if (source.IsNull())
    return;
  blink::WebPlatformMediaStreamSource* const platform_source =
      source.GetPlatformSource();
  DCHECK(platform_source);
  platform_source->StopSource();
}

void MediaElementEventListener::UpdateSources(ExecutionContext* context) {
  for (auto track : media_stream_->getTracks())
    sources_.insert(track->Component()->Source());

  if (!media_element_->currentSrc().IsEmpty() &&
      !media_element_->IsMediaDataCorsSameOrigin()) {
    for (auto source : sources_)
      DidStopMediaStreamSource(source.Get());
  }
}

void MediaElementEventListener::Trace(blink::Visitor* visitor) {
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

  WebMediaStream web_stream;
  web_stream.Initialize(WebVector<WebMediaStreamTrack>(),
                        WebVector<WebMediaStreamTrack>());

  // Create() duplicates the MediaStreamTracks inside |webStream|.
  MediaStream* stream = MediaStream::Create(context, web_stream);

  MediaElementEventListener* listener =
      MakeGarbageCollected<MediaElementEventListener>(&element, stream);
  element.addEventListener(event_type_names::kLoadedmetadata, listener, false);
  element.addEventListener(event_type_names::kEnded, listener, false);

  // If |element| is actually playing a MediaStream, just clone it.
  if (element.GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    MediaStreamDescriptor* const descriptor = element.GetSrcObject();
    DCHECK(descriptor);
    return MediaStream::Create(context, descriptor);
  }

  LocalFrame* frame = ToLocalFrameIfNotDetached(script_state->GetContext());
  DCHECK(frame);
  if (element.HasVideo()) {
    CreateHTMLVideoElementCapturer(frame, &web_stream,
                                   element.GetWebMediaPlayer(),
                                   element.GetExecutionContext()->GetTaskRunner(
                                       TaskType::kInternalMediaRealTime));
  }
  if (element.HasAudio()) {
    CreateHTMLAudioElementCapturer(frame, &web_stream,
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
