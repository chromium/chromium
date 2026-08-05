// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MEDIA_TRACK_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MEDIA_TRACK_ELEMENT_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"

namespace blink {

class CORE_EXPORT HTMLMediaTrackElementBase
    : public HTMLMediaCaptureElementBase {
 public:
  HTMLMediaTrackElementBase(Document& document, const QualifiedName& tag_name);
  void Trace(Visitor*) const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(track, kTrack)

  bool IsHTMLMediaTrackElementBase() const override { return true; }
};

template <>
struct DowncastTraits<HTMLMediaTrackElementBase> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLMediaTrackElementBase();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLMediaTrackElementBase();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLMediaTrackElementBase();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MEDIA_TRACK_ELEMENT_BASE_H_

