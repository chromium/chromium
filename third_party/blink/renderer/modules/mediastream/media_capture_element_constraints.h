// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_CAPTURE_ELEMENT_CONSTRAINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_CAPTURE_ELEMENT_CONSTRAINTS_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HTMLMediaTrackElementBase;
class HTMLUserMediaElement;

class MODULES_EXPORT MediaCaptureElementConstraints final
    : public GarbageCollected<MediaCaptureElementConstraints>,
      public Supplement<HTMLMediaCaptureElementBase> {
 public:
  static const char kSupplementName[];
  static MediaCaptureElementConstraints& From(HTMLMediaCaptureElementBase&);

  // IDL Implementation for HTMLUserMediaElement
  static void setConstraints(HTMLUserMediaElement&,
                             const HTMLMediaStreamConstraints*);

  // IDL Implementation for HTMLMediaTrackElementBase (<camera> and <microphone>)
  static void setConstraints(HTMLMediaTrackElementBase&,
                             const MediaTrackConstraintSet*);

  explicit MediaCaptureElementConstraints(HTMLMediaCaptureElementBase&);

  void SetConstraints(const HTMLMediaStreamConstraints* constraints) {
    constraints_ = constraints;
    did_set_constraints_ = true;
  }
  const HTMLMediaStreamConstraints* Constraints() const {
    return constraints_.Get();
  }
  bool DidSetConstraints() const { return did_set_constraints_; }

  void Trace(Visitor*) const override;

 private:
  Member<const HTMLMediaStreamConstraints> constraints_;
  bool did_set_constraints_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_CAPTURE_ELEMENT_CONSTRAINTS_H_

