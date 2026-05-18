// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_test_service_helper.h"

#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"

namespace gpu {

bool InspectTextureLevelSize(GLManager* gl_manager,
                             unsigned int client_id,
                             unsigned int target,
                             int level,
                             int* width,
                             int* height) {
  gles2::ContextGroup* group = gl_manager->decoder()->GetContextGroup();
  if (!group) {
    return false;
  }
  gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager) {
    return false;
  }
  gles2::TextureRef* texture_ref = texture_manager->GetTexture(client_id);
  if (!texture_ref) {
    return false;
  }
  gles2::Texture* texture = texture_ref->texture();
  if (!texture) {
    return false;
  }

  return texture->GetLevelSize(target, level, width, height, nullptr);
}

}  // namespace gpu
