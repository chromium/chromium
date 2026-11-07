// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_test_service_helper.h"

#include <stdint.h>

#include <vector>

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "ui/gl/gl_bindings.h"

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

std::unique_ptr<SharedImageRepresentationFactoryRef>
RegisterDirtyUnclearedHalfFloatSharedImage(GLManager* gl_manager,
                                           MemoryTypeTracker* memory_tracker,
                                           const Mailbox& mailbox,
                                           int width,
                                           int height) {
  gl::GLApi* api = gl::g_current_gl_context;
  GLint prev_texture = 0;
  api->glGetIntegervFn(GL_TEXTURE_BINDING_2D, &prev_texture);

  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(GL_TEXTURE_2D, service_id);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  api->glTexStorage2DEXTFn(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);

  // IEEE 754 half-precision 1.0f is 0x3C00.
  constexpr uint16_t kOne = 0x3C00;
  std::vector<uint16_t> red_pixels(static_cast<size_t>(width * height * 4), 0);
  for (size_t i = 0; i < red_pixels.size(); i += 4) {
    red_pixels[i] = kOne;
    red_pixels[i + 3] = kOne;
  }
  api->glTexSubImage2DFn(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                         GL_HALF_FLOAT, red_pixels.data());
  api->glBindTextureFn(GL_TEXTURE_2D, prev_texture);

  SharedImageInfo si_info(viz::SinglePlaneFormat::kRGBA_F16,
                          gfx::Size(width, height), gfx::ColorSpace(),
                          kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
                          SharedImageUsageSet({SHARED_IMAGE_USAGE_GLES2_READ,
                                               SHARED_IMAGE_USAGE_GLES2_WRITE}),
                          "TestUnclearedHalfFloat");
  auto backing = std::make_unique<TestImageBacking>(
      mailbox, si_info, /*estimated_size=*/red_pixels.size() * 2, service_id);
  return gl_manager->shared_image_manager()->Register(std::move(backing),
                                                      memory_tracker);
}

}  // namespace gpu
