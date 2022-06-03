// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_surface_lookup.h"

#include "base/check.h"

namespace gpu {

namespace {
GpuSurfaceLookup* g_instance = nullptr;
}  // anonymous namespace

// static
GpuSurfaceLookup* GpuSurfaceLookup::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void GpuSurfaceLookup::InitInstance(GpuSurfaceLookup* lookup) {
  DCHECK(!g_instance || !lookup);
  g_instance = lookup;
}

}  // namespace gpu
