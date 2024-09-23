// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_

#include <GLES2/gl2.h>
#include <stdint.h>

#include <memory>

#include "base/synchronization/lock.h"
#include "gles2_impl_export.h"
#include "gpu/command_buffer/client/client_discardable_manager.h"
#include "gpu/command_buffer/client/client_discardable_texture_manager.h"
#include "gpu/command_buffer/client/ref_counted.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"

namespace gpu {
namespace gles2 {

class GLES2Implementation;
class GLES2ImplementationTest;
class ProgramInfoManager;

typedef void (GLES2Implementation::*DeleteFn)(GLsizei n, const GLuint* ids);
typedef void (GLES2Implementation::*DeleteRangeFn)(const GLuint first_id,
                                                   GLsizei range);
typedef void (GLES2Implementation::*BindFn)(GLenum target, GLuint id);
typedef void (GLES2Implementation::*BindIndexedFn)( \
    GLenum target, GLuint index, GLuint id);
typedef void (GLES2Implementation::*BindIndexedRangeFn)( \
    GLenum target, GLuint index, GLuint id, GLintptr offset, GLsizeiptr size);

using SharedIdNamespaces = id_namespaces::SharedIdNamespaces;

class ShareGroupContextData {
 public:
  struct IdHandlerData {
    IdHandlerData();
    ~IdHandlerData();

    std::vector<GLuint> freed_ids_;
    uint32_t flush_generation_;
  };

  IdHandlerData* id_handler_data(int namespace_id) {
    return &id_handler_data_[namespace_id];
  }

 private:
  IdHandlerData id_handler_data_[static_cast<int>(
      SharedIdNamespaces::kNumSharedIdNamespaces)];
};

// Base class for IdHandlers
class IdHandlerInterface {
 public:
  IdHandlerInterface() = default;
  virtual ~IdHandlerInterface() = default;

  // Makes some ids at or above id_offset.
  virtual void MakeIds(
      GLES2Implementation* gl_impl,
      GLuint id_offset, GLsizei n, GLuint* ids) = 0;

  // Frees some ids.
  virtual bool FreeIds(
      GLES2Implementation* gl_impl, GLsizei n, const GLuint* ids,
      DeleteFn delete_fn) = 0;

  // Marks an id as used for glBind functions. id = 0 does nothing.
  virtual bool MarkAsUsedForBind(
      GLES2Implementation* gl_impl,
      GLenum target,
      GLuint id,
      BindFn bind_fn) = 0;
  // This is for glBindBufferBase.
  virtual bool MarkAsUsedForBind(
      GLES2Implementation* gl_impl,
      GLenum target,
      GLuint index,
      GLuint id,
      BindIndexedFn bind_fn) = 0;
  // This is for glBindBufferRange.
  virtual bool MarkAsUsedForBind(
      GLES2Implementation* gl_impl,
      GLenum target,
      GLuint index,
      GLuint id,
      GLintptr offset,
      GLsizeiptr size,
      BindIndexedRangeFn bind_fn) = 0;

  // Called when a context in the share group is destructed.
  virtual void FreeContext(GLES2Implementation* gl_impl) = 0;
};

class RangeIdHandlerInterface {
 public:
  RangeIdHandlerInterface() = default;
  virtual ~RangeIdHandlerInterface() = default;

  // Makes a continuous range of ids. Stores the first allocated id to
  // |first_id| or 0 if allocation failed.
  virtual void MakeIdRange(GLES2Implementation* gl_impl,
                           GLsizei n,
                           GLuint* first_id) = 0;

  // Frees a continuous |range| of ids beginning at |first_id|.
  virtual void FreeIdRange(GLES2Implementation* gl_impl,
                           const GLuint first_id,
                           GLsizei range,
                           DeleteRangeFn delete_fn) = 0;

  // Called when a context in the share group is destructed.
  virtual void FreeContext(GLES2Implementation* gl_impl) = 0;
};

// ShareGroup manages shared resources for contexts that are sharing resources.
class GLES2_IMPL_EXPORT ShareGroup
    : public gpu::RefCountedThreadSafe<ShareGroup> {
 public:
  ShareGroup(bool bind_generates_resource, uint64_t tracing_guid);

  ShareGroup(const ShareGroup&) = delete;
  ShareGroup& operator=(const ShareGroup&) = delete;

  bool bind_generates_resource() const {
    return bind_generates_resource_;
  }

  IdHandlerInterface* GetIdHandler(SharedIdNamespaces namespace_id) const {
    return id_handlers_[static_cast<int>(namespace_id)].get();
  }

  RangeIdHandlerInterface* GetRangeIdHandler(int range_namespace_id) const {
    return range_id_handlers_[range_namespace_id].get();
  }

  ProgramInfoManager* program_info_manager() {
    return program_info_manager_.get();
  }

  void FreeContext(GLES2Implementation* gl_impl) {
    for (int i = 0;
         i < static_cast<int>(SharedIdNamespaces::kNumSharedIdNamespaces);
         ++i) {
      id_handlers_[i]->FreeContext(gl_impl);
    }
    for (auto& range_id_handler : range_id_handlers_) {
      range_id_handler->FreeContext(gl_impl);
    }
  }

  uint64_t TracingGUID() const { return tracing_guid_; }

  ClientDiscardableTextureManager* discardable_texture_manager() {
    return &discardable_texture_manager_;
  }

  // Mark the ShareGroup as lost when an error occurs on any context in the
  // group. This is thread safe as contexts may be on different threads.
  void Lose();
  // Report if any context in the ShareGroup has reported itself lost. This is
  // thread safe as contexts may be on different threads.
  bool IsLost() const;

 private:
  friend class gpu::RefCountedThreadSafe<ShareGroup>;
  friend class gpu::gles2::GLES2ImplementationTest;
  ~ShareGroup();

  // Install a new program info manager. Used for testing only;
  void SetProgramInfoManagerForTesting(ProgramInfoManager* manager);

  std::unique_ptr<IdHandlerInterface> id_handlers_[static_cast<int>(
      SharedIdNamespaces::kNumSharedIdNamespaces)];
  std::unique_ptr<RangeIdHandlerInterface>
      range_id_handlers_[id_namespaces::kNumRangeIdNamespaces];
  std::unique_ptr<ProgramInfoManager> program_info_manager_;
  ClientDiscardableTextureManager discardable_texture_manager_;

  bool bind_generates_resource_;
  uint64_t tracing_guid_;

  mutable base::Lock lost_lock_;
  bool lost_ GUARDED_BY(lost_lock_) = false;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_
