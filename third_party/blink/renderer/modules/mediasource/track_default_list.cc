// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/track_default_list.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

TrackDefaultList* TrackDefaultList::Create(
    const HeapVector<Member<TrackDefault>>& track_defaults,
    ExceptionState& exception_state) {
  // Per 11 Dec 2014 Editor's Draft
  // https://w3c.github.io/media-source/#trackdefaultlist
  // When this method is invoked, the user agent must run the following steps:
  // 1. If |trackDefaults| contains two or more TrackDefault objects with the
  //    same type and the same byteStreamTrackID, then throw an
  //    InvalidAccessError and abort these steps.
  //    Note: This also applies when byteStreamTrackID contains an empty
  //    string and ensures that there is only one "byteStreamTrackID
  //    independent" default for each TrackDefaultType value.
  using TypeAndID = std::pair<AtomicString, String>;
  using TypeAndIDToTrackDefaultMap =
      HeapHashMap<TypeAndID, Member<TrackDefault>>;
  TypeAndIDToTrackDefaultMap type_and_id_to_track_default_map;

  for (const auto& track_default : track_defaults) {
    TypeAndID key =
        TypeAndID(track_default->type(), track_default->byteStreamTrackID());
    if (!type_and_id_to_track_default_map.insert(key, track_default)
             .is_new_entry) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "Duplicate TrackDefault type (" + key.first +
              ") and byteStreamTrackID (" + key.second + ")");
      return nullptr;
    }
  }

  // 2. Store a shallow copy of |trackDefaults| in this new object so the values
  //    can be returned by the accessor methods.
  // This step is done in constructor initializer.
  return MakeGarbageCollected<TrackDefaultList>(track_defaults);
}

TrackDefault* TrackDefaultList::item(unsigned index) const {
  // Per 11 Dec 2014 Editor's Draft
  // https://w3c.github.io/media-source/#trackdefaultlist
  // When this method is invoked, the user agent must run the following steps:
  // 1. If |index| is greater than or equal to the length attribute then
  //    return undefined and abort these steps.
  if (index >= track_defaults_.size())
    return nullptr;

  // 2. Return the |index|'th TrackDefault object in the list.
  return track_defaults_[index].Get();
}

TrackDefaultList::TrackDefaultList() = default;

TrackDefaultList::TrackDefaultList(
    const HeapVector<Member<TrackDefault>>& track_defaults)
    : track_defaults_(track_defaults) {}

void TrackDefaultList::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_defaults_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
