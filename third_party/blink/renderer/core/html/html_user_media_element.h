// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"

namespace blink {

class CORE_EXPORT HTMLUserMediaElement : public HTMLCapabilityElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool isTypeSupported(const AtomicString& type);

  explicit HTMLUserMediaElement(Document& document);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(stream, kStream)

  // HTML Element
  bool IsHTMLUserMediaElement() const final { return true; }

  void AttributeChanged(const AttributeModificationParams& params) override;

  Vector<mojom::blink::PermissionDescriptorPtr> ParseType(
      const AtomicString& type);
};

// The custom type casting is required for the UserMediaElement OT because the
// generated helpers code can lead to a compilation error or an
// HTMLUserMediaElement appearing in a document that does not have the
// UserMediaElement origin trial enabled (this would result in the creation of
// an HTMLUnknownElement with the "usermedia" tag name).
template <>
struct DowncastTraits<HTMLUserMediaElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLUserMediaElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLUserMediaElement();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLUserMediaElement();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_
