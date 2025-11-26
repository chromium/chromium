// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"

namespace blink {
// Internal element for the Permission element. This element holds the icon
// of the permission element.
class HTMLPermissionIconElement final : public HTMLSpanElement {
 public:
  explicit HTMLPermissionIconElement(Document&);

  CascadeFilter GetCascadeFilter() const override {
    // Reject all properties for which 'kValidForPermissionIcon' is false.
    return CascadeFilter(CSSProperty::kValidForPermissionIcon);
  }
  void SetIcon(mojom::blink::PermissionName permission_type,
               bool is_precise_location);

 private:
  void SetIconImpl(mojom::blink::PermissionName permission_type,
                   bool is_precise_location);
  // blink::Element overrides.
  void AdjustStyle(ComputedStyleBuilder& builder) override;

  // A wrapper method which keeps track of logging console messages before
  // calling the HTMLPermissionElementUtils::AdjustedBoundedLength method.
  Length AdjustedBoundedLengthWrapper(const Length& length,
                                      std::optional<float> lower_bound,
                                      std::optional<float> upper_bound,
                                      bool should_multiply_by_content_size);

  // A bool that tracks whether a specific console message was sent already to
  // ensure it's not sent again.
  bool length_console_error_sent_ = false;

  // A bool that tracks whether a specific console message was sent already to
  // ensure it's not sent again.
  bool width_console_error_sent_ = false;

  // Guard used to prevent re-setting the icon on the permission element for
  // static icons.
  bool is_static_icon_set_ = false;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_
