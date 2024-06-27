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

#include "third_party/blink/renderer/core/css/style_containment_scope.h"
#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LayoutQuote::LayoutQuote(LayoutObject& owner, QuoteType quote)
    : LayoutInline(nullptr),
      type_(quote),
      depth_(0),
      owning_pseudo_(DynamicTo<PseudoElement>(owner.GetNode())) {
  SetDocumentForAnonymous(&owner.GetDocument());
}

LayoutQuote::~LayoutQuote() {
  DCHECK(!scope_);
}

void LayoutQuote::Trace(Visitor* visitor) const {
  visitor->Trace(owning_pseudo_);
  visitor->Trace(scope_);
  LayoutInline::Trace(visitor);
}

void LayoutQuote::WillBeDestroyed() {
  NOT_DESTROYED();
  if (scope_) {
    GetDocument()
        .GetStyleEngine()
        .EnsureStyleContainmentScopeTree()
        .UpdateOutermostQuotesDirtyScope(scope_);
    scope_->DetachQuote(*this);
  }
  LayoutInline::WillBeDestroyed();
}

void LayoutQuote::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  LayoutInline::WillBeRemovedFromTree();
  if (scope_) {
    GetDocument()
        .GetStyleEngine()
        .EnsureStyleContainmentScopeTree()
        .UpdateOutermostQuotesDirtyScope(scope_);
    scope_->DetachQuote(*this);
  }
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
    fragment->SetStyle(IsA<LayoutTextCombine>(fragment->Parent())
                           ? fragment->Parent()->Style()
                           : Style());
    fragment->SetContentString(text_.Impl());
  } else {
    fragment = LayoutTextFragment::CreateAnonymous(GetDocument(), text_.Impl());
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
  if (auto* combine = DynamicTo<LayoutTextCombine>(last_child)) {
    return DynamicTo<LayoutTextFragment>(combine->FirstChild());
  }
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
  NOTREACHED_IN_MIGRATION();
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

}  // namespace blink
