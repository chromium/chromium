/**
 * Copyright (C) 2011 Nokia Inc.  All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_quote.h"

#include <algorithm>

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LayoutQuote::LayoutQuote(PseudoElement& pseudo, QuoteType quote)
    : LayoutInline(nullptr),
      type_(quote),
      depth_(0),
      next_(nullptr),
      previous_(nullptr),
      owning_pseudo_(&pseudo),
      attached_(false) {
  SetDocumentForAnonymous(&pseudo.GetDocument());
}

LayoutQuote::~LayoutQuote() {
  DCHECK(!attached_);
  DCHECK(!next_);
  DCHECK(!previous_);
}

void LayoutQuote::Trace(Visitor* visitor) const {
  visitor->Trace(next_);
  visitor->Trace(previous_);
  visitor->Trace(owning_pseudo_);
  LayoutInline::Trace(visitor);
}

void LayoutQuote::WillBeDestroyed() {
  NOT_DESTROYED();
  DetachQuote();
  LayoutInline::WillBeDestroyed();
}

void LayoutQuote::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutInline::WillBeRemovedFromTree();
  DetachQuote();
}

void LayoutQuote::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutInline::StyleDidChange(diff, old_style);
  UpdateText();
}

static scoped_refptr<const QuotesData> BasicQuotesData() {
  DEFINE_STATIC_REF(QuotesData, static_basic_quotes,
                    (QuotesData::Create(0x201c, 0x201d, 0x2018, 0x2019)));
  return static_basic_quotes;
}

void LayoutQuote::UpdateText() {
  NOT_DESTROYED();
  String text = ComputeText();
  if (text_ == text)
    return;

  text_ = text;

  LayoutTextFragment* fragment = FindFragmentChild();
  if (fragment) {
    fragment->SetStyle(IsA<LayoutNGTextCombine>(fragment->Parent())
                           ? fragment->Parent()->Style()
                           : Style());
    fragment->SetContentString(text_.Impl());
  } else {
    LegacyLayout legacy =
        ForceLegacyLayout() ? LegacyLayout::kForce : LegacyLayout::kAuto;
    fragment = LayoutTextFragment::CreateAnonymous(*owning_pseudo_,
                                                   text_.Impl(), legacy);
    fragment->SetStyle(Style());
    AddChild(fragment);
  }
}

LayoutTextFragment* LayoutQuote::FindFragmentChild() const {
  NOT_DESTROYED();
  // TODO(yosin): Once we support ::first-letter for <q>, we should change
  // this function. See http://crbug.com/1206577
  auto* const last_child = LastChild();
  if (auto* fragment = DynamicTo<LayoutTextFragment>(last_child))
    return fragment;
  if (auto* combine = DynamicTo<LayoutNGTextCombine>(last_child))
    return DynamicTo<LayoutTextFragment>(combine->FirstChild());
  return nullptr;
}

String LayoutQuote::ComputeText() const {
  NOT_DESTROYED();
  switch (type_) {
    case QuoteType::kNoOpen:
    case QuoteType::kNoClose:
      return g_empty_string;
    case QuoteType::kClose:
      return GetQuotesData()->GetCloseQuote(depth_ - 1).Impl();
    case QuoteType::kOpen:
      return GetQuotesData()->GetOpenQuote(depth_).Impl();
  }
  NOTREACHED();
  return g_empty_string;
}

scoped_refptr<const QuotesData> LayoutQuote::GetQuotesData() const {
  NOT_DESTROYED();
  if (scoped_refptr<const QuotesData> custom_quotes = StyleRef().Quotes())
    return custom_quotes;

  if (const LayoutLocale* locale = StyleRef().GetFontDescription().Locale()) {
    if (scoped_refptr<const QuotesData> custom_quotes = locale->GetQuotesData())
      return custom_quotes;
  }

  return BasicQuotesData();
}

void LayoutQuote::AttachQuote() {
  NOT_DESTROYED();
  DCHECK(View());
  DCHECK(!attached_);
  DCHECK(!next_);
  DCHECK(!previous_);
  DCHECK(IsRooted());

  if (!View()->LayoutQuoteHead()) {
    View()->SetLayoutQuoteHead(this);
    attached_ = true;
    return;
  }

  // TODO(crbug.com/882385): Implement style containment for quotes. For now,
  // make sure we don't crash for container queries. If we are inside a size
  // query container, don't connect to previous quote, and don't set it as the
  // LayoutQuoteHead for the LayoutView.
  bool found_container_root = false;

  for (LayoutObject* predecessor = PreviousInPreOrder(); predecessor;
       predecessor = predecessor->PreviousInPreOrder()) {
    if (predecessor->CanMatchSizeContainerQueries()) {
      found_container_root = true;
      break;
    }
    // Skip unattached predecessors to avoid having stale m_previous pointers
    // if the previous node is never attached and is then destroyed.
    if (!predecessor->IsQuote() || !To<LayoutQuote>(predecessor)->IsAttached())
      continue;
    previous_ = To<LayoutQuote>(predecessor);
    next_ = previous_->next_;
    previous_->next_ = this;
    if (next_)
      next_->previous_ = this;
    break;
  }

  if (!previous_ && !found_container_root) {
    next_ = View()->LayoutQuoteHead();
    View()->SetLayoutQuoteHead(this);
    if (next_)
      next_->previous_ = this;
  }
  attached_ = true;

  for (LayoutQuote* quote = this; quote; quote = quote->next_)
    quote->UpdateDepth();

  DCHECK(!next_ || next_->attached_);
  DCHECK(!next_ || next_->previous_ == this);
  DCHECK(!previous_ || previous_->attached_);
  DCHECK(!previous_ || previous_->next_ == this);
}

void LayoutQuote::DetachQuote() {
  NOT_DESTROYED();
  DCHECK(!next_ || next_->attached_);
  DCHECK(!previous_ || previous_->attached_);
  if (!attached_)
    return;

  // Reset our attached status at this point because it's possible for
  // updateDepth() to call into attachQuote(). Attach quote walks the layout
  // tree looking for quotes that are attached and does work on them.
  attached_ = false;

  if (previous_)
    previous_->next_ = next_;
  else if (View())
    View()->SetLayoutQuoteHead(next_);
  if (next_)
    next_->previous_ = previous_;
  if (!DocumentBeingDestroyed()) {
    for (LayoutQuote* quote = next_; quote; quote = quote->next_)
      quote->UpdateDepth();
  }
  next_ = nullptr;
  previous_ = nullptr;
  depth_ = 0;
}

void LayoutQuote::UpdateDepth() {
  NOT_DESTROYED();
  DCHECK(attached_);
  int old_depth = depth_;
  depth_ = 0;
  if (previous_) {
    depth_ = previous_->depth_;
    switch (previous_->type_) {
      case QuoteType::kOpen:
      case QuoteType::kNoOpen:
        depth_++;
        break;
      case QuoteType::kClose:
      case QuoteType::kNoClose:
        if (depth_)
          depth_--;
        break;
    }
  }
  if (old_depth != depth_)
    UpdateText();
}

}  // namespace blink
