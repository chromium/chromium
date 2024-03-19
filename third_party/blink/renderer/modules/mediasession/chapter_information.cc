// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/chapter_information.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_chapter_information_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_metadata_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediasession/media_session.h"
#include "third_party/blink/renderer/modules/mediasession/media_session_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
ChapterInformation* ChapterInformation::From(
    ScriptState* script_state,
    const ChapterInformationInit* chapter,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ChapterInformation>(
      script_state, chapter->title(), chapter->startTime(), chapter->artwork(),
      exception_state);
}

ChapterInformation* ChapterInformation::Create(
    ScriptState* script_state,
    const String& title,
    const double& start_time,
    const HeapVector<Member<MediaImage>>& artwork,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ChapterInformation>(
      script_state, title, start_time, artwork, exception_state);
}

ChapterInformation::ChapterInformation(
    ScriptState* script_state,
    const String& title,
    const double& start_time,
    const HeapVector<Member<MediaImage>>& artwork,
    ExceptionState& exception_state)
    : title_(title), start_time_(start_time) {
  SetArtworkInternal(script_state, artwork, exception_state);
}

String ChapterInformation::title() const {
  return title_;
}

double ChapterInformation::startTime() const {
  return start_time_;
}

const HeapVector<Member<MediaImage>>& ChapterInformation::artwork() const {
  return artwork_;
}

v8::LocalVector<v8::Value> ChapterInformation::artwork(
    ScriptState* script_state) const {
  v8::LocalVector<v8::Value> result(script_state->GetIsolate(),
                                    artwork_.size());

  for (wtf_size_t i = 0; i < artwork_.size(); ++i) {
    result[i] =
        FreezeV8Object(ToV8Traits<MediaImage>::ToV8(script_state, artwork_[i]),
                       script_state->GetIsolate());
  }

  return result;
}

void ChapterInformation::Trace(Visitor* visitor) const {
  visitor->Trace(artwork_);
  ScriptWrappable::Trace(visitor);
}

void ChapterInformation::SetArtworkInternal(
    ScriptState* script_state,
    const HeapVector<Member<MediaImage>>& artwork,
    ExceptionState& exception_state) {
  HeapVector<Member<MediaImage>> processed_artwork =
      media_session_utils::ProcessArtworkVector(script_state, artwork,
                                                exception_state);
  if (processed_artwork.empty()) {
    return;
  }
  artwork_.swap(processed_artwork);
}

}  // namespace blink
