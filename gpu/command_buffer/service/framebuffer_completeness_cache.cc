// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_completeness_cache.h"

namespace gpu {
namespace gles2 {

FramebufferCompletenessCache::FramebufferCompletenessCache() = default;

FramebufferCompletenessCache::~FramebufferCompletenessCache() = default;

bool FramebufferCompletenessCache::IsComplete(
    const std::string& signature) const {
  return cache_.find(signature) != cache_.end();
}

void FramebufferCompletenessCache::SetComplete(const std::string& signature) {
  cache_.insert(signature);
}

}  // namespace gles2
}  // namespace gpu
