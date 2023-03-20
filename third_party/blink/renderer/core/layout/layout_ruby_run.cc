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

#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_text.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_run.h"

namespace blink {

LayoutRubyRun::LayoutRubyRun(ContainerNode* node) : LayoutBlockFlow(nullptr) {
  DCHECK(!node);
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutRubyRun::~LayoutRubyRun() = default;

bool LayoutRubyRun::HasRubyText() const {
  NOT_DESTROYED();
  // The only place where a ruby text can be is in the first position
  // Note: As anonymous blocks, ruby runs do not have ':before' or ':after'
  // content themselves.
  return FirstChild() && FirstChild()->IsRubyText();
}

bool LayoutRubyRun::HasRubyBase() const {
  NOT_DESTROYED();
  // The only place where a ruby base can be is in the last position
  // Note: As anonymous blocks, ruby runs do not have ':before' or ':after'
  // content themselves.
  return LastChild() && LastChild()->IsRubyBase();
}

LayoutRubyText* LayoutRubyRun::RubyText() const {
  NOT_DESTROYED();
  LayoutObject* child = FirstChild();
  // If in future it becomes necessary to support floating or positioned ruby
  // text, layout will have to be changed to handle them properly.
  DCHECK(!child || !child->IsRubyText() ||
         !child->IsFloatingOrOutOfFlowPositioned());
  return DynamicTo<LayoutRubyText>(child);
}

LayoutRubyBase* LayoutRubyRun::RubyBase() const {
  NOT_DESTROYED();
  return DynamicTo<LayoutRubyBase>(LastChild());
}

LayoutRubyBase& LayoutRubyRun::EnsureRubyBase() {
  NOT_DESTROYED();
  if (auto* base = RubyBase())
    return *base;
  auto& new_base = CreateRubyBase();
  LayoutBlockFlow::AddChild(&new_base);
  return new_base;
}

bool LayoutRubyRun::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsRubyText() || child->IsInline();
}

void LayoutRubyRun::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  DCHECK(child);

  if (child->IsRubyText()) {
    if (!before_child) {
      // LayoutRuby has already ascertained that we can add the child here.
      DCHECK(!HasRubyText());
      // prepend ruby texts as first child
      LayoutBlockFlow::AddChild(child, FirstChild());
    } else if (before_child->IsRubyText()) {
      // New text is inserted just before another.
      // In this case the new text takes the place of the old one, and
      // the old text goes into a new run that is inserted as next sibling.
      DCHECK_EQ(before_child->Parent(), this);
      LayoutObject* ruby = Parent();
      DCHECK(ruby->IsRuby());
      auto& new_run = Create(ruby, *ContainingBlock());
      ruby->AddChild(&new_run, NextSibling());
      new_run.EnsureRubyBase();
      // Add the new ruby text and move the old one to the new run
      // Note: Doing it in this order and not using LayoutRubyRun's methods,
      // in order to avoid automatic removal of the ruby run in case there is no
      // other child besides the old ruby text.
      LayoutBlockFlow::AddChild(child, before_child);
      LayoutBlockFlow::RemoveChild(before_child);
      new_run.AddChild(before_child);
    } else if (RubyBase()->FirstChild()) {
      // Insertion before a ruby base object.
      // In this case we need insert a new run before the current one and split
      // the base.
      LayoutObject* ruby = Parent();
      LayoutRubyRun& new_run = Create(ruby, *ContainingBlock());
      ruby->AddChild(&new_run, this);
      auto& new_base = new_run.EnsureRubyBase();
      new_run.AddChild(child);

      // Make sure we don't leave anything in the percentage descendant
      // map before moving the children to the new base.
      LayoutRubyBase& base = EnsureRubyBase();
      if (HasPercentHeightDescendants() || base.HasPercentHeightDescendants()) {
        ClearPercentHeightDescendants();
      }
      base.MoveChildren(new_base, before_child);
    }
  } else {
    // child is not a text -> insert it into the base
    // (append it instead if beforeChild is the ruby text)
    LayoutRubyBase& base = EnsureRubyBase();
    if (before_child == &base)
      before_child = base.FirstChild();
    if (before_child && before_child->IsRubyText())
      before_child = nullptr;
    DCHECK(!before_child || before_child->IsDescendantOf(&base));
    base.AddChild(child, before_child);
  }
}

void LayoutRubyRun::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  // If the child is a ruby text, then merge the ruby base with the base of
  // the right sibling run, if possible.
  if (!BeingDestroyed() && !DocumentBeingDestroyed() && child->IsRubyText()) {
    LayoutRubyBase* base = RubyBase();
    LayoutObject* right_neighbour = NextSibling();
    if (base->FirstChild() && right_neighbour && right_neighbour->IsRubyRun()) {
      auto* right_run = To<LayoutRubyRun>(right_neighbour);
      LayoutRubyBase& right_base = right_run->EnsureRubyBase();
      if (right_base.FirstChild()) {
        // Collect all children in a single base, then swap the bases.
        if (right_base.HasPercentHeightDescendants()) {
          right_base.ClearPercentHeightDescendants();
        }
        right_base.MoveChildren(*base);
        MoveChildTo(right_run, base);
        right_run->MoveChildTo(this, &right_base);
        DCHECK(!RubyBase()->FirstChild());
      }
    }
  }

  LayoutBlockFlow::RemoveChild(child);

  if (!BeingDestroyed() && !DocumentBeingDestroyed()) {
    // If this has only an empty LayoutRubyBase, destroy this sub-tree.
    LayoutBlockFlow* base = RubyBase();
    if (!HasRubyText() && !base->FirstChild()) {
      LayoutBlockFlow::RemoveChild(base);
      base->DeleteLineBoxTree();
      base->Destroy();
      DeleteLineBoxTree();
      Destroy();
    }
  }
}

LayoutRubyBase& LayoutRubyRun::CreateRubyBase() const {
  NOT_DESTROYED();
  auto* layout_object = LayoutRubyBase::CreateAnonymous(&GetDocument(), *this);
  ComputedStyleBuilder new_style_builder =
      GetDocument().GetStyleResolver().CreateAnonymousStyleBuilderWithDisplay(
          StyleRef(), EDisplay::kBlock);
  new_style_builder.SetTextAlign(
      ETextAlign::kCenter);  // FIXME: use WEBKIT_CENTER?
  new_style_builder.SetHasLineIfEmpty(true);
  layout_object->SetStyle(new_style_builder.TakeStyle());
  return *layout_object;
}

// static
LayoutRubyRun& LayoutRubyRun::Create(const LayoutObject* parent_ruby,
                                     const LayoutBlock& containing_block) {
  DCHECK(parent_ruby);
  DCHECK(parent_ruby->IsRuby());
  LayoutRubyRun* rr;
  if (containing_block.IsLayoutNGObject()) {
    rr = MakeGarbageCollected<LayoutNGRubyRun>();
  } else {
    rr = MakeGarbageCollected<LayoutRubyRun>(nullptr);
  }
  rr->SetDocumentForAnonymous(&parent_ruby->GetDocument());
  scoped_refptr<const ComputedStyle> new_style =
      parent_ruby->GetDocument()
          .GetStyleResolver()
          .CreateAnonymousStyleWithDisplay(parent_ruby->StyleRef(),
                                           EDisplay::kInlineBlock);
  rr->SetStyle(std::move(new_style));
  return *rr;
}

LayoutObject* LayoutRubyRun::LayoutSpecialExcludedChild(
    bool relayout_children,
    SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  // Don't bother positioning the LayoutRubyRun yet.
  LayoutRubyText* rt = RubyText();
  if (!rt)
    return nullptr;
  if (relayout_children)
    layout_scope.SetChildNeedsLayout(rt);
  rt->LayoutIfNeeded();
  return rt;
}

void LayoutRubyRun::UpdateLayout() {
  NOT_DESTROYED();
  LayoutBlockFlow::UpdateLayout();

  LayoutRubyText* rt = RubyText();
  if (!rt)
    return;

  rt->SetLogicalLeft(LayoutUnit());

  // Place the LayoutRubyText such that its bottom is flush with the lineTop of
  // the first line of the LayoutRubyBase.
  LayoutUnit last_line_ruby_text_bottom = rt->LogicalHeight();
  LayoutUnit first_line_ruby_text_top;
  if (RootInlineBox* root_box = rt->LastRootBox()) {
    // In order to align, we have to ignore negative leading.
    first_line_ruby_text_top = rt->FirstRootBox()->LogicalTopLayoutOverflow();
    last_line_ruby_text_bottom = root_box->LogicalBottomLayoutOverflow();
  }

  RubyPosition block_start_position = StyleRef().IsFlippedLinesWritingMode()
                                          ? RubyPosition::kAfter
                                          : RubyPosition::kBefore;
  if (StyleRef().GetRubyPosition() == block_start_position) {
    LayoutUnit first_line_top;
    if (LayoutRubyBase* rb = RubyBase()) {
      RootInlineBox* root_box = rb->FirstRootBox();
      if (root_box)
        first_line_top = root_box->LogicalTopLayoutOverflow();
      first_line_top += rb->LogicalTop();
    }

    rt->SetLogicalTop(-last_line_ruby_text_bottom + first_line_top);
  } else {
    LayoutUnit last_line_bottom = LogicalHeight();
    if (LayoutRubyBase* rb = RubyBase()) {
      RootInlineBox* root_box = rb->LastRootBox();
      if (root_box)
        last_line_bottom = root_box->LogicalBottomLayoutOverflow();
      last_line_bottom += rb->LogicalTop();
    }

    rt->SetLogicalTop(-first_line_ruby_text_top + last_line_bottom);
  }

  // Update our overflow to account for the new LayoutRubyText position.
  ComputeLayoutOverflow(ClientLogicalBottom());
}

void LayoutRubyRun::GetOverhang(bool first_line,
                                LayoutObject* start_layout_object,
                                LayoutObject* end_layout_object,
                                int& start_overhang,
                                int& end_overhang) const {
  NOT_DESTROYED();
  DCHECK(!NeedsLayout());

  start_overhang = 0;
  end_overhang = 0;

  LayoutRubyBase* ruby_base = RubyBase();
  LayoutRubyText* ruby_text = RubyText();

  if (!ruby_base || !ruby_text)
    return;

  if (!ruby_base->FirstRootBox())
    return;

  int logical_width = LogicalWidth().ToInt();
  int logical_left_overhang = std::numeric_limits<int>::max();
  int logical_right_overhang = std::numeric_limits<int>::max();
  for (RootInlineBox* root_inline_box = ruby_base->FirstRootBox();
       root_inline_box; root_inline_box = root_inline_box->NextRootBox()) {
    logical_left_overhang = std::min<int>(
        logical_left_overhang, root_inline_box->LogicalLeft().ToInt());
    logical_right_overhang = std::min<int>(
        logical_right_overhang,
        (logical_width - root_inline_box->LogicalRight()).ToInt());
  }

  start_overhang = StyleRef().IsLeftToRightDirection() ? logical_left_overhang
                                                       : logical_right_overhang;
  end_overhang = StyleRef().IsLeftToRightDirection() ? logical_right_overhang
                                                     : logical_left_overhang;

  if (!start_layout_object || !start_layout_object->IsText() ||
      start_layout_object->Style(first_line)->FontSize() >
          ruby_base->Style(first_line)->FontSize())
    start_overhang = 0;

  if (!end_layout_object || !end_layout_object->IsText() ||
      end_layout_object->Style(first_line)->FontSize() >
          ruby_base->Style(first_line)->FontSize())
    end_overhang = 0;

  // We overhang a ruby only if the neighboring layout object is a text.
  // We can overhang the ruby by no more than half the width of the neighboring
  // text and no more than half the font size.
  int half_width_of_font_size = ruby_text->Style(first_line)->FontSize() / 2;
  if (start_overhang)
    start_overhang = std::min<int>(
        start_overhang,
        std::min<int>(To<LayoutText>(start_layout_object)->MinLogicalWidth(),
                      half_width_of_font_size));
  if (end_overhang)
    end_overhang = std::min<int>(
        end_overhang,
        std::min<int>(To<LayoutText>(end_layout_object)->MinLogicalWidth(),
                      half_width_of_font_size));
}

bool LayoutRubyRun::CanBreakBefore(
    const LazyLineBreakIterator& iterator) const {
  NOT_DESTROYED();
  // TODO(kojii): It would be nice to improve this so that it isn't just
  // hard-coded, but lookahead in this case is particularly problematic.
  // See crbug.com/522826.

  if (!iterator.PriorContextLength())
    return true;
  UChar ch = iterator.LastCharacter();
  ULineBreak line_break =
      static_cast<ULineBreak>(u_getIntPropertyValue(ch, UCHAR_LINE_BREAK));
  // UNICODE LINE BREAKING ALGORITHM
  // http://www.unicode.org/reports/tr14/
  // And Requirements for Japanese Text Layout, 3.1.7 Characters Not Starting a
  // Line
  // http://www.w3.org/TR/2012/NOTE-jlreq-20120403/#characters_not_starting_a_line
  switch (line_break) {
    case U_LB_WORD_JOINER:
    case U_LB_GLUE:
    case U_LB_OPEN_PUNCTUATION:
      return false;
    default:
      break;
  }
  return true;
}

}  // namespace blink
