// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_capability_element_base.h"

namespace blink {

class CORE_EXPORT HTMLPermissionElement final
    : public HTMLCapabilityElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool isTypeSupported(const AtomicString& type);

  explicit HTMLPermissionElement(Document&);
  ~HTMLPermissionElement() override;

  const AtomicString& GetType() const override;

  // Given an input type, return permissions list. This method is for testing
  // only.
  static Vector<mojom::blink::PermissionDescriptorPtr>
  ParsePermissionDescriptorsForTesting(const AtomicString& type);

  // HTMLElement overrides.
  bool IsHTMLPermissionElement() const final { return true; }

 protected:
  void AttributeChanged(const AttributeModificationParams& params) override;

  // Called on activation of an <install> element with attributes that fail
  // installability checks.
  void HandleInstallDataError();

 private:
  FRIEND_TEST_ALL_PREFIXES(HTMLPermissionElementTest, SetTypeAfterInsertedInto);

  void setType(const AtomicString& type);

  Vector<mojom::blink::PermissionDescriptorPtr> ParseType(
      const AtomicString& type);

  AtomicString type_;
};

// The custom type casting is required for the PermissionElement OT because the
// generated helpers code can lead to a compilation error or an
// HTMLPermissionElement appearing in a document that does not have the
// PermissionElement origin trial enabled (this would result in the creation of
// an HTMLUnknownElement with the "Permission" tag name).
// TODO((crbug.com/339781931): Once the origin trial has ended, these custom
// type casts will no longer be necessary.
template <>
struct DowncastTraits<HTMLPermissionElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLPermissionElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLPermissionElement();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLPermissionElement();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ELEMENT_H_
