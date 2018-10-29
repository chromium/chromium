// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_media_element_capture.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/modules/encryptedmedia/html_media_element_encrypted_media.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_center.h"

namespace blink {

namespace {

// Class to register to the events of |m_mediaElement|, acting accordingly on
// the tracks of |m_mediaStream|.
class MediaElementEventListener final : public EventListener {
  WTF_MAKE_NONCOPYABLE(MediaElementEventListener);

 public:
  MediaElementEventListener(HTMLMediaElement*, MediaStream*);
  void UpdateSources(ExecutionContext*);

  void Trace(blink::Visitor*) override;

 private:
  // EventListener implementation.
  void handleEvent(ExecutionContext*, Event*) override;
  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  Member<HTMLMediaElement> media_element_;
  Member<MediaStream> media_stream_;
  HeapHashSet<WeakMember<MediaStreamSource>> sources_;
};

MediaElementEventListener::MediaElementEventListener(HTMLMediaElement* element,
                                                     MediaStream* stream)
    : EventListener(kCPPEventListenerType),
      media_element_(element),
      media_stream_(stream) {
  UpdateSources(element->GetExecutionContext());
}

void MediaElementEventListener::handleEvent(ExecutionContext* context,
                                            Event* event) {
  DVLOG(2) << __func__ << " " << event->type();
  DCHECK(media_stream_);

  if (event->type() == EventTypeNames::ended) {
    const MediaStreamTrackVector tracks = media_stream_->getTracks();
    for (const auto& track : tracks) {
      track->stopTrack(context);
      media_stream_->RemoveTrackByComponentAndFireEvents(track->Component());
    }

    media_stream_->StreamEnded();
    return;
  }
  if (event->type() != EventTypeNames::loadedmetadata)
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
    Platform::Current()->CreateHTMLVideoElementCapturer(
        &web_stream, media_element_->GetWebMediaPlayer(),
        media_element_->GetExecutionContext()->GetTaskRunner(
            TaskType::kInternalMediaRealTime));
  }
  if (media_element_->HasAudio()) {
    Platform::Current()->CreateHTMLAudioElementCapturer(
        &web_stream, media_element_->GetWebMediaPlayer());
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

void MediaElementEventListener::UpdateSources(ExecutionContext* context) {
  for (auto track : media_stream_->getTracks())
    sources_.insert(track->Component()->Source());

  if (!media_element_->currentSrc().IsEmpty() &&
      !media_element_->IsMediaDataCORSSameOrigin()) {
    for (auto source : sources_)
      MediaStreamCenter::Instance().DidStopMediaStreamSource(source);
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

  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!element.currentSrc().IsEmpty() && !element.IsMediaDataCORSSameOrigin()) {
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
      new MediaElementEventListener(&element, stream);
  element.addEventListener(EventTypeNames::loadedmetadata, listener, false);
  element.addEventListener(EventTypeNames::ended, listener, false);

  // If |element| is actually playing a MediaStream, just clone it.
  if (element.GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    MediaStreamDescriptor* const descriptor = element.GetSrcObject();
    DCHECK(descriptor);
    return MediaStream::Create(context, descriptor);
  }

  if (element.HasVideo()) {
    Platform::Current()->CreateHTMLVideoElementCapturer(
        &web_stream, element.GetWebMediaPlayer(),
        element.GetExecutionContext()->GetTaskRunner(
            TaskType::kInternalMediaRealTime));
  }
  if (element.HasAudio()) {
    Platform::Current()->CreateHTMLAudioElementCapturer(
        &web_stream, element.GetWebMediaPlayer());
  }
  listener->UpdateSources(context);

  // If element.currentSrc().isNull() then |stream| will have no tracks, those
  // will be added eventually afterwards via MediaElementEventListener.
  return stream;
}

}  // namespace blink
