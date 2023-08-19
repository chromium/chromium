// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class BufferManager;
struct ContextState;
class ErrorState;
class FeatureInfo;
class IndexedBufferBindingHost;
class TestHelper;

// Info about Buffers currently in the system.
class GPU_GLES2_EXPORT Buffer : public base::RefCounted<Buffer> {
 public:
  struct MappedRange {
    GLintptr offset;
    GLsizeiptr size;
    GLenum access;
    raw_ptr<void, DanglingUntriaged> pointer;  // Pointer returned by driver.
    scoped_refptr<gpu::Buffer> shm;  // Client side mem buffer.
    unsigned int shm_offset;  // Client side mem buffer offset.

    MappedRange(GLintptr offset, GLsizeiptr size, GLenum access, void* pointer,
                scoped_refptr<gpu::Buffer> shm, unsigned int shm_offset);
    ~MappedRange();
    void* GetShmPointer() const;
  };

  Buffer(BufferManager* manager, GLuint service_id);

  GLenum initial_target() const { return initial_target_; }

  GLuint service_id() const {
    return service_id_;
  }

  GLsizeiptr size() const {
    return size_;
  }

  GLenum usage() const {
    return usage_;
  }

  bool shadowed() const {
    return !shadow_.empty();
  }

  // Gets the maximum value in the buffer for the given range interpreted as
  // the given type. Returns false if offset and count are out of range.
  // offset is in bytes.
  // count is in elements of type.
  bool GetMaxValueForRange(GLuint offset, GLsizei count, GLenum type,
                           bool primitive_restart_enabled, GLuint* max_value);

  // Returns a pointer to shadowed data.
  const void* GetRange(GLintptr offset, GLsizeiptr size) const;

  // Check if an offset, size range is valid for the current buffer.
  bool CheckRange(GLintptr offset, GLsizeiptr size) const;

  // Sets a range of this buffer's shadowed data.
  void SetRange(GLintptr offset, GLsizeiptr size, const GLvoid * data);

  bool IsDeleted() const {
    return deleted_;
  }

  bool IsValid() const {
    return initial_target() && !IsDeleted();
  }

  bool IsClientSideArray() const {
    return is_client_side_array_;
  }

  void SetMappedRange(GLintptr offset, GLsizeiptr size, GLenum access,
                      void* pointer, scoped_refptr<gpu::Buffer> shm,
                      unsigned int shm_offset);
  void RemoveMappedRange();
  const MappedRange* GetMappedRange() const {
    return mapped_range_.get();
  }

  // These maintain the reference counts for checking whether a buffer is
  // double-bound to transform feedback and non-transform-feedback binding
  // points.
  void OnBind(GLenum target, bool indexed);
  void OnUnbind(GLenum target, bool indexed);

  bool IsBoundForTransformFeedbackAndOther() const {
    return transform_feedback_indexed_binding_count_ > 0 &&
           non_transform_feedback_binding_count_ > 0;
  }

  bool IsDoubleBoundForTransformFeedback() const {
    return transform_feedback_indexed_binding_count_ > 1;
  }

  void SetReadbackShadowAllocation(scoped_refptr<gpu::Buffer> shm,
                                   uint32_t shm_offset);
  scoped_refptr<gpu::Buffer> TakeReadbackShadowAllocation(void** data);

 private:
  friend class BufferManager;
  friend class BufferManagerTestBase;
  friend class base::RefCounted<Buffer>;

  // Represents a range in a buffer.
  class Range {
   public:
    Range(GLuint offset, GLsizei count, GLenum type,
          bool primitive_restart_enabled)
        : offset_(offset),
          count_(count),
          type_(type),
          primitive_restart_enabled_(primitive_restart_enabled) {
    }

    // A less functor provided for std::map so it can find ranges.
    struct Less {
      bool operator() (const Range& lhs, const Range& rhs) const {
        if (lhs.offset_ != rhs.offset_) {
          return lhs.offset_ < rhs.offset_;
        }
        if (lhs.count_ != rhs.count_) {
          return lhs.count_ < rhs.count_;
        }
        if (lhs.type_ != rhs.type_) {
          return lhs.type_ < rhs.type_;
        }
        return lhs.primitive_restart_enabled_ < rhs.primitive_restart_enabled_;
      }
    };

   private:
    GLuint offset_;
    GLsizei count_;
    GLenum type_;
    bool primitive_restart_enabled_;
  };

  ~Buffer();

  void set_initial_target(GLenum target) {
    DCHECK_EQ(0u, initial_target_);
    initial_target_ = target;
  }

  void MarkAsDeleted() {
    deleted_ = true;
  }

  // Setup the shadow buffer. This will either initialize the shadow buffer
  // with the passed data or clear the shadow buffer if no shadow required. This
  // will return a pointer to the shadowed data if using shadow, otherwise will
  // return the original data pointer.
  const GLvoid* StageShadow(bool use_shadow,
                            GLsizeiptr size,
                            const GLvoid* data);

  // Sets the size, usage and initial data of a buffer.
  // If shadow is true then if data is NULL buffer will be initialized to 0.
  void SetInfo(GLsizeiptr size,
               GLenum usage,
               bool use_shadow,
               bool is_client_side_array);

  // Clears any cache of index ranges.
  void ClearCache();

  // The manager that owns this Buffer.
  raw_ptr<BufferManager> manager_;

  // A copy of the data in the buffer. This data is only kept if the conditions
  // checked in UseShadowBuffer() are true.
  std::vector<uint8_t> shadow_;

  // Size of buffer.
  GLsizeiptr size_;

  // True if deleted.
  bool deleted_;

  // Whether or not this Buffer is not uploaded to the GPU but just
  // sitting in local memory.
  bool is_client_side_array_;

  // Keeps track of whether this buffer is currently bound for transform
  // feedback in a WebGL context. Used as an optimization when validating WebGL
  // draw calls for compliance with binding restrictions.
  // http://crbug.com/696345
  int non_transform_feedback_binding_count_ = 0;
  int transform_feedback_indexed_binding_count_ = 0;

  // Service side buffer id.
  GLuint service_id_;

  // The first target of buffer. 0 = unset.
  // It is set the first time bindBuffer() is called and cannot be changed.
  GLenum initial_target_;

  // Usage of buffer.
  GLenum usage_;

  // Data cached from last glMapBufferRange call.
  std::unique_ptr<MappedRange> mapped_range_;

  // A map of ranges to the highest value in that range of a certain type.
  typedef std::map<Range, GLuint, Range::Less> RangeToMaxValueMap;
  RangeToMaxValueMap range_set_;

  scoped_refptr<gpu::Buffer> readback_shm_;
  uint32_t readback_shm_offset_ = 0;
};

// This class keeps track of the buffers and their sizes so we can do
// bounds checking.
//
// NOTE: To support shared resources an instance of this class will need to be
// shared by multiple GLES2Decoders.
class GPU_GLES2_EXPORT BufferManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  BufferManager(MemoryTracker* memory_tracker, FeatureInfo* feature_info);

  BufferManager(const BufferManager&) = delete;
  BufferManager& operator=(const BufferManager&) = delete;

  ~BufferManager() override;

  void MarkContextLost();

  // Must call before destruction.
  void Destroy();

  // Creates a Buffer for the given buffer.
  void CreateBuffer(GLuint client_id, GLuint service_id);

  // Gets the buffer info for the given buffer.
  Buffer* GetBuffer(GLuint client_id);

  // Removes a buffer info for the given buffer.
  void RemoveBuffer(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  // Validates a glBufferSubData, and then calls DoBufferData if validation was
  // successful.
  void ValidateAndDoBufferSubData(ContextState* context_state,
                                  ErrorState* error_state,
                                  GLenum target,
                                  GLintptr offset,
                                  GLsizeiptr size,
                                  const GLvoid* data);

  // Validates a glBufferData, and then calls DoBufferData if validation was
  // successful.
  void ValidateAndDoBufferData(ContextState* context_state,
                               ErrorState* error_state,
                               GLenum target,
                               GLsizeiptr size,
                               const GLvoid* data,
                               GLenum usage);

  // Validates a glCopyBufferSubData, and then calls DoCopyBufferSubData if
  // validation was successful.
  void ValidateAndDoCopyBufferSubData(ContextState* context_state,
                                      ErrorState* error_state,
                                      GLenum readtarget,
                                      GLenum writetarget,
                                      GLintptr readoffset,
                                      GLintptr writeoffset,
                                      GLsizeiptr size);

  // Validates a glGetBufferParameteri64v, and then calls GetBufferParameteri64v
  // if validation was successful.
  void ValidateAndDoGetBufferParameteri64v(ContextState* context_state,
                                           ErrorState* error_state,
                                           GLenum target,
                                           GLenum pname,
                                           GLint64* params);

  // Validates a glGetBufferParameteriv, and then calls GetBufferParameteriv if
  // validation was successful.
  void ValidateAndDoGetBufferParameteriv(ContextState* context_state,
                                         ErrorState* error_state,
                                         GLenum target,
                                         GLenum pname,
                                         GLint* params);

  // Sets the target of a buffer. Returns false if the target can not be set.
  bool SetTarget(Buffer* buffer, GLenum target);

  void set_max_buffer_size(GLsizeiptr max_buffer_size) {
    max_buffer_size_ = max_buffer_size;
  }

  void set_allow_buffers_on_multiple_targets(bool allow) {
    allow_buffers_on_multiple_targets_ = allow;
  }

  void set_allow_fixed_attribs(bool allow) {
    allow_fixed_attribs_ = allow;
  }

  size_t mem_represented() const {
    return memory_type_tracker_->GetMemRepresented();
  }

  // Tells for a given usage if this would be a client side array.
  bool IsUsageClientSideArray(GLenum usage);

  // Tells whether a buffer that is emulated using client-side arrays should be
  // set to a non-zero size.
  bool UseNonZeroSizeForClientSideArrayBuffer();

  void SetPrimitiveRestartFixedIndexIfNecessary(GLenum type);

  Buffer* GetBufferInfoForTarget(ContextState* state, GLenum target) const;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Validate if a buffer is bound at target, if it's unmapped, if it's
  // large enough. Return the buffer bound to |target| if access is granted;
  // return nullptr if a GL error is generated.
  // Generates INVALID_VALUE if offset + size is out of range.
  Buffer* RequestBufferAccess(ContextState* context_state,
                              ErrorState* error_state,
                              GLenum target,
                              GLintptr offset,
                              GLsizeiptr size,
                              const char* func_name);
  // Same as above, but assume to access the entire buffer.
  Buffer* RequestBufferAccess(ContextState* context_state,
                              ErrorState* error_state,
                              GLenum target,
                              const char* func_name);
  // Same as above, but it can be any buffer rather than the buffer bound to
  // |target|. Return true if access is granted; return false if a GL error is
  // generated.
  bool RequestBufferAccess(ErrorState* error_state,
                           Buffer* buffer,
                           const char* func_name,
                           const char* error_message_format,
                           ...);
  // Generates INVALID_OPERATION if offset + size is out of range.
  bool RequestBufferAccess(ErrorState* error_state,
                           Buffer* buffer,
                           GLintptr offset,
                           GLsizeiptr size,
                           const char* func_name,
                           const char* error_message);
  // Returns false and generates INVALID_OPERATION if buffer at binding |ii|
  // doesn't exist, is mapped, or smaller than |variable_sizes[ii]| * |count|.
  // Return true otherwise.
  bool RequestBuffersAccess(ErrorState* error_state,
                            const IndexedBufferBindingHost* bindings,
                            const std::vector<GLsizeiptr>& variable_sizes,
                            GLsizei count,
                            const char* func_name,
                            const char* message_tag);

 private:
  friend class Buffer;
  friend class TestHelper;  // Needs access to DoBufferData.
  friend class BufferManagerTestBase;  // Needs access to DoBufferSubData.
  friend class IndexedBufferBindingHostTest;  // Needs access to SetInfo.

  void StartTracking(Buffer* buffer);
  void StopTracking(Buffer* buffer);

  // Does a glBufferSubData and updates the appropriate accounting.
  // Assumes the values have already been validated.
  void DoBufferSubData(
      Buffer* buffer,
      GLenum target,
      GLintptr offset,
      GLsizeiptr size,
      const GLvoid* data);

  // Does a glBufferData and updates the appropriate accounting.
  // Assumes the values have already been validated.
  void DoBufferData(
      ErrorState* error_state,
      Buffer* buffer,
      GLenum target,
      GLsizeiptr size,
      GLenum usage,
      const GLvoid* data);

  // Does a glCopyBufferSubData and updates the appropriate accounting.
  // Assumes the values have already been validated.
  void DoCopyBufferSubData(
      Buffer* readbuffer,
      GLenum readtarget,
      GLintptr readoffset,
      Buffer* writebuffer,
      GLenum writetarget,
      GLintptr writeoffset,
      GLsizeiptr size);

  // Tests whether a shadow buffer needs to be used.
  bool UseShadowBuffer(GLenum target, GLenum usage);

  // Sets the size, usage and initial data of a buffer.
  // If data is NULL buffer will be initialized to 0 if shadowed.
  void SetInfo(Buffer* buffer,
               GLenum target,
               GLsizeiptr size,
               GLenum usage,
               bool use_shadow);

  // Same as public RequestBufferAccess taking similar arguments, but
  // allows caller to assemble the va_list.
  bool RequestBufferAccessV(ErrorState* error_state,
                            Buffer* buffer,
                            const char* func_name,
                            const char* error_message_format,
                            va_list varargs);

  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  raw_ptr<MemoryTracker> memory_tracker_;
  scoped_refptr<FeatureInfo> feature_info_;

  // Info for each buffer in the system.
  typedef std::unordered_map<GLuint, scoped_refptr<Buffer>> BufferMap;
  BufferMap buffers_;

  // The maximum size of buffers.
  GLsizeiptr max_buffer_size_;

  // Whether or not buffers can be bound to multiple targets.
  bool allow_buffers_on_multiple_targets_;

  // Whether or not allow using GL_FIXED type for vertex attribs.
  bool allow_fixed_attribs_;

  // Counts the number of Buffer allocated with 'this' as its manager.
  // Allows to check no Buffer will outlive this.
  unsigned int buffer_count_;

  GLuint primitive_restart_fixed_index_;

  bool lost_context_;
  bool use_client_side_arrays_for_stream_buffers_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_
