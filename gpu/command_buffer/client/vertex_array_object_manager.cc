// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/vertex_array_object_manager.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

namespace gpu::gles2 {

template <typename T>
static T RoundUpToMultipleOf4(T size) {
  return (size + 3) & ~3;
}

// This class tracks VertexAttribPointers and helps emulate client side buffers.
//
// The way client side buffers work is we shadow all the Vertex Attribs so we
// know which ones are pointing to client side buffers.
//
// At Draw time, for any attribs pointing to client side buffers we copy them
// to a special VBO and reset the actual vertex attrib pointers to point to this
// VBO.
//
// This also means we have to catch calls to query those values so that when
// an attrib is a client side buffer we pass the info back the user expects.

class GLES2_IMPL_EXPORT VertexArrayObject {
 public:
  // Info about Vertex Attributes. This is used to track what the user currently
  // has bound on each Vertex Attribute so we can simulate client side buffers
  // at glDrawXXX time.
  class VertexAttrib {
   public:
    VertexAttrib()
        : enabled_(false),
          buffer_id_(0),
          size_(4),
          type_(GL_FLOAT),
          normalized_(GL_FALSE),
          pointer_(nullptr),
          gl_stride_(0),
          divisor_(0),
          integer_(GL_FALSE) {}

    bool enabled() const {
      return enabled_;
    }

    void set_enabled(bool enabled) {
      enabled_ = enabled;
    }

    GLuint buffer_id() const {
      return buffer_id_;
    }

    void set_buffer_id(GLuint id) {
      buffer_id_ = id;
    }

    GLenum type() const {
      return type_;
    }

    GLint size() const {
      return size_;
    }

    GLsizei stride() const {
      return gl_stride_;
    }

    GLboolean normalized() const {
      return normalized_;
    }

    const GLvoid* pointer() const {
      return pointer_;
    }

    bool IsClientSide() const {
      return buffer_id_ == 0;
    }

    GLuint divisor() const {
      return divisor_;
    }

    GLboolean integer() const {
      return integer_;
    }

    void SetInfo(
        GLuint buffer_id,
        GLint size,
        GLenum type,
        GLboolean normalized,
        GLsizei gl_stride,
        const GLvoid* pointer,
        GLboolean integer) {
      buffer_id_ = buffer_id;
      size_ = size;
      type_ = type;
      normalized_ = normalized;
      gl_stride_ = gl_stride;
      pointer_ = pointer;
      integer_ = integer;
    }

    void SetDivisor(GLuint divisor) {
      divisor_ = divisor;
    }

   private:
    // Whether or not this attribute is enabled.
    bool enabled_;

    // The id of the buffer. 0 = client side buffer.
    GLuint buffer_id_;

    // Number of components (1, 2, 3, 4).
    GLint size_;

    // GL_BYTE, GL_FLOAT, etc. See glVertexAttribPointer.
    GLenum type_;

    // GL_TRUE or GL_FALSE
    GLboolean normalized_;

    // The pointer/offset into the buffer.
    // RAW_PTR_EXCLUSION: The assigned value may be an offset instead of a
    // pointer.
    RAW_PTR_EXCLUSION const GLvoid* pointer_;

    // The stride that will be used to access the buffer. This is the bogus GL
    // stride where 0 = compute the stride based on size and type.
    GLsizei gl_stride_;

    // Divisor, for geometry instancing.
    GLuint divisor_;

    GLboolean integer_;
  };

  typedef std::vector<VertexAttrib> VertexAttribs;

  explicit VertexArrayObject(GLuint max_vertex_attribs);

  VertexArrayObject(const VertexArrayObject&) = delete;
  VertexArrayObject& operator=(const VertexArrayObject&) = delete;

  void UnbindBuffer(GLuint id);

  bool BindElementArray(GLuint id);

  void SetAttribEnable(GLuint index, bool enabled);

  void SetAttribPointer(
    GLuint buffer_id,
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr, GLboolean integer);

  bool GetVertexAttrib(GLuint index, GLenum pname, uint32_t* param) const;

  void SetAttribDivisor(GLuint index, GLuint divisor);

  bool GetAttribPointer(GLuint index, GLenum pname, void** ptr) const;

  const VertexAttribs& vertex_attribs() const {
    return vertex_attribs_;
  }

  GLuint bound_element_array_buffer() const {
    return bound_element_array_buffer_id_;
  }

 private:
  const VertexAttrib* GetAttrib(GLuint index) const;

  // The currently bound element array buffer.
  GLuint bound_element_array_buffer_id_;

  VertexAttribs vertex_attribs_;
};

VertexArrayObject::VertexArrayObject(GLuint max_vertex_attribs)
    : bound_element_array_buffer_id_(0) {
  vertex_attribs_.resize(max_vertex_attribs);
}

void VertexArrayObject::UnbindBuffer(GLuint id) {
  if (id == 0) {
    return;
  }
  for (size_t ii = 0; ii < vertex_attribs_.size(); ++ii) {
    VertexAttrib& attrib = vertex_attribs_[ii];
    if (attrib.buffer_id() == id) {
      attrib.set_buffer_id(0);
    }
  }
  if (bound_element_array_buffer_id_ == id) {
    bound_element_array_buffer_id_ = 0;
  }
}

bool VertexArrayObject::BindElementArray(GLuint id) {
  if (id == bound_element_array_buffer_id_) {
    return false;
  }
  bound_element_array_buffer_id_ = id;
  return true;
}

void VertexArrayObject::SetAttribEnable(GLuint index, bool enabled) {
  if (index < vertex_attribs_.size()) {
    VertexAttrib& attrib = vertex_attribs_[index];
    if (attrib.enabled() != enabled) {
      attrib.set_enabled(enabled);
    }
  }
}

void VertexArrayObject::SetAttribPointer(
    GLuint buffer_id,
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr,
    GLboolean integer) {
  if (index < vertex_attribs_.size()) {
    VertexAttrib& attrib = vertex_attribs_[index];
    attrib.SetInfo(buffer_id, size, type, normalized, stride, ptr, integer);
  }
}

bool VertexArrayObject::GetVertexAttrib(GLuint index,
                                        GLenum pname,
                                        uint32_t* param) const {
  const VertexAttrib* attrib = GetAttrib(index);
  if (!attrib) {
    return false;
  }

  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      *param = attrib->buffer_id();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      *param = attrib->enabled();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      *param = attrib->size();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *param = attrib->stride();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      *param = attrib->type();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      *param = attrib->normalized();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      *param = attrib->integer();
      break;
    default:
      return false;  // pass through to service side.
  }
  return true;
}

void VertexArrayObject::SetAttribDivisor(GLuint index, GLuint divisor) {
  if (index < vertex_attribs_.size()) {
    VertexAttrib& attrib = vertex_attribs_[index];
    attrib.SetDivisor(divisor);
  }
}

// Gets the Attrib pointer for an attrib but only if it's a client side
// pointer. Returns true if it got the pointer.
bool VertexArrayObject::GetAttribPointer(
    GLuint index, GLenum pname, void** ptr) const {
  const VertexAttrib* attrib = GetAttrib(index);
  if (attrib && pname == GL_VERTEX_ATTRIB_ARRAY_POINTER) {
    *ptr = const_cast<void*>(attrib->pointer());
    return true;
  }
  return false;
}

// Gets an attrib if it's in range and it's client side.
const VertexArrayObject::VertexAttrib* VertexArrayObject::GetAttrib(
    GLuint index) const {
  if (index < vertex_attribs_.size()) {
    const VertexAttrib* attrib = &vertex_attribs_[index];
    return attrib;
  }
  return nullptr;
}

VertexArrayObjectManager::VertexArrayObjectManager(GLuint max_vertex_attribs)
    : max_vertex_attribs_(max_vertex_attribs),
      collection_buffer_size_(0),
      default_vertex_array_object_(
          std::make_unique<VertexArrayObject>(max_vertex_attribs)),
      bound_vertex_array_object_(default_vertex_array_object_.get()) {}

VertexArrayObjectManager::~VertexArrayObjectManager() = default;

GLuint VertexArrayObjectManager::bound_element_array_buffer() const {
  return bound_vertex_array_object_->bound_element_array_buffer();
}

void VertexArrayObjectManager::UnbindBuffer(GLuint id) {
  bound_vertex_array_object_->UnbindBuffer(id);
}

bool VertexArrayObjectManager::BindElementArray(GLuint id) {
  return  bound_vertex_array_object_->BindElementArray(id);
}

void VertexArrayObjectManager::GenVertexArrays(
    GLsizei n, const GLuint* arrays) {
  DCHECK_GE(n, 0);
  for (GLsizei i = 0; i < n; ++i) {
    std::pair<VertexArrayObjectMap::iterator, bool> result =
        vertex_array_objects_.insert(std::make_pair(
            UNSAFE_TODO(arrays[i]),
            std::make_unique<VertexArrayObject>(max_vertex_attribs_)));
    DCHECK(result.second);
  }
}

void VertexArrayObjectManager::DeleteVertexArrays(
    GLsizei n, const GLuint* arrays) {
  DCHECK_GE(n, 0);
  for (GLsizei i = 0; i < n; ++i) {
    GLuint id = UNSAFE_TODO(arrays[i]);
    if (id) {
      VertexArrayObjectMap::iterator it = vertex_array_objects_.find(id);
      if (it != vertex_array_objects_.end()) {
        if (bound_vertex_array_object_ == it->second.get()) {
          bound_vertex_array_object_ = default_vertex_array_object_.get();
        }
        vertex_array_objects_.erase(it);
      }
    }
  }
}

bool VertexArrayObjectManager::BindVertexArray(GLuint array, bool* changed) {
  *changed = false;
  VertexArrayObject* vertex_array_object = default_vertex_array_object_.get();
  if (array != 0) {
    VertexArrayObjectMap::iterator it = vertex_array_objects_.find(array);
    if (it == vertex_array_objects_.end()) {
      return false;
    }
    vertex_array_object = it->second.get();
  }
  *changed = vertex_array_object != bound_vertex_array_object_;
  bound_vertex_array_object_ = vertex_array_object;
  return true;
}

void VertexArrayObjectManager::SetAttribEnable(GLuint index, bool enabled) {
  bound_vertex_array_object_->SetAttribEnable(index, enabled);
}

bool VertexArrayObjectManager::GetVertexAttrib(GLuint index,
                                               GLenum pname,
                                               uint32_t* param) {
  return bound_vertex_array_object_->GetVertexAttrib(index, pname, param);
}

bool VertexArrayObjectManager::GetAttribPointer(
    GLuint index, GLenum pname, void** ptr) const {
  return bound_vertex_array_object_->GetAttribPointer(index, pname, ptr);
}

bool VertexArrayObjectManager::SetAttribPointer(
    GLuint buffer_id,
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr,
    GLboolean integer) {
  // Client side arrays are not allowed in vaos.
  if (buffer_id == 0 && ptr) {
    return false;
  }
  bound_vertex_array_object_->SetAttribPointer(
      buffer_id, index, size, type, normalized, stride, ptr, integer);
  return true;
}

void VertexArrayObjectManager::SetAttribDivisor(GLuint index, GLuint divisor) {
  bound_vertex_array_object_->SetAttribDivisor(index, divisor);
}

// Collects the data into the collection buffer and returns the number of
// bytes collected.
GLsizei VertexArrayObjectManager::CollectData(
    const void* data,
    GLsizei bytes_per_element,
    GLsizei real_stride,
    GLsizei num_elements) {
  GLsizei bytes_needed = bytes_per_element * num_elements;
  if (collection_buffer_size_ < bytes_needed) {
    collection_buffer_.reset(new int8_t[bytes_needed]);
    collection_buffer_size_ = bytes_needed;
  }
  const int8_t* src = static_cast<const int8_t*>(data);
  int8_t* dst = collection_buffer_.get();
  int8_t* end = UNSAFE_TODO(dst + bytes_per_element * num_elements);
  for (; dst < end;
       UNSAFE_TODO(src += real_stride), UNSAFE_TODO(dst += bytes_per_element)) {
    UNSAFE_TODO(memcpy(dst, src, bytes_per_element));
  }
  return bytes_needed;
}

}  // namespace gpu::gles2
