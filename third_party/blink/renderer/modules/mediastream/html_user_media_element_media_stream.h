// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_USER_MEDIA_ELEMENT_MEDIA_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_USER_MEDIA_ELEMENT_MEDIA_STREAM_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MediaStream;

class MODULES_EXPORT HTMLUserMediaElementMediaStream final
    : public GarbageCollected<HTMLUserMediaElementMediaStream>,
      public Supplement<HTMLUserMediaElement> {
 public:
  static const char kSupplementName[];

  static HTMLUserMediaElementMediaStream& From(HTMLUserMediaElement&);
  static MediaStream* stream(HTMLUserMediaElement&);
  static ScriptValue error(ScriptState*, HTMLUserMediaElement& element);

  explicit HTMLUserMediaElementMediaStream(HTMLUserMediaElement&);

  MediaStream* GetMediaStream() const { return media_stream_.Get(); }
  void SetMediaStream(MediaStream* stream) { media_stream_ = stream; }

  void SetError(WorldSafeV8Reference<v8::Value> error) { error_ = std::move(error); }

  void Trace(Visitor*) const override;

 private:
  Member<MediaStream> media_stream_;
  WorldSafeV8Reference<v8::Value> error_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_USER_MEDIA_ELEMENT_MEDIA_STREAM_H_
