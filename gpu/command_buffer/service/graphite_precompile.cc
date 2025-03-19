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

using ::skgpu::graphite::DepthStencilFlags;
using ::skgpu::graphite::DrawTypeFlags;
using ::skgpu::graphite::PaintOptions;
using ::skgpu::graphite::RenderPassProperties;

PaintOptions solid_srcover() {
  PaintOptions paintOptions;
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions solid_clear_src_srcover() {
  PaintOptions paintOptions;
  paintOptions.setBlendModes(
      {SkBlendMode::kClear, SkBlendMode::kSrc, SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_srcover() {
  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  PaintOptions paintOptions;
  paintOptions.setShaders(
      {skgpu::graphite::PrecompileShaders::Image({&ci, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_premul_src_srcover() {
  SkColorInfo ci{kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr};
  PaintOptions paintOptions;
  paintOptions.setShaders(
      {skgpu::graphite::PrecompileShaders::Image({&ci, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrc, SkBlendMode::kSrcOver});
  return paintOptions;
}

PaintOptions image_srgb_src() {
  SkColorInfo ci{
      kRGBA_8888_SkColorType, kPremul_SkAlphaType,
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB)};
  PaintOptions paintOptions;
  paintOptions.setShaders(
      {skgpu::graphite::PrecompileShaders::Image({&ci, 1})});
  paintOptions.setBlendModes({SkBlendMode::kSrc});
  return paintOptions;
}

PaintOptions blend_porter_duff_cf_srcover() {
  PaintOptions paintOptions;
  // kSrcOver will trigger the PorterDuffBlender
  paintOptions.setColorFilters({skgpu::graphite::PrecompileColorFilters::Blend(
      {SkBlendMode::kSrcOver})});
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

  const struct PrecompileSettings {
    PaintOptions fPaintOptions;
    DrawTypeFlags fDrawTypeFlags = DrawTypeFlags::kNone;
    RenderPassProperties fRenderPassProps;
  } kPrecompileCases[] = {
      {blend_porter_duff_cf_srcover(), DrawTypeFlags::kNonSimpleShape,
       kBGRA_1_D},
      {solid_srcover(), DrawTypeFlags::kNonSimpleShape, kR_4_DS},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Mask, kBGRA_1_D},
      {solid_srcover(), DrawTypeFlags::kBitmapText_Mask, kBGRA_4_DS},
      {solid_srcover(), DrawTypeFlags::kNonSimpleShape, kBGRA_4_D},
      {solid_srcover(), DrawTypeFlags::kNonSimpleShape, kBGRA_4_DS},
      {solid_srcover(), DrawTypeFlags::kCircularArc, kBGRA_4_DS},
      {solid_clear_src_srcover(), DrawTypeFlags::kSimpleShape, kBGRA_1_D},
      {solid_clear_src_srcover(), DrawTypeFlags::kSimpleShape, kBGRA_4_DS},
      {image_premul_src_srcover(), DrawTypeFlags::kSimpleShape, kBGRA_1_D},
      {image_premul_srcover(), DrawTypeFlags::kSimpleShape, kBGRA_4_DS},
      {image_srgb_src(), DrawTypeFlags::kSimpleShape, kBGRA_1_D_SRGB},
  };

  for (const auto& c : kPrecompileCases) {
    Precompile(precompileContext.get(), c.fPaintOptions, c.fDrawTypeFlags,
               {&c.fRenderPassProps, 1});
  }
}

}  // namespace gpu
