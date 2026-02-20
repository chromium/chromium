// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_COMPLETENESS_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_COMPLETENESS_CACHE_H_

#include <string>
#include <unordered_set>

#include "gpu/gpu_gles2_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace gpu {
namespace gles2 {

// Refcounted wrapper for a hash_set of framebuffer format signatures
// representing framebuffer configurations that are reported by the GL
// driver as complete according to glCheckFramebufferStatusEXT.
class GPU_GLES2_EXPORT FramebufferCompletenessCache {
 public:
  FramebufferCompletenessCache();

  FramebufferCompletenessCache(const FramebufferCompletenessCache&) = delete;
  FramebufferCompletenessCache& operator=(const FramebufferCompletenessCache&) =
      delete;

  ~FramebufferCompletenessCache();

  bool IsComplete(const std::string& signature) const;
  void SetComplete(const std::string& signature);

 private:
  typedef absl::flat_hash_set<std::string> Map;

  Map cache_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FRAMEBUFFER_COMPLETENESS_CACHE_H_
