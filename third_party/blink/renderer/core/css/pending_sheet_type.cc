// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/pending_sheet_type.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/blocking_attribute.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

std::pair<PendingSheetType, RenderBlockingBehavior>
ComputePendingSheetTypeAndRenderBlockingBehavior(Element& sheet_owner,
                                                 bool is_critical_sheet,
                                                 bool is_created_by_parser) {
  if (!is_critical_sheet) {
    return std::make_pair(PendingSheetType::kNonBlocking,
                          RenderBlockingBehavior::kNonBlocking);
  }
  if (is_created_by_parser) {
    bool is_in_body =
        sheet_owner.IsDescendantOf(sheet_owner.GetDocument().body());
    return std::make_pair(PendingSheetType::kBlocking,
                          is_in_body
                              ? RenderBlockingBehavior::kInBodyParserBlocking
                              : RenderBlockingBehavior::kBlocking);
  }
  bool potentially_render_blocking =
      IsA<HTMLElement>(sheet_owner) &&
      To<HTMLElement>(sheet_owner).IsPotentiallyRenderBlocking();
  return potentially_render_blocking
             ? std::make_pair(PendingSheetType::kDynamicRenderBlocking,
                              RenderBlockingBehavior::kBlocking)
             : std::make_pair(PendingSheetType::kNonBlocking,
                              RenderBlockingBehavior::kNonBlockingDynamic);
}

}  // namespace blink
