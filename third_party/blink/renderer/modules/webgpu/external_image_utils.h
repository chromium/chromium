// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_EXTERNAL_IMAGE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_EXTERNAL_IMAGE_UTILS_H_

#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/modules/webgpu/external_texture_helper.h"

namespace blink {

class StaticBitmapImage;
class ImageData;
class ImageBitmap;
class HTMLCanvasElement;
class HTMLImageElement;
class OffscreenCanvas;
class VideoFrame;

struct ExternalImageSource {
  ExternalTextureSource external_texture_source;
  scoped_refptr<StaticBitmapImage> image = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
};

// Describes how the ExternalImage will be used, which is needed to redecode
// images in some cases, to not lose information.
struct ExternalImageDstInfo {
  bool premultiplied_alpha;
  PredefinedColorSpace color_space;
};

// Imports an ExternalImageSource depending on the type. Returns nullopt on
// failure cases and may or may not throw exceptions (so it's best to throw a
// second time, just in case and have the fist exception thrown win).
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    HTMLVideoElement* video,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    VideoFrame* frame,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    HTMLCanvasElement* canvas,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    OffscreenCanvas* canvas,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    HTMLImageElement* image,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    ImageBitmap* image,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);
std::optional<ExternalImageSource> GetExternalImageSourceFrom(
    ImageData* image,
    ExternalImageDstInfo dst_info,
    ExceptionState& exception_state);

// Helper method to copy ExternalImages to wgpu::Textures
bool CopyStaticImageBitmapToWGPUTexture(
    const scoped_refptr<DawnControlClientHolder>& dawn_control_client,
    const wgpu::Device& device,
    StaticBitmapImage* image,
    const wgpu::Origin2D& origin,
    const wgpu::Extent3D& copy_size,
    const wgpu::TexelCopyTextureInfo& destination,
    bool dst_premultiplied_alpha,
    PredefinedColorSpace dst_color_space,
    bool flipY);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_EXTERNAL_IMAGE_UTILS_H_
