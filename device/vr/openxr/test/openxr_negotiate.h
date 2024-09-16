// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_OPENXR_NEGOTIATE_H_
#define DEVICE_VR_OPENXR_TEST_OPENXR_NEGOTIATE_H_

#include <unknwn.h>

#include "device/vr/openxr/openxr_platform.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_loader_negotiation.h"

// This file contains functions that are used by the openxr_loader.dll to call
// into the fake OpenXR Runtime. Used for testing purposes only, so this should
// only be used to call the fake OpenXR APIs defined in
// fake_openxr_impl_api.cc.

XrResult XRAPI_PTR xrGetInstanceProcAddr(XrInstance instance,
                                         const char* name,
                                         PFN_xrVoidFunction* function);

// The single exported function in fake OpenXR Runtime DLL which the OpenXR
// loader calls for negotiation. xrGetInstanceProcAddr is returned to the
// loader, which is then used by the loader to call OpenXR APIs.
XrResult __stdcall xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo,
    XrNegotiateRuntimeRequest* runtimeRequest) {
  runtimeRequest->runtimeInterfaceVersion = 1;
  runtimeRequest->runtimeApiVersion = XR_CURRENT_API_VERSION;
  runtimeRequest->getInstanceProcAddr = xrGetInstanceProcAddr;

  return XR_SUCCESS;
}

#endif  // DEVICE_VR_OPENXR_TEST_OPENXR_NEGOTIATE_H_
