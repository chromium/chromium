// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/buffer_manager.h"

#include <stdint.h>

#include <limits>
#include <memory>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace gles2 {
namespace {
static const GLsizeiptr kDefaultMaxBufferSize = 1u << 30;  // 1GB
}

BufferManager::BufferManager(MemoryTracker* memory_tracker,
                             FeatureInfo* feature_info)
    : memory_type_tracker_(new MemoryTypeTracker(memory_tracker)),
      memory_tracker_(memory_tracker),
      feature_info_(feature_info),
      max_buffer_size_(kDefaultMaxBufferSize),
      allow_buffers_on_multiple_targets_(false),
      allow_fixed_attribs_(false),
      buffer_count_(0),
      primitive_restart_fixed_index_(0),
      lost_context_(false),
      use_client_side_arrays_for_stream_buffers_(
          feature_info ? feature_info->workarounds()
                             .use_client_side_arrays_for_stream_buffers
                       : false) {
  // When created from InProcessCommandBuffer, we won't have a |memory_tracker_|
  // so don't register a dump provider.
  if (memory_tracker_) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "gpu::BufferManager",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

BufferManager::~BufferManager() {
  DCHECK(buffers_.empty());
  CHECK_EQ(buffer_count_, 0u);

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void BufferManager::MarkContextLost() {
  lost_context_ = true;
}

void BufferManager::Destroy() {
  buffers_.clear();
  DCHECK_EQ(0u, memory_type_tracker_->GetMemRepresented());
}

void BufferManager::CreateBuffer(GLuint client_id, GLuint service_id) {
  scoped_refptr<Buffer> buffer(new Buffer(this, service_id));
  std::pair<BufferMap::iterator, bool> result =
      buffers_.insert(std::make_pair(client_id, buffer));
  DCHECK(result.second);
}

Buffer* BufferManager::GetBuffer(
    GLuint client_id) {
  BufferMap::iterator it = buffers_.find(client_id);
  return it != buffers_.end() ? it->second.get() : nullptr;
}

void BufferManager::RemoveBuffer(GLuint client_id) {
  BufferMap::iterator it = buffers_.find(client_id);
  if (it != buffers_.end()) {
    Buffer* buffer = it->second.get();
    buffer->MarkAsDeleted();
    buffers_.erase(it);
  }
}

void BufferManager::StartTracking(Buffer* /* buffer */) {
  ++buffer_count_;
}

void BufferManager::StopTracking(Buffer* buffer) {
  memory_type_tracker_->TrackMemFree(buffer->size());
  --buffer_count_;
}

Buffer::MappedRange::MappedRange(
    GLintptr offset, GLsizeiptr size, GLenum access, void* pointer,
    scoped_refptr<gpu::Buffer> shm, unsigned int shm_offset)
    : offset(offset),
      size(size),
      access(access),
      pointer(pointer),
      shm(shm),
      shm_offset(shm_offset) {
  DCHECK(pointer);
  DCHECK(shm.get() && GetShmPointer());
}

Buffer::MappedRange::~MappedRange() = default;

void* Buffer::MappedRange::GetShmPointer() const {
  DCHECK(shm.get());
  return shm->GetDataAddress(shm_offset, static_cast<unsigned int>(size));
}

Buffer::Buffer(BufferManager* manager, GLuint service_id)
    : manager_(manager),
      size_(0),
      deleted_(false),
      is_client_side_array_(false),
      service_id_(service_id),
      initial_target_(0),
      usage_(GL_STATIC_DRAW) {
  manager_->StartTracking(this);
}

Buffer::~Buffer() {
  if (manager_) {
    if (!manager_->lost_context_) {
      GLuint id = service_id();
      glDeleteBuffersARB(1, &id);
    }
    RemoveMappedRange();
    manager_->StopTracking(this);
    manager_ = nullptr;
  }
}

void Buffer::OnBind(GLenum target, bool indexed) {
  if (target == GL_TRANSFORM_FEEDBACK_BUFFER && indexed) {
    ++transform_feedback_indexed_binding_count_;
  } else if (target != GL_TRANSFORM_FEEDBACK_BUFFER) {
    ++non_transform_feedback_binding_count_;
  }
  // Note that the transform feedback generic (non-indexed) binding point does
  // not count as a transform feedback indexed binding point *or* a non-
  // transform- feedback binding point, so no reference counts need to change in
  // that case. See https://crbug.com/853978
}

void Buffer::OnUnbind(GLenum target, bool indexed) {
  if (target == GL_TRANSFORM_FEEDBACK_BUFFER && indexed) {
    --transform_feedback_indexed_binding_count_;
  } else if (target != GL_TRANSFORM_FEEDBACK_BUFFER) {
    --non_transform_feedback_binding_count_;
  }
  DCHECK(transform_feedback_indexed_binding_count_ >= 0);
  DCHECK(non_transform_feedback_binding_count_ >= 0);
}

const GLvoid* Buffer::StageShadow(bool use_shadow,
                                  GLsizeiptr size,
                                  const GLvoid* data) {
  shadow_.clear();
  if (use_shadow) {
    if (data) {
      shadow_.insert(shadow_.begin(),
                     static_cast<const uint8_t*>(data),
                     static_cast<const uint8_t*>(data) + size);
    } else {
      shadow_.resize(size);
      memset(shadow_.data(), 0, static_cast<size_t>(size));
    }
    return shadow_.data();
  } else {
    return data;
  }
}

void Buffer::SetInfo(GLsizeiptr size,
                     GLenum usage,
                     bool use_shadow,
                     bool is_client_side_array) {
  usage_ = usage;
  is_client_side_array_ = is_client_side_array;
  ClearCache();

  // Shadow must have been setup already.
  DCHECK_EQ(shadow_.size(), static_cast<size_t>(use_shadow ? size : 0u));
  size_ = size;

  mapped_range_.reset(nullptr);
  readback_shm_ = nullptr;
  readback_shm_offset_ = 0;
}

bool Buffer::CheckRange(GLintptr offset, GLsizeiptr size) const {
  if (offset < 0 || offset > std::numeric_limits<int32_t>::max() ||
      size < 0 || size > std::numeric_limits<int32_t>::max()) {
    return false;
  }
  int32_t max;
  return base::CheckAdd(offset, size).AssignIfValid(&max) && max <= size_;
}

void Buffer::SetRange(GLintptr offset, GLsizeiptr size, const GLvoid * data) {
  DCHECK(CheckRange(offset, size));
  if (!shadow_.empty()) {
    DCHECK_LE(static_cast<size_t>(offset + size), shadow_.size());
    memcpy(shadow_.data() + offset, data, size);
    ClearCache();
  }
}

const void* Buffer::GetRange(GLintptr offset, GLsizeiptr size) const {
  if (shadow_.empty()) {
    return nullptr;
  }
  if (!CheckRange(offset, size)) {
    return nullptr;
  }
  DCHECK_LE(static_cast<size_t>(offset + size), shadow_.size());
  return shadow_.data() + offset;
}

void Buffer::ClearCache() {
  range_set_.clear();
}

template <typename T>
GLuint GetMaxValue(const void* data, GLuint offset, GLsizei count,
    GLuint primitive_restart_index) {
  GLuint max_value = 0;
  const T* element =
      reinterpret_cast<const T*>(static_cast<const int8_t*>(data) + offset);
  const T* end = element + count;
  for (; element < end; ++element) {
    if (*element > max_value) {
      if (*element == primitive_restart_index) {
        continue;
      }
      max_value = *element;
    }
  }
  return max_value;
}

bool Buffer::GetMaxValueForRange(
    GLuint offset, GLsizei count, GLenum type, bool primitive_restart_enabled,
    GLuint* max_value) {
  GLuint primitive_restart_index = 0;
  if (primitive_restart_enabled) {
    switch (type) {
      case GL_UNSIGNED_BYTE:
        primitive_restart_index = 0xFF;
        break;
      case GL_UNSIGNED_SHORT:
        primitive_restart_index = 0xFFFF;
        break;
      case GL_UNSIGNED_INT:
        primitive_restart_index = 0xFFFFFFFF;
        break;
      default:
        NOTREACHED_IN_MIGRATION();  // should never get here by validation.
        break;
    }
  }

  Range range(offset, count, type, primitive_restart_enabled);
  RangeToMaxValueMap::iterator it = range_set_.find(range);
  if (it != range_set_.end()) {
    *max_value = it->second;
    return true;
  }
  // Optimization. If:
  //  - primitive restart is enabled
  //  - we don't have an entry in the range set for these parameters
  //    for the situation when primitive restart is enabled
  //  - we do have an entry in the range set for these parameters for
  //    the situation when primitive restart is disabled
  //  - this entry is less than the primitive restart index
  // Then we can repurpose this entry for the situation when primitive
  // restart is enabled. Otherwise, we need to compute the max index
  // from scratch.
  if (primitive_restart_enabled) {
    Range disabled_range(offset, count, type, false);
    it = range_set_.find(disabled_range);
    if (it != range_set_.end() && it->second < primitive_restart_index) {
      // This reuses the max value for the case where primitive
      // restart is enabled.
      range_set_.insert(std::make_pair(range, it->second));
      *max_value = it->second;
      return true;
    }
  }

  uint32_t size;
  if (!base::CheckAdd(
           offset,
           base::CheckMul(count, GLES2Util::GetGLTypeSizeForBuffers(type)))
           .AssignIfValid(&size)) {
    return false;
  }

  if (size > static_cast<uint32_t>(size_)) {
    return false;
  }

  if (shadow_.empty()) {
    return false;
  }

  // Scan the range for the max value and store
  GLuint max_v = 0;
  switch (type) {
    case GL_UNSIGNED_BYTE:
      max_v = GetMaxValue<uint8_t>(shadow_.data(), offset, count,
                                   primitive_restart_index);
      break;
    case GL_UNSIGNED_SHORT:
      // Check we are not accessing an odd byte for a 2 byte value.
      if ((offset & 1) != 0) {
        return false;
      }
      max_v = GetMaxValue<uint16_t>(shadow_.data(), offset, count,
                                    primitive_restart_index);
      break;
    case GL_UNSIGNED_INT:
      // Check we are not accessing a non aligned address for a 4 byte value.
      if ((offset & 3) != 0) {
        return false;
      }
      max_v = GetMaxValue<uint32_t>(shadow_.data(), offset, count,
                                    primitive_restart_index);
      break;
    default:
      NOTREACHED_IN_MIGRATION();  // should never get here by validation.
      break;
  }
  range_set_.insert(std::make_pair(range, max_v));
  *max_value = max_v;
  return true;
}

void Buffer::SetMappedRange(GLintptr offset, GLsizeiptr size, GLenum access,
                            void* pointer, scoped_refptr<gpu::Buffer> shm,
                            unsigned int shm_offset) {
  mapped_range_ = std::make_unique<MappedRange>(offset, size, access, pointer,
                                                shm, shm_offset);
}

void Buffer::RemoveMappedRange() {
  mapped_range_.reset(nullptr);
}

void Buffer::SetReadbackShadowAllocation(scoped_refptr<gpu::Buffer> shm,
                                         uint32_t shm_offset) {
  DCHECK(shm);
  readback_shm_ = std::move(shm);
  readback_shm_offset_ = shm_offset;
}

scoped_refptr<gpu::Buffer> Buffer::TakeReadbackShadowAllocation(void** data) {
  DCHECK(readback_shm_);
  *data = readback_shm_->GetDataAddress(readback_shm_offset_, size_);
  readback_shm_offset_ = 0;
  return std::move(readback_shm_);
}

bool BufferManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (BufferMap::const_iterator it = buffers_.begin();
       it != buffers_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

bool BufferManager::IsUsageClientSideArray(GLenum usage) {
  return usage == GL_STREAM_DRAW && use_client_side_arrays_for_stream_buffers_;
}

bool BufferManager::UseNonZeroSizeForClientSideArrayBuffer() {
  return feature_info_.get() &&
         feature_info_->workarounds()
             .use_non_zero_size_for_client_side_stream_buffers;
}

bool BufferManager::UseShadowBuffer(GLenum target, GLenum usage) {
  const bool is_client_side_array = IsUsageClientSideArray(usage);

  // TODO(zmo): Don't shadow buffer data on ES3. crbug.com/491002.
  return (target == GL_ELEMENT_ARRAY_BUFFER ||
          allow_buffers_on_multiple_targets_ || is_client_side_array);
}

void BufferManager::SetInfo(Buffer* buffer,
                            GLenum target,
                            GLsizeiptr size,
                            GLenum usage,
                            bool use_shadow) {
  DCHECK(buffer);
  memory_type_tracker_->TrackMemFree(buffer->size());
  buffer->SetInfo(size, usage, use_shadow, IsUsageClientSideArray(usage));
  memory_type_tracker_->TrackMemAlloc(buffer->size());
}

void BufferManager::ValidateAndDoBufferData(ContextState* context_state,
                                            ErrorState* error_state,
                                            GLenum target,
                                            GLsizeiptr size,
                                            const GLvoid* data,
                                            GLenum usage) {
  if (!feature_info_->validators()->buffer_target.IsValid(target)) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
        error_state, "glBufferData", target, "target");
    return;
  }
  if (!feature_info_->validators()->buffer_usage.IsValid(usage)) {
    ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(
        error_state, "glBufferData", usage, "usage");
    return;
  }
  if (size < 0) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_VALUE, "glBufferData", "size < 0");
    return;
  }

  if (size > max_buffer_size_) {
    ERRORSTATE_SET_GL_ERROR(error_state, GL_OUT_OF_MEMORY, "glBufferData",
                            "cannot allocate more than 1GB.");
    return;
  }

  Buffer* buffer = GetBufferInfoForTarget(context_state, target);
  if (!buffer) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_VALUE, "glBufferData", "unknown buffer");
    return;
  }

  if (buffer->IsBoundForTransformFeedbackAndOther()) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, "glBufferData",
        "buffer is bound for transform feedback and other use simultaneously");
    return;
  }

  DoBufferData(error_state, buffer, target, size, usage, data);

  if (context_state->bound_transform_feedback.get()) {
    // buffer size might have changed, and on Desktop GL lower than 4.2,
    // we might need to reset transform feedback buffer range.
    context_state->bound_transform_feedback->OnBufferData(buffer);
  }
}

void BufferManager::DoBufferData(
    ErrorState* error_state,
    Buffer* buffer,
    GLenum target,
    GLsizeiptr size,
    GLenum usage,
    const GLvoid* data) {
  // Stage the shadow buffer first if we are using a shadow buffer so that we
  // validate what we store internally.
  const bool use_shadow = UseShadowBuffer(buffer->initial_target(), usage);
  data = buffer->StageShadow(use_shadow, size, data);

  ERRORSTATE_COPY_REAL_GL_ERRORS_TO_WRAPPER(error_state, "glBufferData");
  if (IsUsageClientSideArray(usage)) {
    GLsizei empty_size = UseNonZeroSizeForClientSideArrayBuffer() ? 1 : 0;
    glBufferData(target, empty_size, nullptr, usage);
  } else {
    if (data || !size) {
      glBufferData(target, size, data, usage);
    } else {
      auto zero = base::HeapArray<char>::WithSize(size);
      glBufferData(target, size, zero.data(), usage);
    }
  }
  GLenum error = ERRORSTATE_PEEK_GL_ERROR(error_state, "glBufferData");
  if (error != GL_NO_ERROR) {
    DCHECK_EQ(static_cast<GLenum>(GL_OUT_OF_MEMORY), error);
    size = 0;
    // TODO(zmo): This doesn't seem correct. There might be shadow data from
    // a previous successful BufferData() call.
    buffer->StageShadow(false, 0, nullptr);  // Also clear the shadow.
    return;
  }

  SetInfo(buffer, target, size, usage, use_shadow);
}

void BufferManager::ValidateAndDoBufferSubData(ContextState* context_state,
                                               ErrorState* error_state,
                                               GLenum target,
                                               GLintptr offset,
                                               GLsizeiptr size,
                                               const GLvoid* data) {
  Buffer* buffer = RequestBufferAccess(context_state, error_state, target,
                                       offset, size, "glBufferSubData");
  if (!buffer) {
    return;
  }
  DoBufferSubData(buffer, target, offset, size, data);
}

void BufferManager::DoBufferSubData(
    Buffer* buffer, GLenum target, GLintptr offset, GLsizeiptr size,
    const GLvoid* data) {
  buffer->SetRange(offset, size, data);

  if (!buffer->IsClientSideArray()) {
    glBufferSubData(target, offset, size, data);
  }
}

void BufferManager::ValidateAndDoCopyBufferSubData(ContextState* context_state,
                                                   ErrorState* error_state,
                                                   GLenum readtarget,
                                                   GLenum writetarget,
                                                   GLintptr readoffset,
                                                   GLintptr writeoffset,
                                                   GLsizeiptr size) {
  const char* func_name = "glCopyBufferSubData";
  Buffer* readbuffer = RequestBufferAccess(
      context_state, error_state, readtarget, readoffset, size, func_name);
  if (!readbuffer)
    return;
  Buffer* writebuffer = RequestBufferAccess(
      context_state, error_state, writetarget, writeoffset, size, func_name);
  if (!writebuffer)
    return;

  if (readbuffer == writebuffer &&
      ((writeoffset >= readoffset && writeoffset < readoffset + size) ||
       (readoffset >= writeoffset && readoffset < writeoffset + size))) {
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_VALUE, func_name,
        "read/write ranges overlap");
    return;
  }

  if (!allow_buffers_on_multiple_targets_) {
    if ((readbuffer->initial_target() == GL_ELEMENT_ARRAY_BUFFER &&
         writebuffer->initial_target() != GL_ELEMENT_ARRAY_BUFFER) ||
        (writebuffer->initial_target() == GL_ELEMENT_ARRAY_BUFFER &&
         readbuffer->initial_target() != GL_ELEMENT_ARRAY_BUFFER)) {
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, func_name,
          "copying between ELEMENT_ARRAY_BUFFER and another buffer type");
      return;
    }
  }

  DoCopyBufferSubData(readbuffer, readtarget, readoffset,
                      writebuffer, writetarget, writeoffset, size);
}

void BufferManager::DoCopyBufferSubData(
    Buffer* readbuffer,
    GLenum readtarget,
    GLintptr readoffset,
    Buffer* writebuffer,
    GLenum writetarget,
    GLintptr writeoffset,
    GLsizeiptr size) {
  DCHECK(readbuffer);
  DCHECK(writebuffer);
  if (writebuffer->shadowed()) {
    const void* data = readbuffer->GetRange(readoffset, size);
    DCHECK(data);
    writebuffer->SetRange(writeoffset, size, data);
  }

  glCopyBufferSubData(readtarget, writetarget, readoffset, writeoffset, size);
}

void BufferManager::ValidateAndDoGetBufferParameteri64v(
    ContextState* context_state,
    ErrorState* error_state,
    GLenum target,
    GLenum pname,
    GLint64* params) {
  Buffer* buffer = GetBufferInfoForTarget(context_state, target);
  if (!buffer) {
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION,
                            "glGetBufferParameteri64v",
                            "no buffer bound for target");
    return;
  }
  switch (pname) {
    case GL_BUFFER_SIZE:
      *params = buffer->size();
      break;
    case GL_BUFFER_MAP_LENGTH:
      {
        const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
        *params = mapped_range ? mapped_range->size : 0;
        break;
      }
    case GL_BUFFER_MAP_OFFSET:
      {
        const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
        *params = mapped_range ? mapped_range->offset : 0;
        break;
      }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void BufferManager::ValidateAndDoGetBufferParameteriv(
    ContextState* context_state,
    ErrorState* error_state,
    GLenum target,
    GLenum pname,
    GLint* params) {
  Buffer* buffer = GetBufferInfoForTarget(context_state, target);
  if (!buffer) {
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION,
                            "glGetBufferParameteriv",
                            "no buffer bound for target");
    return;
  }
  switch (pname) {
    case GL_BUFFER_SIZE:
      *params = buffer->size();
      break;
    case GL_BUFFER_USAGE:
      *params = buffer->usage();
      break;
    case GL_BUFFER_ACCESS_FLAGS:
      {
        const Buffer::MappedRange* mapped_range = buffer->GetMappedRange();
        *params = mapped_range ? mapped_range->access : 0;
        break;
      }
    case GL_BUFFER_MAPPED:
      *params = buffer->GetMappedRange() == nullptr ? false : true;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool BufferManager::SetTarget(Buffer* buffer, GLenum target) {
  if (!allow_buffers_on_multiple_targets_) {
    // After being bound to ELEMENT_ARRAY_BUFFER target, a buffer cannot be
    // bound to any other targets except for COPY_READ/WRITE_BUFFER target;
    // After being bound to non ELEMENT_ARRAY_BUFFER target, a buffer cannot
    // be bound to ELEMENT_ARRAY_BUFFER target.

    switch (buffer->initial_target()) {
      case GL_ELEMENT_ARRAY_BUFFER:
        switch (target) {
          case GL_ARRAY_BUFFER:
          case GL_PIXEL_PACK_BUFFER:
          case GL_PIXEL_UNPACK_BUFFER:
          case GL_TRANSFORM_FEEDBACK_BUFFER:
          case GL_UNIFORM_BUFFER:
            return false;
          default:
            break;
        }
        break;
      case GL_ARRAY_BUFFER:
      case GL_COPY_READ_BUFFER:
      case GL_COPY_WRITE_BUFFER:
      case GL_PIXEL_PACK_BUFFER:
      case GL_PIXEL_UNPACK_BUFFER:
      case GL_TRANSFORM_FEEDBACK_BUFFER:
      case GL_UNIFORM_BUFFER:
        if (target == GL_ELEMENT_ARRAY_BUFFER) {
          return false;
        }
        break;
      default:
        break;
    }
  }
  if (buffer->initial_target() == 0)
    buffer->set_initial_target(target);
  return true;
}

// Since one BufferManager can be shared by multiple decoders, ContextState is
// passed in each time and not just passed in during initialization.
Buffer* BufferManager::GetBufferInfoForTarget(
    ContextState* state, GLenum target) const {
  switch (target) {
    case GL_ARRAY_BUFFER:
      return state->bound_array_buffer.get();
    case GL_ELEMENT_ARRAY_BUFFER:
      return state->vertex_attrib_manager->element_array_buffer();
    case GL_COPY_READ_BUFFER:
      return state->bound_copy_read_buffer.get();
    case GL_COPY_WRITE_BUFFER:
      return state->bound_copy_write_buffer.get();
    case GL_PIXEL_PACK_BUFFER:
      return state->bound_pixel_pack_buffer.get();
    case GL_PIXEL_UNPACK_BUFFER:
      return state->bound_pixel_unpack_buffer.get();
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      return state->bound_transform_feedback_buffer.get();
    case GL_UNIFORM_BUFFER:
      return state->bound_uniform_buffer.get();
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void BufferManager::SetPrimitiveRestartFixedIndexIfNecessary(GLenum type) {
  GLuint index = 0;
  switch (type) {
    case GL_UNSIGNED_BYTE:
      index = 0xFF;
      break;
    case GL_UNSIGNED_SHORT:
      index = 0xFFFF;
      break;
    case GL_UNSIGNED_INT:
      index = 0xFFFFFFFF;
      break;
    default:
      NOTREACHED_IN_MIGRATION();  // should never get here by validation.
      break;
  }
  if (primitive_restart_fixed_index_ != index) {
    glPrimitiveRestartIndex(index);
    primitive_restart_fixed_index_ = index;
  }
}

bool BufferManager::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryDumpLevelOfDetail;

  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    std::string dump_name =
        base::StringPrintf("gpu/gl/buffers/context_group_0x%" PRIX64 "",
                           memory_tracker_->ContextGroupTracingId());
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, mem_represented());

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& buffer_entry : buffers_) {
    const auto& client_buffer_id = buffer_entry.first;
    const auto& buffer = buffer_entry.second;

    std::string dump_name = base::StringPrintf(
        "gpu/gl/buffers/context_group_0x%" PRIX64 "/buffer_0x%" PRIX32,
        memory_tracker_->ContextGroupTracingId(), client_buffer_id);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    static_cast<uint64_t>(buffer->size()));

    auto* mapped_range = buffer->GetMappedRange();
    if (!mapped_range)
      continue;
    auto shared_memory_guid = mapped_range->shm->backing()->GetGUID();
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           0 /* importance */);
    } else {
      auto guid = gl::GetGLBufferGUIDForTracing(
          memory_tracker_->ContextGroupTracingId(), client_buffer_id);
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid);
    }
  }

  return true;
}

Buffer* BufferManager::RequestBufferAccess(ContextState* context_state,
                                           ErrorState* error_state,
                                           GLenum target,
                                           GLintptr offset,
                                           GLsizeiptr size,
                                           const char* func_name) {
  DCHECK(context_state);
  Buffer* buffer = GetBufferInfoForTarget(context_state, target);
  if (!RequestBufferAccess(error_state, buffer, func_name,
                           "bound to target 0x%04x", target)) {
    return nullptr;
  }
  if (!buffer->CheckRange(offset, size)) {
    std::string msg = base::StringPrintf(
        "bound to target 0x%04x : offset/size out of range", target);
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_VALUE, func_name,
                            msg.c_str());
    return nullptr;
  }
  return buffer;
}

Buffer* BufferManager::RequestBufferAccess(ContextState* context_state,
                                           ErrorState* error_state,
                                           GLenum target,
                                           const char* func_name) {
  DCHECK(context_state);
  Buffer* buffer = GetBufferInfoForTarget(context_state, target);
  return RequestBufferAccess(error_state, buffer, func_name,
                             "bound to target 0x%04x", target)
             ? buffer
             : nullptr;
}

bool BufferManager::RequestBufferAccess(ErrorState* error_state,
                                        Buffer* buffer,
                                        const char* func_name,
                                        const char* error_message_format,
                                        ...) {
  DCHECK(error_state);

  va_list varargs;
  va_start(varargs, error_message_format);
  bool result = RequestBufferAccessV(error_state, buffer, func_name,
                                     error_message_format, varargs);
  va_end(varargs);
  return result;
}

bool BufferManager::RequestBufferAccess(ErrorState* error_state,
                                        Buffer* buffer,
                                        GLintptr offset,
                                        GLsizeiptr size,
                                        const char* func_name,
                                        const char* error_message) {
  if (!RequestBufferAccess(error_state, buffer, func_name, error_message)) {
    return false;
  }
  if (!buffer->CheckRange(offset, size)) {
    std::string msg = base::StringPrintf(
        "%s : offset/size out of range", error_message);
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, func_name, msg.c_str());
    return false;
  }
  return true;
}

bool BufferManager::RequestBuffersAccess(
    ErrorState* error_state,
    const IndexedBufferBindingHost* bindings,
    const std::vector<GLsizeiptr>& variable_sizes,
    GLsizei count,
    const char* func_name,
    const char* message_tag) {
  DCHECK(error_state);
  DCHECK(bindings);

  for (size_t ii = 0; ii < variable_sizes.size(); ++ii) {
    if (variable_sizes[ii] == 0)
      continue;
    Buffer* buffer = bindings->GetBufferBinding(ii);
    if (!buffer) {
      std::string msg = base::StringPrintf(
          "%s : no buffer bound at index %zu", message_tag, ii);
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, func_name, msg.c_str());
      return false;
    }
    if (buffer->GetMappedRange()) {
      std::string msg = base::StringPrintf(
          "%s : buffer is mapped at index %zu", message_tag, ii);
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, func_name, msg.c_str());
      return false;
    }
    if (buffer->IsBoundForTransformFeedbackAndOther()) {
      std::string msg = base::StringPrintf(
          "%s : buffer at index %zu is bound for transform feedback and other "
          "use simultaneously",
          message_tag, ii);
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, func_name,
                              msg.c_str());
      return false;
    }
    GLsizeiptr size = bindings->GetEffectiveBufferSize(ii);
    GLsizeiptr required_size;
    if (!base::CheckMul(variable_sizes[ii], count)
             .AssignIfValid(&required_size) ||
        size < required_size) {
      std::string msg = base::StringPrintf(
          "%s : buffer or buffer range at index %zu not large enough",
          message_tag, ii);
      ERRORSTATE_SET_GL_ERROR(
          error_state, GL_INVALID_OPERATION, func_name, msg.c_str());
      return false;
    }
  }
  return true;
}

bool BufferManager::RequestBufferAccessV(ErrorState* error_state,
                                         Buffer* buffer,
                                         const char* func_name,
                                         const char* error_message_format,
                                         va_list varargs) {
  DCHECK(error_state);

  if (!buffer || buffer->IsDeleted()) {
    std::string message_tag = base::StringPrintV(error_message_format, varargs);
    std::string msg = base::StringPrintf("%s : no buffer", message_tag.c_str());
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, func_name,
                            msg.c_str());
    return false;
  }
  if (buffer->GetMappedRange()) {
    std::string message_tag = base::StringPrintV(error_message_format, varargs);
    std::string msg = base::StringPrintf("%s : buffer is mapped",
                                         message_tag.c_str());
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, func_name,
                            msg.c_str());
    return false;
  }
  if (buffer->IsBoundForTransformFeedbackAndOther()) {
    std::string message_tag = base::StringPrintV(error_message_format, varargs);
    std::string msg = base::StringPrintf(
        "%s : buffer is bound for transform feedback and other use "
        "simultaneously",
        message_tag.c_str());
    ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_OPERATION, func_name,
                            msg.c_str());
    return false;
  }
  return true;
}

}  // namespace gles2
}  // namespace gpu
