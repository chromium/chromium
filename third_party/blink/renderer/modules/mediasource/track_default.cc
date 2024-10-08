// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/track_default.h"

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

AtomicString TrackDefault::AudioKeyword() {
  return AtomicString("audio");
}

AtomicString TrackDefault::VideoKeyword() {
  return AtomicString("video");
}

AtomicString TrackDefault::TextKeyword() {
  return AtomicString("text");
}

ScriptValue TrackDefault::kinds(ScriptState* script_state) const {
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLSequence<IDLString>>::ToV8(script_state, kinds_));
}

TrackDefault* TrackDefault::Create(const V8TrackDefaultType& type,
                                   const String& language,
                                   const String& label,
                                   const Vector<String>& kinds,
                                   const String& byte_stream_track_id,
                                   ExceptionState& exception_state) {
  // Per 11 Nov 2014 Editor's Draft
  // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#idl-def-TrackDefault
  // with expectation that
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=27352 will be fixed soon:
  // When this method is invoked, the user agent must run the following steps:
  // 1. if |language| is not an empty string and |language| is not a BCP 47
  //    language tag, then throw an INVALID_ACCESS_ERR and abort these steps.
  // FIXME: Implement BCP 47 language tag validation.

  if (type.AsEnum() == V8TrackDefaultType::Enum::kAudio) {
    // 2.1. If |type| equals "audio":
    //      If any string in |kinds| contains a value that is not listed as
    //      applying to audio in the kind categories table, then throw a
    //      TypeError and abort these steps.
    for (const String& kind : kinds) {
      if (!AudioTrack::IsValidKindKeyword(kind)) {
        exception_state.ThrowTypeError("Invalid audio track default kind '" +
                                       kind + "'");
        return nullptr;
      }
    }
  } else if (type.AsEnum() == V8TrackDefaultType::Enum::kVideo) {
    // 2.2. If |type| equals "video":
    //      If any string in |kinds| contains a value that is not listed as
    //      applying to video in the kind categories table, then throw a
    //      TypeError and abort these steps.
    for (const String& kind : kinds) {
      if (!VideoTrack::IsValidKindKeyword(kind)) {
        exception_state.ThrowTypeError("Invalid video track default kind '" +
                                       kind + "'");
        return nullptr;
      }
    }
  } else if (type.AsEnum() == V8TrackDefaultType::Enum::kText) {
    // 2.3. If |type| equals "text":
    //      If any string in |kinds| contains a value that is not listed in the
    //      text track kind list, then throw a TypeError and abort these
    //      steps.
    for (const String& kind : kinds) {
      if (!TextTrack::IsValidKindKeyword(kind)) {
        exception_state.ThrowTypeError("Invalid text track default kind '" +
                                       kind + "'");
        return nullptr;
      }
    }
  } else {
    NOTREACHED();
  }

  // 3. Set the type attribute on this new object to |type|.
  // 4. Set the language attribute on this new object to |language|.
  // 5. Set the label attribute on this new object to |label|.
  // 6. Set the kinds attribute on this new object to |kinds|.
  // 7. Set the byteStreamTrackID attribute on this new object to
  //    |byteStreamTrackID|.
  // These steps are done as constructor initializers.
  return MakeGarbageCollected<TrackDefault>(type, language, label, kinds,
                                            byte_stream_track_id);
}

TrackDefault::~TrackDefault() = default;

TrackDefault::TrackDefault(const V8TrackDefaultType& type,
                           const String& language,
                           const String& label,
                           const Vector<String>& kinds,
                           const String& byte_stream_track_id)
    : type_(type),
      byte_stream_track_id_(byte_stream_track_id),
      language_(language),
      label_(label),
      kinds_(kinds) {}

}  // namespace blink
