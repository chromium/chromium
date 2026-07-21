// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_USER_MEDIA_ELEMENT_MEDIA_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_USER_MEDIA_ELEMENT_MEDIA_STREAM_H_

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MediaStream;

class MODULES_EXPORT HTMLUserMediaElementMediaStream final
    : public GarbageCollected<HTMLUserMediaElementMediaStream>,
      public Supplement<HTMLMediaCaptureElementBase> {
 public:
  static const char kSupplementName[];

  static HTMLUserMediaElementMediaStream& From(HTMLMediaCaptureElementBase&);
  static MediaStream* stream(HTMLMediaCaptureElementBase&);

  explicit HTMLUserMediaElementMediaStream(HTMLMediaCaptureElementBase&);

  MediaStream* GetMediaStream() const { return media_stream_.Get(); }
  void SetMediaStream(MediaStream* stream) { media_stream_ = stream; }

  void Trace(Visitor*) const override;

 private:
  Member<MediaStream> media_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_USER_MEDIA_ELEMENT_MEDIA_STREAM_H_
