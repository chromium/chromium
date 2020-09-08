/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILL_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILL_LAYER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct FillSize {
  DISALLOW_NEW();
  FillSize() : type(EFillSizeType::kSizeLength) {}

  FillSize(EFillSizeType t, const LengthSize& l) : type(t), size(l) {}

  bool operator==(const FillSize& o) const {
    return type == o.type && size == o.size;
  }
  bool operator!=(const FillSize& o) const { return !(*this == o); }

  EFillSizeType type;
  LengthSize size;
};

// FIXME(Oilpan): Move FillLayer to Oilpan's heap.
class CORE_EXPORT FillLayer {
  USING_FAST_MALLOC(FillLayer);

 public:
  FillLayer(EFillLayerType, bool use_initial_values = false);
  ~FillLayer();

  StyleImage* GetImage() const { return image_.Get(); }
  const Length& PositionX() const { return position_x_; }
  const Length& PositionY() const { return position_y_; }
  BackgroundEdgeOrigin BackgroundXOrigin() const {
    return static_cast<BackgroundEdgeOrigin>(background_x_origin_);
  }
  BackgroundEdgeOrigin BackgroundYOrigin() const {
    return static_cast<BackgroundEdgeOrigin>(background_y_origin_);
  }
  EFillAttachment Attachment() const {
    return static_cast<EFillAttachment>(attachment_);
  }
  EFillBox Clip() const { return static_cast<EFillBox>(clip_); }
  EFillBox Origin() const { return static_cast<EFillBox>(origin_); }
  EFillRepeat RepeatX() const { return static_cast<EFillRepeat>(repeat_x_); }
  EFillRepeat RepeatY() const { return static_cast<EFillRepeat>(repeat_y_); }
  CompositeOperator Composite() const {
    return static_cast<CompositeOperator>(composite_);
  }
  BlendMode GetBlendMode() const { return static_cast<BlendMode>(blend_mode_); }
  const LengthSize& SizeLength() const { return size_length_; }
  EFillSizeType SizeType() const {
    return static_cast<EFillSizeType>(size_type_);
  }
  FillSize Size() const {
    return FillSize(static_cast<EFillSizeType>(size_type_), size_length_);
  }

  const FillLayer* Next() const { return next_; }
  FillLayer* Next() { return next_; }
  FillLayer* EnsureNext() {
    if (!next_)
      next_ = new FillLayer(GetType());
    return next_;
  }

  bool IsImageSet() const { return image_set_; }
  bool IsPositionXSet() const { return pos_x_set_; }
  bool IsPositionYSet() const { return pos_y_set_; }
  bool IsBackgroundXOriginSet() const { return background_x_origin_set_; }
  bool IsBackgroundYOriginSet() const { return background_y_origin_set_; }
  bool IsAttachmentSet() const { return attachment_set_; }
  bool IsClipSet() const { return clip_set_; }
  bool IsOriginSet() const { return origin_set_; }
  bool IsRepeatXSet() const { return repeat_x_set_; }
  bool IsRepeatYSet() const { return repeat_y_set_; }
  bool IsCompositeSet() const { return composite_set_; }
  bool IsBlendModeSet() const { return blend_mode_set_; }
  bool IsSizeSet() const {
    return size_type_ != static_cast<unsigned>(EFillSizeType::kSizeNone);
  }

  void SetImage(StyleImage* i) {
    image_ = i;
    image_set_ = true;
  }
  void SetPositionX(const Length& position) {
    position_x_ = position;
    pos_x_set_ = true;
    background_x_origin_set_ = false;
    background_x_origin_ = static_cast<unsigned>(BackgroundEdgeOrigin::kLeft);
  }
  void SetPositionY(const Length& position) {
    position_y_ = position;
    pos_y_set_ = true;
    background_y_origin_set_ = false;
    background_y_origin_ = static_cast<unsigned>(BackgroundEdgeOrigin::kTop);
  }
  void SetBackgroundXOrigin(BackgroundEdgeOrigin origin) {
    background_x_origin_ = static_cast<unsigned>(origin);
    background_x_origin_set_ = true;
  }
  void SetBackgroundYOrigin(BackgroundEdgeOrigin origin) {
    background_y_origin_ = static_cast<unsigned>(origin);
    background_y_origin_set_ = true;
  }
  void SetAttachment(EFillAttachment attachment) {
    DCHECK(!cached_properties_computed_);
    attachment_ = static_cast<unsigned>(attachment);
    attachment_set_ = true;
  }
  void SetClip(EFillBox b) {
    DCHECK(!cached_properties_computed_);
    clip_ = static_cast<unsigned>(b);
    clip_set_ = true;
  }
  void SetOrigin(EFillBox b) {
    DCHECK(!cached_properties_computed_);
    origin_ = static_cast<unsigned>(b);
    origin_set_ = true;
  }
  void SetRepeatX(EFillRepeat r) {
    repeat_x_ = static_cast<unsigned>(r);
    repeat_x_set_ = true;
  }
  void SetRepeatY(EFillRepeat r) {
    repeat_y_ = static_cast<unsigned>(r);
    repeat_y_set_ = true;
  }
  void SetComposite(CompositeOperator c) {
    composite_ = c;
    composite_set_ = true;
  }
  void SetBlendMode(BlendMode b) {
    blend_mode_ = static_cast<unsigned>(b);
    blend_mode_set_ = true;
  }
  void SetSizeType(EFillSizeType b) { size_type_ = static_cast<unsigned>(b); }
  void SetSizeLength(const LengthSize& length) { size_length_ = length; }
  void SetSize(const FillSize& f) {
    size_type_ = static_cast<unsigned>(f.type);
    size_length_ = f.size;
  }

  void ClearImage() {
    image_.Clear();
    image_set_ = false;
  }
  void ClearPositionX() {
    pos_x_set_ = false;
    background_x_origin_set_ = false;
  }
  void ClearPositionY() {
    pos_y_set_ = false;
    background_y_origin_set_ = false;
  }

  void ClearAttachment() { attachment_set_ = false; }
  void ClearClip() { clip_set_ = false; }
  void ClearOrigin() { origin_set_ = false; }
  void ClearRepeatX() { repeat_x_set_ = false; }
  void ClearRepeatY() { repeat_y_set_ = false; }
  void ClearComposite() { composite_set_ = false; }
  void ClearBlendMode() { blend_mode_set_ = false; }
  void ClearSize() {
    size_type_ = static_cast<unsigned>(EFillSizeType::kSizeNone);
  }

  FillLayer& operator=(const FillLayer&);
  FillLayer(const FillLayer&);

  bool operator==(const FillLayer&) const;
  bool operator!=(const FillLayer& o) const { return !(*this == o); }

  bool VisuallyEqual(const FillLayer&) const;

  bool ImagesAreLoaded() const;

  bool ImageOccludesNextLayers(const Document&, const ComputedStyle&) const;
  bool HasRepeatXY() const;
  bool ClipOccludesNextLayers() const;

  EFillLayerType GetType() const { return static_cast<EFillLayerType>(type_); }

  void FillUnsetProperties();
  void CullEmptyLayers();

  static bool ImagesIdentical(const FillLayer*, const FillLayer*);

  EFillBox LayersClipMax() const {
    ComputeCachedPropertiesIfNeeded();
    return static_cast<EFillBox>(layers_clip_max_);
  }
  bool AnyLayerUsesContentBox() const {
    ComputeCachedPropertiesIfNeeded();
    return any_layer_uses_content_box_;
  }
  bool AnyLayerHasImage() const {
    ComputeCachedPropertiesIfNeeded();
    return any_layer_has_image_;
  }
  bool AnyLayerHasUrlImage() const {
    ComputeCachedPropertiesIfNeeded();
    return any_layer_has_url_image_;
  }
  bool AnyLayerHasLocalAttachmentImage() const {
    ComputeCachedPropertiesIfNeeded();
    return any_layer_has_local_attachment_image_;
  }
  bool AnyLayerHasFixedAttachmentImage() const {
    ComputeCachedPropertiesIfNeeded();
    return any_layer_has_fixed_attachment_image_;
  }
  bool AnyLayerHasDefaultAttachmentImage() const {
    ComputeCachedPropertiesIfNeeded();
    return any_layer_has_default_attachment_image_;
  }

  static EFillAttachment InitialFillAttachment(EFillLayerType) {
    return EFillAttachment::kScroll;
  }
  static EFillBox InitialFillClip(EFillLayerType) { return EFillBox::kBorder; }
  static EFillBox InitialFillOrigin(EFillLayerType type) {
    return type == EFillLayerType::kBackground ? EFillBox::kPadding
                                               : EFillBox::kBorder;
  }
  static EFillRepeat InitialFillRepeatX(EFillLayerType) {
    return EFillRepeat::kRepeatFill;
  }
  static EFillRepeat InitialFillRepeatY(EFillLayerType) {
    return EFillRepeat::kRepeatFill;
  }
  static CompositeOperator InitialFillComposite(EFillLayerType) {
    return kCompositeSourceOver;
  }
  static BlendMode InitialFillBlendMode(EFillLayerType) {
    return BlendMode::kNormal;
  }
  static EFillSizeType InitialFillSizeType(EFillLayerType) {
    return EFillSizeType::kSizeLength;
  }
  static LengthSize InitialFillSizeLength(EFillLayerType) {
    return LengthSize();
  }
  static FillSize InitialFillSize(EFillLayerType type) {
    return FillSize(InitialFillSizeType(type), InitialFillSizeLength(type));
  }
  static Length InitialFillPositionX(EFillLayerType) {
    return Length::Percent(0.0);
  }
  static Length InitialFillPositionY(EFillLayerType) {
    return Length::Percent(0.0);
  }
  static StyleImage* InitialFillImage(EFillLayerType) { return nullptr; }

 private:
  friend class ComputedStyle;

  FillLayer() = default;

  bool ImageIsOpaque(const Document&, const ComputedStyle&) const;
  bool ImageTilesLayer() const;
  bool LayerPropertiesEqual(const FillLayer&) const;

  void ComputeCachedPropertiesIfNeeded() const {
    if (!cached_properties_computed_)
      ComputeCachedProperties();
  }
  void ComputeCachedProperties() const;

  FillLayer* next_;

  Persistent<StyleImage> image_;

  Length position_x_;
  Length position_y_;

  LengthSize size_length_;

  unsigned attachment_ : 2;           // EFillAttachment
  unsigned clip_ : 2;                 // EFillBox
  unsigned origin_ : 2;               // EFillBox
  unsigned repeat_x_ : 3;             // EFillRepeat
  unsigned repeat_y_ : 3;             // EFillRepeat
  unsigned composite_ : 4;            // CompositeOperator
  unsigned size_type_ : 2;            // EFillSizeType
  unsigned blend_mode_ : 5;           // BlendMode
  unsigned background_x_origin_ : 2;  // BackgroundEdgeOrigin
  unsigned background_y_origin_ : 2;  // BackgroundEdgeOrigin

  unsigned image_set_ : 1;
  unsigned attachment_set_ : 1;
  unsigned clip_set_ : 1;
  unsigned origin_set_ : 1;
  unsigned repeat_x_set_ : 1;
  unsigned repeat_y_set_ : 1;
  unsigned pos_x_set_ : 1;
  unsigned pos_y_set_ : 1;
  unsigned background_x_origin_set_ : 1;
  unsigned background_y_origin_set_ : 1;
  unsigned composite_set_ : 1;
  unsigned blend_mode_set_ : 1;

  unsigned type_ : 1;  // EFillLayerType

  // EFillBox, maximum clip_ value from this to bottom layer
  mutable unsigned layers_clip_max_ : 2;
  // True if any of this or subsequent layers has content-box clip or origin.
  mutable unsigned any_layer_uses_content_box_ : 1;
  // True if any of this or subsequent layers has image.
  mutable unsigned any_layer_has_image_ : 1;
  // True if any of this of subsequent layers has a url() image.
  mutable unsigned any_layer_has_url_image_ : 1;
  // True if any of this or subsequent layers has local attachment image.
  mutable unsigned any_layer_has_local_attachment_image_ : 1;
  // True if any of this or subsequent layers has fixed attachment image.
  mutable unsigned any_layer_has_fixed_attachment_image_ : 1;
  // True if any of this or subsequent layers has default attachment image.
  mutable unsigned any_layer_has_default_attachment_image_ : 1;
  // Set once any of the above is accessed. The layers will be frozen
  // thereafter.
  mutable unsigned cached_properties_computed_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FILL_LAYER_H_
