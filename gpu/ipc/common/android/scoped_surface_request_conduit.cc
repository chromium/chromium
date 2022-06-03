// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"

#include "base/check.h"

namespace gpu {
namespace {

ScopedSurfaceRequestConduit* g_instance = nullptr;

}  // namespace

// static
ScopedSurfaceRequestConduit* ScopedSurfaceRequestConduit::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void ScopedSurfaceRequestConduit::SetInstance(
    ScopedSurfaceRequestConduit* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

}  // namespace gpu
