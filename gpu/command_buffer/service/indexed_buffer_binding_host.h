// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_INDEXED_BUFFER_BINDING_HOST_H_
#define GPU_COMMAND_BUFFER_SERVICE_INDEXED_BUFFER_BINDING_HOST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class Buffer;

// This is a base class for indexed buffer bindings tracking.
// TransformFeedback and Program should inherit from this base class,
// for tracking indexed TRANSFORM_FEEDBACK_BUFFER / UNIFORM_BUFFER bindings.
class GPU_GLES2_EXPORT IndexedBufferBindingHost
    : public base::RefCounted<IndexedBufferBindingHost> {
 public:
  // In theory |needs_emulation| needs to be true on Desktop GL 4.1 or lower.
  // However, we set it to true everywhere, not to trust drivers to handle
  // out-of-bounds buffer accesses.
  IndexedBufferBindingHost(uint32_t max_bindings,
                           GLenum target,
                           bool needs_emulation,
                           bool round_down_uniform_bind_buffer_range_size);

  // The following two functions do state update and call the underlying GL
  // function.  All validations have been done already and the GL function is
  // guaranteed to succeed.
  void DoBindBufferBase(GLuint index, Buffer* buffer);
  void DoBindBufferRange(GLuint index,
                         Buffer* buffer,
                         GLintptr offset,
                         GLsizeiptr size);

  // This is called on the active host when glBufferData is called and buffer
  // size might change.
  void OnBufferData(Buffer* buffer);

  void RemoveBoundBuffer(GLenum target,
                         Buffer* buffer,
                         Buffer* target_generic_bound_buffer,
                         bool have_context);

  void SetIsBound(bool bound);

  Buffer* GetBufferBinding(GLuint index) const;
  // Returns |size| set by glBindBufferRange; 0 if set by glBindBufferBase.
  GLsizeiptr GetBufferSize(GLuint index) const;
  // For glBindBufferBase, return the actual buffer size when this function is
  // called, not when glBindBufferBase is called.
  // For glBindBufferRange, return the |size| set by glBindBufferRange minus
  // the range that's beyond the buffer.
  GLsizeiptr GetEffectiveBufferSize(GLuint index) const;
  GLintptr GetBufferStart(GLuint index) const;

  // This is used only for UNIFORM_BUFFER bindings in context switching.
  void RestoreBindings(IndexedBufferBindingHost* prev);

  // Check if |buffer| is currently bound to one of the indexed binding point
  // from 0 to |used_binding_count| - 1.
  bool UsesBuffer(size_t used_binding_count, const Buffer* buffer) const;

 protected:
  friend class base::RefCounted<IndexedBufferBindingHost>;

  virtual ~IndexedBufferBindingHost();

  // Whether this object is currently bound into the context.
  bool is_bound_;

  // Whether or not to call Buffer::OnBind/OnUnbind whenever bindings change.
  // This is only necessary for WebGL contexts to implement
  // https://crbug.com/696345
  bool do_buffer_refcounting_;

 private:
  enum class IndexedBufferBindingType {
    kBindBufferBase,
    kBindBufferRange,
    kBindBufferNone
  };

  struct IndexedBufferBinding {
    IndexedBufferBindingType type;
    scoped_refptr<Buffer> buffer;

    // The following fields are only used if |type| is kBindBufferRange.
    GLintptr offset;
    GLsizeiptr size;
    // The full buffer size at the last successful glBindBufferRange call.
    GLsizeiptr effective_full_buffer_size;

    IndexedBufferBinding();
    IndexedBufferBinding(const IndexedBufferBinding& other);
    ~IndexedBufferBinding();

    bool operator==(const IndexedBufferBinding& other) const;

    void SetBindBufferBase(Buffer* _buffer);
    void SetBindBufferRange(
        Buffer* _buffer, GLintptr _offset, GLsizeiptr _size);
    void Reset();
  };

  // This is called when |needs_emulation_| is true, where the range
  // (offset + size) can't go beyond the buffer's size.
  static void DoAdjustedBindBufferRange(
      GLenum target,
      GLuint index,
      GLuint service_id,
      GLintptr offset,
      GLsizeiptr size,
      GLsizeiptr full_buffer_size,
      bool round_down_uniform_bind_buffer_range_size);

  void UpdateMaxNonNullBindingIndex(size_t changed_index);

  std::vector<IndexedBufferBinding> buffer_bindings_;

  bool needs_emulation_;
  bool round_down_uniform_bind_buffer_range_size_;

  // This is used for optimization purpose in context switching.
  size_t max_non_null_binding_index_plus_one_;

  // The GL binding point that this host manages
  // (e.g. GL_TRANSFORM_FEEDBACK_BUFFER).
  GLenum target_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_INDEXED_BUFFER_BINDING_HOST_H_
