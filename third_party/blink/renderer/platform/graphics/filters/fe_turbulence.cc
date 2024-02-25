/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Renata Hodovan <reni@inf.u-szeged.hu>
 * Copyright (C) 2011 Gabor Loki <loki@webkit.org>
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_turbulence.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

FETurbulence::FETurbulence(Filter* filter,
                           TurbulenceType type,
                           float base_frequency_x,
                           float base_frequency_y,
                           int num_octaves,
                           float seed,
                           bool stitch_tiles)
    : FilterEffect(filter),
      type_(type),
      base_frequency_x_(base_frequency_x),
      base_frequency_y_(base_frequency_y),
      num_octaves_(num_octaves),
      seed_(seed),
      stitch_tiles_(stitch_tiles) {}

TurbulenceType FETurbulence::GetType() const {
  return type_;
}

bool FETurbulence::SetType(TurbulenceType type) {
  if (type_ == type)
    return false;
  type_ = type;
  return true;
}

float FETurbulence::BaseFrequencyY() const {
  return base_frequency_y_;
}

bool FETurbulence::SetBaseFrequencyY(float base_frequency_y) {
  if (base_frequency_y_ == base_frequency_y)
    return false;
  base_frequency_y_ = base_frequency_y;
  return true;
}

float FETurbulence::BaseFrequencyX() const {
  return base_frequency_x_;
}

bool FETurbulence::SetBaseFrequencyX(float base_frequency_x) {
  if (base_frequency_x_ == base_frequency_x)
    return false;
  base_frequency_x_ = base_frequency_x;
  return true;
}

float FETurbulence::Seed() const {
  return seed_;
}

bool FETurbulence::SetSeed(float seed) {
  if (seed_ == seed)
    return false;
  seed_ = seed;
  return true;
}

int FETurbulence::NumOctaves() const {
  return num_octaves_;
}

bool FETurbulence::SetNumOctaves(int num_octaves) {
  if (num_octaves_ == num_octaves)
    return false;
  num_octaves_ = num_octaves;
  return true;
}

bool FETurbulence::StitchTiles() const {
  return stitch_tiles_;
}

bool FETurbulence::SetStitchTiles(bool stitch) {
  if (stitch_tiles_ == stitch)
    return false;
  stitch_tiles_ = stitch;
  return true;
}

sk_sp<PaintFilter> FETurbulence::CreateImageFilter() {
  float base_frequency_x = base_frequency_x_;
  float base_frequency_y = base_frequency_y_;
  if (base_frequency_x < 0 || base_frequency_y < 0) {
    // Negative values are unsupported which means it should be treated as
    // if they hadn't been specified. So, it implies "0 0"(the initial
    // value).
    base_frequency_x = base_frequency_y = 0;
  }

  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  TurbulencePaintFilter::TurbulenceType type =
      GetType() == FETURBULENCE_TYPE_FRACTALNOISE
          ? TurbulencePaintFilter::TurbulenceType::kFractalNoise
          : TurbulencePaintFilter::TurbulenceType::kTurbulence;
  const SkISize size = SkISize::Make(FilterPrimitiveSubregion().width(),
                                     FilterPrimitiveSubregion().height());
  // Frequency should be scaled by page zoom, but not by primitiveUnits.
  // So we apply only the transform scale (as Filter::apply*Scale() do)
  // and not the target bounding box scale (as SVGFilter::apply*Scale()
  // would do). Note also that we divide by the scale since this is
  // a frequency, not a period.
  base_frequency_x /= GetFilter()->Scale();
  base_frequency_y /= GetFilter()->Scale();

  // Cap the number of octaves to the maximum detectable when rendered with
  // 8 bits per pixel, plus one for higher bit depth.
  int capped_num_octaves = std::min(NumOctaves(), 9);
  return sk_make_sp<TurbulencePaintFilter>(
      type, SkFloatToScalar(base_frequency_x),
      SkFloatToScalar(base_frequency_y), capped_num_octaves,
      SkFloatToScalar(Seed()), StitchTiles() ? &size : nullptr,
      base::OptionalToPtr(crop_rect));
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const TurbulenceType& type) {
  switch (type) {
    case FETURBULENCE_TYPE_UNKNOWN:
      ts << "UNKNOWN";
      break;
    case FETURBULENCE_TYPE_TURBULENCE:
      ts << "TURBULENCE";
      break;
    case FETURBULENCE_TYPE_FRACTALNOISE:
      ts << "NOISE";
      break;
  }
  return ts;
}

WTF::TextStream& FETurbulence::ExternalRepresentation(WTF::TextStream& ts,
                                                      int indent) const {
  WriteIndent(ts, indent);
  ts << "[feTurbulence";
  FilterEffect::ExternalRepresentation(ts);
  ts << " type=\"" << GetType() << "\" "
     << "baseFrequency=\"" << BaseFrequencyX() << ", " << BaseFrequencyY()
     << "\" "
     << "seed=\"" << Seed() << "\" "
     << "numOctaves=\"" << NumOctaves() << "\" "
     << "stitchTiles=\"" << StitchTiles() << "\"]\n";
  return ts;
}

}  // namespace blink
