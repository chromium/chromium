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

namespace blink {

IntSize SVGImageForContainer::Size() const {
  // The image orientation is irrelevant because there is not concept of
  // orientation for SVG images.
  return RoundedIntSize(SizeAsFloat(kRespectImageOrientation));
}

FloatSize SVGImageForContainer::SizeAsFloat(RespectImageOrientationEnum) const {
  return container_size_.ScaledBy(zoom_);
}

void SVGImageForContainer::Draw(cc::PaintCanvas* canvas,
                                const cc::PaintFlags& flags,
                                const FloatRect& dst_rect,
                                const FloatRect& src_rect,
                                RespectImageOrientationEnum,
                                ImageClampingMode,
                                ImageDecodingMode) {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, url_);
  image_->DrawForContainer(draw_info, canvas, flags, dst_rect, src_rect);
}

void SVGImageForContainer::DrawPattern(GraphicsContext& context,
                                       const FloatRect& src_rect,
                                       const FloatSize& scale,
                                       const FloatPoint& phase,
                                       SkBlendMode op,
                                       const FloatRect& dst_rect,
                                       const FloatSize& repeat_spacing,
                                       RespectImageOrientationEnum) {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, url_);
  image_->DrawPatternForContainer(draw_info, context, src_rect, scale, phase,
                                  op, dst_rect, repeat_spacing);
}

bool SVGImageForContainer::ApplyShader(cc::PaintFlags& flags,
                                       const SkMatrix& local_matrix) {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, url_);
  return image_->ApplyShaderForContainer(draw_info, flags, local_matrix);
}

PaintImage SVGImageForContainer::PaintImageForCurrentFrame() {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, url_);
  auto builder = CreatePaintImageBuilder();
  image_->PopulatePaintRecordForCurrentFrameForContainer(draw_info, builder);
  return builder.TakePaintImage();
}

}  // namespace blink
