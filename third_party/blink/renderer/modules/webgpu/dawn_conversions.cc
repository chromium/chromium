// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_data_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_doublesequence_gpucolordict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuautolayoutmode_gpupipelinelayout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuextent3ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuorigin3ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

namespace blink {

WGPUColor AsDawnColor(const Vector<double>& webgpu_color) {
  DCHECK_EQ(webgpu_color.size(), 4UL);

  WGPUColor dawn_color = {};
  dawn_color.r = webgpu_color[0];
  dawn_color.g = webgpu_color[1];
  dawn_color.b = webgpu_color[2];
  dawn_color.a = webgpu_color[3];

  return dawn_color;
}

WGPUColor AsDawnType(const GPUColorDict* webgpu_color) {
  DCHECK(webgpu_color);

  WGPUColor dawn_color = {};
  dawn_color.r = webgpu_color->r();
  dawn_color.g = webgpu_color->g();
  dawn_color.b = webgpu_color->b();
  dawn_color.a = webgpu_color->a();

  return dawn_color;
}

WGPUColor AsDawnType(const V8GPUColor* webgpu_color) {
  DCHECK(webgpu_color);

  switch (webgpu_color->GetContentType()) {
    case V8GPUColor::ContentType::kDoubleSequence:
      return AsDawnColor(webgpu_color->GetAsDoubleSequence());
    case V8GPUColor::ContentType::kGPUColorDict:
      return AsDawnType(webgpu_color->GetAsGPUColorDict());
  }

  NOTREACHED();
  return WGPUColor{};
}

WGPUExtent3D AsDawnType(const V8GPUExtent3D* webgpu_extent) {
  DCHECK(webgpu_extent);

  // Set all extents to their default value of 1.
  WGPUExtent3D dawn_extent = {1, 1, 1};

  switch (webgpu_extent->GetContentType()) {
    case V8GPUExtent3D::ContentType::kGPUExtent3DDict: {
      const GPUExtent3DDict* webgpu_extent_3d_dict =
          webgpu_extent->GetAsGPUExtent3DDict();
      dawn_extent.width = webgpu_extent_3d_dict->width();
      dawn_extent.height = webgpu_extent_3d_dict->height();
      dawn_extent.depthOrArrayLayers =
          webgpu_extent_3d_dict->depthOrArrayLayers();
      break;
    }
    case V8GPUExtent3D::ContentType::kUnsignedLongEnforceRangeSequence: {
      const Vector<uint32_t>& webgpu_extent_sequence =
          webgpu_extent->GetAsUnsignedLongEnforceRangeSequence();

      // The WebGPU spec states that if the sequence isn't big enough then the
      // default values of 1 are used (which are set above).
      switch (webgpu_extent_sequence.size()) {
        default:
          dawn_extent.depthOrArrayLayers = webgpu_extent_sequence[2];
          [[fallthrough]];
        case 2:
          dawn_extent.height = webgpu_extent_sequence[1];
          [[fallthrough]];
        case 1:
          dawn_extent.width = webgpu_extent_sequence[0];
          [[fallthrough]];
        case 0:
          break;
      }
      break;
    }
  }

  return dawn_extent;
}

WGPUOrigin3D AsDawnType(const V8GPUOrigin3D* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {0, 0, 0};

  switch (webgpu_origin->GetContentType()) {
    case V8GPUOrigin3D::ContentType::kGPUOrigin3DDict: {
      const GPUOrigin3DDict* webgpu_origin_3d_dict =
          webgpu_origin->GetAsGPUOrigin3DDict();
      dawn_origin.x = webgpu_origin_3d_dict->x();
      dawn_origin.y = webgpu_origin_3d_dict->y();
      dawn_origin.z = webgpu_origin_3d_dict->z();
      break;
    }
    case V8GPUOrigin3D::ContentType::kUnsignedLongEnforceRangeSequence: {
      const Vector<uint32_t>& webgpu_origin_sequence =
          webgpu_origin->GetAsUnsignedLongEnforceRangeSequence();

      // The WebGPU spec states that if the sequence isn't big enough then the
      // default values of 0 are used (which are set above).
      switch (webgpu_origin_sequence.size()) {
        default:
          dawn_origin.z = webgpu_origin_sequence[2];
          [[fallthrough]];
        case 2:
          dawn_origin.y = webgpu_origin_sequence[1];
          [[fallthrough]];
        case 1:
          dawn_origin.x = webgpu_origin_sequence[0];
          [[fallthrough]];
        case 0:
          break;
      }
      break;
    }
  }

  return dawn_origin;
}

WGPUImageCopyTexture AsDawnType(const GPUImageCopyTexture* webgpu_view) {
  DCHECK(webgpu_view);
  DCHECK(webgpu_view->texture());

  WGPUImageCopyTexture dawn_view = {};
  dawn_view.texture = webgpu_view->texture()->GetHandle();
  dawn_view.mipLevel = webgpu_view->mipLevel();
  dawn_view.origin = AsDawnType(webgpu_view->origin());
  dawn_view.aspect = AsDawnEnum(webgpu_view->aspect());

  return dawn_view;
}

// Dawn represents `undefined` as the special uint32_t value
// WGPU_STRIDE_UNDEFINED (0xFFFF'FFFF). Blink must make sure that an actual
// value of 0xFFFF'FFFF coming in from JS is not treated as
// WGPU_STRIDE_UNDEFINED, so it injects an error in that case.
const char* ValidateTextureDataLayout(const GPUImageDataLayout* webgpu_layout,
                                      WGPUTextureDataLayout* dawn_layout) {
  DCHECK(webgpu_layout);

  uint32_t bytesPerRow = 0;
  if (webgpu_layout->hasBytesPerRow()) {
    bytesPerRow = webgpu_layout->bytesPerRow();
    if (bytesPerRow == WGPU_STRIDE_UNDEFINED) {
      return "bytesPerRow must be a multiple of 256";
    }
  } else {
    bytesPerRow = WGPU_STRIDE_UNDEFINED;
  }

  uint32_t rowsPerImage = 0;
  if (webgpu_layout->hasRowsPerImage()) {
    rowsPerImage = webgpu_layout->rowsPerImage();
    if (rowsPerImage == WGPU_STRIDE_UNDEFINED) {
      return "rowsPerImage is too large";
    }
  } else {
    rowsPerImage = WGPU_STRIDE_UNDEFINED;
  }

  *dawn_layout = {};
  dawn_layout->offset = webgpu_layout->offset();
  dawn_layout->bytesPerRow = bytesPerRow;
  dawn_layout->rowsPerImage = rowsPerImage;

  return nullptr;
}

WGPUTextureFormat AsDawnType(SkColorType color_type) {
  switch (color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      return WGPUTextureFormat_RGBA8Unorm;
    case SkColorType::kBGRA_8888_SkColorType:
      return WGPUTextureFormat_BGRA8Unorm;
    case SkColorType::kRGBA_1010102_SkColorType:
      return WGPUTextureFormat_RGB10A2Unorm;
    case SkColorType::kRGBA_F16_SkColorType:
      return WGPUTextureFormat_RGBA16Float;
    case SkColorType::kRGBA_F32_SkColorType:
      return WGPUTextureFormat_RGBA32Float;
    case SkColorType::kR8G8_unorm_SkColorType:
      return WGPUTextureFormat_RG8Unorm;
    case SkColorType::kR16G16_float_SkColorType:
      return WGPUTextureFormat_RG16Float;
    default:
      return WGPUTextureFormat_Undefined;
  }
}

WGPUPipelineLayout AsDawnType(
    V8UnionGPUAutoLayoutModeOrGPUPipelineLayout* webgpu_layout) {
  DCHECK(webgpu_layout);

  switch (webgpu_layout->GetContentType()) {
    case V8UnionGPUAutoLayoutModeOrGPUPipelineLayout::ContentType::
        kGPUPipelineLayout:
      return AsDawnType(webgpu_layout->GetAsGPUPipelineLayout());
    case V8UnionGPUAutoLayoutModeOrGPUPipelineLayout::ContentType::
        kGPUAutoLayoutMode:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace blink
