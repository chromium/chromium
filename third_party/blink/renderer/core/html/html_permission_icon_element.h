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

  void SetIcon(mojom::blink::PermissionName permission_type,
               bool is_precise_location);

 private:
  // Guard used to prevent re-setting the icon on the permission element. The
  // state of the element can change, and the text changes with it, but the icon
  // is always the same. There is no need to set it again.
  bool is_icon_set_ = false;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_PERMISSION_ICON_ELEMENT_H_
