// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_media_track_element_base.h"

namespace blink {

HTMLMediaTrackElementBase::HTMLMediaTrackElementBase(
    Document& document,
    const QualifiedName& tag_name)
    : HTMLMediaCaptureElementBase(document, tag_name) {}

void HTMLMediaTrackElementBase::Trace(Visitor* visitor) const {
  HTMLMediaCaptureElementBase::Trace(visitor);
  Supplementable<HTMLMediaTrackElementBase>::Trace(visitor);
}

}  // namespace blink

