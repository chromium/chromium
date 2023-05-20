// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CONTEXT_CREATION_ATTRIBS_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CONTEXT_CREATION_ATTRIBS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_channel.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::ContextColorSpace, gpu::ColorSpace> {
  static gpu::mojom::ContextColorSpace ToMojom(gpu::ColorSpace color_space) {
    switch (color_space) {
      case gpu::COLOR_SPACE_UNSPECIFIED:
        return gpu::mojom::ContextColorSpace::kUnspecified;
      case gpu::COLOR_SPACE_SRGB:
        return gpu::mojom::ContextColorSpace::kSRGB;
      case gpu::COLOR_SPACE_DISPLAY_P3:
        return gpu::mojom::ContextColorSpace::kDisplayP3;
      default:
        NOTREACHED();
    }
  }

  static bool FromMojom(gpu::mojom::ContextColorSpace color_space,
                        gpu::ColorSpace* out) {
    switch (color_space) {
      case gpu::mojom::ContextColorSpace::kUnspecified:
        *out = gpu::COLOR_SPACE_UNSPECIFIED;
        return true;
      case gpu::mojom::ContextColorSpace::kSRGB:
        *out = gpu::COLOR_SPACE_SRGB;
        return true;
      case gpu::mojom::ContextColorSpace::kDisplayP3:
        *out = gpu::COLOR_SPACE_DISPLAY_P3;
        return true;
      default:
        return false;
    }
  }
};

template <>
struct GPU_EXPORT EnumTraits<gpu::mojom::ContextType, gpu::ContextType> {
  static gpu::mojom::ContextType ToMojom(gpu::ContextType type) {
    switch (type) {
      case gpu::CONTEXT_TYPE_WEBGL1:
        return gpu::mojom::ContextType::kWebGL1;
      case gpu::CONTEXT_TYPE_WEBGL2:
        return gpu::mojom::ContextType::kWebGL2;
      case gpu::CONTEXT_TYPE_OPENGLES2:
        return gpu::mojom::ContextType::kOpenGLES2;
      case gpu::CONTEXT_TYPE_OPENGLES3:
        return gpu::mojom::ContextType::kOpenGLES3;
      case gpu::CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
        return gpu::mojom::ContextType::kOpenGLES31ForTesting;
      case gpu::CONTEXT_TYPE_WEBGPU:
        return gpu::mojom::ContextType::kWebGPU;
      default:
        NOTREACHED();
    }
  }

  static bool FromMojom(gpu::mojom::ContextType type, gpu::ContextType* out) {
    switch (type) {
      case gpu::mojom::ContextType::kWebGL1:
        *out = gpu::CONTEXT_TYPE_WEBGL1;
        return true;
      case gpu::mojom::ContextType::kWebGL2:
        *out = gpu::CONTEXT_TYPE_WEBGL2;
        return true;
      case gpu::mojom::ContextType::kOpenGLES2:
        *out = gpu::CONTEXT_TYPE_OPENGLES2;
        return true;
      case gpu::mojom::ContextType::kOpenGLES3:
        *out = gpu::CONTEXT_TYPE_OPENGLES3;
        return true;
      case gpu::mojom::ContextType::kOpenGLES31ForTesting:
        *out = gpu::CONTEXT_TYPE_OPENGLES31_FOR_TESTING;
        return true;
      case gpu::mojom::ContextType::kWebGPU:
        *out = gpu::CONTEXT_TYPE_WEBGPU;
        return true;
      default:
        return false;
    }
  }
};

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::ContextCreationAttribsDataView,
                               gpu::ContextCreationAttribs> {
  static gfx::Size offscreen_framebuffer_size(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.offscreen_framebuffer_size;
  }

  static gl::GpuPreference gpu_preference(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.gpu_preference;
  }

  static int32_t alpha_size(const gpu::ContextCreationAttribs& attribs) {
    return attribs.alpha_size;
  }

  static int32_t blue_size(const gpu::ContextCreationAttribs& attribs) {
    return attribs.blue_size;
  }

  static int32_t green_size(const gpu::ContextCreationAttribs& attribs) {
    return attribs.green_size;
  }

  static int32_t red_size(const gpu::ContextCreationAttribs& attribs) {
    return attribs.red_size;
  }

  static int32_t depth_size(const gpu::ContextCreationAttribs& attribs) {
    return attribs.depth_size;
  }

  static int32_t stencil_size(const gpu::ContextCreationAttribs& attribs) {
    return attribs.stencil_size;
  }

  static int32_t samples(const gpu::ContextCreationAttribs& attribs) {
    return attribs.samples;
  }

  static int32_t sample_buffers(const gpu::ContextCreationAttribs& attribs) {
    return attribs.sample_buffers;
  }

  static bool buffer_preserved(const gpu::ContextCreationAttribs& attribs) {
    return attribs.buffer_preserved;
  }

  static bool bind_generates_resource(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.bind_generates_resource;
  }

  static bool fail_if_major_perf_caveat(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.fail_if_major_perf_caveat;
  }

  static bool lose_context_when_out_of_memory(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.lose_context_when_out_of_memory;
  }

  static bool should_use_native_gmb_for_backbuffer(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.should_use_native_gmb_for_backbuffer;
  }

  static bool single_buffer(const gpu::ContextCreationAttribs& attribs) {
    return attribs.single_buffer;
  }

  static bool enable_gles2_interface(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.enable_gles2_interface;
  }

  static bool enable_grcontext(const gpu::ContextCreationAttribs& attribs) {
    return attribs.enable_grcontext;
  }

  static bool enable_raster_interface(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.enable_raster_interface;
  }

  static bool enable_oop_rasterization(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.enable_oop_rasterization;
  }

  static bool enable_swap_timestamps_if_supported(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.enable_swap_timestamps_if_supported;
  }

  static gpu::ContextType context_type(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.context_type;
  }

  static gpu::ColorSpace color_space(
      const gpu::ContextCreationAttribs& attribs) {
    return attribs.color_space;
  }

  static bool Read(gpu::mojom::ContextCreationAttribsDataView data,
                   gpu::ContextCreationAttribs* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CONTEXT_CREATION_ATTRIBS_MOJOM_TRAITS_H_
