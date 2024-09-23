// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_VERTEX_ATTRIB_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_VERTEX_ATTRIB_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class BufferManager;
class FeatureInfo;
class GLES2Decoder;
class Program;
class VertexArrayManager;

// Info about a Vertex Attribute. This is used to track what the user currently
// has bound on each Vertex Attribute so that checking can be done at
// glDrawXXX time.
class GPU_GLES2_EXPORT VertexAttrib {
 public:
  typedef std::list<raw_ptr<VertexAttrib, CtnExperimental>> VertexAttribList;

  VertexAttrib();
  VertexAttrib(const VertexAttrib& other);
  ~VertexAttrib();

  // Returns true if this VertexAttrib can access index.
  bool CanAccess(GLuint index) const;

  Buffer* buffer() const { return buffer_.get(); }

  GLsizei offset() const {
    return offset_;
  }

  GLuint index() const {
    return index_;
  }

  GLint size() const {
    return size_;
  }

  GLenum type() const {
    return type_;
  }

  GLboolean normalized() const {
    return normalized_;
  }

  GLsizei gl_stride() const {
    return gl_stride_;
  }

  GLuint divisor() const {
    return divisor_;
  }

  GLboolean integer() const {
    return integer_;
  }

  bool enabled() const {
    return enabled_;
  }

  bool enabled_in_driver() const { return enabled_in_driver_; }

  // Find the maximum vertex accessed, accounting for instancing.
  GLuint MaxVertexAccessed(GLsizei primcount,
                           GLuint max_vertex_accessed) const {
    return divisor_ ? ((primcount - 1) / divisor_) : max_vertex_accessed;
  }

  // For performance issue we are having separate overloading functions
  // which takes in basevertex and baseinstance
  GLuint MaxVertexAccessed(GLsizei primcount,
                           GLuint max_vertex_accessed,
                           GLint basevertex,
                           GLuint baseinstance) const {
    return divisor_ ? ((primcount - 1) / divisor_) + baseinstance
                    : max_vertex_accessed + basevertex;
  }

  bool is_client_side_array() const {
    return is_client_side_array_;
  }

  void set_is_client_side_array(bool value) {
    is_client_side_array_ = value;
  }

 private:
  friend class VertexAttribManager;

  void set_enabled(bool enabled) {
    enabled_ = enabled;
  }

  void set_index(GLuint index) {
    index_ = index;
  }

  void SetList(VertexAttribList* new_list) {
    DCHECK(new_list);

    if (list_) {
      list_->erase(it_);
    }

    it_ = new_list->insert(new_list->end(), this);
    list_ = new_list;
  }

  void SetInfo(
      Buffer* buffer,
      GLint size,
      GLenum type,
      GLboolean normalized,
      GLsizei gl_stride,
      GLsizei real_stride,
      GLsizei offset,
      GLboolean integer);

  void SetDivisor(GLsizei divisor) {
    divisor_ = divisor;
  }

  // The index of this attrib.
  GLuint index_;

  // Whether or not this attribute is enabled.
  bool enabled_;

  // Whether or not this attribute is actually enabled in the driver.
  bool enabled_in_driver_;

  // number of components (1, 2, 3, 4)
  GLint size_;

  // GL_BYTE, GL_FLOAT, etc. See glVertexAttribPointer.
  GLenum type_;

  // The offset into the buffer.
  GLsizei offset_;

  GLboolean normalized_;

  // The stride passed to glVertexAttribPointer.
  GLsizei gl_stride_;

  // The stride that will be used to access the buffer. This is the actual
  // stide, NOT the GL bogus stride. In other words there is never a stride
  // of 0.
  GLsizei real_stride_;

  GLsizei divisor_;

  GLboolean integer_;

  // Will be true if this was assigned to a client side array.
  bool is_client_side_array_;

  // The buffer bound to this attribute.
  scoped_refptr<Buffer> buffer_;

  // List this info is on.
  raw_ptr<VertexAttribList> list_;

  // Iterator for list this info is on. Enabled/Disabled
  VertexAttribList::iterator it_;
};

// Manages vertex attributes.
// This class also acts as the service-side representation of a
// vertex array object and it's contained state.
class GPU_GLES2_EXPORT VertexAttribManager
    : public base::RefCounted<VertexAttribManager> {
 public:
  typedef std::list<raw_ptr<VertexAttrib, CtnExperimental>> VertexAttribList;

  explicit VertexAttribManager(bool do_buffer_refcounting);

  void Initialize(uint32_t num_vertex_attribs);

  bool Enable(GLuint index, bool enable);

  bool HaveFixedAttribs() const {
    return num_fixed_attribs_ != 0;
  }

  const VertexAttribList& GetEnabledVertexAttribs() const {
    return enabled_vertex_attribs_;
  }

  VertexAttrib* GetVertexAttrib(GLuint index) {
    if (index < vertex_attribs_.size()) {
      return &vertex_attribs_[index];
    }
    return nullptr;
  }

  void UpdateAttribBaseTypeAndMask(GLuint loc, GLenum base_type) {
    DCHECK(loc < vertex_attribs_.size());
    int shift_bits = (loc % 16) * 2;
    attrib_enabled_mask_[loc / 16] |= (0x3 << shift_bits);
    attrib_base_type_mask_[loc / 16] &= ~(0x3 << shift_bits);
    attrib_base_type_mask_[loc / 16] |= base_type << shift_bits;
  }

  // Sets the Enable/DisableVertexAttribArray state in the driver. This state
  // is tracked for the current virtual context. Because of this, virtual
  // context restore code should not call this function.
  void SetDriverVertexAttribEnabled(GLuint index, bool enable) {
    DCHECK_LT(index, vertex_attribs_.size());
    VertexAttrib& attrib = vertex_attribs_[index];

    if (enable != attrib.enabled_in_driver_) {
      attrib.enabled_in_driver_ = enable;
      if (enable) {
        glEnableVertexAttribArray(index);
      } else {
        glDisableVertexAttribArray(index);
      }
    }
  }

  const std::vector<uint32_t>& attrib_base_type_mask() const {
    return attrib_base_type_mask_;
  }
  const std::vector<uint32_t>& attrib_enabled_mask() const {
    return attrib_enabled_mask_;
  }

  void SetAttribInfo(
      GLuint index,
      Buffer* buffer,
      GLint size,
      GLenum type,
      GLboolean normalized,
      GLsizei gl_stride,
      GLsizei real_stride,
      GLsizei offset,
      GLboolean integer) {
    VertexAttrib* attrib = GetVertexAttrib(index);
    if (attrib) {
      if (attrib->type() == GL_FIXED) {
        --num_fixed_attribs_;
      }
      if (type == GL_FIXED) {
        ++num_fixed_attribs_;
      }
      if (do_buffer_refcounting_ && is_bound_ && attrib->buffer_)
        attrib->buffer_->OnUnbind(GL_ARRAY_BUFFER, true);
      attrib->SetInfo(buffer, size, type, normalized, gl_stride, real_stride,
                      offset, integer);
      if (do_buffer_refcounting_ && is_bound_ && buffer)
        buffer->OnBind(GL_ARRAY_BUFFER, true);
    }
  }

  void SetDivisor(GLuint index, GLuint divisor) {
    VertexAttrib* attrib = GetVertexAttrib(index);
    if (attrib) {
      attrib->SetDivisor(divisor);
    }
  }

  void SetElementArrayBuffer(Buffer* buffer);

  Buffer* element_array_buffer() const { return element_array_buffer_.get(); }

  GLuint service_id() const {
    return service_id_;
  }

  void Unbind(Buffer* buffer, Buffer* bound_array_buffer);

  bool IsDeleted() const {
    return deleted_;
  }

  bool IsValid() const {
    return !IsDeleted();
  }

  size_t num_attribs() const {
    return vertex_attribs_.size();
  }

  bool ValidateBindings(const char* function_name,
                        GLES2Decoder* decoder,
                        FeatureInfo* feature_info,
                        BufferManager* buffer_manager,
                        Program* current_program,
                        GLuint max_vertex_accessed,
                        bool instanced,
                        GLsizei primcount,
                        GLint basevertex,
                        GLuint baseinstance);

  void SetIsBound(bool is_bound);

 private:
  friend class VertexArrayManager;
  friend class VertexArrayManagerTest;
  friend class base::RefCounted<VertexAttribManager>;

  // Used when creating from a VertexArrayManager
  VertexAttribManager(VertexArrayManager* manager,
                      GLuint service_id,
                      uint32_t num_vertex_attribs,
                      bool do_buffer_refcounting);

  ~VertexAttribManager();

  void MarkAsDeleted() {
    deleted_ = true;
  }

  // number of attribs using type GL_FIXED.
  int num_fixed_attribs_;

  // Info for each vertex attribute saved so we can check at glDrawXXX time
  // if it is safe to draw.
  std::vector<VertexAttrib> vertex_attribs_;

  // Vertex attrib base types: FLOAT, INT, or UINT.
  // Each base type is encoded into 2 bits, the lowest 2 bits for location 0,
  // the highest 2 bits for location (max_vertex_attribs - 1).
  std::vector<uint32_t> attrib_base_type_mask_;
  // Same layout as above, 2 bits per location, 0x03 if a location for an
  // vertex attrib is enabled by enabbleVertexAttribArray, 0x00 if it is
  // disabled by disableVertexAttribArray. Every location is 0x00 by default.
  std::vector<uint32_t> attrib_enabled_mask_;

  // The currently bound element array buffer. If this is 0 it is illegal
  // to call glDrawElements.
  scoped_refptr<Buffer> element_array_buffer_;

  // Lists for which vertex attribs are enabled, disabled.
  VertexAttribList enabled_vertex_attribs_;
  VertexAttribList disabled_vertex_attribs_;

  // The VertexArrayManager that owns this VertexAttribManager
  raw_ptr<VertexArrayManager> manager_;

  // True if deleted.
  bool deleted_;

  // True if this is the currently bound VAO.
  bool is_bound_;

  // Whether or not to call Buffer::OnBind/OnUnbind whenever bindings change.
  // This is only necessary for WebGL contexts to implement
  // https://crbug.com/696345
  bool do_buffer_refcounting_;

  // Service side vertex array object id.
  GLuint service_id_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_VERTEX_ATTRIB_MANAGER_H_
