// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_surface_mock.h"

namespace gpu {

GLSurfaceMock::GLSurfaceMock() = default;

GLSurfaceMock::~GLSurfaceMock() {
  InvalidateWeakPtrs();
}

}  // namespace gpu
