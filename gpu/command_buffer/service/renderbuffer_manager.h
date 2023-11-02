// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class GpuDriverBugWorkarounds;

namespace gles2 {

class FeatureInfo;
class Framebuffer;
class RenderbufferManager;

// Info about a Renderbuffer.
class GPU_GLES2_EXPORT Renderbuffer : public base::RefCounted<Renderbuffer> {
 public:
  Renderbuffer(RenderbufferManager* manager,
               GLuint client_id,
               GLuint service_id);

  GLuint service_id() const {
    return service_id_;
  }

  GLuint client_id() const {
    return client_id_;
  }

  bool cleared() const {
    return cleared_;
  }

  GLenum internal_format() const {
    return internal_format_;
  }

  GLsizei samples() const {
    return samples_;
  }

  GLsizei width() const {
    return width_;
  }

  GLsizei height() const {
    return height_;
  }

  bool IsDeleted() const {
    return client_id_ == 0;
  }

  void MarkAsValid() {
    has_been_bound_ = true;
  }

  bool IsValid() const {
    return has_been_bound_ && !IsDeleted();
  }

  // Regenerates the object backing this client_id, creating a new service_id.
  // Also reattaches any framebuffers using this renderbuffer.
  bool RegenerateAndBindBackingObjectIfNeeded(
      const GpuDriverBugWorkarounds& workarounds);

  void AddFramebufferAttachmentPoint(Framebuffer* framebuffer,
                                     GLenum attachment);
  void RemoveFramebufferAttachmentPoint(Framebuffer* framebuffer,
                                        GLenum attachment);

  size_t EstimatedSize();

  size_t GetSignatureSize() const;
  void AddToSignature(std::string* signature) const;

 private:
  friend class RenderbufferManager;
  friend class base::RefCounted<Renderbuffer>;

  ~Renderbuffer();

  void set_cleared(bool cleared) {
    cleared_ = cleared;
  }

  void SetInfoAndInvalidate(GLsizei samples,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height);

  void MarkAsDeleted() {
    client_id_ = 0;
  }

  // RenderbufferManager that owns this Renderbuffer.
  raw_ptr<RenderbufferManager> manager_;

  // Client side renderbuffer id.
  GLuint client_id_;

  // Service side renderbuffer id.
  GLuint service_id_;

  // Whether this renderbuffer has been cleared
  bool cleared_;

  // Whether this renderbuffer has been allocated.
  bool allocated_;

  // Whether this renderbuffer has ever been bound.
  bool has_been_bound_;

  // Number of samples (for multi-sampled renderbuffers)
  GLsizei samples_;

  // Renderbuffer internalformat set through RenderbufferStorage().
  GLenum internal_format_;

  // Dimensions of renderbuffer.
  GLsizei width_;
  GLsizei height_;

  // Framebuffer objects that this renderbuffer is attached to
  // (client ID, attachment).
  base::flat_set<std::pair<Framebuffer*, GLenum>>
      framebuffer_attachment_points_;
};

// This class keeps track of the renderbuffers and whether or not they have
// been cleared.
class GPU_GLES2_EXPORT RenderbufferManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  RenderbufferManager(MemoryTracker* memory_tracker,
                      GLint max_renderbuffer_size,
                      GLint max_samples,
                      FeatureInfo* feature_info);

  RenderbufferManager(const RenderbufferManager&) = delete;
  RenderbufferManager& operator=(const RenderbufferManager&) = delete;

  ~RenderbufferManager() override;

  GLint max_renderbuffer_size() const {
    return max_renderbuffer_size_;
  }

  GLint max_samples() const {
    return max_samples_;
  }

  bool HaveUnclearedRenderbuffers() const {
    return num_uncleared_renderbuffers_ != 0;
  }

  void SetInfoAndInvalidate(Renderbuffer* renderbuffer,
                            GLsizei samples,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height);

  void SetCleared(Renderbuffer* renderbuffer, bool cleared);

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Renderbuffer for the given renderbuffer ids.
  void CreateRenderbuffer(GLuint client_id, GLuint service_id);

  // Gets the renderbuffer for the given renderbuffer id.
  Renderbuffer* GetRenderbuffer(GLuint client_id);

  // Removes a renderbuffer for the given renderbuffer id.
  void RemoveRenderbuffer(GLuint client_id);

  size_t mem_represented() const {
    return memory_type_tracker_->GetMemRepresented();
  }

  bool ComputeEstimatedRenderbufferSize(int width,
                                        int height,
                                        int samples,
                                        int internal_format,
                                        uint32_t* size) const;
  GLenum InternalRenderbufferFormatToImplFormat(GLenum impl_format) const;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class Renderbuffer;

  void StartTracking(Renderbuffer* renderbuffer);
  void StopTracking(Renderbuffer* renderbuffer);

  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  raw_ptr<MemoryTracker> memory_tracker_;

  GLint max_renderbuffer_size_;
  GLint max_samples_;

  scoped_refptr<FeatureInfo> feature_info_;

  int num_uncleared_renderbuffers_;

  // Counts the number of Renderbuffer allocated with 'this' as its manager.
  // Allows to check no Renderbuffer will outlive this.
  unsigned renderbuffer_count_;

  bool have_context_;

  // Info for each renderbuffer in the system.
  typedef std::unordered_map<GLuint, scoped_refptr<Renderbuffer>>
      RenderbufferMap;
  RenderbufferMap renderbuffers_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RENDERBUFFER_MANAGER_H_
