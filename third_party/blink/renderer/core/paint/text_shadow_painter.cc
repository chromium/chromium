// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_shadow_painter.h"

#include "base/containers/heap_array.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"

namespace blink {

namespace {

sk_sp<PaintFilter> MakeOneTextShadowFilter(
    const ShadowData& shadow,
    const Color& current_color,
    mojom::blink::ColorScheme color_scheme,
    DropShadowPaintFilter::ShadowMode shadow_mode) {
  const Color& color = shadow.GetColor().Resolve(current_color, color_scheme);
  // Detect when there's no effective shadow.
  if (color.IsFullyTransparent()) {
    return nullptr;
  }
  const gfx::Vector2dF& offset = shadow.Offset();

  const float blur = shadow.Blur();
  DCHECK_GE(blur, 0);
  const auto sigma = BlurRadiusToStdDev(blur);
  return sk_make_sp<DropShadowPaintFilter>(offset.x(), offset.y(), sigma, sigma,
                                           color.toSkColor4f(), shadow_mode,
                                           nullptr);
}

sk_sp<PaintFilter> MakeTextShadowFilter(const TextPaintStyle& text_style) {
  DCHECK(text_style.shadow);
  const auto& shadow_list = text_style.shadow->Shadows();
  if (shadow_list.size() == 1) {
    return MakeOneTextShadowFilter(
        shadow_list[0], text_style.current_color, text_style.color_scheme,
        DropShadowPaintFilter::ShadowMode::kDrawShadowOnly);
  }
  auto shadow_filters =
      base::HeapArray<sk_sp<PaintFilter>>::WithSize(shadow_list.size());
  wtf_size_t count = 0;
  for (const ShadowData& shadow : shadow_list) {
    if (sk_sp<PaintFilter> shadow_filter = MakeOneTextShadowFilter(
            shadow, text_style.current_color, text_style.color_scheme,
            DropShadowPaintFilter::ShadowMode::kDrawShadowOnly)) {
      shadow_filters[count++] = std::move(shadow_filter);
    }
  }
  if (count == 0) {
    return nullptr;
  }
  // Reverse to get the proper paint order (last shadow painted first).
  base::span<sk_sp<PaintFilter>> used_filters(shadow_filters.first(count));
  base::ranges::reverse(used_filters);
  return sk_make_sp<MergePaintFilter>(
      used_filters.data(), base::saturated_cast<int>(used_filters.size()));
}

}  // namespace

void ScopedTextShadowPainter::ApplyShadowList(
    GraphicsContext& context,
    const TextPaintStyle& text_style) {
  sk_sp<PaintFilter> shadow_filter = MakeTextShadowFilter(text_style);
  if (!shadow_filter) {
    return;
  }
  context_ = &context;
  context_->BeginLayer(std::move(shadow_filter));
}

}  // namespace blink
