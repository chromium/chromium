// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

// These are defined here because of PaintLayer dependency.

FragmentData::RareData::RareData() : unique_id(NewUniqueObjectId()) {}

FragmentData::RareData::~RareData() = default;

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
  visitor->Trace(next_fragment_);
}

void FragmentData::ClearNextFragment() {
  if (!rare_data_)
    return;
  // Take next_fragment_ which clears it in this fragment.
  FragmentData* next = rare_data_->next_fragment_.Release();
  while (next && next->rare_data_) {
    next = next->rare_data_->next_fragment_.Release();
  }
}

FragmentData& FragmentData::EnsureNextFragment() {
  if (!NextFragment())
    EnsureRareData().next_fragment_ = MakeGarbageCollected<FragmentData>();
  return *rare_data_->next_fragment_;
}

FragmentData& FragmentData::LastFragment() {
  for (FragmentData* fragment = this;;) {
    FragmentData* next = fragment->NextFragment();
    if (!next)
      return *fragment;
    fragment = next;
  }
}

const FragmentData& FragmentData::LastFragment() const {
  return const_cast<FragmentData*>(this)->LastFragment();
}

FragmentData::RareData& FragmentData::EnsureRareData() {
  if (!rare_data_)
    rare_data_ = MakeGarbageCollected<RareData>();
  return *rare_data_;
}

void FragmentData::SetLayer(PaintLayer* layer) {
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

}  // namespace blink
