// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_metadata.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata_init.h"
#include "third_party/blink/renderer/modules/mediasession/media_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::Create(ScriptState* script_state,
                                     const MediaMetadataInit* metadata,
                                     ExceptionState& exception_state) {
  return MakeGarbageCollected<MediaMetadata>(script_state, metadata,
                                             exception_state);
}

MediaMetadata::MediaMetadata(ScriptState* script_state,
                             const MediaMetadataInit* metadata,
                             ExceptionState& exception_state)
    : notify_session_timer_(ExecutionContext::From(script_state)
                                ->GetTaskRunner(TaskType::kMiscPlatformAPI),
                            this,
                            &MediaMetadata::NotifySessionTimerFired) {
  title_ = metadata->title();
  artist_ = metadata->artist();
  album_ = metadata->album();
  SetArtworkInternal(script_state, metadata->artwork(), exception_state);
}

String MediaMetadata::title() const {
  return title_;
}

String MediaMetadata::artist() const {
  return artist_;
}

String MediaMetadata::album() const {
  return album_;
}

const HeapVector<Member<MediaImage>>& MediaMetadata::artwork() const {
  return artwork_;
}

Vector<v8::Local<v8::Value>> MediaMetadata::artwork(
    ScriptState* script_state) const {
  Vector<v8::Local<v8::Value>> result(artwork_.size());

  for (wtf_size_t i = 0; i < artwork_.size(); ++i) {
    result[i] = FreezeV8Object(ToV8(artwork_[i], script_state),
                               script_state->GetIsolate());
  }

  return result;
}

void MediaMetadata::setTitle(const String& title) {
  title_ = title;
  NotifySessionAsync();
}

void MediaMetadata::setArtist(const String& artist) {
  artist_ = artist;
  NotifySessionAsync();
}

void MediaMetadata::setAlbum(const String& album) {
  album_ = album;
  NotifySessionAsync();
}

void MediaMetadata::setArtwork(ScriptState* script_state,
                               const HeapVector<Member<MediaImage>>& artwork,
                               ExceptionState& exception_state) {
  SetArtworkInternal(script_state, artwork, exception_state);
  NotifySessionAsync();
}

void MediaMetadata::SetSession(MediaSession* session) {
  session_ = session;
}

void MediaMetadata::NotifySessionAsync() {
  if (!session_ || notify_session_timer_.IsActive())
    return;
  notify_session_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void MediaMetadata::NotifySessionTimerFired(TimerBase*) {
  if (!session_)
    return;
  session_->OnMetadataChanged();
}

void MediaMetadata::SetArtworkInternal(
    ScriptState* script_state,
    const HeapVector<Member<MediaImage>>& artwork,
    ExceptionState& exception_state) {
  HeapVector<Member<MediaImage>> processed_artwork(artwork);

  for (MediaImage* image : processed_artwork) {
    KURL url = ExecutionContext::From(script_state)->CompleteURL(image->src());
    if (!url.IsValid()) {
      exception_state.ThrowTypeError("'" + image->src() +
                                     "' can't be resolved to a valid URL.");
      return;
    }
    image->setSrc(url);
  }

  DCHECK(!exception_state.HadException());
  artwork_.swap(processed_artwork);
}

void MediaMetadata::Trace(blink::Visitor* visitor) {
  visitor->Trace(artwork_);
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
