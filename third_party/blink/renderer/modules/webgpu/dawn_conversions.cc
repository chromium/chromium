// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/bindings/modules/v8/double_sequence_or_gpu_color_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_data_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
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

WGPUColor AsDawnType(const DoubleSequenceOrGPUColorDict* webgpu_color) {
  DCHECK(webgpu_color);

  if (webgpu_color->IsDoubleSequence()) {
    return AsDawnColor(webgpu_color->GetAsDoubleSequence());
  } else if (webgpu_color->IsGPUColorDict()) {
    return AsDawnType(webgpu_color->GetAsGPUColorDict());
  }
  NOTREACHED();
  WGPUColor dawn_color = {};
  return dawn_color;
}

WGPUExtent3D AsDawnType(
    const UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict* webgpu_extent,
    GPUDevice* device) {
  DCHECK(webgpu_extent);

  WGPUExtent3D dawn_extent = {1, 1, 1, 1};

  if (webgpu_extent->IsUnsignedLongEnforceRangeSequence()) {
    const Vector<uint32_t>& webgpu_extent_sequence =
        webgpu_extent->GetAsUnsignedLongEnforceRangeSequence();

    // The WebGPU spec states that if the sequence isn't big enough then the
    // default values of 1 are used (which are set above).
    switch (webgpu_extent_sequence.size()) {
      default:
        dawn_extent.depthOrArrayLayers = webgpu_extent_sequence[2];
        FALLTHROUGH;
      case 2:
        dawn_extent.height = webgpu_extent_sequence[1];
        FALLTHROUGH;
      case 1:
        dawn_extent.width = webgpu_extent_sequence[0];
        FALLTHROUGH;
      case 0:
        break;
    }

  } else if (webgpu_extent->IsGPUExtent3DDict()) {
    const GPUExtent3DDict* webgpu_extent_3d_dict =
        webgpu_extent->GetAsGPUExtent3DDict();
    dawn_extent.width = webgpu_extent_3d_dict->width();
    dawn_extent.height = webgpu_extent_3d_dict->height();

    if (webgpu_extent_3d_dict->hasDepth()) {
      device->AddConsoleWarning(
          "Specifying an extent depth is deprecated. Use depthOrArrayLayers.");
      dawn_extent.depthOrArrayLayers = webgpu_extent_3d_dict->depth();
    } else {
      dawn_extent.depthOrArrayLayers =
          webgpu_extent_3d_dict->depthOrArrayLayers();
    }

  } else {
    NOTREACHED();
  }

  return dawn_extent;
}

WGPUOrigin3D AsDawnType(
    const UnsignedLongEnforceRangeSequenceOrGPUOrigin3DDict* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {0, 0, 0};

  if (webgpu_origin->IsUnsignedLongEnforceRangeSequence()) {
    const Vector<uint32_t>& webgpu_origin_sequence =
        webgpu_origin->GetAsUnsignedLongEnforceRangeSequence();

    // The WebGPU spec states that if the sequence isn't big enough then the
    // default values of 0 are used (which are set above).
    switch (webgpu_origin_sequence.size()) {
      default:
        dawn_origin.z = webgpu_origin_sequence[2];
        FALLTHROUGH;
      case 2:
        dawn_origin.y = webgpu_origin_sequence[1];
        FALLTHROUGH;
      case 1:
        dawn_origin.x = webgpu_origin_sequence[0];
        FALLTHROUGH;
      case 0:
        break;
    }

  } else if (webgpu_origin->IsGPUOrigin3DDict()) {
    const GPUOrigin3DDict* webgpu_origin_3d_dict =
        webgpu_origin->GetAsGPUOrigin3DDict();
    dawn_origin.x = webgpu_origin_3d_dict->x();
    dawn_origin.y = webgpu_origin_3d_dict->y();
    dawn_origin.z = webgpu_origin_3d_dict->z();

  } else {
    NOTREACHED();
  }

  return dawn_origin;
}

WGPUTextureCopyView AsDawnType(const GPUImageCopyTexture* webgpu_view,
                               GPUDevice* device) {
  DCHECK(webgpu_view);
  DCHECK(webgpu_view->texture());

  WGPUTextureCopyView dawn_view = {};
  dawn_view.texture = webgpu_view->texture()->GetHandle();
  dawn_view.mipLevel = webgpu_view->mipLevel();
  dawn_view.origin = AsDawnType(&webgpu_view->origin());
  dawn_view.aspect = AsDawnEnum<WGPUTextureAspect>(webgpu_view->aspect());

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

OwnedProgrammableStageDescriptor AsDawnType(
    const GPUProgrammableStage* webgpu_stage) {
  DCHECK(webgpu_stage);

  std::string entry_point = webgpu_stage->entryPoint().Ascii();
  // length() is in bytes (not utf-8 characters or something), so this is ok.
  size_t byte_size = entry_point.length() + 1;

  std::unique_ptr<char[]> entry_point_keepalive =
      std::make_unique<char[]>(byte_size);
  char* entry_point_ptr = entry_point_keepalive.get();
  memcpy(entry_point_ptr, entry_point.c_str(), byte_size);

  WGPUProgrammableStageDescriptor dawn_stage = {};
  dawn_stage.module = webgpu_stage->module()->GetHandle();
  dawn_stage.entryPoint = entry_point_ptr;

  return std::make_tuple(dawn_stage, std::move(entry_point_keepalive));
}

}  // namespace blink
