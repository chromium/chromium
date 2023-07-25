// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_IMAGE_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_IMAGE_PROVIDER_H_

#include "third_party/skia/include/gpu/graphite/ImageProvider.h"

namespace gpu {

// This class is used by Graphite to create Graphite-backed SkImages from non-
// Graphite-backed SkImages. It is given to a Graphite Recorder on creation. If
// no ImageProvider is given to a Recorder, then any non-Graphite-backed SkImage
// draws on that Recorder will fail.
//
// See https://crsrc.org/c/third_party/skia/include/gpu/graphite/ImageProvider.h
// for details on Skia's requirements for ImageProvider.
//
// TODO(https://crbug.com/1457525): Currently this class uploads every image it
// encounters to a new texture. Instead, it could do some caching to avoid
// redundant work.
class GraphiteImageProvider : public skgpu::graphite::ImageProvider {
 public:
  ~GraphiteImageProvider() override = default;

  sk_sp<SkImage> findOrCreate(
      skgpu::graphite::Recorder* recorder,
      const SkImage* image,
      SkImage::RequiredProperties requiredProps) override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_IMAGE_PROVIDER_H_
