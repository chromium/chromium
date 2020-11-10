/*
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

#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

IntSize SVGImageForContainer::Size() const {
  // The image orientation is irrelevant because there is not concept of
  // orientation for SVG images.
  return RoundedIntSize(SizeAsFloat(kRespectImageOrientation));
}

FloatSize SVGImageForContainer::SizeAsFloat(RespectImageOrientationEnum) const {
  FloatSize scaled_container_size(container_size_);
  scaled_container_size.Scale(zoom_);
  return scaled_container_size;
}

void SVGImageForContainer::Draw(cc::PaintCanvas* canvas,
                                const cc::PaintFlags& flags,
                                const FloatRect& dst_rect,
                                const FloatRect& src_rect,
                                RespectImageOrientationEnum,
                                ImageClampingMode,
                                ImageDecodingMode) {
  image_->DrawForContainer(canvas, flags, container_size_, zoom_, dst_rect,
                           src_rect, url_);
}

void SVGImageForContainer::DrawPattern(GraphicsContext& context,
                                       const FloatRect& src_rect,
                                       const FloatSize& scale,
                                       const FloatPoint& phase,
                                       SkBlendMode op,
                                       const FloatRect& dst_rect,
                                       const FloatSize& repeat_spacing,
                                       RespectImageOrientationEnum) {
  image_->DrawPatternForContainer(context, container_size_, zoom_, src_rect,
                                  scale, phase, op, dst_rect, repeat_spacing,
                                  url_);
}

bool SVGImageForContainer::ApplyShader(cc::PaintFlags& flags,
                                       const SkMatrix& local_matrix) {
  return image_->ApplyShaderForContainer(container_size_, zoom_, url_, flags,
                                         local_matrix);
}

PaintImage SVGImageForContainer::PaintImageForCurrentFrame() {
  auto builder = CreatePaintImageBuilder();
  image_->PopulatePaintRecordForCurrentFrameForContainer(builder, Size(), zoom_,
                                                         url_);
  return builder.TakePaintImage();
}

}  // namespace blink
