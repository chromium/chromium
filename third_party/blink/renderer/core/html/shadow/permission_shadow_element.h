// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_SHADOW_PERMISSION_SHADOW_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_SHADOW_PERMISSION_SHADOW_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"

namespace blink {

class PermissionShadowElement : public HTMLDivElement {
 public:
  explicit PermissionShadowElement(HTMLPermissionElement&);

  ~PermissionShadowElement() override;

  void Trace(Visitor*) const override;

 private:
  // Element implements:
  const ComputedStyle* CustomStyleForLayoutObject(
      const StyleRecalcContext&) override;

  Member<HTMLPermissionElement> permission_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_SHADOW_PERMISSION_SHADOW_ELEMENT_H_
