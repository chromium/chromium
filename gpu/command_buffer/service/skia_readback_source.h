// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SKIA_READBACK_SOURCE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SKIA_READBACK_SOURCE_H_

#include <variant>

#include "base/memory/stack_allocated.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"

class SkImage;
class SkSurface;

namespace gpu {

// Wraps either an SkSurface or SkImage that will be used as the source for
// readback operations. This avoids duplicate functions to handle surface vs
// image readback. Should never be stored as a member variable.
class GPU_GLES2_EXPORT SkiaReadbackSource {
  STACK_ALLOCATED();

 public:
  explicit SkiaReadbackSource(SkSurface* surface);
  explicit SkiaReadbackSource(SkImage* image);
  SkiaReadbackSource(const SkiaReadbackSource& other);
  SkiaReadbackSource& operator=(const SkiaReadbackSource& other);
  ~SkiaReadbackSource();

  SkSurface* surface() const {
    if (const auto* s = std::get_if<SkSurface*>(&source_)) {
      return *s;
    }
    return nullptr;
  }
  SkImage* image() const {
    if (const auto* i = std::get_if<SkImage*>(&source_)) {
      return *i;
    }
    return nullptr;
  }
  SkAlphaType GetAlphaType() const;

 private:
  std::variant<SkSurface*, SkImage*> source_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SKIA_READBACK_SOURCE_H_
