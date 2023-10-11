// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {

Capabilities::Capabilities() = default;

Capabilities::Capabilities(const Capabilities& other) = default;

Capabilities::~Capabilities() = default;

GLCapabilities::GLCapabilities() = default;

GLCapabilities::GLCapabilities(const GLCapabilities& other) = default;

GLCapabilities::~GLCapabilities() = default;

GLCapabilities::PerStagePrecisions::PerStagePrecisions() = default;

}  // namespace gpu
