// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_MEDIA_TRACK_ELEMENT_MEDIA_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_MEDIA_TRACK_ELEMENT_MEDIA_TRACK_H_

#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"
#include "third_party/blink/renderer/core/html/html_media_track_element_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MediaStreamTrack;

// Supplements HTMLMediaCaptureElementBase (in renderer/core/) with a reference to
// MediaStreamTrack (in renderer/modules/).
//
// HTMLMediaTrackElementBase cannot hold a Member<MediaStreamTrack> directly
// because core/ cannot depend on modules/ in Blink's layered architecture.
// This supplement class bridges that gap, following the same pattern as
// HTMLUserMediaElementMediaStream for MediaStream.
//
// Note: This class supplements HTMLMediaCaptureElementBase instead of
// HTMLMediaTrackElementBase because blink discourages deriving multiple
// Supplementable<T> types within an inheritance hierarchy.
// C++ method signatures and WebIDL mixins enforce that track remains strictly
// restricted to <camera> and <microphone>
class MODULES_EXPORT HTMLMediaTrackElementMediaTrack final
    : public GarbageCollected<HTMLMediaTrackElementMediaTrack>,
      public Supplement<HTMLMediaCaptureElementBase> {
 public:
  static const char kSupplementName[];

  static HTMLMediaTrackElementMediaTrack& From(HTMLMediaCaptureElementBase&);
  static MediaStreamTrack* track(HTMLMediaTrackElementBase&);

  explicit HTMLMediaTrackElementMediaTrack(HTMLMediaCaptureElementBase&);

  MediaStreamTrack* MediaTrack() const { return media_track_.Get(); }
  void SetMediaTrack(MediaStreamTrack* track) { media_track_ = track; }

  void Trace(Visitor*) const override;

 private:
  Member<MediaStreamTrack> media_track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_HTML_MEDIA_TRACK_ELEMENT_MEDIA_TRACK_H_

