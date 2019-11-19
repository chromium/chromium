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

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
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

void LayoutQuote::WillBeDestroyed() {
  DetachQuote();
  LayoutInline::WillBeDestroyed();
}

void LayoutQuote::WillBeRemovedFromTree() {
  LayoutInline::WillBeRemovedFromTree();
  DetachQuote();
}

void LayoutQuote::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  LayoutInline::StyleDidChange(diff, old_style);
  UpdateText();
}

struct Language {
  const char* lang;
  UChar open1;
  UChar close1;
  UChar open2;
  UChar close2;
  QuotesData* data;

  bool operator<(const Language& b) const { return strcmp(lang, b.lang) < 0; }
};

// Table of quotes from
// http://www.whatwg.org/specs/web-apps/current-work/multipage/rendering.html#quote
Language g_languages[] = {
    {"af", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"agq", 0x201e, 0x201d, 0x201a, 0x2019, nullptr},
    {"ak", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"am", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"ar", 0x201d, 0x201c, 0x2019, 0x2018, nullptr},
    {"asa", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"az-cyrl", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"bas", 0x00ab, 0x00bb, 0x201e, 0x201c, nullptr},
    {"bem", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"bez", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"bg", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"bm", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"bn", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"br", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"brx", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"bs-cyrl", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"ca", 0x201c, 0x201d, 0x00ab, 0x00bb, nullptr},
    {"cgg", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"chr", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"cs", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"da", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"dav", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"de", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"de-ch", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"dje", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"dua", 0x00ab, 0x00bb, 0x2018, 0x2019, nullptr},
    {"dyo", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"dz", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ebu", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ee", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"el", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"en", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"en-gb", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"es", 0x201c, 0x201d, 0x00ab, 0x00bb, nullptr},
    {"et", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"eu", 0x201c, 0x201d, 0x00ab, 0x00bb, nullptr},
    {"ewo", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"fa", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"ff", 0x201e, 0x201d, 0x201a, 0x2019, nullptr},
    {"fi", 0x201d, 0x201d, 0x2019, 0x2019, nullptr},
    {"fr", 0x00ab, 0x00bb, 0x00ab, 0x00bb, nullptr},
    {"fr-ca", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"fr-ch", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"gsw", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"gu", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"guz", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ha", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"he", 0x0022, 0x0022, 0x0027, 0x0027, nullptr},
    {"hi", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"hr", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"hu", 0x201e, 0x201d, 0x00bb, 0x00ab, nullptr},
    {"id", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ig", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"it", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"ja", 0x300c, 0x300d, 0x300e, 0x300f, nullptr},
    {"jgo", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"jmc", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"kab", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"kam", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"kde", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"kea", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"khq", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ki", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"kkj", 0x00ab, 0x00bb, 0x2039, 0x203a, nullptr},
    {"kln", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"km", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"kn", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ko", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ksb", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ksf", 0x00ab, 0x00bb, 0x2018, 0x2019, nullptr},
    {"lag", 0x201d, 0x201d, 0x2019, 0x2019, nullptr},
    {"lg", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ln", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"lo", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"lt", 0x201e, 0x201c, 0x201e, 0x201c, nullptr},
    {"lu", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"luo", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"luy", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"lv", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mas", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mer", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mfe", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mg", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"mgo", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mk", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"ml", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mr", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ms", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"mua", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"my", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"naq", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"nb", 0x00ab, 0x00bb, 0x2018, 0x2019, nullptr},
    {"nd", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"nl", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"nmg", 0x201e, 0x201d, 0x00ab, 0x00bb, nullptr},
    {"nn", 0x00ab, 0x00bb, 0x2018, 0x2019, nullptr},
    {"nnh", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"nus", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"nyn", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"pl", 0x201e, 0x201d, 0x00ab, 0x00bb, nullptr},
    {"pt", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"pt-pt", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"rn", 0x201d, 0x201d, 0x2019, 0x2019, nullptr},
    {"ro", 0x201e, 0x201d, 0x00ab, 0x00bb, nullptr},
    {"rof", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ru", 0x00ab, 0x00bb, 0x201e, 0x201c, nullptr},
    {"rw", 0x00ab, 0x00bb, 0x2018, 0x2019, nullptr},
    {"rwk", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"saq", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"sbp", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"seh", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ses", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"sg", 0x00ab, 0x00bb, 0x201c, 0x201d, nullptr},
    {"shi", 0x00ab, 0x00bb, 0x201e, 0x201d, nullptr},
    {"shi-tfng", 0x00ab, 0x00bb, 0x201e, 0x201d, nullptr},
    {"si", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"sk", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"sl", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"sn", 0x201d, 0x201d, 0x2019, 0x2019, nullptr},
    {"so", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"sq", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"sr", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"sr-latn", 0x201e, 0x201c, 0x201a, 0x2018, nullptr},
    {"sv", 0x201d, 0x201d, 0x2019, 0x2019, nullptr},
    {"sw", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"swc", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ta", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"te", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"teo", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"th", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"ti-er", 0x2018, 0x2019, 0x201c, 0x201d, nullptr},
    {"to", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"tr", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"twq", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"tzm", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"uk", 0x00ab, 0x00bb, 0x201e, 0x201c, nullptr},
    {"ur", 0x201d, 0x201c, 0x2019, 0x2018, nullptr},
    {"vai", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"vai-latn", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"vi", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"vun", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"xh", 0x2018, 0x2019, 0x201c, 0x201d, nullptr},
    {"xog", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"yav", 0x00ab, 0x00bb, 0x00ab, 0x00bb, nullptr},
    {"yo", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"zh", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
    {"zh-hant", 0x300c, 0x300d, 0x300e, 0x300f, nullptr},
    {"zu", 0x201c, 0x201d, 0x2018, 0x2019, nullptr},
};

const QuotesData* QuotesDataForLanguage(const AtomicString& lang) {
  if (lang.IsNull())
    return nullptr;

  // This could be just a hash table, but doing that adds 200k to LayoutQuote.o
  Language* languages_end = g_languages + base::size(g_languages);
  std::string lowercase_lang = lang.LowerASCII().Utf8();
  Language key = {lowercase_lang.c_str(), 0, 0, 0, 0, nullptr};
  Language* match = std::lower_bound(g_languages, languages_end, key);
  if (match == languages_end || strcmp(match->lang, key.lang))
    return nullptr;

  if (!match->data) {
    auto data = QuotesData::Create(match->open1, match->close1, match->open2,
                                   match->close2);
    data->AddRef();
    match->data = data.get();
  }

  return match->data;
}

static const QuotesData* BasicQuotesData() {
  // FIXME: The default quotes should be the fancy quotes for "en".
  DEFINE_STATIC_REF(QuotesData, static_basic_quotes,
                    (QuotesData::Create('"', '"', '\'', '\'')));
  return static_basic_quotes;
}

void LayoutQuote::UpdateText() {
  String text = ComputeText();
  if (text_ == text)
    return;

  text_ = text;

  LayoutTextFragment* fragment = FindFragmentChild();
  if (fragment) {
    fragment->SetStyle(Style());
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
  // We walk from the end of the child list because, if we've had a first-letter
  // LayoutObject inserted then the remaining text will be at the end.
  while (LayoutObject* child = LastChild()) {
    if (child->IsText() && ToLayoutText(child)->IsTextFragment())
      return ToLayoutTextFragment(child);
  }

  return nullptr;
}

String LayoutQuote::ComputeText() const {
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

const QuotesData* LayoutQuote::GetQuotesData() const {
  if (const QuotesData* custom_quotes = StyleRef().Quotes())
    return custom_quotes;

  if (const QuotesData* quotes = QuotesDataForLanguage(StyleRef().Locale()))
    return quotes;

  return BasicQuotesData();
}

void LayoutQuote::AttachQuote() {
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

  for (LayoutObject* predecessor = PreviousInPreOrder(); predecessor;
       predecessor = predecessor->PreviousInPreOrder()) {
    // Skip unattached predecessors to avoid having stale m_previous pointers
    // if the previous node is never attached and is then destroyed.
    if (!predecessor->IsQuote() || !ToLayoutQuote(predecessor)->IsAttached())
      continue;
    previous_ = ToLayoutQuote(predecessor);
    next_ = previous_->next_;
    previous_->next_ = this;
    if (next_)
      next_->previous_ = this;
    break;
  }

  if (!previous_) {
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
