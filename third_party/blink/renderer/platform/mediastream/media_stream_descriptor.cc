/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

namespace {

static int g_unique_media_stream_descriptor_id = 0;

}  // namespace

// static
int MediaStreamDescriptor::GenerateUniqueId() {
  return ++g_unique_media_stream_descriptor_id;
}

void MediaStreamDescriptor::AddComponent(MediaStreamComponent* component) {
  switch (component->GetSourceType()) {
    case MediaStreamSource::kTypeAudio:
      if (audio_components_.Find(component) == kNotFound)
        audio_components_.push_back(component);
      break;
    case MediaStreamSource::kTypeVideo:
      if (video_components_.Find(component) == kNotFound)
        video_components_.push_back(component);
      break;
  }

  // Iterate over a copy of |observers_| to avoid re-entrancy issues.
  Vector<WebMediaStreamObserver*> observers = observers_;
  for (auto*& observer : observers)
    observer->TrackAdded(WebString(component->Id()));
}

void MediaStreamDescriptor::RemoveComponent(MediaStreamComponent* component) {
  wtf_size_t pos = kNotFound;
  switch (component->GetSourceType()) {
    case MediaStreamSource::kTypeAudio:
      pos = audio_components_.Find(component);
      if (pos != kNotFound)
        audio_components_.EraseAt(pos);
      break;
    case MediaStreamSource::kTypeVideo:
      pos = video_components_.Find(component);
      if (pos != kNotFound)
        video_components_.EraseAt(pos);
      break;
  }

  // Iterate over a copy of |observers_| to avoid re-entrancy issues.
  Vector<WebMediaStreamObserver*> observers = observers_;
  for (auto*& observer : observers)
    observer->TrackRemoved(WebString(component->Id()));
}

void MediaStreamDescriptor::AddRemoteTrack(MediaStreamComponent* component) {
  if (client_) {
    client_->AddTrackByComponentAndFireEvents(
        component,
        MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
  } else {
    AddComponent(component);
  }
}

void MediaStreamDescriptor::RemoveRemoteTrack(MediaStreamComponent* component) {
  if (client_) {
    client_->RemoveTrackByComponentAndFireEvents(
        component,
        MediaStreamDescriptorClient::DispatchEventTiming::kScheduled);
  } else {
    RemoveComponent(component);
  }
}

void MediaStreamDescriptor::SetActive(bool active) {
  if (active == active_)
    return;

  active_ = active;
  // Iterate over a copy of |observers_| to avoid re-entrancy issues.
  Vector<WebMediaStreamObserver*> observers = observers_;
  for (auto*& observer : observers)
    observer->ActiveStateChanged(active_);
}

void MediaStreamDescriptor::AddObserver(WebMediaStreamObserver* observer) {
  DCHECK_EQ(observers_.Find(observer), kNotFound);
  observers_.push_back(observer);
}

void MediaStreamDescriptor::RemoveObserver(WebMediaStreamObserver* observer) {
  wtf_size_t index = observers_.Find(observer);
  DCHECK(index != kNotFound);
  observers_.EraseAt(index);
}

MediaStreamDescriptor::MediaStreamDescriptor(
    const MediaStreamComponentVector& audio_components,
    const MediaStreamComponentVector& video_components)
    : MediaStreamDescriptor(WTF::CreateCanonicalUUIDString(),
                            audio_components,
                            video_components) {}

MediaStreamDescriptor::MediaStreamDescriptor(
    const String& id,
    const MediaStreamComponentVector& audio_components,
    const MediaStreamComponentVector& video_components)
    : client_(nullptr), id_(id), unique_id_(GenerateUniqueId()), active_(true) {
  DCHECK(id_.length());
  for (MediaStreamComponentVector::const_iterator iter =
           audio_components.begin();
       iter != audio_components.end(); ++iter)
    audio_components_.push_back((*iter));
  for (MediaStreamComponentVector::const_iterator iter =
           video_components.begin();
       iter != video_components.end(); ++iter)
    video_components_.push_back((*iter));
}

void MediaStreamDescriptor::Trace(Visitor* visitor) const {
  visitor->Trace(audio_components_);
  visitor->Trace(video_components_);
  visitor->Trace(client_);
}

}  // namespace blink
