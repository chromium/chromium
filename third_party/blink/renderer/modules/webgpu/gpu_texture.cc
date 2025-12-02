// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "base/containers/heap_array.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_usage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"

namespace blink {

namespace {

bool ConvertToDawn(const GPUTextureDescriptor* in,
                   wgpu::TextureDescriptor* out,
                   wgpu::TextureBindingViewDimensionDescriptor*
                       out_texture_binding_view_dimension,
                   std::string* label,
                   base::HeapArray<wgpu::TextureFormat>* view_formats,
                   GPUDevice* device,
                   ExceptionState& exception_state) {
  DCHECK(in);
  DCHECK(out);
  DCHECK(out_texture_binding_view_dimension);
  DCHECK(label);
  DCHECK(view_formats);
  DCHECK(device);

  *out = {};
  out->usage = static_cast<wgpu::TextureUsage>(in->usage());
  out->dimension = AsDawnEnum(in->dimension());
  out->format = AsDawnEnum(in->format());
  out->mipLevelCount = in->mipLevelCount();
  out->sampleCount = in->sampleCount();

  if (in->hasTextureBindingViewDimension()) {
    wgpu::TextureViewDimension texture_binding_view_dimension =
        AsDawnEnum(in->textureBindingViewDimension());
    if (texture_binding_view_dimension !=
        wgpu::TextureViewDimension::Undefined) {
      *out_texture_binding_view_dimension = {};
      out_texture_binding_view_dimension->textureBindingViewDimension =
          texture_binding_view_dimension;
      out->nextInChain = out_texture_binding_view_dimension;
    }
  }

  *label = in->label().Utf8();
  if (!label->empty()) {
    out->label = label->c_str();
  }

  *view_formats = AsDawnEnum<wgpu::TextureFormat>(in->viewFormats());
  out->viewFormatCount = in->viewFormats().size();
  out->viewFormats = view_formats->data();

  return ConvertToDawn(in->size(), &out->size, device, exception_state);
}

void ConvertToDawnType(const GPUTextureViewDescriptor* webgpu_desc,
                       OwnedTextureViewDescriptor* dawn_desc_info) {
  DCHECK(webgpu_desc);
  DCHECK(dawn_desc_info);

  if (webgpu_desc->hasFormat()) {
    dawn_desc_info->dawn_desc.format = AsDawnEnum(webgpu_desc->format());
  }
  if (webgpu_desc->hasDimension()) {
    dawn_desc_info->dawn_desc.dimension = AsDawnEnum(webgpu_desc->dimension());
  }
  dawn_desc_info->dawn_desc.baseMipLevel = webgpu_desc->baseMipLevel();
  if (webgpu_desc->hasMipLevelCount()) {
    dawn_desc_info->dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  }
  dawn_desc_info->dawn_desc.baseArrayLayer = webgpu_desc->baseArrayLayer();
  if (webgpu_desc->hasArrayLayerCount()) {
    dawn_desc_info->dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();
  }
  dawn_desc_info->dawn_desc.aspect = AsDawnEnum(webgpu_desc->aspect());
  if (!webgpu_desc->label().empty()) {
    dawn_desc_info->label = webgpu_desc->label().Utf8();
    dawn_desc_info->dawn_desc.label = dawn_desc_info->label.c_str();
  }
  if (webgpu_desc->hasUsage()) {
    dawn_desc_info->dawn_desc.usage =
        static_cast<wgpu::TextureUsage>(webgpu_desc->usage());
  }
  const auto& swizzle = webgpu_desc->swizzle();
  // Only pass the swizzle descriptor to Dawn if swizzle is non-default because
  // the C API will produce validation errors if a chained struct is passed
  // without its feature being enabled.
  if (swizzle != "rgba") {
    dawn_desc_info->swizzle_desc =
        std::make_unique<wgpu::TextureComponentSwizzleDescriptor>();
    dawn_desc_info->swizzle_desc->swizzle.r = AsDawnEnum(swizzle[0]);
    dawn_desc_info->swizzle_desc->swizzle.g = AsDawnEnum(swizzle[1]);
    dawn_desc_info->swizzle_desc->swizzle.b = AsDawnEnum(swizzle[2]);
    dawn_desc_info->swizzle_desc->swizzle.a = AsDawnEnum(swizzle[3]);
    dawn_desc_info->dawn_desc.nextInChain = dawn_desc_info->swizzle_desc.get();
  }
}

// Validate swizzle must be a four-character string that only includes "r", "g",
// "b", "a", "0", or "1".
bool ValidateSwizzle(const String& swizzle, ExceptionState& exception_state) {
  if (swizzle.length() != 4) {
    exception_state.ThrowTypeError(String::Format(
        "Swizzle ('%s') must be exactly a four-character string.",
        swizzle.Utf8().c_str()));
    return false;
  }

  if (AsDawnEnum(swizzle[0]) == wgpu::ComponentSwizzle::Undefined ||
      AsDawnEnum(swizzle[1]) == wgpu::ComponentSwizzle::Undefined ||
      AsDawnEnum(swizzle[2]) == wgpu::ComponentSwizzle::Undefined ||
      AsDawnEnum(swizzle[3]) == wgpu::ComponentSwizzle::Undefined) {
    exception_state.ThrowTypeError(String::Format(
        "Swizzle ('%s') must contain only 'r', 'g', 'b', 'a', '0', "
        "or '1' characters.",
        swizzle.Utf8().c_str()));
    return false;
  }

  return true;
}

// Dawn represents `undefined` as the special uint32_t value (0xFFFF'FFFF).
// Blink must make sure that an actual value of 0xFFFF'FFFF coming in from JS
// is not treated as the special `undefined` value, so it injects an error in
// that case.
std::string ValidateTextureMipLevelAndArrayLayerCounts(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  if (webgpu_desc->hasMipLevelCount() &&
      webgpu_desc->mipLevelCount() == wgpu::kMipLevelCountUndefined) {
    std::ostringstream error;
    error << "mipLevelCount (" << webgpu_desc->mipLevelCount()
          << ") is too large when validating [GPUTextureViewDescriptor";
    if (!webgpu_desc->label().empty()) {
      error << " '" << webgpu_desc->label().Utf8() << "'";
    }
    error << "].";
    return error.str();
  }

  if (webgpu_desc->hasArrayLayerCount() &&
      webgpu_desc->arrayLayerCount() == wgpu::kArrayLayerCountUndefined) {
    std::ostringstream error;
    error << "arrayLayerCount (" << webgpu_desc->arrayLayerCount()
          << ") is too large when validating [GPUTextureViewDescriptor";
    if (!webgpu_desc->label().empty()) {
      error << " '" << webgpu_desc->label().Utf8() << "'";
    }
    error << "].";
    return error.str();
  }

  return std::string();
}

}  // anonymous namespace

// static
GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const GPUTextureDescriptor* webgpu_desc,
                               ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  wgpu::TextureDescriptor dawn_desc;
  wgpu::TextureBindingViewDimensionDescriptor
      texture_binding_view_dimension_desc;

  std::string label;
  base::HeapArray<wgpu::TextureFormat> view_formats;
  if (!ConvertToDawn(webgpu_desc, &dawn_desc,
                     &texture_binding_view_dimension_desc, &label,
                     &view_formats, device, exception_state)) {
    return nullptr;
  }

  if (!device->ValidateTextureFormatUsage(webgpu_desc->format(),
                                          exception_state)) {
    return nullptr;
  }

  for (auto view_format : webgpu_desc->viewFormats()) {
    if (!device->ValidateTextureFormatUsage(view_format, exception_state)) {
      return nullptr;
    }
  }

  GPUTexture* texture = MakeGarbageCollected<GPUTexture>(
      device, device->GetHandle().CreateTexture(&dawn_desc),
      webgpu_desc->label());
  return texture;
}

GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const wgpu::TextureDescriptor* desc) {
  DCHECK(device);
  DCHECK(desc);

  return MakeGarbageCollected<GPUTexture>(
      device, device->GetHandle().CreateTexture(desc),
      String::FromUTF8(desc->label));
}

// static
GPUTexture* GPUTexture::CreateError(GPUDevice* device,
                                    const wgpu::TextureDescriptor* desc) {
  DCHECK(device);
  DCHECK(desc);
  return MakeGarbageCollected<GPUTexture>(
      device, device->GetHandle().CreateErrorTexture(desc),
      String::FromUTF8(desc->label));
}

GPUTexture::GPUTexture(GPUDevice* device,
                       wgpu::Texture texture,
                       const String& label)
    : DawnObject<wgpu::Texture>(device, std::move(texture), label),
      dimension_(GetHandle().GetDimension()),
      format_(GetHandle().GetFormat()),
      usage_(GetHandle().GetUsage()) {}

GPUTexture::GPUTexture(GPUDevice* device,
                       wgpu::TextureFormat format,
                       wgpu::TextureUsage usage,
                       scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
                       const String& label)
    : DawnObject<wgpu::Texture>(device, mailbox_texture->GetTexture(), label),
      format_(format),
      usage_(usage),
      mailbox_texture_(std::move(mailbox_texture)) {
  if (mailbox_texture_) {
    device_->TrackTextureWithMailbox(this);
  }

  // Mailbox textures are all 2d texture.
  dimension_ = wgpu::TextureDimension::e2D;
}

GPUTextureView* GPUTexture::createView(
    const GPUTextureViewDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(webgpu_desc);

  if (webgpu_desc->hasFormat() && !device()->ValidateTextureFormatUsage(
                                      webgpu_desc->format(), exception_state)) {
    return nullptr;
  }

  if (!ValidateSwizzle(webgpu_desc->swizzle(), exception_state)) {
    return nullptr;
  }

  std::string error = ValidateTextureMipLevelAndArrayLayerCounts(webgpu_desc);
  if (!error.empty()) {
    device()->InjectError(wgpu::ErrorType::Validation, error.c_str());
    return MakeGarbageCollected<GPUTextureView>(
        device(), GetHandle().CreateErrorView(nullptr), String());
  }

  OwnedTextureViewDescriptor dawn_desc_info;
  ConvertToDawnType(webgpu_desc, &dawn_desc_info);
  GPUTextureView* view = MakeGarbageCollected<GPUTextureView>(
      device_, GetHandle().CreateView(&dawn_desc_info.dawn_desc),
      webgpu_desc->label());
  return view;
}

GPUTexture::~GPUTexture() {
  DissociateMailbox();
}

void GPUTexture::destroy() {
  if (destroyed_) {
    return;
  }

  if (destroy_callback_) {
    std::move(destroy_callback_).Run();
  }

  if (mailbox_texture_) {
    DissociateMailbox();
    device_->UntrackTextureWithMailbox(this);
  }
  GetHandle().Destroy();
  destroyed_ = true;
}

uint32_t GPUTexture::width() const {
  return GetHandle().GetWidth();
}

uint32_t GPUTexture::height() const {
  return GetHandle().GetHeight();
}

uint32_t GPUTexture::depthOrArrayLayers() const {
  return GetHandle().GetDepthOrArrayLayers();
}

uint32_t GPUTexture::mipLevelCount() const {
  return GetHandle().GetMipLevelCount();
}

uint32_t GPUTexture::sampleCount() const {
  return GetHandle().GetSampleCount();
}

V8GPUTextureDimension GPUTexture::dimension() const {
  return FromDawnEnum(GetHandle().GetDimension());
}

V8GPUTextureFormat GPUTexture::format() const {
  return FromDawnEnum(GetHandle().GetFormat());
}

uint32_t GPUTexture::usage() const {
  return static_cast<uint32_t>(GetHandle().GetUsage());
}

void GPUTexture::DissociateMailbox() {
  if (mailbox_texture_) {
    mailbox_texture_->Dissociate();
    mailbox_texture_ = nullptr;
  }
}

scoped_refptr<WebGPUMailboxTexture> GPUTexture::GetMailboxTexture() {
  return mailbox_texture_;
}

void GPUTexture::SetBeforeDestroyCallback(base::OnceClosure callback) {
  destroy_callback_ = std::move(callback);
}

void GPUTexture::ClearBeforeDestroyCallback() {
  destroy_callback_.Reset();
}

}  // namespace blink
