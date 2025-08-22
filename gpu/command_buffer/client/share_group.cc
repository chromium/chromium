// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/client/share_group.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/stack.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/program_info_manager.h"
#include "gpu/command_buffer/common/id_allocator.h"

namespace gpu {
namespace gles2 {

ShareGroupContextData::IdHandlerData::IdHandlerData() : flush_generation_(0) {}
ShareGroupContextData::IdHandlerData::~IdHandlerData() = default;

static_assert(gpu::kInvalidResource == 0,
              "GL expects kInvalidResource to be 0");

// The standard id handler.
class IdHandler : public IdHandlerInterface {
 public:
  IdHandler() = default;
  ~IdHandler() override = default;

  // Overridden from IdHandlerInterface.
  void MakeIds(GLES2Implementation* /* gl_impl */,
               GLuint id_offset,
               GLsizei n,
               GLuint* ids) override {
    base::AutoLock auto_lock(lock_);
    if (id_offset == 0) {
      for (GLsizei ii = 0; ii < n; ++ii) {
        ids[ii] = id_allocator_.AllocateID();
      }
    } else {
      for (GLsizei ii = 0; ii < n; ++ii) {
        ids[ii] = id_allocator_.AllocateIDAtOrAbove(id_offset);
        id_offset = ids[ii] + 1;
      }
    }
  }

  // Overridden from IdHandlerInterface.
  bool FreeIds(GLES2Implementation* gl_impl,
               GLsizei n,
               const GLuint* ids,
               DeleteFn delete_fn) override {
    base::AutoLock auto_lock(lock_);

    for (GLsizei ii = 0; ii < n; ++ii) {
      id_allocator_.FreeID(ids[ii]);
    }

    (gl_impl->*delete_fn)(n, ids);
    // We need to ensure that the delete call is evaluated on the service side
    // before any other contexts issue commands using these client ids.
    gl_impl->helper()->CommandBufferHelper::OrderingBarrier();
    return true;
  }

  // Overridden from IdHandlerInterface.
  bool MarkAsUsedForBind(GLES2Implementation* gl_impl,
                         GLenum target,
                         GLuint id,
                         BindFn bind_fn) override {
    base::AutoLock auto_lock(lock_);
    bool result = id ? id_allocator_.MarkAsUsed(id) : true;
    (gl_impl->*bind_fn)(target, id);
    return result;
  }
  bool MarkAsUsedForBind(GLES2Implementation* gl_impl,
                         GLenum target,
                         GLuint index,
                         GLuint id,
                         BindIndexedFn bind_fn) override {
    base::AutoLock auto_lock(lock_);
    bool result = id ? id_allocator_.MarkAsUsed(id) : true;
    (gl_impl->*bind_fn)(target, index, id);
    return result;
  }
  bool MarkAsUsedForBind(GLES2Implementation* gl_impl,
                         GLenum target,
                         GLuint index,
                         GLuint id,
                         GLintptr offset,
                         GLsizeiptr size,
                         BindIndexedRangeFn bind_fn) override {
    base::AutoLock auto_lock(lock_);
    bool result = id ? id_allocator_.MarkAsUsed(id) : true;
    (gl_impl->*bind_fn)(target, index, id, offset, size);
    return result;
  }

  void FreeContext(GLES2Implementation* gl_impl) override {}

 private:
  base::Lock lock_;
  IdAllocator id_allocator_ GUARDED_BY(lock_);
};

// An id handler that requires Gen before Bind.
class StrictIdHandler : public IdHandlerInterface {
 public:
  explicit StrictIdHandler(int id_namespace) : id_namespace_(id_namespace) {}
  ~StrictIdHandler() override = default;

  // Overridden from IdHandler.
  void MakeIds(GLES2Implementation* gl_impl,
               GLuint /* id_offset */,
               GLsizei n,
               GLuint* ids) override {
    base::AutoLock auto_lock(lock_);

    // Collect pending FreeIds from other flush_generation.
    CollectPendingFreeIds(gl_impl);

    for (GLsizei ii = 0; ii < n; ++ii) {
      if (!free_ids_.empty()) {
        // Allocate a previously freed Id.
        ids[ii] = free_ids_.top();
        free_ids_.pop();

        // Record kIdInUse state.
        DCHECK(id_states_[ids[ii] - 1] == kIdFree);
        id_states_[ids[ii] - 1] = kIdInUse;
      } else {
        // Allocate a new Id.
        id_states_.push_back(kIdInUse);
        ids[ii] = id_states_.size();
      }
    }
  }

  // Overridden from IdHandler.
  bool FreeIds(GLES2Implementation* gl_impl,
               GLsizei n,
               const GLuint* ids,
               DeleteFn delete_fn) override {
    // Delete stub must run before CollectPendingFreeIds.
    (gl_impl->*delete_fn)(n, ids);

    bool return_value = true;

    {
      base::AutoLock auto_lock(lock_);

      // Collect pending FreeIds from other flush_generation.
      CollectPendingFreeIds(gl_impl);

      // Save Ids to free in a later flush_generation.
      ShareGroupContextData::IdHandlerData* ctxt_data =
          gl_impl->share_group_context_data()->id_handler_data(id_namespace_);

      GLuint max_valid_id = id_states_.size();
      for (GLsizei ii = 0; ii < n; ++ii) {
        GLuint id = ids[ii];
        if (id != 0) {
          if (id > max_valid_id) {
            // Caller will generate an error.
            return_value = false;
            continue;
          }
          // Save freed Id for later.
          if (id_states_[id - 1] != kIdInUse) {
            DVLOG(1) << "Already freed id " << id;
            return_value = false;
            continue;
          }
          id_states_[id - 1] = kIdPendingFree;
          ctxt_data->freed_ids_.push_back(id);
        }
      }
    }

    return return_value;
  }

  // Overridden from IdHandler.
  bool MarkAsUsedForBind(GLES2Implementation* gl_impl,
                         GLenum target,
                         GLuint id,
                         BindFn bind_fn) override {
#ifndef NDEBUG
    if (id != 0) {
      base::AutoLock auto_lock(lock_);
      DCHECK(id_states_[id - 1] == kIdInUse);
    }
#endif
    // StrictIdHandler is used if |bind_generates_resource| is false. In that
    // case, |bind_fn| will not use Flush() after helper->Bind*(), so it is OK
    // to call |bind_fn| without holding the lock.
    (gl_impl->*bind_fn)(target, id);
    return true;
  }
  bool MarkAsUsedForBind(GLES2Implementation* gl_impl,
                         GLenum target,
                         GLuint index,
                         GLuint id,
                         BindIndexedFn bind_fn) override {
#ifndef NDEBUG
    if (id != 0) {
      base::AutoLock auto_lock(lock_);
      DCHECK(id_states_[id - 1] == kIdInUse);
    }
#endif
    // StrictIdHandler is used if |bind_generates_resource| is false. In that
    // case, |bind_fn| will not use Flush() after helper->Bind*(), so it is OK
    // to call |bind_fn| without holding the lock.
    (gl_impl->*bind_fn)(target, index, id);
    return true;
  }
  bool MarkAsUsedForBind(GLES2Implementation* gl_impl,
                         GLenum target,
                         GLuint index,
                         GLuint id,
                         GLintptr offset,
                         GLsizeiptr size,
                         BindIndexedRangeFn bind_fn) override {
#ifndef NDEBUG
    if (id != 0) {
      base::AutoLock auto_lock(lock_);
      DCHECK(id_states_[id - 1] == kIdInUse);
    }
#endif
    // StrictIdHandler is used if |bind_generates_resource| is false. In that
    // case, |bind_fn| will not use Flush() after helper->Bind*(), so it is OK
    // to call |bind_fn| without holding the lock.
    (gl_impl->*bind_fn)(target, index, id, offset, size);
    return true;
  }

  // Overridden from IdHandlerInterface.
  void FreeContext(GLES2Implementation* gl_impl) override {
    base::AutoLock auto_lock(lock_);
    CollectPendingFreeIds(gl_impl);
  }

 private:
  enum IdState { kIdFree, kIdPendingFree, kIdInUse };

  void CollectPendingFreeIds(GLES2Implementation* gl_impl)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    uint32_t flush_generation = gl_impl->helper()->flush_generation();
    ShareGroupContextData::IdHandlerData* ctxt_data =
        gl_impl->share_group_context_data()->id_handler_data(id_namespace_);

    if (ctxt_data->flush_generation_ != flush_generation) {
      ctxt_data->flush_generation_ = flush_generation;
      for (uint32_t ii = 0; ii < ctxt_data->freed_ids_.size(); ++ii) {
        const GLuint id = ctxt_data->freed_ids_[ii];
        DCHECK(id_states_[id - 1] == kIdPendingFree);
        id_states_[id - 1] = kIdFree;
        free_ids_.push(id);
      }
      ctxt_data->freed_ids_.clear();
    }
  }

  int id_namespace_;

  base::Lock lock_;
  std::vector<uint8_t> id_states_ GUARDED_BY(lock_);
  base::stack<uint32_t> free_ids_ GUARDED_BY(lock_);
};

// An id handler for ids that are never reused.
class NonReusedIdHandler : public IdHandlerInterface {
 public:
  NonReusedIdHandler() : last_id_(0) {}
  ~NonReusedIdHandler() override = default;

  // Overridden from IdHandlerInterface.
  void MakeIds(GLES2Implementation* /* gl_impl */,
               GLuint id_offset,
               GLsizei n,
               GLuint* ids) override {
    base::AutoLock auto_lock(lock_);
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = ++last_id_ + id_offset;
    }
  }

  // Overridden from IdHandlerInterface.
  bool FreeIds(GLES2Implementation* gl_impl,
               GLsizei n,
               const GLuint* ids,
               DeleteFn delete_fn) override {
    // Ids are never freed.
    (gl_impl->*delete_fn)(n, ids);
    return true;
  }

  // Overridden from IdHandlerInterface.
  bool MarkAsUsedForBind(GLES2Implementation* /* gl_impl */,
                         GLenum /* target */,
                         GLuint /* id */,
                         BindFn /* bind_fn */) override {
    // This is only used for Shaders and Programs which have no bind.
    return false;
  }
  bool MarkAsUsedForBind(GLES2Implementation* /* gl_impl */,
                         GLenum /* target */,
                         GLuint /* index */,
                         GLuint /* id */,
                         BindIndexedFn /* bind_fn */) override {
    // This is only used for Shaders and Programs which have no bind.
    return false;
  }
  bool MarkAsUsedForBind(GLES2Implementation* /* gl_impl */,
                         GLenum /* target */,
                         GLuint /* index */,
                         GLuint /* id */,
                         GLintptr /* offset */,
                         GLsizeiptr /* size */,
                         BindIndexedRangeFn /* bind_fn */) override {
    // This is only used for Shaders and Programs which have no bind.
    return false;
  }

  void FreeContext(GLES2Implementation* gl_impl) override {}

 private:
  base::Lock lock_;
  GLuint last_id_ GUARDED_BY(lock_);
};

class RangeIdHandler : public RangeIdHandlerInterface {
 public:
  RangeIdHandler() = default;

  void MakeIdRange(GLES2Implementation* /*gl_impl*/,
                   GLsizei n,
                   GLuint* first_id) override {
    base::AutoLock auto_lock(lock_);
    *first_id = id_allocator_.AllocateIDRange(n);
  }

  void FreeIdRange(GLES2Implementation* gl_impl,
                   const GLuint first_id,
                   GLsizei range,
                   DeleteRangeFn delete_fn) override {
    base::AutoLock auto_lock(lock_);
    DCHECK(range > 0);
    id_allocator_.FreeIDRange(first_id, range);
    (gl_impl->*delete_fn)(first_id, range);
    gl_impl->helper()->CommandBufferHelper::OrderingBarrier();
  }

  void FreeContext(GLES2Implementation* gl_impl) override {}

 private:
  base::Lock lock_;
  IdAllocator id_allocator_ GUARDED_BY(lock_);
};

ShareGroup::ShareGroup(bool bind_generates_resource, uint64_t tracing_guid)
    : bind_generates_resource_(bind_generates_resource),
      tracing_guid_(tracing_guid) {
  if (bind_generates_resource) {
    for (int i = 0;
         i < static_cast<int>(SharedIdNamespaces::kNumSharedIdNamespaces);
         ++i) {
      if (i == static_cast<int>(SharedIdNamespaces::kProgramsAndShaders)) {
        id_handlers_[i] = std::make_unique<NonReusedIdHandler>();
      } else {
        id_handlers_[i] = std::make_unique<IdHandler>();
      }
    }
  } else {
    for (int i = 0;
         i < static_cast<int>(SharedIdNamespaces::kNumSharedIdNamespaces);
         ++i) {
      if (i == static_cast<int>(SharedIdNamespaces::kProgramsAndShaders)) {
        id_handlers_[i] = std::make_unique<NonReusedIdHandler>();
      } else {
        id_handlers_[i] = std::make_unique<StrictIdHandler>(i);
      }
    }
  }
  program_info_manager_ = std::make_unique<ProgramInfoManager>();
  for (auto& range_id_handler : range_id_handlers_) {
    range_id_handler = std::make_unique<RangeIdHandler>();
  }
}

void ShareGroup::Lose() {
  base::AutoLock hold(lost_lock_);
  lost_ = true;
}

bool ShareGroup::IsLost() const {
  base::AutoLock hold(lost_lock_);
  return lost_;
}

void ShareGroup::SetProgramInfoManagerForTesting(ProgramInfoManager* manager) {
  program_info_manager_.reset(manager);
}

ShareGroup::~ShareGroup() = default;

}  // namespace gles2
}  // namespace gpu
