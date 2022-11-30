// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_VERTEX_ARRAY_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_VERTEX_ARRAY_MANAGER_H_

#include <stdint.h>

#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
namespace gles2 {

class VertexAttribManager;

// This class keeps track of the vertex arrays and their sizes so we can do
// bounds checking.
class GPU_GLES2_EXPORT VertexArrayManager {
 public:
  VertexArrayManager();

  VertexArrayManager(const VertexArrayManager&) = delete;
  VertexArrayManager& operator=(const VertexArrayManager&) = delete;

  ~VertexArrayManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  void MarkContextLost() { have_context_ = false; }

  // Creates a VertexAttribManager and if client_visible,
  // maps it to the client_id.
  scoped_refptr<VertexAttribManager> CreateVertexAttribManager(
      GLuint client_id,
      GLuint service_id,
      uint32_t num_vertex_attribs,
      bool client_visible,
      bool do_buffer_refcounting);

  // Gets the vertex attrib manager for the given vertex array.
  VertexAttribManager* GetVertexAttribManager(GLuint client_id);

  // Removes the vertex attrib manager for the given vertex array.
  void RemoveVertexAttribManager(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

 private:
  friend class VertexAttribManager;

  void StartTracking(VertexAttribManager* vertex_attrib_manager);
  void StopTracking(VertexAttribManager* vertex_attrib_manager);

  // Info for each vertex array in the system.
  typedef std::unordered_map<GLuint, scoped_refptr<VertexAttribManager>>
      VertexAttribManagerMap;
  VertexAttribManagerMap client_vertex_attrib_managers_;

  // Vertex attrib managers for emulation purposes, not visible to clients.
  std::vector<scoped_refptr<VertexAttribManager>> other_vertex_attrib_managers_;

  // Counts the number of VertexArrayInfo allocated with 'this' as its manager.
  // Allows to check no VertexArrayInfo will outlive this.
  unsigned int vertex_attrib_manager_count_;

  bool have_context_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_VERTEX_ARRAY_MANAGER_H_
