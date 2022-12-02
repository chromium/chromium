// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_SHADOW_METER_SHADOW_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_SHADOW_METER_SHADOW_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class HTMLMeterElement;

class MeterShadowElement : public HTMLDivElement {
 public:
  explicit MeterShadowElement(Document&);

 private:
  HTMLMeterElement* MeterElement() const;
  scoped_refptr<ComputedStyle> CustomStyleForLayoutObject(
      const StyleRecalcContext&) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_SHADOW_METER_SHADOW_ELEMENT_H_
