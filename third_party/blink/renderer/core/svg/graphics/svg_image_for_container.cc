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

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace blink {

const SVGImageViewInfo* SVGImageForContainer::CreateViewInfo(
    SVGImage& image,
    const String& fragment) {
  return image.CreateViewInfo(fragment);
}

const SVGImageViewInfo* SVGImageForContainer::CreateViewInfo(SVGImage& image,
                                                             const KURL& url) {
  if (!url.HasFragmentIdentifier()) {
    return nullptr;
  }
  return CreateViewInfo(image, url.FragmentIdentifier().ToString());
}

const SVGImageViewInfo* SVGImageForContainer::CreateViewInfo(
    SVGImage& image,
    const Element& element) {
  KURL url = element.GetDocument().CompleteURL(element.ImageSourceURL());
  return CreateViewInfo(image, url);
}

const SVGImageViewInfo* SVGImageForContainer::CreateViewInfo(SVGImage& image,
                                                             const Node* node) {
  if (auto* element = DynamicTo<Element>(node)) {
    return CreateViewInfo(image, *element);
  }
  return nullptr;
}

bool SVGImageForContainer::GetNaturalDimensions(
    SVGImage& image,
    const SVGImageViewInfo* info,
    IntrinsicSizingInfo& sizing_info) {
  const SVGViewSpec* override_viewspec = info ? info->ViewSpec() : nullptr;
  return image.GetIntrinsicSizingInfo(override_viewspec, sizing_info);
}

gfx::SizeF SVGImageForContainer::ConcreteObjectSize(
    SVGImage& image,
    const SVGImageViewInfo* info,
    const gfx::SizeF& default_object_size) {
  IntrinsicSizingInfo sizing_info;
  if (!GetNaturalDimensions(image, info, sizing_info)) {
    return default_object_size;
  }
  return blink::ConcreteObjectSize(sizing_info, default_object_size);
}

gfx::Size SVGImageForContainer::SizeWithConfig(SizeConfig config) const {
  return gfx::ToRoundedSize(SizeWithConfigAsFloat(config));
}

gfx::SizeF SVGImageForContainer::SizeWithConfigAsFloat(SizeConfig) const {
  return gfx::ScaleSize(container_size_, zoom_);
}

SVGImageForContainer::SVGImageForContainer(SVGImage& image,
                                           const gfx::SizeF& container_size,
                                           float zoom,
                                           const SVGImageViewInfo* viewinfo)
    : image_(image),
      viewinfo_(viewinfo),
      container_size_(container_size),
      zoom_(zoom) {}

SVGImageForContainer::SVGImageForContainer(
    SVGImage& image,
    const gfx::SizeF& container_size,
    float zoom,
    const SVGImageViewInfo* viewinfo,
    mojom::blink::PreferredColorScheme preferred_color_scheme)
    : SVGImageForContainer(image, container_size, zoom, viewinfo) {
  image_.SetPreferredColorScheme(preferred_color_scheme);
}

bool SVGImageForContainer::HasIntrinsicSize() const {
  return image_.HasIntrinsicSize();
}

void SVGImageForContainer::Draw(cc::PaintCanvas* canvas,
                                const cc::PaintFlags& flags,
                                const gfx::RectF& dst_rect,
                                const gfx::RectF& src_rect,
                                const ImageDrawOptions& draw_options) {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, viewinfo_,
                                     draw_options.apply_dark_mode);
  image_.DrawForContainer(draw_info, canvas, flags, dst_rect, src_rect);
}

void SVGImageForContainer::DrawPattern(GraphicsContext& context,
                                       const cc::PaintFlags& flags,
                                       const gfx::RectF& dst_rect,
                                       const ImageTilingInfo& tiling_info,
                                       const ImageDrawOptions& draw_options) {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, viewinfo_,
                                     draw_options.apply_dark_mode);
  image_.DrawPatternForContainer(draw_info, context, flags, dst_rect,
                                 tiling_info);
}

bool SVGImageForContainer::ApplyShader(cc::PaintFlags& flags,
                                       const SkMatrix& local_matrix,
                                       const gfx::RectF& src_rect,
                                       const ImageDrawOptions& draw_options) {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, viewinfo_,
                                     draw_options.apply_dark_mode);
  return image_.ApplyShaderForContainer(draw_info, flags, src_rect,
                                        local_matrix);
}

PaintImage SVGImageForContainer::PaintImageForCurrentFrame() {
  const SVGImage::DrawInfo draw_info(container_size_, zoom_, viewinfo_, false);
  auto builder = CreatePaintImageBuilder();
  image_.PopulatePaintRecordForCurrentFrameForContainer(draw_info, builder);
  return builder.TakePaintImage();
}

}  // namespace blink
