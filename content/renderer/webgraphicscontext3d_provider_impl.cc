// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "cc/paint/paint_image.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "components/viz/common/gl_helper.h"
#include "gpu/command_buffer/client/context_support.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<ws::ContextProviderCommandBuffer> provider)
    : provider_(std::move(provider)) {}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {
  provider_->RemoveObserver(this);
}

bool WebGraphicsContext3DProviderImpl::BindToCurrentThread() {
  // TODO(danakj): Could plumb this result out to the caller so they know to
  // retry or not, if any client cared to know if it should retry or not.
  // Call AddObserver here instead of in constructor so that it's called on the
  // correct thread.
  provider_->AddObserver(this);
  return provider_->BindToCurrentThread() == gpu::ContextResult::kSuccess;
}

gpu::gles2::GLES2Interface* WebGraphicsContext3DProviderImpl::ContextGL() {
  return provider_->ContextGL();
}

gpu::webgpu::WebGPUInterface*
WebGraphicsContext3DProviderImpl::WebGPUInterface() {
  return provider_->WebGPUInterface();
}

GrContext* WebGraphicsContext3DProviderImpl::GetGrContext() {
  return provider_->GrContext();
}

const gpu::Capabilities& WebGraphicsContext3DProviderImpl::GetCapabilities()
    const {
  return provider_->ContextCapabilities();
}

const gpu::GpuFeatureInfo& WebGraphicsContext3DProviderImpl::GetGpuFeatureInfo()
    const {
  return provider_->GetGpuFeatureInfo();
}

viz::GLHelper* WebGraphicsContext3DProviderImpl::GetGLHelper() {
  if (!gl_helper_) {
    gl_helper_ = std::make_unique<viz::GLHelper>(provider_->ContextGL(),
                                                 provider_->ContextSupport());
  }
  return gl_helper_.get();
}

void WebGraphicsContext3DProviderImpl::SetLostContextCallback(
    base::RepeatingClosure c) {
  context_lost_callback_ = std::move(c);
}

void WebGraphicsContext3DProviderImpl::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> c) {
  provider_->ContextSupport()->SetErrorMessageCallback(std::move(c));
}

void WebGraphicsContext3DProviderImpl::OnContextLost() {
  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();
}

cc::ImageDecodeCache* WebGraphicsContext3DProviderImpl::ImageDecodeCache(
    SkColorType color_type) {
  DCHECK(GetGrContext()->colorTypeSupportedAsImage(color_type));
  auto cache_iterator = image_decode_cache_map_.find(color_type);
  if (cache_iterator != image_decode_cache_map_.end())
    return cache_iterator->second.get();

  // This denotes the allocated GPU memory budget for the cache used for
  // book-keeping. The cache indicates when the total memory locked exceeds this
  // budget in cc::DecodedDrawImage.
  static const size_t kMaxWorkingSetBytes = 64 * 1024 * 1024;

  // TransferCache is used only with OOP raster.
  const bool use_transfer_cache = false;

  auto insertion_result = image_decode_cache_map_.insert(
      std::pair<SkColorType, std::unique_ptr<cc::ImageDecodeCache>>(
          color_type, std::make_unique<cc::GpuImageDecodeCache>(
                          provider_.get(), use_transfer_cache, color_type,
                          kMaxWorkingSetBytes,
                          provider_->ContextCapabilities().max_texture_size,
                          cc::PaintImage::kDefaultGeneratorClientId)));
  DCHECK(insertion_result.second);
  cache_iterator = insertion_result.first;
  return cache_iterator->second.get();
}

}  // namespace content
