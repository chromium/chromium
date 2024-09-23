// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_font_accessor_win.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {

namespace {

void GetFontsUsedByLayoutObject(const LayoutObject& layout_object,
                                FontFamilyNames& result);

void GetFontsUsedByFragment(const PhysicalBoxFragment& fragment,
                            FontFamilyNames& result) {
  for (InlineCursor cursor(fragment); cursor; cursor.MoveToNext()) {
    const FragmentItem& item = *cursor.Current().Item();
    if (item.IsText()) {
      if (const ShapeResultView* shape_result_view = item.TextShapeResult()) {
        HeapHashSet<Member<const SimpleFontData>> used_fonts =
            shape_result_view->UsedFonts();
        for (const auto& used_font : used_fonts) {
          result.font_names.insert(used_font->PlatformData().FontFamilyName());
        }
      }
      continue;
    }

    // If this is a nested BFC (e.g., inline block, floats), compute its area.
    if (item.Type() == FragmentItem::kBox) {
      if (const auto* layout_box = DynamicTo<LayoutBox>(item.GetLayoutObject()))
        GetFontsUsedByLayoutObject(*layout_box, result);
    }
  }

  // Traverse out-of-flow children. They are not in |FragmentItems|.
  for (const PhysicalFragmentLink& child : fragment.Children()) {
    if (const auto* child_layout_box =
            DynamicTo<LayoutBox>(child->GetLayoutObject()))
      GetFontsUsedByLayoutObject(*child_layout_box, result);
  }
}

void GetFontsUsedByLayoutObject(const LayoutObject& layout_object,
                                FontFamilyNames& result) {
  const LayoutObject* target = &layout_object;
  while (target) {
    // Use |InlineCursor| to traverse if |target| is an IFC.
    if (const auto* block_flow = DynamicTo<LayoutBlockFlow>(target)) {
      if (block_flow->HasFragmentItems()) {
        for (const PhysicalBoxFragment& fragment :
             block_flow->PhysicalFragments()) {
          GetFontsUsedByFragment(fragment, result);
        }
        target = target->NextInPreOrderAfterChildren(&layout_object);
        continue;
      }
    }
    target = target->NextInPreOrder(&layout_object);
  }
}

}  // namespace

void GetFontsUsedByFrame(const LocalFrame& frame, FontFamilyNames& result) {
  GetFontsUsedByLayoutObject(frame.ContentLayoutObject()->RootBox(), result);
}

}  // namespace blink
