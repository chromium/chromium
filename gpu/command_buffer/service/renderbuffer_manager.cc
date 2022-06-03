// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/renderbuffer_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace gles2 {

// This should contain everything to uniquely identify a Renderbuffer.
static const char RenderbufferTag[] = "|Renderbuffer|";
struct RenderbufferSignature {
  GLenum internal_format_;
  GLsizei samples_;
  GLsizei width_;
  GLsizei height_;

  // Since we will be hashing this signature structure, the padding must be
  // zero initialized. Although the C++11 specifications specify that this is
  // true, we will use a constructor with a memset to further enforce it instead
  // of relying on compilers adhering to this deep dark corner specification.
  RenderbufferSignature(GLenum internal_format,
                        GLsizei samples,
                        GLsizei width,
                        GLsizei height) {
    memset(this, 0, sizeof(RenderbufferSignature));
    internal_format_ = internal_format;
    samples_ = samples;
    width_ = width;
    height_ = height;
  }
};

RenderbufferManager::RenderbufferManager(MemoryTracker* memory_tracker,
                                         GLint max_renderbuffer_size,
                                         GLint max_samples,
                                         FeatureInfo* feature_info)
    : memory_type_tracker_(
          new MemoryTypeTracker(memory_tracker)),
      memory_tracker_(memory_tracker),
      max_renderbuffer_size_(max_renderbuffer_size),
      max_samples_(max_samples),
      feature_info_(feature_info),
      num_uncleared_renderbuffers_(0),
      renderbuffer_count_(0),
      have_context_(true) {
  // When created from InProcessCommandBuffer, we won't have a |memory_tracker_|
  // so don't register a dump provider.
  if (memory_tracker_) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::RenderbufferManager", base::ThreadTaskRunnerHandle::Get());
  }
}

RenderbufferManager::~RenderbufferManager() {
  DCHECK(renderbuffers_.empty());
  // If this triggers, that means something is keeping a reference to
  // a Renderbuffer belonging to this.
  CHECK_EQ(renderbuffer_count_, 0u);

  DCHECK_EQ(0, num_uncleared_renderbuffers_);

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

size_t Renderbuffer::EstimatedSize() {
  uint32_t size = 0;
  manager_->ComputeEstimatedRenderbufferSize(
      width_, height_, samples_, internal_format_, &size);
  return size;
}


size_t Renderbuffer::GetSignatureSize() const {
  return sizeof(RenderbufferTag) + sizeof(RenderbufferSignature);
}

void Renderbuffer::SetInfoAndInvalidate(GLsizei samples,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height) {
  samples_ = samples;
  internal_format_ = internalformat;
  width_ = width;
  height_ = height;
  cleared_ = false;
  allocated_ = true;
  for (auto& point : framebuffer_attachment_points_) {
    point.first->UnmarkAsComplete();
  }
}

void Renderbuffer::AddToSignature(std::string* signature) const {
  DCHECK(signature);
  RenderbufferSignature signature_data(internal_format_,
                                       samples_,
                                       width_,
                                       height_);

  signature->append(RenderbufferTag, sizeof(RenderbufferTag));
  signature->append(reinterpret_cast<const char*>(&signature_data),
                    sizeof(signature_data));
}

Renderbuffer::Renderbuffer(RenderbufferManager* manager,
                           GLuint client_id,
                           GLuint service_id)
    : manager_(manager),
      client_id_(client_id),
      service_id_(service_id),
      cleared_(true),
      allocated_(false),
      has_been_bound_(false),
      samples_(0),
      internal_format_(GL_RGBA4),
      width_(0),
      height_(0) {
  manager_->StartTracking(this);
}

bool Renderbuffer::RegenerateAndBindBackingObjectIfNeeded(
    const GpuDriverBugWorkarounds& workarounds) {
  // There are two workarounds which need this code path:
  //   depth_stencil_renderbuffer_resize_emulation
  //   multisample_renderbuffer_resize_emulation
  bool multisample_workaround =
      workarounds.multisample_renderbuffer_resize_emulation;
  bool depth_stencil_workaround =
      workarounds.depth_stencil_renderbuffer_resize_emulation;
  if (!multisample_workaround && !depth_stencil_workaround) {
    return false;
  }

  if (!allocated_ || !has_been_bound_) {
    return false;
  }

  bool workaround_needed = (multisample_workaround && samples_ > 0) ||
                           (depth_stencil_workaround &&
                            TextureManager::ExtractFormatFromStorageFormat(
                                internal_format_) == GL_DEPTH_STENCIL);

  if (!workaround_needed) {
    return false;
  }

  GLint original_fbo = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &original_fbo);

  glDeleteRenderbuffersEXT(1, &service_id_);
  service_id_ = 0;
  glGenRenderbuffersEXT(1, &service_id_);
  glBindRenderbufferEXT(GL_RENDERBUFFER, service_id_);

  // Attach new renderbuffer to all framebuffers
  for (auto& point : framebuffer_attachment_points_) {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, point.first->service_id());
    glFramebufferRenderbufferEXT(GL_DRAW_FRAMEBUFFER, point.second,
                                 GL_RENDERBUFFER, service_id_);
  }

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, original_fbo);

  allocated_ = false;
  return true;
}

void Renderbuffer::AddFramebufferAttachmentPoint(Framebuffer* framebuffer,
                                                 GLenum attachment) {
  DCHECK_NE(static_cast<GLenum>(GL_DEPTH_STENCIL_ATTACHMENT), attachment);
  framebuffer_attachment_points_.insert(
      std::make_pair(framebuffer, attachment));
}

void Renderbuffer::RemoveFramebufferAttachmentPoint(Framebuffer* framebuffer,
                                                    GLenum attachment) {
  DCHECK_NE(static_cast<GLenum>(GL_DEPTH_STENCIL_ATTACHMENT), attachment);
  framebuffer_attachment_points_.erase(std::make_pair(framebuffer, attachment));
}

Renderbuffer::~Renderbuffer() {
  if (manager_) {
    if (manager_->have_context_) {
      GLuint id = service_id();
      glDeleteRenderbuffersEXT(1, &id);
    }
    manager_->StopTracking(this);
    manager_ = nullptr;
  }
}

void RenderbufferManager::Destroy(bool have_context) {
  have_context_ = have_context;
  renderbuffers_.clear();
  DCHECK_EQ(0u, memory_type_tracker_->GetMemRepresented());
}

void RenderbufferManager::StartTracking(Renderbuffer* /* renderbuffer */) {
  ++renderbuffer_count_;
}

void RenderbufferManager::StopTracking(Renderbuffer* renderbuffer) {
  --renderbuffer_count_;
  if (!renderbuffer->cleared()) {
    --num_uncleared_renderbuffers_;
  }
  memory_type_tracker_->TrackMemFree(renderbuffer->EstimatedSize());
}

void RenderbufferManager::SetInfoAndInvalidate(Renderbuffer* renderbuffer,
                                               GLsizei samples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height) {
  DCHECK(renderbuffer);
  if (!renderbuffer->cleared()) {
    --num_uncleared_renderbuffers_;
  }
  memory_type_tracker_->TrackMemFree(renderbuffer->EstimatedSize());
  renderbuffer->SetInfoAndInvalidate(samples, internalformat, width, height);
  memory_type_tracker_->TrackMemAlloc(renderbuffer->EstimatedSize());
  if (!renderbuffer->cleared()) {
    ++num_uncleared_renderbuffers_;
  }
}

void RenderbufferManager::SetCleared(Renderbuffer* renderbuffer,
                                     bool cleared) {
  DCHECK(renderbuffer);
  if (!renderbuffer->cleared()) {
    --num_uncleared_renderbuffers_;
  }
  renderbuffer->set_cleared(cleared);
  if (!renderbuffer->cleared()) {
    ++num_uncleared_renderbuffers_;
  }
}

void RenderbufferManager::CreateRenderbuffer(
    GLuint client_id, GLuint service_id) {
  scoped_refptr<Renderbuffer> renderbuffer(
      new Renderbuffer(this, client_id, service_id));
  std::pair<RenderbufferMap::iterator, bool> result =
      renderbuffers_.insert(std::make_pair(client_id, renderbuffer));
  DCHECK(result.second);
  if (!renderbuffer->cleared()) {
    ++num_uncleared_renderbuffers_;
  }
}

Renderbuffer* RenderbufferManager::GetRenderbuffer(
    GLuint client_id) {
  RenderbufferMap::iterator it = renderbuffers_.find(client_id);
  return it != renderbuffers_.end() ? it->second.get() : nullptr;
}

void RenderbufferManager::RemoveRenderbuffer(GLuint client_id) {
  RenderbufferMap::iterator it = renderbuffers_.find(client_id);
  if (it != renderbuffers_.end()) {
    Renderbuffer* renderbuffer = it->second.get();
    renderbuffer->MarkAsDeleted();
    renderbuffers_.erase(it);
  }
}

bool RenderbufferManager::ComputeEstimatedRenderbufferSize(
    int width,
    int height,
    int samples,
    int internal_format,
    uint32_t* size) const {
  DCHECK(size);
  GLenum impl_format = InternalRenderbufferFormatToImplFormat(internal_format);
  uint32_t bytes_per_pixel = GLES2Util::RenderbufferBytesPerPixel(impl_format);
  base::CheckedNumeric<uint32_t> checked_size = width;
  checked_size *= height;
  checked_size *= (samples == 0 ? 1 : samples);
  checked_size *= bytes_per_pixel;
  return checked_size.AssignIfValid(size);
}

GLenum RenderbufferManager::InternalRenderbufferFormatToImplFormat(
    GLenum impl_format) const {
  if (!feature_info_->gl_version_info().BehavesLikeGLES()) {
    switch (impl_format) {
      case GL_DEPTH_COMPONENT16:
        return GL_DEPTH_COMPONENT;
      case GL_RGBA4:
      case GL_RGB5_A1:
        return GL_RGBA;
      case GL_RGB565:
        return GL_RGB;
    }
  } else {
    // Upgrade 16-bit depth to 24-bit if possible.
    if (impl_format == GL_DEPTH_COMPONENT16 &&
        feature_info_->feature_flags().oes_depth24)
      return GL_DEPTH_COMPONENT24;
  }
  return impl_format;
}

bool RenderbufferManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;
  const uint64_t context_group_tracing_id =
      memory_tracker_->ContextGroupTracingId();

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name =
        base::StringPrintf("gpu/gl/renderbuffers/context_group_0x%" PRIX64,
                           context_group_tracing_id);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, mem_represented());

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& renderbuffer_entry : renderbuffers_) {
    const auto& client_renderbuffer_id = renderbuffer_entry.first;
    const auto& renderbuffer = renderbuffer_entry.second;

    std::string dump_name =
        base::StringPrintf("gpu/gl/renderbuffers/context_group_0x%" PRIX64
                           "/renderbuffer_0x%" PRIX32,
                           context_group_tracing_id, client_renderbuffer_id);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    static_cast<uint64_t>(renderbuffer->EstimatedSize()));

    auto guid = gl::GetGLRenderbufferGUIDForTracing(context_group_tracing_id,
                                                    client_renderbuffer_id);
    pmd->CreateSharedGlobalAllocatorDump(guid);
    pmd->AddOwnershipEdge(dump->guid(), guid);
  }

  return true;
}

}  // namespace gles2
}  // namespace gpu
