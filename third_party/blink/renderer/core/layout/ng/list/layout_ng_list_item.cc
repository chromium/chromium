// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker_image.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

LayoutNGListItem::LayoutNGListItem(Element* element)
    : LayoutNGBlockFlow(element),
      marker_type_(kStatic),
      is_marker_text_updated_(false) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
}

bool LayoutNGListItem::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListItem || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGListItem::WillBeDestroyed() {
  DestroyMarker();

  LayoutNGBlockFlow::WillBeDestroyed();
}

void LayoutNGListItem::InsertedIntoTree() {
  LayoutNGBlockFlow::InsertedIntoTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::WillBeRemovedFromTree() {
  LayoutNGBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);

  UpdateMarker();
}

void LayoutNGListItem::OrdinalValueChanged() {
  if (marker_type_ == kOrdinalValue && is_marker_text_updated_) {
    is_marker_text_updated_ = false;
    DCHECK(marker_);
    marker_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::kListValueChange);
  }
}

void LayoutNGListItem::SubtreeDidChange() {
  if (!marker_)
    return;

  if (ordinal_.NotInListChanged()) {
    UpdateMarker();
    ordinal_.SetNotInListChanged(false);
    return;
  }

  // Make sure outside marker is the direct child of ListItem.
  if (!IsInside() && marker_->Parent() != this) {
    marker_->Remove();
    AddChild(marker_, FirstChild());
  }

  UpdateMarkerContentIfNeeded();
}

void LayoutNGListItem::WillCollectInlines() {
  UpdateMarkerTextIfNeeded();
}

// Returns true if this is 'list-style-position: inside', or should be laid out
// as 'inside'.
bool LayoutNGListItem::IsInside() const {
  return ordinal_.NotInList() ||
         StyleRef().ListStylePosition() == EListStylePosition::kInside;
}

// Destroy the list marker objects if exists.
void LayoutNGListItem::DestroyMarker() {
  if (marker_) {
    marker_->Destroy();
    marker_ = nullptr;
  }
}

void LayoutNGListItem::UpdateMarkerText(LayoutText* text) {
  DCHECK(text);
  StringBuilder marker_text_builder;
  marker_type_ = MarkerText(&marker_text_builder, kWithSuffix);
  text->SetText(marker_text_builder.ToString().ReleaseImpl());
  is_marker_text_updated_ = true;
}

void LayoutNGListItem::UpdateMarkerText() {
  DCHECK(marker_);
  UpdateMarkerText(ToLayoutText(marker_->SlowFirstChild()));
}

void LayoutNGListItem::UpdateMarker() {
  const ComputedStyle& style = StyleRef();
  if (style.ListStyleType() == EListStyleType::kNone) {
    DestroyMarker();
    marker_type_ = kStatic;
    is_marker_text_updated_ = true;
    return;
  }

  // Create a marker box if it does not exist yet.
  scoped_refptr<ComputedStyle> marker_style;
  if (IsInside()) {
    if (marker_ && !marker_->IsLayoutInline())
      DestroyMarker();
    if (!marker_)
      marker_ = LayoutInline::CreateAnonymous(&GetDocument());
    marker_style = ComputedStyle::CreateAnonymousStyleWithDisplay(
        style, EDisplay::kInline);
    auto margins =
        LayoutListMarker::InlineMarginsForInside(style, IsMarkerImage());
    marker_style->SetMarginStart(Length(margins.first, kFixed));
    marker_style->SetMarginEnd(Length(margins.second, kFixed));
  } else {
    if (marker_ && !marker_->IsLayoutBlockFlow())
      DestroyMarker();
    if (!marker_)
      marker_ = LayoutNGListMarker::CreateAnonymous(&GetDocument());
    marker_style = ComputedStyle::CreateAnonymousStyleWithDisplay(
        style, EDisplay::kInlineBlock);
    // Do not break inside the marker, and honor the trailing spaces.
    marker_style->SetWhiteSpace(EWhiteSpace::kPre);
    // Compute margins for 'outside' during layout, because it requires the
    // layout size of the marker.
    // TODO(kojii): absolute position looks more reasonable, and maybe required
    // in some cases, but this is currently blocked by crbug.com/734554
    // marker_style->SetPosition(EPosition::kAbsolute);
    // marker_->SetPositionState(EPosition::kAbsolute);
  }
  marker_->SetStyle(std::move(marker_style));

  UpdateMarkerContentIfNeeded();

  LayoutObject* first_child = FirstChild();
  if (first_child != marker_) {
    marker_->Remove();
    AddChild(marker_, FirstChild());
  }
}

int LayoutNGListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

LayoutNGListItem::MarkerType LayoutNGListItem::MarkerText(
    StringBuilder* text,
    MarkerTextFormat format) const {
  const ComputedStyle& style = StyleRef();
  switch (style.ListStyleType()) {
    case EListStyleType::kNone:
      return kStatic;
    case EListStyleType::kDisc:
    case EListStyleType::kCircle:
    case EListStyleType::kSquare:
      // value is ignored for these types
      text->Append(list_marker_text::GetText(Style()->ListStyleType(), 0));
      if (format == kWithSuffix)
        text->Append(' ');
      return kSymbolValue;
    case EListStyleType::kArabicIndic:
    case EListStyleType::kArmenian:
    case EListStyleType::kBengali:
    case EListStyleType::kCambodian:
    case EListStyleType::kCjkIdeographic:
    case EListStyleType::kCjkEarthlyBranch:
    case EListStyleType::kCjkHeavenlyStem:
    case EListStyleType::kDecimalLeadingZero:
    case EListStyleType::kDecimal:
    case EListStyleType::kDevanagari:
    case EListStyleType::kEthiopicHalehame:
    case EListStyleType::kEthiopicHalehameAm:
    case EListStyleType::kEthiopicHalehameTiEr:
    case EListStyleType::kEthiopicHalehameTiEt:
    case EListStyleType::kGeorgian:
    case EListStyleType::kGujarati:
    case EListStyleType::kGurmukhi:
    case EListStyleType::kHangul:
    case EListStyleType::kHangulConsonant:
    case EListStyleType::kHebrew:
    case EListStyleType::kHiragana:
    case EListStyleType::kHiraganaIroha:
    case EListStyleType::kKannada:
    case EListStyleType::kKatakana:
    case EListStyleType::kKatakanaIroha:
    case EListStyleType::kKhmer:
    case EListStyleType::kKoreanHangulFormal:
    case EListStyleType::kKoreanHanjaFormal:
    case EListStyleType::kKoreanHanjaInformal:
    case EListStyleType::kLao:
    case EListStyleType::kLowerAlpha:
    case EListStyleType::kLowerArmenian:
    case EListStyleType::kLowerGreek:
    case EListStyleType::kLowerLatin:
    case EListStyleType::kLowerRoman:
    case EListStyleType::kMalayalam:
    case EListStyleType::kMongolian:
    case EListStyleType::kMyanmar:
    case EListStyleType::kOriya:
    case EListStyleType::kPersian:
    case EListStyleType::kSimpChineseFormal:
    case EListStyleType::kSimpChineseInformal:
    case EListStyleType::kTelugu:
    case EListStyleType::kThai:
    case EListStyleType::kTibetan:
    case EListStyleType::kTradChineseFormal:
    case EListStyleType::kTradChineseInformal:
    case EListStyleType::kUpperAlpha:
    case EListStyleType::kUpperArmenian:
    case EListStyleType::kUpperLatin:
    case EListStyleType::kUpperRoman:
    case EListStyleType::kUrdu: {
      int value = Value();
      text->Append(list_marker_text::GetText(Style()->ListStyleType(), value));
      if (format == kWithSuffix) {
        text->Append(list_marker_text::Suffix(Style()->ListStyleType(), value));
        text->Append(' ');
      }
      return kOrdinalValue;
    }
  }
  NOTREACHED();
  return kStatic;
}

String LayoutNGListItem::MarkerTextWithSuffix() const {
  StringBuilder text;
  MarkerText(&text, kWithSuffix);
  return text.ToString();
}

String LayoutNGListItem::MarkerTextWithoutSuffix() const {
  StringBuilder text;
  MarkerText(&text, kWithoutSuffix);
  return text.ToString();
}

void LayoutNGListItem::UpdateMarkerContentIfNeeded() {
  DCHECK(marker_);

  LayoutObject* child = marker_->SlowFirstChild();
  if (IsMarkerImage()) {
    StyleImage* list_style_image = StyleRef().ListStyleImage();
    if (child) {
      // If the url of `list-style-image` changed, create a new LayoutImage.
      if (!child->IsLayoutImage() ||
          ToLayoutImage(child)->ImageResource()->ImagePtr() !=
              list_style_image->Data()) {
        child->Destroy();
        child = nullptr;
      }
    }
    if (!child) {
      LayoutNGListMarkerImage* image =
          LayoutNGListMarkerImage::CreateAnonymous(&GetDocument());
      scoped_refptr<ComputedStyle> image_style =
          ComputedStyle::CreateAnonymousStyleWithDisplay(marker_->StyleRef(),
                                                         EDisplay::kInline);
      image->SetStyle(image_style);
      image->SetImageResource(
          LayoutImageResourceStyleImage::Create(list_style_image));
      image->SetIsGeneratedContent();
      marker_->AddChild(image);
    }
  } else {
    // Create a LayoutText in it.
    LayoutText* text = nullptr;
    if (child) {
      if (child->IsText()) {
        text = ToLayoutText(child);
        text->SetStyle(marker_->MutableStyle());
      } else {
        child->Destroy();
        child = nullptr;
      }
    }
    if (!child) {
      text = LayoutText::CreateEmptyAnonymous(GetDocument(),
                                              marker_->MutableStyle());
      marker_->AddChild(text);
      is_marker_text_updated_ = false;
    }
  }
}

LayoutObject* LayoutNGListItem::SymbolMarkerLayoutText() const {
  if (marker_type_ != kSymbolValue)
    return nullptr;
  DCHECK(marker_);
  return marker_->SlowFirstChild();
}

const LayoutObject* LayoutNGListItem::FindSymbolMarkerLayoutText(
    const LayoutObject* object) {
  if (!object)
    return nullptr;

  if (object->IsLayoutNGListItem())
    return ToLayoutNGListItem(object)->SymbolMarkerLayoutText();

  if (object->IsLayoutNGListMarker())
    return ToLayoutNGListMarker(object)->SymbolMarkerLayoutText();

  if (object->IsAnonymousBlock())
    return FindSymbolMarkerLayoutText(object->Parent());

  return nullptr;
}

}  // namespace blink
