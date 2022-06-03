// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/vertex_array_manager.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"

namespace gpu {
namespace gles2 {

VertexArrayManager::VertexArrayManager()
    : vertex_attrib_manager_count_(0),
      have_context_(true) {
}

VertexArrayManager::~VertexArrayManager() {
  DCHECK(client_vertex_attrib_managers_.empty());
  DCHECK(other_vertex_attrib_managers_.empty());
  CHECK_EQ(vertex_attrib_manager_count_, 0u);
}

void VertexArrayManager::Destroy(bool have_context) {
  have_context_ = have_context;
  client_vertex_attrib_managers_.clear();
  other_vertex_attrib_managers_.clear();
}

scoped_refptr<VertexAttribManager>
VertexArrayManager::CreateVertexAttribManager(GLuint client_id,
                                              GLuint service_id,
                                              uint32_t num_vertex_attribs,
                                              bool client_visible,
                                              bool do_buffer_refcounting) {
  scoped_refptr<VertexAttribManager> vertex_attrib_manager(
      new VertexAttribManager(this, service_id, num_vertex_attribs,
                              do_buffer_refcounting));

  if (client_visible) {
    std::pair<VertexAttribManagerMap::iterator, bool> result =
        client_vertex_attrib_managers_.insert(
            std::make_pair(client_id, vertex_attrib_manager));
    DCHECK(result.second);
  } else {
    other_vertex_attrib_managers_.push_back(vertex_attrib_manager);
  }

  return vertex_attrib_manager;
}

VertexAttribManager* VertexArrayManager::GetVertexAttribManager(
    GLuint client_id) {
  VertexAttribManagerMap::iterator it =
      client_vertex_attrib_managers_.find(client_id);
  return it != client_vertex_attrib_managers_.end() ? it->second.get()
                                                    : nullptr;
}

void VertexArrayManager::RemoveVertexAttribManager(GLuint client_id) {
  VertexAttribManagerMap::iterator it =
      client_vertex_attrib_managers_.find(client_id);
  if (it != client_vertex_attrib_managers_.end()) {
    VertexAttribManager* vertex_attrib_manager = it->second.get();
    vertex_attrib_manager->MarkAsDeleted();
    client_vertex_attrib_managers_.erase(it);
  }
}

void VertexArrayManager::StartTracking(
    VertexAttribManager* /* vertex_attrib_manager */) {
  ++vertex_attrib_manager_count_;
}

void VertexArrayManager::StopTracking(
    VertexAttribManager* /* vertex_attrib_manager */) {
  --vertex_attrib_manager_count_;
}

bool VertexArrayManager::GetClientId(
    GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (VertexAttribManagerMap::const_iterator it =
           client_vertex_attrib_managers_.begin();
       it != client_vertex_attrib_managers_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

}  // namespace gles2
}  // namespace gpu


