/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/style/fill_layer.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_mask_source_image.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsFillLayer {
  Member<FillLayerWrapper> next_;
  Member<StyleImage> image_;

  Length position_x_;
  Length position_y_;

  LengthSize size_length_;
  FillRepeat repeat_;

  unsigned bitfields1_;
  unsigned bitfields2_;
};

ASSERT_SIZE(FillLayer, SameSizeAsFillLayer);

FillLayer::FillLayer(EFillLayerType type, bool use_initial_values)
    : next_(nullptr),
      image_(FillLayer::InitialFillImage(type)),
      position_x_(FillLayer::InitialFillPositionX(type)),
      position_y_(FillLayer::InitialFillPositionY(type)),
      size_length_(FillLayer::InitialFillSizeLength(type)),
      repeat_(FillLayer::InitialFillRepeat(type)),
      attachment_(
          static_cast<unsigned>(FillLayer::InitialFillAttachment(type))),
      clip_(static_cast<unsigned>(FillLayer::InitialFillClip(type))),
      origin_(static_cast<unsigned>(FillLayer::InitialFillOrigin(type))),
      compositing_operator_(static_cast<unsigned>(
          FillLayer::InitialFillCompositingOperator(type))),
      size_type_(
          use_initial_values
              ? static_cast<unsigned>(FillLayer::InitialFillSizeType(type))
              : static_cast<unsigned>(EFillSizeType::kSizeNone)),
      blend_mode_(static_cast<unsigned>(FillLayer::InitialFillBlendMode(type))),
      background_x_origin_(static_cast<unsigned>(BackgroundEdgeOrigin::kLeft)),
      background_y_origin_(static_cast<unsigned>(BackgroundEdgeOrigin::kTop)),
      mask_mode_(static_cast<unsigned>(FillLayer::InitialFillMaskMode(type))),
      image_set_(use_initial_values),
      attachment_set_(use_initial_values),
      clip_set_(use_initial_values),
      origin_set_(use_initial_values),
      repeat_set_(use_initial_values),
      mask_mode_set_(use_initial_values),
      pos_x_set_(use_initial_values),
      pos_y_set_(use_initial_values),
      background_x_origin_set_(false),
      background_y_origin_set_(false),
      compositing_operator_set_(use_initial_values ||
                                type == EFillLayerType::kMask),
      blend_mode_set_(use_initial_values),
      type_(static_cast<unsigned>(type)),
      layers_clip_max_(0),
      any_layer_uses_content_box_(false),
      any_layer_has_image_(false),
      any_layer_has_url_image_(false),
      any_layer_has_local_attachment_(false),
      any_layer_has_fixed_attachment_image_(false),
      any_layer_has_default_attachment_image_(false),
      cached_properties_computed_(false) {}

FillLayer::FillLayer(const FillLayer& o)
    : next_(o.next_ ? MakeGarbageCollected<FillLayerWrapper>(*o.next_)
                    : nullptr),
      image_(o.image_),
      position_x_(o.position_x_),
      position_y_(o.position_y_),
      size_length_(o.size_length_),
      repeat_(o.repeat_),
      attachment_(o.attachment_),
      clip_(o.clip_),
      origin_(o.origin_),
      compositing_operator_(o.compositing_operator_),
      size_type_(o.size_type_),
      blend_mode_(o.blend_mode_),
      background_x_origin_(o.background_x_origin_),
      background_y_origin_(o.background_y_origin_),
      mask_mode_(o.mask_mode_),
      image_set_(o.image_set_),
      attachment_set_(o.attachment_set_),
      clip_set_(o.clip_set_),
      origin_set_(o.origin_set_),
      repeat_set_(o.repeat_set_),
      mask_mode_set_(o.mask_mode_set_),
      pos_x_set_(o.pos_x_set_),
      pos_y_set_(o.pos_y_set_),
      background_x_origin_set_(o.background_x_origin_set_),
      background_y_origin_set_(o.background_y_origin_set_),
      compositing_operator_set_(o.compositing_operator_set_),
      blend_mode_set_(o.blend_mode_set_),
      type_(o.type_),
      layers_clip_max_(0),
      any_layer_uses_content_box_(false),
      any_layer_has_image_(false),
      any_layer_has_url_image_(false),
      any_layer_has_local_attachment_(false),
      any_layer_has_fixed_attachment_image_(false),
      any_layer_has_default_attachment_image_(false),
      cached_properties_computed_(false) {}

void FillLayer::Trace(Visitor* visitor) const {
  visitor->Trace(next_);
  visitor->Trace(image_);
}

FillLayer& FillLayer::operator=(const FillLayer& o) {
  if (next_ != o.next_) {
    next_ =
        o.next_ ? MakeGarbageCollected<FillLayerWrapper>(*o.next_) : nullptr;
  }

  image_ = o.image_;
  position_x_ = o.position_x_;
  position_y_ = o.position_y_;
  background_x_origin_ = o.background_x_origin_;
  background_y_origin_ = o.background_y_origin_;
  mask_mode_ = o.mask_mode_;
  background_x_origin_set_ = o.background_x_origin_set_;
  background_y_origin_set_ = o.background_y_origin_set_;
  size_length_ = o.size_length_;
  attachment_ = o.attachment_;
  clip_ = o.clip_;
  compositing_operator_ = o.compositing_operator_;
  blend_mode_ = o.blend_mode_;
  origin_ = o.origin_;
  repeat_ = o.repeat_;
  size_type_ = o.size_type_;

  image_set_ = o.image_set_;
  attachment_set_ = o.attachment_set_;
  clip_set_ = o.clip_set_;
  compositing_operator_set_ = o.compositing_operator_set_;
  blend_mode_set_ = o.blend_mode_set_;
  origin_set_ = o.origin_set_;
  repeat_set_ = o.repeat_set_;
  mask_mode_set_ = o.mask_mode_set_;
  pos_x_set_ = o.pos_x_set_;
  pos_y_set_ = o.pos_y_set_;

  type_ = o.type_;

  cached_properties_computed_ = false;

  return *this;
}

bool FillLayer::LayerPropertiesEqual(const FillLayer& o) const {
  return base::ValuesEquivalent(image_, o.image_) &&
         position_x_ == o.position_x_ && position_y_ == o.position_y_ &&
         background_x_origin_ == o.background_x_origin_ &&
         background_y_origin_ == o.background_y_origin_ &&
         mask_mode_ == o.mask_mode_ && attachment_ == o.attachment_ &&
         clip_ == o.clip_ && compositing_operator_ == o.compositing_operator_ &&
         blend_mode_ == o.blend_mode_ && origin_ == o.origin_ &&
         repeat_ == o.repeat_ && size_type_ == o.size_type_ &&
         size_length_ == o.size_length_ && type_ == o.type_;
}

bool FillLayer::operator==(const FillLayer& o) const {
  return LayerPropertiesEqual(o) &&
         ((Next() && o.Next()) ? *Next() == *o.Next() : Next() == o.Next());
}

bool FillLayer::VisuallyEqual(const FillLayer& o) const {
  if (image_ || o.image_) {
    if (!LayerPropertiesEqual(o)) {
      return false;
    }
  } else if (clip_ != o.clip_) {
    return false;
  }
  if (next_ && o.next_) {
    return next_->layer.VisuallyEqual(o.next_->layer);
  }
  return next_ == o.next_;
}

void FillLayer::FillUnsetProperties() {
  FillLayer* curr;
  for (curr = this; curr && curr->IsPositionXSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->position_x_ = pattern->position_x_;
      if (pattern->IsBackgroundXOriginSet()) {
        curr->background_x_origin_ = pattern->background_x_origin_;
      }
      if (pattern->IsBackgroundYOriginSet()) {
        curr->background_y_origin_ = pattern->background_y_origin_;
      }
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsPositionYSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->position_y_ = pattern->position_y_;
      if (pattern->IsBackgroundXOriginSet()) {
        curr->background_x_origin_ = pattern->background_x_origin_;
      }
      if (pattern->IsBackgroundYOriginSet()) {
        curr->background_y_origin_ = pattern->background_y_origin_;
      }
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsAttachmentSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->attachment_ = pattern->attachment_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsClipSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->clip_ = pattern->clip_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsCompositingOperatorSet();
       curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->compositing_operator_ = pattern->compositing_operator_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsBlendModeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->blend_mode_ = pattern->blend_mode_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsOriginSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->origin_ = pattern->origin_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsRepeatSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->repeat_ = pattern->repeat_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsSizeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->size_type_ = pattern->size_type_;
      curr->size_length_ = pattern->size_length_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsMaskModeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->mask_mode_ = pattern->mask_mode_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }
}

void FillLayer::CullEmptyLayers() {
  FillLayer* next;
  for (FillLayer* p = this; p; p = next) {
    next = p->Next();
    if (next && !next->IsImageSet()) {
      p->next_ = nullptr;
      break;
    }
  }
}

EFillBox FillLayer::EffectiveClip() const {
  // When the layer is for a mask and the image is an SVG <mask> reference, the
  // effective clip value is no-clip.
  if (GetType() == EFillLayerType::kMask) {
    const auto* mask_source = DynamicTo<StyleMaskSourceImage>(GetImage());
    if (mask_source && mask_source->HasSVGMask()) {
      return EFillBox::kNoClip;
    }
  }
  return Clip();
}

void FillLayer::ComputeCachedProperties() const {
  DCHECK(!cached_properties_computed_);

  const EFillBox effective_clip = EffectiveClip();
  layers_clip_max_ = static_cast<unsigned>(effective_clip);
  any_layer_uses_content_box_ =
      effective_clip == EFillBox::kContent || Origin() == EFillBox::kContent;
  any_layer_has_image_ = !!GetImage();
  any_layer_has_url_image_ =
      any_layer_has_image_ && GetImage()->CssValue()->MayContainUrl();
  any_layer_has_local_attachment_ = Attachment() == EFillAttachment::kLocal;
  any_layer_has_fixed_attachment_image_ =
      any_layer_has_image_ && Attachment() == EFillAttachment::kFixed;
  any_layer_has_default_attachment_image_ =
      any_layer_has_image_ && Attachment() == EFillAttachment::kScroll;
  any_layer_uses_current_color_ =
      (image_ && image_->IsGeneratedImage() &&
       To<StyleGeneratedImage>(image_.Get())->IsUsingCurrentColor());
  cached_properties_computed_ = true;

  if (auto* next = Next()) {
    next->ComputeCachedPropertiesIfNeeded();
    layers_clip_max_ = static_cast<unsigned>(
        EnclosingFillBox(LayersClipMax(), next->LayersClipMax()));
    any_layer_uses_content_box_ |= next->any_layer_uses_content_box_;
    any_layer_has_image_ |= next->any_layer_has_image_;
    any_layer_has_url_image_ |= next->any_layer_has_url_image_;
    any_layer_has_local_attachment_ |= next->any_layer_has_local_attachment_;
    any_layer_has_fixed_attachment_image_ |=
        next->any_layer_has_fixed_attachment_image_;
    any_layer_has_default_attachment_image_ |=
        next->any_layer_has_default_attachment_image_;
    any_layer_uses_current_color_ |= next->any_layer_uses_current_color_;
  }
}

bool FillLayer::ClipOccludesNextLayers() const {
  return Clip() == LayersClipMax();
}

bool FillLayer::ImageIsOpaque(const Document& document,
                              const ComputedStyle& style) const {
  // Returns whether we have an image that will cover the content below it when
  // Composite() == CompositeSourceOver && GetBlendMode() == BlendMode::kNormal.
  // Note that it doesn't matter what orientation we use because we are only
  // checking for IsEmpty.
  return image_->KnownToBeOpaque(document, style) &&
         !image_
              ->ImageSize(style.EffectiveZoom(), gfx::SizeF(),
                          kRespectImageOrientation)
              .IsEmpty();
}

bool FillLayer::ImageTilesLayer() const {
  // Returns true if an image will be tiled such that it covers any sized
  // rectangle. We could relax the repeat mode requirement if we also knew
  // the rect we had to fill, and the portion of the image we need to use, and
  // know that the latter covers the former.
  FillRepeat repeat = Repeat();

  return (repeat.x == EFillRepeat::kRepeatFill ||
          repeat.x == EFillRepeat::kRoundFill) &&
         (repeat.y == EFillRepeat::kRepeatFill ||
          repeat.y == EFillRepeat::kRoundFill);
}

bool FillLayer::ImageOccludesNextLayers(const Document& document,
                                        const ComputedStyle& style) const {
  // We can't cover without an image, regardless of other parameters
  if (!image_ || !image_->CanRender()) {
    return false;
  }

  switch (Composite()) {
    case kCompositeClear:
    case kCompositeCopy:
      return ImageTilesLayer();
    case kCompositeSourceOver:
      return GetBlendMode() == BlendMode::kNormal && ImageTilesLayer() &&
             ImageIsOpaque(document, style);
    default: {
    }
  }

  return false;
}

static inline bool LayerImagesIdentical(const FillLayer& layer1,
                                        const FillLayer& layer2) {
  // We just care about pointer equivalency.
  return layer1.GetImage() == layer2.GetImage();
}

bool FillLayer::ImagesIdentical(const FillLayer* layer1,
                                const FillLayer* layer2) {
  for (; layer1 && layer2; layer1 = layer1->Next(), layer2 = layer2->Next()) {
    if (!LayerImagesIdentical(*layer1, *layer2)) {
      return false;
    }
  }

  return !layer1 && !layer2;
}

CompositeOperator FillLayer::Composite() const {
  switch (CompositingOperator()) {
    case CompositingOperator::kAdd:
      return kCompositeSourceOver;
    case CompositingOperator::kSubtract:
      return kCompositeSourceOut;
    case CompositingOperator::kIntersect:
      return kCompositeSourceIn;
    case CompositingOperator::kExclude:
      return kCompositeXOR;
    case CompositingOperator::kClear:
      return kCompositeClear;
    case CompositingOperator::kCopy:
      return kCompositeCopy;
    case CompositingOperator::kSourceOver:
      return kCompositeSourceOver;
    case CompositingOperator::kSourceIn:
      return kCompositeSourceIn;
    case CompositingOperator::kSourceOut:
      return kCompositeSourceOut;
    case CompositingOperator::kSourceAtop:
      return kCompositeSourceAtop;
    case CompositingOperator::kDestinationOver:
      return kCompositeDestinationOver;
    case CompositingOperator::kDestinationIn:
      return kCompositeDestinationIn;
    case CompositingOperator::kDestinationOut:
      return kCompositeDestinationOut;
    case CompositingOperator::kDestinationAtop:
      return kCompositeDestinationAtop;
    case CompositingOperator::kXOR:
      return kCompositeXOR;
    case CompositingOperator::kPlusLighter:
      return kCompositePlusLighter;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace blink
