// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CAMERA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CAMERA_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_media_track_element_base.h"

namespace blink {

class CORE_EXPORT HTMLCameraElement : public HTMLMediaTrackElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLCameraElement(Document& document);

  ElementType GetElementType() const final {
    return ElementType::kHTMLCameraElement;
  }
  bool IsHTMLCameraElement() const final { return true; }

  void ApplyDefaultConstraints() override;
};

template <>
struct DowncastTraits<HTMLCameraElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLCameraElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLCameraElement();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLCameraElement();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_CAMERA_ELEMENT_H_
