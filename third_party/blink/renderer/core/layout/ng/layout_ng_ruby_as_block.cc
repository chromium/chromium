// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_as_block.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_ruby.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_run.h"

namespace blink {

LayoutNGRubyAsBlock::LayoutNGRubyAsBlock(Element* element)
    : LayoutNGBlockFlow(element) {
  UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
}

LayoutNGRubyAsBlock::~LayoutNGRubyAsBlock() = default;

bool LayoutNGRubyAsBlock::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectRuby || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGRubyAsBlock::AddChild(LayoutObject* child,
                                   LayoutObject* before_child) {
  NOT_DESTROYED();
  // If the child is a ruby run, just add it normally.
  if (child->IsRubyRun()) {
    LayoutNGBlockFlow::AddChild(child, before_child);
    return;
  }

  if (before_child) {
    // insert child into run
    LayoutObject* run = before_child;
    while (run && !run->IsRubyRun()) {
      run = run->Parent();
    }
    if (run) {
      if (before_child == run) {
        before_child = To<LayoutNGRubyRun>(before_child)->FirstChild();
      }
      DCHECK(!before_child || before_child->IsDescendantOf(run));
      run->AddChild(child, before_child);
      return;
    }
    NOTREACHED();  // before_child should always have a run as parent!
                   // Emergency fallback: fall through and just append.
  }

  // If the new child would be appended, try to add the child to the previous
  // run if possible, or create a new run otherwise.
  // (The LayoutNGRubyRun object will handle the details)
  auto* last_run = LayoutRubyAsInline::LastRubyRun(*this);
  if (!last_run || last_run->HasRubyText()) {
    last_run = &LayoutNGRubyRun::Create(this, *this);
    LayoutNGBlockFlow::AddChild(last_run, before_child);
    last_run->EnsureRubyBase();
  }
  last_run->AddChild(child);
}

void LayoutNGRubyAsBlock::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child's parent is *this (must be a ruby run), just use the normal
  // remove method.
  if (child->Parent() == this) {
    DCHECK(child->IsRubyRun());
    LayoutNGBlockFlow::RemoveChild(child);
    return;
  }

  // Otherwise find the containing run and remove it from there.
  auto* run = LayoutRubyAsInline::FindRubyRunParent(child);
  DCHECK(run);
  run->RemoveChild(child);
}

void LayoutNGRubyAsBlock::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();
}

bool LayoutNGRubyAsBlock::CreatesAnonymousWrapper() const {
  NOT_DESTROYED();
  return true;
}

void LayoutNGRubyAsBlock::RemoveLeftoverAnonymousBlock(LayoutBlock*) {
  NOT_DESTROYED();
  NOTREACHED();
}

}  // namespace blink
