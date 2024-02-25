/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_blend.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

FEBlend::FEBlend(Filter* filter, BlendMode mode)
    : FilterEffect(filter), mode_(mode) {}

bool FEBlend::SetBlendMode(BlendMode mode) {
  if (mode_ == mode)
    return false;
  mode_ = mode;
  return true;
}

sk_sp<PaintFilter> FEBlend::CreateImageFilter() {
  sk_sp<PaintFilter> foreground(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  sk_sp<PaintFilter> background(paint_filter_builder::Build(
      InputEffect(1), OperatingInterpolationSpace()));
  SkBlendMode mode =
      WebCoreCompositeToSkiaComposite(kCompositeSourceOver, mode_);
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  return sk_make_sp<XfermodePaintFilter>(mode, std::move(background),
                                         std::move(foreground),
                                         base::OptionalToPtr(crop_rect));
}

WTF::TextStream& FEBlend::ExternalRepresentation(WTF::TextStream& ts,
                                                 int indent) const {
  WriteIndent(ts, indent);
  ts << "[feBlend";
  FilterEffect::ExternalRepresentation(ts);
  ts << " mode=\"" << BlendModeToString(mode_) << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  InputEffect(1)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
