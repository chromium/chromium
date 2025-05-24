// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * This file is, almost entirely, copy and pasted from Skia's
 * ChromePrecompileTest.cpp and then massaged to better match Chrome's coding
 * standard. Almost any change to this file should first be performed in the
 * Skia file and then copied here.
 */

#include "gpu/command_buffer/service/graphite_precompile.h"

#include "third_party/skia/include/gpu/graphite/PrecompileContext.h"
#include "third_party/skia/include/gpu/graphite/precompile/PaintOptions.h"
#include "third_party/skia/include/gpu/graphite/precompile/Precompile.h"
#include "third_party/skia/include/gpu/graphite/precompile/PrecompileColorFilter.h"
#include "third_party/skia/include/gpu/graphite/precompile/PrecompileShader.h"

namespace gpu {
namespace {

using ::skgpu::graphite::PrecompileShaders::GradientShaderFlags;
using ::skgpu::graphite::PrecompileShaders::ImageShaderFlags;
using ::skgpu::graphite::PrecompileShaders::YUVImageShaderFlags;

using ::skgpu::graphite::DepthStencilFlags;
using ::skgpu::graphite::DrawTypeFlags;
using ::skgpu::graphite::PaintOptions;
using ::skgpu::graphite::RenderPassProperties;

PaintOptions solid_srcover() {
  PaintOptions paintOptions;
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions linear_grad_sm_srcover() {
  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::LinearGradient(
      GradientShaderFlags::kSmall)});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions linear_grad_SRGB_sm_med_srcover() {
  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::LinearGradient(
      GradientShaderFlags::kNoLarge,
      {SkGradientShader::Interpolation::InPremul::kNo,
       SkGradientShader::Interpolation::ColorSpace::kSRGB,
       SkGradientShader::Interpolation::HueMethod::kShorter})});

  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  paintOptions.setDither(true);

  return paintOptions;
}

PaintOptions xparent_paint_image_premul_hw_and_clamp_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  SkTileMode tm = SkTileMode::kClamp;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {&tm, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  paintOptions.setPaintColorIsOpaque(false);
  return paintOptions;
}

PaintOptions xparent_paint_image_premul_hw_only_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  paintOptions.setPaintColorIsOpaque(false);
  return paintOptions;
}

PaintOptions xparent_paint_srcover() {
  PaintOptions paintOptions;

  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  paintOptions.setPaintColorIsOpaque(false);
  return paintOptions;
}

PaintOptions solid_clear_src_srcover() {
  PaintOptions paintOptions;
  paintOptions.setBlendModes(
      {SkBlendMode::kClear, SkBlendMode::kSrc, SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions solid_src_srcover() {
  PaintOptions paintOptions;
  paintOptions.setBlendModes({SkBlendMode::kSrc, SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_no_cubic_srcover() {
  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  SkTileMode tm = SkTileMode::kClamp;
  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {&tm, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_hw_only_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_clamp_no_cubic_dstin() {
  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  SkTileMode tm = SkTileMode::kClamp;
  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {&tm, 1})});
  paintOptions.setBlendModes({SkBlendMode::kDstIn});
  return paintOptions;
}

PaintOptions image_premul_hw_only_dstin() {
  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setBlendModes({SkBlendMode::kDstIn});
  return paintOptions;
}

PaintOptions yuv_image_srgb_no_cubic_srcover() {
  SkColorInfo ci{
      kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB)};

  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::YUVImage(
      YUVImageShaderFlags::kExcludeCubic, {&ci, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions yuv_image_srgb_srcover2() {
  SkColorInfo ci{
      kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB)};

  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::YUVImage(
      YUVImageShaderFlags::kNoCubicNoNonSwizzledHW, {&ci, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_no_cubic_src_srcover() {
  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  PaintOptions paintOptions;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setBlendModes({SkBlendMode::kSrc, SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_srgb_no_cubic_src() {
  PaintOptions paintOptions;

  SkColorInfo ci{
      kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB)};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setBlendModes({SkBlendMode::kSrc});
  return paintOptions;
}

PaintOptions blend_porter_duff_cf_srcover() {
  PaintOptions paintOptions;
  // kSrcOver will trigger the PorterDuffBlender
  paintOptions.setColorFilters(
      {::skgpu::graphite::PrecompileColorFilters::Blend(
          {SkBlendMode::kSrcOver})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});

  return paintOptions;
}

PaintOptions image_alpha_hw_only_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{kAlpha_8_SkColorType, kUnpremul_SkAlphaType, nullptr};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_alpha_no_cubic_src() {
  PaintOptions paintOptions;

  SkColorInfo ci{kAlpha_8_SkColorType, kUnpremul_SkAlphaType, nullptr};
  SkTileMode tm = SkTileMode::kRepeat;
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {&tm, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrc});
  return paintOptions;
}

PaintOptions image_premul_hw_only_porter_duff_cf_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setColorFilters(
      {::skgpu::graphite::PrecompileColorFilters::Blend(
          {SkBlendMode::kSrcOver})});

  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_hw_only_matrix_cf_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});
  paintOptions.setColorFilters(
      {::skgpu::graphite::PrecompileColorFilters::Matrix()});

  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_hw_only_srgb_srcover() {
  PaintOptions paintOptions;

  SkColorInfo ci{
      kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB)};
  paintOptions.setShaders({::skgpu::graphite::PrecompileShaders::Image(
      ImageShaderFlags::kExcludeCubic, {&ci, 1}, {})});

  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

}  // anonymous namespace

void GraphitePerformPrecompilation(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompileContext) {
  // Single sampled R w/ just depth
  const RenderPassProperties kR_1_D{DepthStencilFlags::kDepth,
                                    kAlpha_8_SkColorType,
                                    /* fDstCS= */ nullptr,
                                    /* fRequiresMSAA= */ false};

  // MSAA R w/ depth and stencil
  const RenderPassProperties kR_4_DS{DepthStencilFlags::kDepthStencil,
                                     kAlpha_8_SkColorType,
                                     /* fDstCS= */ nullptr,
                                     /* fRequiresMSAA= */ true};

  // Single sampled BGRA w/ just depth
  const RenderPassProperties kBGRA_1_D{DepthStencilFlags::kDepth,
                                       kBGRA_8888_SkColorType,
                                       /* fDstCS= */ nullptr,
                                       /* fRequiresMSAA= */ false};

  // MSAA BGRA w/ just depth
  const RenderPassProperties kBGRA_4_D{DepthStencilFlags::kDepth,
                                       kBGRA_8888_SkColorType,
                                       /* fDstCS= */ nullptr,
                                       /* fRequiresMSAA= */ true};

  // MSAA BGRA w/ depth and stencil
  const RenderPassProperties kBGRA_4_DS{DepthStencilFlags::kDepthStencil,
                                        kBGRA_8888_SkColorType,
                                        /* fDstCS= */ nullptr,
                                        /* fRequiresMSAA= */ true};

  // The same as kBGRA_1_D but w/ an SRGB colorSpace
  const RenderPassProperties kBGRA_1_D_SRGB{DepthStencilFlags::kDepth,
                                            kBGRA_8888_SkColorType,
                                            SkColorSpace::MakeSRGB(),
                                            /* fRequiresMSAA= */ false};

  // The same as kBGRA_1_D but w/ an Adobe RGB colorSpace
  const RenderPassProperties kBGRA_1_D_Adobe{
      DepthStencilFlags::kDepth, kBGRA_8888_SkColorType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB),
      /* fRequiresMSAA= */ false};

  // The same as kBGRA_4_DS but w/ an SRGB colorSpace
  const RenderPassProperties kBGRA_4_DS_SRGB{DepthStencilFlags::kDepthStencil,
                                             kBGRA_8888_SkColorType,
                                             SkColorSpace::MakeSRGB(),
                                             /* fRequiresMSAA= */ true};

  // The same as kBGRA_4_DS but w/ an Adobe RGB colorSpace
  const RenderPassProperties kBGRA_4_DS_Adobe{
      DepthStencilFlags::kDepthStencil, kBGRA_8888_SkColorType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB),
      /* fRequiresMSAA= */ true};

  constexpr DrawTypeFlags kRRectAndNonAARect = static_cast<DrawTypeFlags>(
      DrawTypeFlags::kAnalyticRRect | DrawTypeFlags::kNonAAFillRect);
  constexpr DrawTypeFlags kQuadAndNonAARect = static_cast<DrawTypeFlags>(
      DrawTypeFlags::kPerEdgeAAQuad | DrawTypeFlags::kNonAAFillRect);

  const struct PrecompileSettings {
    PaintOptions fPaintOptions;
    DrawTypeFlags fDrawTypeFlags = DrawTypeFlags::kNone;
    RenderPassProperties fRenderPassProps;
  } kPrecompileCases[] = {
      {blend_porter_duff_cf_srcover(), DrawTypeFlags::kBitmapText_Mask,
       kBGRA_1_D},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Mask, kBGRA_1_D},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Mask, kBGRA_4_D},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Mask, kBGRA_4_DS},
      {linear_grad_sm_srcover(), DrawTypeFlags::kBitmapText_Mask, kBGRA_4_DS},
      {blend_porter_duff_cf_srcover(), DrawTypeFlags::kBitmapText_Mask,
       kBGRA_4_DS},
      {xparent_paint_srcover(), DrawTypeFlags::kBitmapText_Color, kBGRA_1_D},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Color, kBGRA_1_D_Adobe},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Color, kBGRA_4_DS_Adobe},
      {solid_srcover(), kRRectAndNonAARect, kR_1_D},
      {image_alpha_hw_only_srcover(), DrawTypeFlags::kPerEdgeAAQuad, kR_1_D},
      {image_alpha_no_cubic_src(), DrawTypeFlags::kNonAAFillRect, kR_1_D},
      {image_premul_clamp_no_cubic_dstin(), kQuadAndNonAARect, kBGRA_1_D},
      {image_premul_hw_only_matrix_cf_srcover(), DrawTypeFlags::kNonAAFillRect,
       kBGRA_1_D},
      {image_premul_hw_only_porter_duff_cf_srcover(),
       DrawTypeFlags::kPerEdgeAAQuad, kBGRA_1_D},
      {image_premul_no_cubic_srcover(), DrawTypeFlags::kAnalyticRRect,
       kBGRA_1_D},
      {image_premul_no_cubic_src_srcover(), kQuadAndNonAARect, kBGRA_1_D},
      {linear_grad_sm_srcover(), DrawTypeFlags::kNonAAFillRect, kBGRA_1_D},
      {solid_src_srcover(), DrawTypeFlags::kSimpleShape, kBGRA_1_D},
      {xparent_paint_image_premul_hw_and_clamp_srcover(), kQuadAndNonAARect,
       kBGRA_1_D},
      {linear_grad_SRGB_sm_med_srcover(), kRRectAndNonAARect, kBGRA_1_D_Adobe},
      {image_hw_only_srgb_srcover(), kRRectAndNonAARect, kBGRA_1_D_SRGB},
      {image_srgb_no_cubic_src(), kQuadAndNonAARect, kBGRA_1_D_SRGB},
      {yuv_image_srgb_no_cubic_srcover(), DrawTypeFlags::kSimpleShape,
       kBGRA_1_D_SRGB},
      {image_premul_hw_only_dstin(), DrawTypeFlags::kPerEdgeAAQuad, kBGRA_4_D},
      {image_premul_hw_only_srcover(), kQuadAndNonAARect, kBGRA_4_D},
      {solid_src_srcover(), kRRectAndNonAARect, kBGRA_4_D},
      {blend_porter_duff_cf_srcover(), DrawTypeFlags::kNonAAFillRect,
       kBGRA_4_DS},
      {image_premul_hw_only_dstin(), DrawTypeFlags::kPerEdgeAAQuad, kBGRA_4_DS},
      {image_premul_hw_only_matrix_cf_srcover(), DrawTypeFlags::kNonAAFillRect,
       kBGRA_4_DS},
      {image_premul_no_cubic_srcover(), kQuadAndNonAARect, kBGRA_4_DS},
      {solid_clear_src_srcover(), DrawTypeFlags::kNonAAFillRect, kBGRA_4_DS},
      {solid_srcover(), DrawTypeFlags::kNonSimpleShape, kBGRA_4_DS},
      {solid_srcover(), DrawTypeFlags::kAnalyticRRect, kBGRA_4_DS},
      {xparent_paint_image_premul_hw_only_srcover(),
       DrawTypeFlags::kPerEdgeAAQuad, kBGRA_4_DS},
      {linear_grad_SRGB_sm_med_srcover(), kRRectAndNonAARect, kBGRA_4_DS_Adobe},
      {image_hw_only_srgb_srcover(), DrawTypeFlags::kAnalyticRRect,
       kBGRA_4_DS_SRGB},
      {yuv_image_srgb_srcover2(), DrawTypeFlags::kSimpleShape, kBGRA_4_DS_SRGB},
  };

  for (const auto& c : kPrecompileCases) {
    Precompile(precompileContext.get(), c.fPaintOptions, c.fDrawTypeFlags,
               {&c.fRenderPassProps, 1});
  }
}

}  // namespace gpu
