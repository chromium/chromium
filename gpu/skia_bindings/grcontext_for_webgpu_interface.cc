// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/grcontext_for_webgpu_interface.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace {

void PrintDeviceError(WGPUErrorType, const char* message, void*) {
  DLOG(ERROR) << "*** WebGPU client Device error: " << message;
}

}  // namespace

namespace skia_bindings {

GrContextForWebGPUInterface::GrContextForWebGPUInterface(
    gpu::webgpu::WebGPUInterface* webgpu,
    gpu::ContextSupport* context_support,
    const gpu::Capabilities& capabilities,
    size_t max_resource_cache_bytes,
    size_t max_glyph_cache_texture_bytes)
    : context_support_(context_support) {
  GrContextOptions options;
  options.fGlyphCacheTextureMaximumBytes = max_glyph_cache_texture_bytes;
  options.fAvoidStencilBuffers = capabilities.avoid_stencil_buffers;
  options.fAllowPathMaskCaching = false;
  options.fShaderErrorHandler = this;
  options.fInternalMultisampleCount = 0;
  // TODO(senorblanco): create an actual passed-in Device, rather than this
  // default hacky one.  http://crbug.com/1078775
  WGPUDevice device = webgpu->DeprecatedEnsureDefaultDeviceSync();
  wgpuDeviceSetUncapturedErrorCallback(device, PrintDeviceError, 0);
  gr_context_ = GrDirectContext::MakeDawn(device, options);
  if (gr_context_) {
    gr_context_->setResourceCacheLimit(max_resource_cache_bytes);
    context_support_->SetGrContext(gr_context_.get());
  }
}

GrContextForWebGPUInterface::~GrContextForWebGPUInterface() {
  // At this point the WebGPUInterface is going to be destroyed, so have
  // the GrContext clean up and not try to use it anymore.
  if (gr_context_) {
    gr_context_->releaseResourcesAndAbandonContext();
    context_support_->SetGrContext(nullptr);
  }
}

void GrContextForWebGPUInterface::compileError(const char* shader,
                                               const char* errors) {
  DLOG(ERROR) << "Skia shader compilation error\n"
              << "------------------------\n"
              << shader << "\nErrors:\n"
              << errors;
}

GrDirectContext* GrContextForWebGPUInterface::get() {
  return gr_context_.get();
}

}  // namespace skia_bindings
