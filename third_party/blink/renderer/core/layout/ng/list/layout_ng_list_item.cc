// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_image_resource_style_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"
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

  if (old_style && (old_style->ListStyleType() != StyleRef().ListStyleType() ||
                    (StyleRef().ListStyleType() == EListStyleType::kString &&
                     old_style->ListStyleStringValue() !=
                         StyleRef().ListStyleStringValue())))
    ListStyleTypeChanged();
}

// If the value of ListStyleType changed, we need to the marker text has been
// updated.
void LayoutNGListItem::ListStyleTypeChanged() {
  if (!is_marker_text_updated_)
    return;

  is_marker_text_updated_ = false;
  if (marker_) {
    marker_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kListStyleTypeChange);
  }
}

void LayoutNGListItem::OrdinalValueChanged() {
  if (marker_type_ == kOrdinalValue && is_marker_text_updated_) {
    is_marker_text_updated_ = false;

    // |marker_| can be a nullptr, for example, in the case of :after list item
    // elements.
    if (marker_) {
      marker_->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          layout_invalidation_reason::kListValueChange);
    }
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
  text->SetTextIfNeeded(marker_text_builder.ToString().ReleaseImpl());
  is_marker_text_updated_ = true;
}

void LayoutNGListItem::UpdateMarkerText() {
  DCHECK(marker_);
  UpdateMarkerText(ToLayoutText(marker_->SlowFirstChild()));
}

void LayoutNGListItem::UpdateMarker() {
  const ComputedStyle& style = StyleRef();
  if (style.ListStyleType() == EListStyleType::kNone && !IsMarkerImage()) {
    DestroyMarker();
    marker_type_ = kStatic;
    is_marker_text_updated_ = true;
    return;
  }

  // Create a marker box if it does not exist yet.
  Node* list_item = GetNode();
  const ComputedStyle* cached_marker_style =
      list_item->IsPseudoElement()
          ? nullptr
          : ToElement(list_item)->CachedStyleForPseudoElement(kPseudoIdMarker);
  scoped_refptr<ComputedStyle> marker_style;
  if (cached_marker_style) {
    marker_style = ComputedStyle::Clone(*cached_marker_style);
  } else {
    marker_style = ComputedStyle::Create();
    marker_style->InheritFrom(style);
  }
  if (IsInside()) {
    if (marker_ && !marker_->IsLayoutInline())
      DestroyMarker();
    if (!marker_)
      marker_ = LayoutNGInsideListMarker::CreateAnonymous(&GetDocument());
    marker_style->SetDisplay(EDisplay::kInline);
    auto margins =
        LayoutListMarker::InlineMarginsForInside(style, IsMarkerImage());
    marker_style->SetMarginStart(Length::Fixed(margins.first));
    marker_style->SetMarginEnd(Length::Fixed(margins.second));
    // Markers should have unicode-bidi:isolate according to the spec
    // (https://drafts.csswg.org/css-lists/#ua-stylesheet).
    // Note this is only relevant for inside markers with arbitrary strings.
    if (style.ListStyleType() == EListStyleType::kString)
      marker_style->SetUnicodeBidi(UnicodeBidi::kIsolate);
  } else {
    if (marker_ && !marker_->IsLayoutBlockFlow())
      DestroyMarker();
    if (!marker_)
      marker_ = LayoutNGListMarker::CreateAnonymous(&GetDocument());
    marker_style->SetDisplay(EDisplay::kInlineBlock);
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

LayoutNGListItem* LayoutNGListItem::FromMarker(const LayoutObject& marker) {
  DCHECK(marker.IsLayoutNGListMarkerIncludingInside());
  for (LayoutObject* parent = marker.Parent(); parent;
       parent = parent->Parent()) {
    if (parent->IsLayoutNGListItem()) {
      DCHECK(ToLayoutNGListItem(parent)->Marker() == &marker);
      return ToLayoutNGListItem(parent);
    }
    // These DCHECKs are not critical but to ensure we cover all cases we know.
    DCHECK(parent->IsAnonymous());
    DCHECK(parent->IsLayoutBlockFlow() || parent->IsLayoutFlowThread());
  }
  return nullptr;
}

int LayoutNGListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

LayoutNGListItem::MarkerType LayoutNGListItem::MarkerText(
    StringBuilder* text,
    MarkerTextFormat format) const {
  if (IsMarkerImage()) {
    if (format == kWithSuffix)
      text->Append(' ');
    return kStatic;
  }

  const ComputedStyle& style = StyleRef();
  switch (style.ListStyleType()) {
    case EListStyleType::kNone:
      return kStatic;
    case EListStyleType::kString: {
      text->Append(style.ListStyleStringValue());
      return kStatic;
    }
    case EListStyleType::kDisc:
    case EListStyleType::kCircle:
    case EListStyleType::kSquare:
      // value is ignored for these types
      text->Append(list_marker_text::GetText(style.ListStyleType(), 0));
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
      text->Append(list_marker_text::GetText(style.ListStyleType(), value));
      if (format == kWithSuffix) {
        text->Append(list_marker_text::Suffix(style.ListStyleType(), value));
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

String LayoutNGListItem::TextAlternative(const LayoutObject& marker) {
  // For accessibility, return the marker string in the logical order even in
  // RTL, reflecting speech order.
  if (LayoutNGListItem* list_item = FromMarker(marker))
    return list_item->MarkerTextWithSuffix();
  return g_empty_string;
}

void LayoutNGListItem::UpdateMarkerContentIfNeeded() {
  DCHECK(marker_);

  LayoutObject* child = marker_->SlowFirstChild();
  // There should be at most one child.
  DCHECK(!child || !child->SlowFirstChild());
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
          MakeGarbageCollected<LayoutImageResourceStyleImage>(
              list_style_image));
      image->SetIsGeneratedContent();
      marker_->AddChild(image);
    }
  } else {
    // Create a LayoutText in it.
    LayoutText* text = nullptr;
    // |text_style| should be as same as style propagated in
    // |LayoutObject::PropagateStyleToAnonymousChildren()| to avoid unexpected
    // full layout due by style difference. See http://crbug.com/980399
    scoped_refptr<ComputedStyle> text_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(
            marker_->StyleRef(), marker_->StyleRef().Display());
    if (child) {
      if (child->IsText()) {
        text = ToLayoutText(child);
        text->SetStyle(text_style);
      } else {
        child->Destroy();
        child = nullptr;
      }
    }
    if (!child) {
      text = LayoutText::CreateEmptyAnonymous(GetDocument(), text_style,
                                              LegacyLayout::kAuto);
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
