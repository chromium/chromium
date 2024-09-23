// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"

#include <stddef.h>
#include <string.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "gpu/skia_bindings/gles2_implementation_with_grcontext_support.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"

namespace skia_bindings {

GrContextForGLES2Interface::GrContextForGLES2Interface(
    gpu::gles2::GLES2Interface* gl,
    gpu::ContextSupport* context_support,
    const gpu::Capabilities& capabilities,
    size_t max_resource_cache_bytes,
    size_t max_glyph_cache_texture_bytes,
    bool support_bilerp_from_flyph_atlas)
    : context_support_(context_support) {
  GrContextOptions options;
  options.fGlyphCacheTextureMaximumBytes = max_glyph_cache_texture_bytes;
  options.fAvoidStencilBuffers = capabilities.avoid_stencil_buffers;
  options.fAllowPathMaskCaching = false;
  options.fShaderErrorHandler = this;
  // TODO(csmartdalton): enable internal multisampling after the related Skia
  // rolls are in.
  options.fInternalMultisampleCount = 0;
  options.fSupportBilerpFromGlyphAtlas = support_bilerp_from_flyph_atlas;
  sk_sp<GrGLInterface> interface(
      skia_bindings::CreateGLES2InterfaceBindings(gl, context_support));
  gr_context_ = GrDirectContexts::MakeGL(std::move(interface), options);
  if (gr_context_) {
    gr_context_->setResourceCacheLimit(max_resource_cache_bytes);
    context_support_->SetGrContext(gr_context_.get());
  }
}

GrContextForGLES2Interface::~GrContextForGLES2Interface() {
  // At this point the GLES2Interface is going to be destroyed, so have
  // the GrContext clean up and not try to use it anymore.
  if (gr_context_) {
    gr_context_->releaseResourcesAndAbandonContext();
    context_support_->SetGrContext(nullptr);
  }
}

void GrContextForGLES2Interface::compileError(const char* shader,
                                              const char* errors) {
  LOG(ERROR) << "Skia shader compilation error\n"
             << "------------------------\n"
             << shader << "\nErrors:\n"
             << errors;
}

void GrContextForGLES2Interface::OnLostContext() {
  if (gr_context_)
    gr_context_->abandonContext();
}

void GrContextForGLES2Interface::FreeGpuResources() {
  if (gr_context_) {
    TRACE_EVENT_INSTANT0("gpu", "GrContext::freeGpuResources",
                         TRACE_EVENT_SCOPE_THREAD);
    gr_context_->freeGpuResources();
  }
}

GrDirectContext* GrContextForGLES2Interface::get() {
  return gr_context_.get();
}

}  // namespace skia_bindings
