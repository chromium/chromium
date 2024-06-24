// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_as_block.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/layout/ruby_container.h"

namespace blink {

LayoutRubyAsBlock::LayoutRubyAsBlock(Element* element)
    : LayoutBlockFlow(element) {
  UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
}

LayoutRubyAsBlock::~LayoutRubyAsBlock() = default;

void LayoutRubyAsBlock::AddChild(LayoutObject* child,
                                 LayoutObject* before_child) {
  NOT_DESTROYED();

  LayoutObject* inline_ruby = FirstChild();
  if (!inline_ruby) {
    if (RuntimeEnabledFeatures::RubyLineBreakableEnabled()) {
      inline_ruby = MakeGarbageCollected<LayoutInline>(nullptr);
    } else {
      inline_ruby = MakeGarbageCollected<LayoutRuby>(nullptr);
    }
    inline_ruby->SetDocumentForAnonymous(&GetDocument());
    ComputedStyleBuilder new_style_builder =
        GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
            StyleRef(), EDisplay::kRuby);
    inline_ruby->SetStyle(new_style_builder.TakeStyle());
    LayoutBlockFlow::AddChild(inline_ruby);
  } else if (before_child == inline_ruby) {
    inline_ruby->AddChild(child, inline_ruby->SlowFirstChild());
    return;
  }
  inline_ruby->AddChild(child, before_child);
}

void LayoutRubyAsBlock::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutBlockFlow::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();

  // Because LayoutInline::AnonymousHasStylePropagationOverride() returns
  // true, PropagateStyleToAnonymousChildren() doesn't update the style of
  // the LayoutRuby child.
  if (auto* inline_ruby = FirstChild()) {
    ComputedStyleBuilder new_style_builder =
        GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
            StyleRef(), inline_ruby->StyleRef().Display());
    UpdateAnonymousChildStyle(inline_ruby, new_style_builder);
    inline_ruby->SetStyle(new_style_builder.TakeStyle());
  }
}

void LayoutRubyAsBlock::RemoveLeftoverAnonymousBlock(LayoutBlock*) {
  NOT_DESTROYED();
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink
