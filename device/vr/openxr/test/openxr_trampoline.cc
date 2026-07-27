// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>

#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_loader_negotiation.h"

namespace {

PFN_xrGetInstanceProcAddr g_get_instance_proc_addr = nullptr;

}  // namespace

extern "C" {

void SetMockOpenXrDispatchTable(
    PFN_xrGetInstanceProcAddr get_instance_proc_addr) {
  g_get_instance_proc_addr = get_instance_proc_addr;
}

XrResult XRAPI_CALL
xrNegotiateLoaderRuntimeInterface(const XrNegotiateLoaderInfo* loaderInfo,
                                  XrNegotiateRuntimeRequest* runtimeRequest) {
  if (!loaderInfo || !runtimeRequest) {
    return XR_ERROR_INITIALIZATION_FAILED;
  }
  if (!g_get_instance_proc_addr) {
    std::fprintf(stderr,
                 "openxr_trampoline: g_get_instance_proc_addr is null!\n");
    return XR_ERROR_RUNTIME_UNAVAILABLE;
  }

  runtimeRequest->runtimeInterfaceVersion = 1;
  runtimeRequest->runtimeApiVersion = XR_API_VERSION_1_0;
  runtimeRequest->getInstanceProcAddr = g_get_instance_proc_addr;

  return XR_SUCCESS;
}

}  // extern "C"
