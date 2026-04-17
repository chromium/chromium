// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_USER_MEDIA_ELEMENT_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class CORE_EXPORT HTMLUserMediaElement
    : public HTMLCapabilityElementBase,
      public Supplementable<HTMLUserMediaElement> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool isTypeSupported(const AtomicString& type);

  explicit HTMLUserMediaElement(Document& document);
  void Trace(Visitor*) const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(stream, kStream)

  // HTML Element
  ElementType GetElementType() const final {
    return ElementType::kHTMLUserMediaElement;
  }
  bool IsHTMLUserMediaElement() const final { return true; }

  void AttributeChanged(const AttributeModificationParams& params) override;

  // HTMLCapabilityElementBase
  void OnPermissionStatusChange(mojom::blink::PermissionName permission_name,
                                mojom::blink::PermissionStatus status) override;
  void DefaultEventHandler(Event& event) override;
  mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor() override;

  Vector<mojom::blink::PermissionDescriptorPtr> ParseType(
      const AtomicString& type);

  // Migration branching logic: Returns true if the 'type' attribute is present.
  // When the 'type' attribute is explicitly defined, the element falls back to
  // legacy behavior the same as the legacy <permission> element.
  // TODO(crbug.com/493632110): Deprecate `type` attribute once the adoption of
  // <usermedia> element is stable.
  bool IsLegacyMode() const;

  void OnConstraintsSet(bool has_video, bool has_audio);

  void ResetMediaStreamRequestTime();

  const Vector<mojom::blink::PermissionDescriptorPtr>&
  GetPermissionDescriptors() const {
    return permission_descriptors_;
  }

 private:
  void StartMediaStreamRequest();
  bool has_constraints_ = false;
  base::TimeTicks media_stream_request_start_time_;
};

// The custom type casting is required for the UserMediaElement OT because the
// generated helpers code can lead to a compilation error or an
// HTMLUserMediaElement appearing in a document that does not have the
// UserMediaElement origin trial enabled (this would result in the creation of
// an HTMLUnknownElement with the "usermedia" tag name).
// See third_party/blink/renderer/core/html/Custom_element_type_helpers.md
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
