// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"

namespace blink {

class CORE_EXPORT HTMLUserMediaElement : public HTMLMediaCaptureElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool isTypeSupported(const AtomicString& type);

  explicit HTMLUserMediaElement(Document& document);

  ElementType GetElementType() const final {
    return ElementType::kHTMLUserMediaElement;
  }
  bool IsHTMLUserMediaElement() const final { return true; }

  DOMException* error() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(stream, kStream)

  void AttributeChanged(const AttributeModificationParams& params) override;
  void OnPermissionStatusChange(mojom::blink::PermissionName permission_name,
                                mojom::blink::PermissionStatus status) override;
  void OnEmbeddedPermissionsDecided(
      mojom::blink::EmbeddedPermissionControlResult result) override;
  void DefaultEventHandler(Event& event) override;
  void OnActivationFailed(const String& error_message) override;

  void ApplyDefaultConstraints() override;

  Vector<mojom::blink::PermissionDescriptorPtr> ParseType(
      const AtomicString& type);

  // Migration branching logic: Returns true if the 'type' attribute is present.
  // When the 'type' attribute is explicitly defined, the element falls back to
  // legacy behavior the same as the legacy <permission> element.
  // TODO(crbug.com/493632110): Deprecate `type` attribute once the adoption of
  // <usermedia> element is stable.
  bool IsLegacyMode() const;

 protected:
  bool ShouldShowGrantedAppearance() const override;
};

// The custom type casting is required for the UserMediaElement OT because the
// generated helpers code can lead to a compilation error or an
// HTMLUserMediaElement appearing in a document that does not have the
// UserMediaElement origin trial enabled (this would result in the creation of
// an HTMLUnknownElement with the "usermedia" tag name).
// See
// https://chromium.googlesource.com/chromium/src.git/+/main/docs/custom_type_helpers_for_origin_trial_elements.md
// for more details.
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
