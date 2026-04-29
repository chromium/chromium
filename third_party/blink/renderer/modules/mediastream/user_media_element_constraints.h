// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_ELEMENT_CONSTRAINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_ELEMENT_CONSTRAINTS_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class MODULES_EXPORT UserMediaElementConstraints final
    : public GarbageCollected<UserMediaElementConstraints>,
      public Supplement<HTMLUserMediaElement> {
 public:
  static const char kSupplementName[];
  static UserMediaElementConstraints& From(HTMLUserMediaElement&);

  // IDL Implementation
  static void setConstraints(HTMLUserMediaElement&,
                             const HTMLMediaStreamConstraints*);

  explicit UserMediaElementConstraints(HTMLUserMediaElement&);

  void SetConstraints(const HTMLMediaStreamConstraints* constraints) {
    constraints_ = constraints;
  }
  const HTMLMediaStreamConstraints* Constraints() const {
    return constraints_.Get();
  }

  void Trace(Visitor*) const override;

 private:
  Member<const HTMLMediaStreamConstraints> constraints_;
  bool did_set_constraints_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_USER_MEDIA_ELEMENT_CONSTRAINTS_H_
