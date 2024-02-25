/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/platform/graphics/filters/fe_displacement_map.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

FEDisplacementMap::FEDisplacementMap(Filter* filter,
                                     ChannelSelectorType x_channel_selector,
                                     ChannelSelectorType y_channel_selector,
                                     float scale)
    : FilterEffect(filter),
      x_channel_selector_(x_channel_selector),
      y_channel_selector_(y_channel_selector),
      scale_(scale) {}

gfx::RectF FEDisplacementMap::MapEffect(const gfx::RectF& rect) const {
  gfx::RectF result = rect;
  result.Outset(gfx::OutsetsF::VH(
      GetFilter()->ApplyVerticalScale(std::abs(scale_) / 2),
      GetFilter()->ApplyHorizontalScale(std::abs(scale_) / 2)));
  return result;
}

gfx::RectF FEDisplacementMap::MapInputs(const gfx::RectF& rect) const {
  return InputEffect(0)->MapRect(rect);
}

ChannelSelectorType FEDisplacementMap::XChannelSelector() const {
  return x_channel_selector_;
}

bool FEDisplacementMap::SetXChannelSelector(
    const ChannelSelectorType x_channel_selector) {
  if (x_channel_selector_ == x_channel_selector)
    return false;
  x_channel_selector_ = x_channel_selector;
  return true;
}

ChannelSelectorType FEDisplacementMap::YChannelSelector() const {
  return y_channel_selector_;
}

bool FEDisplacementMap::SetYChannelSelector(
    const ChannelSelectorType y_channel_selector) {
  if (y_channel_selector_ == y_channel_selector)
    return false;
  y_channel_selector_ = y_channel_selector;
  return true;
}

float FEDisplacementMap::Scale() const {
  return scale_;
}

bool FEDisplacementMap::SetScale(float scale) {
  if (scale_ == scale)
    return false;
  scale_ = scale;
  return true;
}

static SkColorChannel ToSkiaMode(ChannelSelectorType type) {
  switch (type) {
    case CHANNEL_R:
      return SkColorChannel::kR;
    case CHANNEL_G:
      return SkColorChannel::kG;
    case CHANNEL_B:
      return SkColorChannel::kB;
    case CHANNEL_A:
      return SkColorChannel::kA;
    case CHANNEL_UNKNOWN:
    default:
      // Historically, Skia's raster backend treated unknown as blue.
      return SkColorChannel::kB;
  }
}

sk_sp<PaintFilter> FEDisplacementMap::CreateImageFilter() {
  sk_sp<PaintFilter> color = paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace());
  // FEDisplacementMap must be a pass-through filter if
  // the origin is tainted. See:
  // https://drafts.fxtf.org/filter-effects/#fedisplacemnentmap-restrictions.
  if (InputEffect(1)->OriginTainted())
    return color;

  sk_sp<PaintFilter> displ = paint_filter_builder::Build(
      InputEffect(1), OperatingInterpolationSpace());
  SkColorChannel type_x = ToSkiaMode(x_channel_selector_);
  SkColorChannel type_y = ToSkiaMode(y_channel_selector_);
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  // FIXME : Only applyHorizontalScale is used and applyVerticalScale is ignored
  // This can be fixed by adding a 2nd scale parameter to
  // DisplacementMapEffectPaintFilter.
  return sk_make_sp<DisplacementMapEffectPaintFilter>(
      type_x, type_y,
      SkFloatToScalar(GetFilter()->ApplyHorizontalScale(scale_)),
      std::move(displ), std::move(color), base::OptionalToPtr(crop_rect));
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const ChannelSelectorType& type) {
  switch (type) {
    case CHANNEL_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case CHANNEL_R:
      ts << "RED";
      break;
    case CHANNEL_G:
      ts << "GREEN";
      break;
    case CHANNEL_B:
      ts << "BLUE";
      break;
    case CHANNEL_A:
      ts << "ALPHA";
      break;
  }
  return ts;
}

WTF::TextStream& FEDisplacementMap::ExternalRepresentation(WTF::TextStream& ts,
                                                           int indent) const {
  WriteIndent(ts, indent);
  ts << "[feDisplacementMap";
  FilterEffect::ExternalRepresentation(ts);
  ts << " scale=\"" << scale_ << "\" "
     << "xChannelSelector=\"" << x_channel_selector_ << "\" "
     << "yChannelSelector=\"" << y_channel_selector_ << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  InputEffect(1)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
