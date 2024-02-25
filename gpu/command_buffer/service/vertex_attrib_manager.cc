// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/vertex_attrib_manager.h"

#include <stdint.h>

#include <list>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/vertex_array_manager.h"

namespace gpu {
namespace gles2 {

VertexAttrib::VertexAttrib()
    : index_(0),
      enabled_(false),
      enabled_in_driver_(false),
      size_(4),
      type_(GL_FLOAT),
      offset_(0),
      normalized_(GL_FALSE),
      gl_stride_(0),
      real_stride_(16),
      divisor_(0),
      integer_(GL_FALSE),
      is_client_side_array_(false),
      list_(nullptr) {}

VertexAttrib::VertexAttrib(const VertexAttrib& other) = default;

VertexAttrib::~VertexAttrib() = default;

void VertexAttrib::SetInfo(
    Buffer* buffer,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei gl_stride,
    GLsizei real_stride,
    GLsizei offset,
    GLboolean integer) {
  DCHECK_GT(real_stride, 0);
  buffer_ = buffer;
  size_ = size;
  type_ = type;
  normalized_ = normalized;
  gl_stride_ = gl_stride;
  real_stride_ = real_stride;
  offset_ = offset;
  integer_ = integer;
}

bool VertexAttrib::CanAccess(GLuint index) const {
  if (!enabled_) {
    return true;
  }

  DCHECK(buffer_.get() && !buffer_->IsDeleted());
  // The number of elements that can be accessed.
  GLsizeiptr buffer_size = buffer_->size();
  if (offset_ > buffer_size || real_stride_ == 0) {
    return false;
  }

  uint32_t usable_size = buffer_size - offset_;
  GLuint num_elements = usable_size / real_stride_ +
      ((usable_size % real_stride_) >=
       (GLES2Util::GetGroupSizeForBufferType(size_, type_)) ? 1 : 0);
  return index < num_elements;
}

VertexAttribManager::VertexAttribManager(bool do_buffer_refcounting)
    : num_fixed_attribs_(0),
      element_array_buffer_(nullptr),
      manager_(nullptr),
      deleted_(false),
      is_bound_(false),
      do_buffer_refcounting_(do_buffer_refcounting),
      service_id_(0) {}

VertexAttribManager::VertexAttribManager(VertexArrayManager* manager,
                                         GLuint service_id,
                                         uint32_t num_vertex_attribs,
                                         bool do_buffer_refcounting)
    : num_fixed_attribs_(0),
      element_array_buffer_(nullptr),
      manager_(manager),
      deleted_(false),
      is_bound_(false),
      do_buffer_refcounting_(do_buffer_refcounting),
      service_id_(service_id) {
  manager_->StartTracking(this);
  Initialize(num_vertex_attribs);
}

VertexAttribManager::~VertexAttribManager() {
  if (manager_) {
    if (manager_->have_context_) {
      if (service_id_ != 0)  // 0 indicates an emulated VAO
        glDeleteVertexArraysOES(1, &service_id_);
    }
    manager_->StopTracking(this);
    manager_ = nullptr;
  }
}

void VertexAttribManager::Initialize(uint32_t max_vertex_attribs) {
  vertex_attribs_.resize(max_vertex_attribs);
  uint32_t packed_size = (max_vertex_attribs + 15) / 16;
  attrib_base_type_mask_.resize(packed_size);
  attrib_enabled_mask_.resize(packed_size);

  for (uint32_t ii = 0; ii < packed_size; ++ii) {
    attrib_enabled_mask_[ii] = 0u;
    attrib_base_type_mask_[ii] = 0u;
  }

  for (uint32_t vv = 0; vv < vertex_attribs_.size(); ++vv) {
    vertex_attribs_[vv].set_index(vv);
    vertex_attribs_[vv].SetList(&disabled_vertex_attribs_);
  }
}

void VertexAttribManager::SetElementArrayBuffer(Buffer* buffer) {
  if (do_buffer_refcounting_ && is_bound_ && element_array_buffer_)
    element_array_buffer_->OnUnbind(GL_ELEMENT_ARRAY_BUFFER, false);
  element_array_buffer_ = buffer;
  if (do_buffer_refcounting_ && is_bound_ && buffer)
    buffer->OnBind(GL_ELEMENT_ARRAY_BUFFER, false);
}

bool VertexAttribManager::Enable(GLuint index, bool enable) {
  if (index >= vertex_attribs_.size()) {
    return false;
  }

  VertexAttrib& info = vertex_attribs_[index];
  if (info.enabled() != enable) {
    info.set_enabled(enable);
    info.SetList(enable ? &enabled_vertex_attribs_ : &disabled_vertex_attribs_);
    GLuint shift_bits = (index % 16) * 2;
    if (enable) {
      attrib_enabled_mask_[index / 16] |= (0x3 << shift_bits);
    } else {
      attrib_enabled_mask_[index / 16] &= ~(0x3 << shift_bits);
    }
  }
  return true;
}

void VertexAttribManager::Unbind(Buffer* buffer, Buffer* bound_array_buffer) {
  DCHECK(buffer);
  DCHECK(is_bound_);
  if (element_array_buffer_.get() == buffer) {
    if (do_buffer_refcounting_)
      buffer->OnUnbind(GL_ELEMENT_ARRAY_BUFFER, false);
    if (manager_ && manager_->have_context_)
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    element_array_buffer_ = nullptr;
  }
  // When a vertex array object is bound, some drivers (AMD Linux,
  // Qualcomm, etc.) have a bug where it incorrectly generates an
  // GL_INVALID_OPERATION on glVertexAttribPointer() if pointer is
  // NULL, no buffer is bound on GL_ARRAY_BUFFER.  Therefore, in order
  // to clear the buffer bindings, we create a new array buffer,
  // redirect all bindings to the new buffer, and then delete the
  // buffer.
  GLuint new_buffer = 0;
  for (uint32_t vv = 0; vv < vertex_attribs_.size(); ++vv) {
    if (vertex_attribs_[vv].buffer_ == buffer) {
      if (do_buffer_refcounting_)
        buffer->OnUnbind(GL_ARRAY_BUFFER, true);
      vertex_attribs_[vv].buffer_ = nullptr;
      if (manager_ && manager_->have_context_) {
        if (!new_buffer) {
          glGenBuffersARB(1, &new_buffer);
          DCHECK_NE(0u, new_buffer);
          glBindBuffer(GL_ARRAY_BUFFER, new_buffer);
          // TODO(zmo): Do we need to also call glBufferData() here?
        }
        glVertexAttribPointer(
            vv, vertex_attribs_[vv].size_, vertex_attribs_[vv].type_,
            vertex_attribs_[vv].normalized_, vertex_attribs_[vv].gl_stride_, 0);
      }
    }
  }
  if (new_buffer) {
    glDeleteBuffersARB(1, &new_buffer);
    glBindBuffer(GL_ARRAY_BUFFER,
                 bound_array_buffer ? bound_array_buffer->service_id() : 0u);
  }
}

void VertexAttribManager::SetIsBound(bool is_bound) {
  if (is_bound == is_bound_)
    return;
  is_bound_ = is_bound;
  if (do_buffer_refcounting_) {
    if (element_array_buffer_) {
      if (is_bound)
        element_array_buffer_->OnBind(GL_ELEMENT_ARRAY_BUFFER, false);
      else
        element_array_buffer_->OnUnbind(GL_ELEMENT_ARRAY_BUFFER, false);
    }
    for (const auto& va : vertex_attribs_) {
      if (va.buffer_) {
        if (is_bound) {
          va.buffer_->OnBind(GL_ARRAY_BUFFER, true);
        } else {
          va.buffer_->OnUnbind(GL_ARRAY_BUFFER, true);
        }
      }
    }
  }
}

bool VertexAttribManager::ValidateBindings(const char* function_name,
                                           GLES2Decoder* decoder,
                                           FeatureInfo* feature_info,
                                           BufferManager* buffer_manager,
                                           Program* current_program,
                                           GLuint max_vertex_accessed,
                                           bool instanced,
                                           GLsizei primcount,
                                           GLint basevertex,
                                           GLuint baseinstance) {
  DCHECK(primcount);
  ErrorState* error_state = decoder->GetErrorState();
  // true if any enabled, used divisor is zero
  bool divisor0 = false;
  bool have_enabled_active_attribs = false;
  const GLuint kInitialBufferId = 0xFFFFFFFFU;
  GLuint current_buffer_id = kInitialBufferId;
  bool use_client_side_arrays_for_stream_buffers = feature_info->workarounds(
      ).use_client_side_arrays_for_stream_buffers;
  // Validate all attribs currently enabled. If they are used by the current
  // program then check that they have enough elements to handle the draw call.
  // If they are not used by the current program check that they have a buffer
  // assigned.
  for (VertexAttribList::iterator it = enabled_vertex_attribs_.begin();
       it != enabled_vertex_attribs_.end(); ++it) {
    VertexAttrib* attrib = *it;
    Buffer* buffer = attrib->buffer();
    if (!buffer_manager->RequestBufferAccess(error_state, buffer, function_name,
                                             "attached to enabled attrib %u",
                                             attrib->index())) {
      return false;
    }
    const Program::VertexAttrib* attrib_info =
        current_program->GetAttribInfoByLocation(attrib->index());

    // Make sure that every attrib in enabled_vertex_attribs_ is really enabled
    // in the driver, if AND ONLY IF it is consumed by the current shader
    // program. (Note that since the containing loop is over
    // enabled_vertex_attribs_, not all vertex attribs, it doesn't erroneously
    // enable any attribs that should be disabled.)
    // This is for http://crbug.com/756293 but also subsumes some workaround
    // code for use_client_side_arrays_for_stream_buffers.
    SetDriverVertexAttribEnabled(attrib->index(), attrib_info != nullptr);

    if (attrib_info) {
      divisor0 |= (attrib->divisor() == 0);
      have_enabled_active_attribs = true;
      GLuint count = attrib->MaxVertexAccessed(primcount, max_vertex_accessed,
                                               basevertex, baseinstance);
      // This attrib is used in the current program.
      if (!attrib->CanAccess(count)) {
        ERRORSTATE_SET_GL_ERROR(
            error_state, GL_INVALID_OPERATION, function_name,
            (std::string(
                 "attempt to access out of range vertices in attribute ") +
             base::NumberToString(attrib->index()))
                .c_str());
        return false;
      }
      if (use_client_side_arrays_for_stream_buffers) {
        if (buffer->IsClientSideArray()) {
          if (current_buffer_id != 0) {
            current_buffer_id = 0;
            glBindBuffer(GL_ARRAY_BUFFER, 0);
          }
          attrib->set_is_client_side_array(true);
          const void* ptr = buffer->GetRange(attrib->offset(), 0);
          DCHECK(ptr);
          glVertexAttribPointer(
              attrib->index(),
              attrib->size(),
              attrib->type(),
              attrib->normalized(),
              attrib->gl_stride(),
              ptr);
        } else if (attrib->is_client_side_array()) {
          attrib->set_is_client_side_array(false);
          GLuint new_buffer_id = buffer->service_id();
          if (new_buffer_id != current_buffer_id) {
            current_buffer_id = new_buffer_id;
            glBindBuffer(GL_ARRAY_BUFFER, current_buffer_id);
          }
          const void* ptr = reinterpret_cast<const void*>(attrib->offset());
          glVertexAttribPointer(
              attrib->index(),
              attrib->size(),
              attrib->type(),
              attrib->normalized(),
              attrib->gl_stride(),
              ptr);
        }
      }
    }
  }

  // Due to D3D9 limitation, in ES2/WebGL1, instanced drawing needs at least
  // one enabled attribute with divisor zero. This does not apply to D3D11,
  // therefore, it also does not apply to ES3/WebGL2.
  // Non-instanced drawing is fine with having no attributes at all, but if
  // there are attributes, at least one should have divisor zero.
  // (See ANGLE_instanced_arrays spec)
  if (feature_info->IsWebGL1OrES2Context() && !divisor0 &&
      (instanced || have_enabled_active_attribs)) {
    ERRORSTATE_SET_GL_ERROR(
        error_state, GL_INVALID_OPERATION, function_name,
        "attempt to draw with all attributes having non-zero divisors");
    return false;
  }

  if (current_buffer_id != kInitialBufferId) {
    // Restore the buffer binding.
    decoder->RestoreBufferBindings();
  }

  return true;
}

}  // namespace gles2
}  // namespace gpu
