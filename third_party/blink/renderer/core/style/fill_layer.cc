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
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsFillLayer {
  FillLayer* next_;

  Persistent<StyleImage> image_;

  Length position_x_;
  Length position_y_;

  LengthSize size_length_;

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
      attachment_(
          static_cast<unsigned>(FillLayer::InitialFillAttachment(type))),
      clip_(static_cast<unsigned>(FillLayer::InitialFillClip(type))),
      origin_(static_cast<unsigned>(FillLayer::InitialFillOrigin(type))),
      repeat_x_(static_cast<unsigned>(FillLayer::InitialFillRepeatX(type))),
      repeat_y_(static_cast<unsigned>(FillLayer::InitialFillRepeatY(type))),
      composite_(FillLayer::InitialFillComposite(type)),
      size_type_(
          use_initial_values
              ? static_cast<unsigned>(FillLayer::InitialFillSizeType(type))
              : static_cast<unsigned>(EFillSizeType::kSizeNone)),
      blend_mode_(static_cast<unsigned>(FillLayer::InitialFillBlendMode(type))),
      background_x_origin_(static_cast<unsigned>(BackgroundEdgeOrigin::kLeft)),
      background_y_origin_(static_cast<unsigned>(BackgroundEdgeOrigin::kTop)),
      image_set_(use_initial_values),
      attachment_set_(use_initial_values),
      clip_set_(use_initial_values),
      origin_set_(use_initial_values),
      repeat_x_set_(use_initial_values),
      repeat_y_set_(use_initial_values),
      pos_x_set_(use_initial_values),
      pos_y_set_(use_initial_values),
      background_x_origin_set_(false),
      background_y_origin_set_(false),
      composite_set_(use_initial_values || type == EFillLayerType::kMask),
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
    : next_(o.next_ ? new FillLayer(*o.next_) : nullptr),
      image_(o.image_),
      position_x_(o.position_x_),
      position_y_(o.position_y_),
      size_length_(o.size_length_),
      attachment_(o.attachment_),
      clip_(o.clip_),
      origin_(o.origin_),
      repeat_x_(o.repeat_x_),
      repeat_y_(o.repeat_y_),
      composite_(o.composite_),
      size_type_(o.size_type_),
      blend_mode_(o.blend_mode_),
      background_x_origin_(o.background_x_origin_),
      background_y_origin_(o.background_y_origin_),
      image_set_(o.image_set_),
      attachment_set_(o.attachment_set_),
      clip_set_(o.clip_set_),
      origin_set_(o.origin_set_),
      repeat_x_set_(o.repeat_x_set_),
      repeat_y_set_(o.repeat_y_set_),
      pos_x_set_(o.pos_x_set_),
      pos_y_set_(o.pos_y_set_),
      background_x_origin_set_(o.background_x_origin_set_),
      background_y_origin_set_(o.background_y_origin_set_),
      composite_set_(o.composite_set_),
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

FillLayer::~FillLayer() {
  delete next_;
}

FillLayer& FillLayer::operator=(const FillLayer& o) {
  if (next_ != o.next_) {
    delete next_;
    next_ = o.next_ ? new FillLayer(*o.next_) : nullptr;
  }

  image_ = o.image_;
  position_x_ = o.position_x_;
  position_y_ = o.position_y_;
  background_x_origin_ = o.background_x_origin_;
  background_y_origin_ = o.background_y_origin_;
  background_x_origin_set_ = o.background_x_origin_set_;
  background_y_origin_set_ = o.background_y_origin_set_;
  size_length_ = o.size_length_;
  attachment_ = o.attachment_;
  clip_ = o.clip_;
  composite_ = o.composite_;
  blend_mode_ = o.blend_mode_;
  origin_ = o.origin_;
  repeat_x_ = o.repeat_x_;
  repeat_y_ = o.repeat_y_;
  size_type_ = o.size_type_;

  image_set_ = o.image_set_;
  attachment_set_ = o.attachment_set_;
  clip_set_ = o.clip_set_;
  composite_set_ = o.composite_set_;
  blend_mode_set_ = o.blend_mode_set_;
  origin_set_ = o.origin_set_;
  repeat_x_set_ = o.repeat_x_set_;
  repeat_y_set_ = o.repeat_y_set_;
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
         attachment_ == o.attachment_ && clip_ == o.clip_ &&
         composite_ == o.composite_ && blend_mode_ == o.blend_mode_ &&
         origin_ == o.origin_ && repeat_x_ == o.repeat_x_ &&
         repeat_y_ == o.repeat_y_ && size_type_ == o.size_type_ &&
         size_length_ == o.size_length_ && type_ == o.type_;
}

bool FillLayer::operator==(const FillLayer& o) const {
  return LayerPropertiesEqual(o) &&
         ((next_ && o.next_) ? *next_ == *o.next_ : next_ == o.next_);
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
    return next_->VisuallyEqual(*o.next_);
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

  for (curr = this; curr && curr->IsCompositeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->composite_ = pattern->composite_;
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

  for (curr = this; curr && curr->IsRepeatXSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->repeat_x_ = pattern->repeat_x_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern) {
        pattern = this;
      }
    }
  }

  for (curr = this; curr && curr->IsRepeatYSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->repeat_y_ = pattern->repeat_y_;
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
}

void FillLayer::CullEmptyLayers() {
  FillLayer* next;
  for (FillLayer* p = this; p; p = next) {
    next = p->next_;
    if (next && !next->IsImageSet()) {
      delete next;
      p->next_ = nullptr;
      break;
    }
  }
}

void FillLayer::ComputeCachedProperties() const {
  DCHECK(!cached_properties_computed_);

  layers_clip_max_ = static_cast<unsigned>(Clip());
  any_layer_uses_content_box_ =
      Clip() == EFillBox::kContent || Origin() == EFillBox::kContent;
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

  if (next_) {
    next_->ComputeCachedPropertiesIfNeeded();
    layers_clip_max_ = static_cast<unsigned>(
        EnclosingFillBox(LayersClipMax(), next_->LayersClipMax()));
    any_layer_uses_content_box_ |= next_->any_layer_uses_content_box_;
    any_layer_has_image_ |= next_->any_layer_has_image_;
    any_layer_has_url_image_ |= next_->any_layer_has_url_image_;
    any_layer_has_local_attachment_ |= next_->any_layer_has_local_attachment_;
    any_layer_has_fixed_attachment_image_ |=
        next_->any_layer_has_fixed_attachment_image_;
    any_layer_has_default_attachment_image_ |=
        next_->any_layer_has_default_attachment_image_;
    any_layer_uses_current_color_ |= next_->any_layer_uses_current_color_;
  }
}

bool FillLayer::ClipOccludesNextLayers() const {
  return Clip() == LayersClipMax();
}

bool FillLayer::ImagesAreLoaded() const {
  const FillLayer* curr;
  for (curr = this; curr; curr = curr->Next()) {
    if (curr->image_ && !curr->image_->IsLoaded()) {
      return false;
    }
  }

  return true;
}

bool FillLayer::ImageIsOpaque(const Document& document,
                              const ComputedStyle& style) const {
  // Returns whether we have an image that will cover the content below it when
  // composite_ == CompositeSourceOver && blend_mode_ == BlendMode::kNormal.
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
  // rectangle.
  // TODO(schenney) We could relax the repeat mode requirement if we also knew
  // the rect we had to fill, and the portion of the image we need to use, and
  // know that the latter covers the former.
  return (RepeatX() == EFillRepeat::kRepeatFill ||
          RepeatX() == EFillRepeat::kRoundFill) &&
         (RepeatY() == EFillRepeat::kRepeatFill ||
          RepeatY() == EFillRepeat::kRoundFill);
}

bool FillLayer::ImageOccludesNextLayers(const Document& document,
                                        const ComputedStyle& style) const {
  // We can't cover without an image, regardless of other parameters
  if (!image_ || !image_->CanRender()) {
    return false;
  }

  switch (composite_) {
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

}  // namespace blink
