// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

// These are defined here because of PaintLayer dependency.

FragmentData::RareData::RareData() = default;
FragmentData::RareData::~RareData() = default;

void FragmentData::RareData::EnsureId() {
  if (!unique_id) {
    unique_id = NewUniqueObjectId();
  }
}

void FragmentData::RareData::SetLayer(PaintLayer* new_layer) {
  if (layer && layer != new_layer) {
    layer->Destroy();
    sticky_constraints = nullptr;
  }
  layer = new_layer;
}

void FragmentData::RareData::Trace(Visitor* visitor) const {
  visitor->Trace(layer);
  visitor->Trace(sticky_constraints);
  visitor->Trace(additional_fragments);
  visitor->Trace(paint_properties);
  visitor->Trace(local_border_box_properties);
}

FragmentData::RareData& FragmentData::EnsureRareData() {
  if (!rare_data_)
    rare_data_ = MakeGarbageCollected<RareData>();
  return *rare_data_;
}

void FragmentData::SetLayer(PaintLayer* layer) {
  AssertIsFirst();
  if (rare_data_ || layer)
    EnsureRareData().SetLayer(layer);
}

const TransformPaintPropertyNodeOrAlias& FragmentData::PreTransform() const {
  if (const auto* properties = PaintProperties()) {
    for (const TransformPaintPropertyNode* transform :
         properties->AllCSSTransformPropertiesOutsideToInside()) {
      if (transform) {
        DCHECK(transform->Parent());
        return *transform->Parent();
      }
    }
  }
  return LocalBorderBoxProperties().Transform();
}

const TransformPaintPropertyNodeOrAlias& FragmentData::ContentsTransform()
    const {
  if (const auto* properties = PaintProperties()) {
    if (properties->TransformIsolationNode())
      return *properties->TransformIsolationNode();
    if (properties->ScrollTranslation())
      return *properties->ScrollTranslation();
    if (properties->ReplacedContentTransform())
      return *properties->ReplacedContentTransform();
    if (properties->Perspective())
      return *properties->Perspective();
  }
  return LocalBorderBoxProperties().Transform();
}

const ClipPaintPropertyNodeOrAlias& FragmentData::PreClip() const {
  if (const auto* properties = PaintProperties()) {
    if (const auto* clip = properties->ClipPathClip()) {
      DCHECK(clip->Parent());
      return *clip->Parent();
    }
    if (const auto* mask_clip = properties->MaskClip()) {
      DCHECK(mask_clip->Parent());
      return *mask_clip->Parent();
    }
    if (const auto* css_clip = properties->CssClip()) {
      DCHECK(css_clip->Parent());
      return *css_clip->Parent();
    }
    if (const auto* clip = properties->PixelMovingFilterClipExpander()) {
      DCHECK(clip->Parent());
      return *clip->Parent();
    }
  }
  return LocalBorderBoxProperties().Clip();
}

const ClipPaintPropertyNodeOrAlias& FragmentData::ContentsClip() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->ClipIsolationNode())
      return *properties->ClipIsolationNode();
    if (properties->OverflowClip())
      return *properties->OverflowClip();
    if (properties->InnerBorderRadiusClip())
      return *properties->InnerBorderRadiusClip();
  }
  return LocalBorderBoxProperties().Clip();
}

const EffectPaintPropertyNodeOrAlias& FragmentData::PreEffect() const {
  if (const auto* properties = PaintProperties()) {
    if (const auto* effect = properties->Effect()) {
      DCHECK(effect->Parent());
      return *effect->Parent();
    }
    if (const auto* filter = properties->Filter()) {
      DCHECK(filter->Parent());
      return *filter->Parent();
    }
  }
  return LocalBorderBoxProperties().Effect();
}

const EffectPaintPropertyNodeOrAlias& FragmentData::ContentsEffect() const {
  if (const auto* properties = PaintProperties()) {
    if (properties->EffectIsolationNode())
      return *properties->EffectIsolationNode();
  }
  return LocalBorderBoxProperties().Effect();
}

FragmentData& FragmentDataList::AppendNewFragment() {
  AssertIsFirst();
  FragmentData* new_fragment = MakeGarbageCollected<FragmentData>();
  EnsureRareData().additional_fragments.push_back(new_fragment);
  return *new_fragment;
}

void FragmentDataList::Shrink(wtf_size_t new_size) {
  CHECK_GE(new_size, 1u);
  CHECK_LE(new_size, size());
  if (rare_data_) {
    rare_data_->additional_fragments.resize(new_size - 1);
  }
}

FragmentData& FragmentDataList::back() {
  AssertIsFirst();
  if (rare_data_ && !rare_data_->additional_fragments.empty()) {
    return *rare_data_->additional_fragments.back();
  }
  return *this;
}

const FragmentData& FragmentDataList::back() const {
  return const_cast<FragmentDataList*>(this)->back();
}

FragmentData& FragmentDataList::at(wtf_size_t idx) {
  AssertIsFirst();
  if (idx == 0) {
    return *this;
  }
  CHECK(rare_data_);
  return *rare_data_->additional_fragments.at(idx - 1);
}

const FragmentData& FragmentDataList::at(wtf_size_t idx) const {
  return const_cast<FragmentDataList*>(this)->at(idx);
}

wtf_size_t FragmentDataList::size() const {
  return rare_data_ ? rare_data_->additional_fragments.size() + 1 : 1;
}

}  // namespace blink
