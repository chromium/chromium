/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/media_stream.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

static bool ContainsSource(MediaStreamTrackVector& track_vector,
                           MediaStreamSource* source) {
  for (MediaStreamTrack* track : track_vector) {
    if (source->Id() == track->Component()->Source()->Id())
      return true;
  }
  return false;
}

static void ProcessTrack(MediaStreamTrack* track,
                         MediaStreamTrackVector& track_vector) {
  MediaStreamSource* source = track->Component()->Source();
  if (!ContainsSource(track_vector, source))
    track_vector.push_back(track);
}

MediaStream* MediaStream::Create(ExecutionContext* context) {
  MediaStreamTrackVector audio_tracks;
  MediaStreamTrackVector video_tracks;

  return MakeGarbageCollected<MediaStream>(context, audio_tracks, video_tracks);
}

MediaStream* MediaStream::Create(ExecutionContext* context,
                                 MediaStream* stream) {
  DCHECK(stream);

  MediaStreamTrackVector audio_tracks;
  MediaStreamTrackVector video_tracks;

  for (MediaStreamTrack* track : stream->audio_tracks_)
    ProcessTrack(track, audio_tracks);

  for (MediaStreamTrack* track : stream->video_tracks_)
    ProcessTrack(track, video_tracks);

  return MakeGarbageCollected<MediaStream>(context, audio_tracks, video_tracks);
}

MediaStream* MediaStream::Create(ExecutionContext* context,
                                 const MediaStreamTrackVector& tracks) {
  MediaStreamTrackVector audio_tracks;
  MediaStreamTrackVector video_tracks;

  for (MediaStreamTrack* track : tracks) {
    ProcessTrack(track, track->kind() == "audio" ? audio_tracks : video_tracks);
  }

  return MakeGarbageCollected<MediaStream>(context, audio_tracks, video_tracks);
}

MediaStream* MediaStream::Create(ExecutionContext* context,
                                 MediaStreamDescriptor* stream_descriptor) {
  return MakeGarbageCollected<MediaStream>(context, stream_descriptor);
}

MediaStream* MediaStream::Create(ExecutionContext* context,
                                 MediaStreamDescriptor* stream_descriptor,
                                 const MediaStreamTrackVector& audio_tracks,
                                 const MediaStreamTrackVector& video_tracks) {
  return MakeGarbageCollected<MediaStream>(context, stream_descriptor,
                                           audio_tracks, video_tracks);
}

MediaStream::MediaStream(ExecutionContext* context,
                         MediaStreamDescriptor* stream_descriptor)
    : ContextClient(context),
      descriptor_(stream_descriptor),
      scheduled_event_timer_(
          context->GetTaskRunner(TaskType::kMediaElementEvent),
          this,
          &MediaStream::ScheduledEventTimerFired) {
  descriptor_->SetClient(this);

  uint32_t number_of_audio_tracks = descriptor_->NumberOfAudioComponents();
  audio_tracks_.ReserveCapacity(number_of_audio_tracks);
  for (uint32_t i = 0; i < number_of_audio_tracks; i++) {
    MediaStreamTrack* new_track =
        MediaStreamTrack::Create(context, descriptor_->AudioComponent(i));
    new_track->RegisterMediaStream(this);
    audio_tracks_.push_back(new_track);
  }

  uint32_t number_of_video_tracks = descriptor_->NumberOfVideoComponents();
  video_tracks_.ReserveCapacity(number_of_video_tracks);
  for (uint32_t i = 0; i < number_of_video_tracks; i++) {
    MediaStreamTrack* new_track =
        MediaStreamTrack::Create(context, descriptor_->VideoComponent(i));
    new_track->RegisterMediaStream(this);
    video_tracks_.push_back(new_track);
  }

  if (EmptyOrOnlyEndedTracks()) {
    descriptor_->SetActive(false);
  }
}

MediaStream::MediaStream(ExecutionContext* context,
                         MediaStreamDescriptor* stream_descriptor,
                         const MediaStreamTrackVector& audio_tracks,
                         const MediaStreamTrackVector& video_tracks)
    : ContextClient(context),
      descriptor_(stream_descriptor),
      scheduled_event_timer_(
          context->GetTaskRunner(TaskType::kMediaElementEvent),
          this,
          &MediaStream::ScheduledEventTimerFired) {
  descriptor_->SetClient(this);

  audio_tracks_.ReserveCapacity(audio_tracks.size());
  for (MediaStreamTrack* audio_track : audio_tracks) {
    DCHECK_EQ("audio", audio_track->kind());
    audio_track->RegisterMediaStream(this);
    audio_tracks_.push_back(audio_track);
  }
  video_tracks_.ReserveCapacity(video_tracks.size());
  for (MediaStreamTrack* video_track : video_tracks) {
    DCHECK_EQ("video", video_track->kind());
    video_track->RegisterMediaStream(this);
    video_tracks_.push_back(video_track);
  }
  DCHECK(TracksMatchDescriptor());

  if (EmptyOrOnlyEndedTracks()) {
    descriptor_->SetActive(false);
  }
}

MediaStream::MediaStream(ExecutionContext* context,
                         const MediaStreamTrackVector& audio_tracks,
                         const MediaStreamTrackVector& video_tracks)
    : ContextClient(context),
      scheduled_event_timer_(
          context->GetTaskRunner(TaskType::kMediaElementEvent),
          this,
          &MediaStream::ScheduledEventTimerFired) {
  MediaStreamComponentVector audio_components;
  MediaStreamComponentVector video_components;

  MediaStreamTrackVector::const_iterator iter;
  for (iter = audio_tracks.begin(); iter != audio_tracks.end(); ++iter) {
    (*iter)->RegisterMediaStream(this);
    audio_components.push_back((*iter)->Component());
  }
  for (iter = video_tracks.begin(); iter != video_tracks.end(); ++iter) {
    (*iter)->RegisterMediaStream(this);
    video_components.push_back((*iter)->Component());
  }

  descriptor_ = MakeGarbageCollected<MediaStreamDescriptor>(audio_components,
                                                            video_components);
  descriptor_->SetClient(this);

  audio_tracks_ = audio_tracks;
  video_tracks_ = video_tracks;
  if (EmptyOrOnlyEndedTracks()) {
    descriptor_->SetActive(false);
  }
}

MediaStream::~MediaStream() = default;

bool MediaStream::HasPendingActivity() const {
  return !scheduled_events_.IsEmpty();
}

bool MediaStream::EmptyOrOnlyEndedTracks() {
  if (!audio_tracks_.size() && !video_tracks_.size()) {
    return true;
  }
  for (MediaStreamTrackVector::iterator iter = audio_tracks_.begin();
       iter != audio_tracks_.end(); ++iter) {
    if (!iter->Get()->Ended())
      return false;
  }
  for (MediaStreamTrackVector::iterator iter = video_tracks_.begin();
       iter != video_tracks_.end(); ++iter) {
    if (!iter->Get()->Ended())
      return false;
  }
  return true;
}

bool MediaStream::TracksMatchDescriptor() {
  if (audio_tracks_.size() != descriptor_->NumberOfAudioComponents())
    return false;
  for (wtf_size_t i = 0; i < audio_tracks_.size(); i++) {
    if (audio_tracks_[i]->Component() != descriptor_->AudioComponent(i))
      return false;
  }
  if (video_tracks_.size() != descriptor_->NumberOfVideoComponents())
    return false;
  for (wtf_size_t i = 0; i < video_tracks_.size(); i++) {
    if (video_tracks_[i]->Component() != descriptor_->VideoComponent(i))
      return false;
  }
  return true;
}

MediaStreamTrackVector MediaStream::getTracks() {
  MediaStreamTrackVector tracks;
  for (MediaStreamTrackVector::iterator iter = audio_tracks_.begin();
       iter != audio_tracks_.end(); ++iter)
    tracks.push_back(iter->Get());
  for (MediaStreamTrackVector::iterator iter = video_tracks_.begin();
       iter != video_tracks_.end(); ++iter)
    tracks.push_back(iter->Get());
  return tracks;
}

void MediaStream::addTrack(MediaStreamTrack* track,
                           ExceptionState& exception_state) {
  if (!track) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTypeMismatchError,
        "The MediaStreamTrack provided is invalid.");
    return;
  }

  if (getTrackById(track->id()))
    return;

  switch (track->Component()->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      audio_tracks_.push_back(track);
      break;
    case MediaStreamSource::kTypeVideo:
      video_tracks_.push_back(track);
      break;
  }
  track->RegisterMediaStream(this);
  descriptor_->AddComponent(track->Component());

  if (!active() && !track->Ended()) {
    descriptor_->SetActive(true);
    ScheduleDispatchEvent(Event::Create(event_type_names::kActive));
  }

  for (auto& observer : observers_)
    observer->OnStreamAddTrack(this, track);
}

void MediaStream::removeTrack(MediaStreamTrack* track,
                              ExceptionState& exception_state) {
  if (!track) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTypeMismatchError,
        "The MediaStreamTrack provided is invalid.");
    return;
  }

  wtf_size_t pos = kNotFound;
  switch (track->Component()->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      pos = audio_tracks_.Find(track);
      if (pos != kNotFound)
        audio_tracks_.EraseAt(pos);
      break;
    case MediaStreamSource::kTypeVideo:
      pos = video_tracks_.Find(track);
      if (pos != kNotFound)
        video_tracks_.EraseAt(pos);
      break;
  }

  if (pos == kNotFound)
    return;
  track->UnregisterMediaStream(this);
  descriptor_->RemoveComponent(track->Component());

  if (active() && EmptyOrOnlyEndedTracks()) {
    descriptor_->SetActive(false);
    ScheduleDispatchEvent(Event::Create(event_type_names::kInactive));
  }

  for (auto& observer : observers_)
    observer->OnStreamRemoveTrack(this, track);
}

MediaStreamTrack* MediaStream::getTrackById(String id) {
  for (MediaStreamTrackVector::iterator iter = audio_tracks_.begin();
       iter != audio_tracks_.end(); ++iter) {
    if ((*iter)->id() == id)
      return iter->Get();
  }

  for (MediaStreamTrackVector::iterator iter = video_tracks_.begin();
       iter != video_tracks_.end(); ++iter) {
    if ((*iter)->id() == id)
      return iter->Get();
  }

  return nullptr;
}

MediaStream* MediaStream::clone(ScriptState* script_state) {
  MediaStreamTrackVector tracks;
  ExecutionContext* context = ExecutionContext::From(script_state);
  for (MediaStreamTrackVector::iterator iter = audio_tracks_.begin();
       iter != audio_tracks_.end(); ++iter)
    tracks.push_back((*iter)->clone(script_state));
  for (MediaStreamTrackVector::iterator iter = video_tracks_.begin();
       iter != video_tracks_.end(); ++iter)
    tracks.push_back((*iter)->clone(script_state));
  return MediaStream::Create(context, tracks);
}

void MediaStream::TrackEnded() {
  for (MediaStreamTrackVector::iterator iter = audio_tracks_.begin();
       iter != audio_tracks_.end(); ++iter) {
    if (!(*iter)->Ended())
      return;
  }

  for (MediaStreamTrackVector::iterator iter = video_tracks_.begin();
       iter != video_tracks_.end(); ++iter) {
    if (!(*iter)->Ended())
      return;
  }

  StreamEnded();
}

void MediaStream::RegisterObserver(MediaStreamObserver* observer) {
  DCHECK(observer);
  observers_.insert(observer);
}

void MediaStream::UnregisterObserver(MediaStreamObserver* observer) {
  observers_.erase(observer);
}

void MediaStream::StreamEnded() {
  if (!GetExecutionContext())
    return;

  if (active()) {
    descriptor_->SetActive(false);
    ScheduleDispatchEvent(Event::Create(event_type_names::kInactive));
  }
}

bool MediaStream::AddEventListenerInternal(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsResolved* options) {
  if (event_type == event_type_names::kActive) {
    UseCounter::Count(GetExecutionContext(), WebFeature::kMediaStreamOnActive);
  } else if (event_type == event_type_names::kInactive) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kMediaStreamOnInactive);
  }

  return EventTargetWithInlineData::AddEventListenerInternal(event_type,
                                                             listener, options);
}

const AtomicString& MediaStream::InterfaceName() const {
  return event_target_names::kMediaStream;
}

void MediaStream::AddTrackByComponentAndFireEvents(
    MediaStreamComponent* component) {
  DCHECK(component);
  if (!GetExecutionContext())
    return;
  MediaStreamTrack* track =
      MediaStreamTrack::Create(GetExecutionContext(), component);
  AddTrackAndFireEvents(track);
}

void MediaStream::RemoveTrackByComponentAndFireEvents(
    MediaStreamComponent* component) {
  DCHECK(component);
  if (!GetExecutionContext())
    return;

  MediaStreamTrackVector* tracks = nullptr;
  switch (component->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      tracks = &audio_tracks_;
      break;
    case MediaStreamSource::kTypeVideo:
      tracks = &video_tracks_;
      break;
  }

  wtf_size_t index = kNotFound;
  for (wtf_size_t i = 0; i < tracks->size(); ++i) {
    if ((*tracks)[i]->Component() == component) {
      index = i;
      break;
    }
  }
  if (index == kNotFound)
    return;

  descriptor_->RemoveComponent(component);

  MediaStreamTrack* track = (*tracks)[index];
  track->UnregisterMediaStream(this);
  tracks->EraseAt(index);
  ScheduleDispatchEvent(
      MediaStreamTrackEvent::Create(event_type_names::kRemovetrack, track));

  if (active() && EmptyOrOnlyEndedTracks()) {
    descriptor_->SetActive(false);
    ScheduleDispatchEvent(Event::Create(event_type_names::kInactive));
  }
}

void MediaStream::AddTrackAndFireEvents(MediaStreamTrack* track) {
  DCHECK(track);
  switch (track->Component()->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      audio_tracks_.push_back(track);
      break;
    case MediaStreamSource::kTypeVideo:
      video_tracks_.push_back(track);
      break;
  }
  track->RegisterMediaStream(this);
  descriptor_->AddComponent(track->Component());

  ScheduleDispatchEvent(
      MediaStreamTrackEvent::Create(event_type_names::kAddtrack, track));

  if (!active() && !track->Ended()) {
    descriptor_->SetActive(true);
    ScheduleDispatchEvent(Event::Create(event_type_names::kActive));
  }
}

void MediaStream::RemoveTrackAndFireEvents(MediaStreamTrack* track) {
  DCHECK(track);
  RemoveTrackByComponentAndFireEvents(track->Component());
}

void MediaStream::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);

  if (!scheduled_event_timer_.IsActive())
    scheduled_event_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void MediaStream::ScheduledEventTimerFired(TimerBase*) {
  if (!GetExecutionContext())
    return;

  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  HeapVector<Member<Event>>::iterator it = events.begin();
  for (; it != events.end(); ++it)
    DispatchEvent(*it->Release());

  events.clear();
}

void MediaStream::Trace(blink::Visitor* visitor) {
  visitor->Trace(audio_tracks_);
  visitor->Trace(video_tracks_);
  visitor->Trace(descriptor_);
  visitor->Trace(observers_);
  visitor->Trace(scheduled_events_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
  MediaStreamDescriptorClient::Trace(visitor);
}

MediaStream* ToMediaStream(MediaStreamDescriptor* descriptor) {
  return static_cast<MediaStream*>(descriptor->Client());
}

}  // namespace blink
