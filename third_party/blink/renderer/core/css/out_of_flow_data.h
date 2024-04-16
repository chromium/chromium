// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CORE_EXPORT OutOfFlowData final
    : public GarbageCollected<OutOfFlowData>,
      public ElementRareDataField {
 public:
  // TODO(crbug.com/331841274): Store last remembered successful position
  // option here.
  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OUT_OF_FLOW_DATA_H_
