// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"

namespace blink {

enum class PermissionIconType {
  kNone = 0,
  kLocation = 1,
  kLocationPrecise = 2,
  kCamera = 3,
  kMicrophone = 4,
  kInstall = 5,
  kLaunch = 6,
  // TODO(crbug.com/475285741): Add Open in Chrome product icon.
};

// Internal element for the Permission element. This element holds the icon
// of the permission element.
class HTMLPermissionIconElement final : public HTMLSpanElement {
 public:
  explicit HTMLPermissionIconElement(Document&);

  CascadeFilter GetCascadeFilter() const override {
    // Reject all properties for which 'kValidForPermissionIcon' is false.
    return CascadeFilter(CSSProperty::kValidForPermissionIcon);
  }
  void SetIcon(PermissionIconType icon_type);

 private:
  void SetIconImpl(PermissionIconType icon_type);
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
  PermissionIconType current_icon_type_ = PermissionIconType::kNone;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_
