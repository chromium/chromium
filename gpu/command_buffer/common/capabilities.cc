// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {

Capabilities::Capabilities() {
  mappable_formats = base::MakeFlatSet<viz::SharedImageFormat>(std::vector({
      viz::SinglePlaneFormat::kBGR_565,
      viz::SinglePlaneFormat::kRGBA_4444,
      viz::SinglePlaneFormat::kRGBA_8888,
      viz::SinglePlaneFormat::kRGBX_8888,
      viz::MultiPlaneFormat::kYV12,
  }));
}

Capabilities::Capabilities(const Capabilities& other) = default;

Capabilities::~Capabilities() = default;

GLCapabilities::GLCapabilities() = default;

GLCapabilities::GLCapabilities(const GLCapabilities& other) = default;

GLCapabilities::~GLCapabilities() = default;

GLCapabilities::PerStagePrecisions::PerStagePrecisions() = default;

}  // namespace gpu
