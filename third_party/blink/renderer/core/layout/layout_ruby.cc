/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_ruby.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// === generic helper functions to avoid excessive code duplication ===

static LayoutRubyRun* LastRubyRun(const LayoutObject* ruby) {
  LayoutObject* child = ruby->SlowLastChild();
  DCHECK(!child || child->IsRubyRun());
  return ToLayoutRubyRun(child);
}

static inline LayoutRubyRun* FindRubyRunParent(LayoutObject* child) {
  while (child && !child->IsRubyRun())
    child = child->Parent();
  return ToLayoutRubyRun(child);
}

// === ruby as inline object ===

LayoutRubyAsInline::LayoutRubyAsInline(Element* element)
    : LayoutInline(element) {
  UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
}

LayoutRubyAsInline::~LayoutRubyAsInline() = default;

void LayoutRubyAsInline::StyleDidChange(StyleDifference diff,
                                        const ComputedStyle* old_style) {
  LayoutInline::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();
}

void LayoutRubyAsInline::AddChild(LayoutObject* child,
                                  LayoutObject* before_child) {
  // If the child is a ruby run, just add it normally.
  if (child->IsRubyRun()) {
    LayoutInline::AddChild(child, before_child);
    return;
  }

  if (before_child) {
    // insert child into run
    LayoutObject* run = before_child;
    while (run && !run->IsRubyRun())
      run = run->Parent();
    if (run) {
      if (before_child == run)
        before_child = ToLayoutRubyRun(before_child)->FirstChild();
      DCHECK(!before_child || before_child->IsDescendantOf(run));
      run->AddChild(child, before_child);
      return;
    }
    NOTREACHED();  // beforeChild should always have a run as parent!
                   // Emergency fallback: fall through and just append.
  }

  // If the new child would be appended, try to add the child to the previous
  // run if possible, or create a new run otherwise.
  // (The LayoutRubyRun object will handle the details)
  LayoutRubyRun* last_run = LastRubyRun(this);
  if (!last_run || last_run->HasRubyText()) {
    last_run = LayoutRubyRun::StaticCreateRubyRun(this);
    LayoutInline::AddChild(last_run, before_child);
  }
  last_run->AddChild(child);
}

void LayoutRubyAsInline::RemoveChild(LayoutObject* child) {
  // If the child's parent is *this (must be a ruby run), just use the normal
  // remove method.
  if (child->Parent() == this) {
    DCHECK(child->IsRubyRun());
    LayoutInline::RemoveChild(child);
    return;
  }

  // Otherwise find the containing run and remove it from there.
  LayoutRubyRun* run = FindRubyRunParent(child);
  DCHECK(run);
  run->RemoveChild(child);
}

// === ruby as block object ===

LayoutRubyAsBlock::LayoutRubyAsBlock(Element* element)
    : LayoutBlockFlow(element) {
  UseCounter::Count(GetDocument(), WebFeature::kRenderRuby);
}

LayoutRubyAsBlock::~LayoutRubyAsBlock() = default;

void LayoutRubyAsBlock::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  LayoutBlockFlow::StyleDidChange(diff, old_style);
  PropagateStyleToAnonymousChildren();
}

void LayoutRubyAsBlock::AddChild(LayoutObject* child,
                                 LayoutObject* before_child) {
  // If the child is a ruby run, just add it normally.
  if (child->IsRubyRun()) {
    LayoutBlockFlow::AddChild(child, before_child);
    return;
  }

  if (before_child) {
    // insert child into run
    LayoutObject* run = before_child;
    while (run && !run->IsRubyRun())
      run = run->Parent();
    if (run) {
      if (before_child == run)
        before_child = ToLayoutRubyRun(before_child)->FirstChild();
      DCHECK(!before_child || before_child->IsDescendantOf(run));
      run->AddChild(child, before_child);
      return;
    }
    NOTREACHED();  // beforeChild should always have a run as parent!
                   // Emergency fallback: fall through and just append.
  }

  // If the new child would be appended, try to add the child to the previous
  // run if possible, or create a new run otherwise.
  // (The LayoutRubyRun object will handle the details)
  LayoutRubyRun* last_run = LastRubyRun(this);
  if (!last_run || last_run->HasRubyText()) {
    last_run = LayoutRubyRun::StaticCreateRubyRun(this);
    LayoutBlockFlow::AddChild(last_run, before_child);
  }
  last_run->AddChild(child);
}

void LayoutRubyAsBlock::RemoveChild(LayoutObject* child) {
  // If the child's parent is *this (must be a ruby run), just use the normal
  // remove method.
  if (child->Parent() == this) {
    DCHECK(child->IsRubyRun());
    LayoutBlockFlow::RemoveChild(child);
    return;
  }

  // Otherwise find the containing run and remove it from there.
  LayoutRubyRun* run = FindRubyRunParent(child);
  DCHECK(run);
  run->RemoveChild(child);
}

}  // namespace blink
