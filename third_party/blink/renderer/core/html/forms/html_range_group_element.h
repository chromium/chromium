// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_RANGE_GROUP_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_RANGE_GROUP_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"

namespace blink {

class CORE_EXPORT HTMLRangeGroupElement final : public HTMLFormControlElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLRangeGroupElement(Document&);

  ElementType GetElementType() const final {
    return ElementType::kHTMLRangeGroupElement;
  }

 private:
  bool IsEnumeratable() const override { return true; }
  bool IsSubmittableElement() override { return false; }
  bool RecalcWillValidate() const override { return false; }
  mojom::blink::FormControlType FormControlType() const override;
  const AtomicString& FormControlTypeAsString() const override;
  FocusableState SupportsFocus(UpdateBehavior) const override;
  bool AlwaysCreateUserAgentShadowRoot() const override { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_RANGE_GROUP_ELEMENT_H_
