// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_CONTEXT_PROVIDER_CALLBACKS_H_
#define DEVICE_VR_OPENXR_CONTEXT_PROVIDER_CALLBACKS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace viz {
class ContextProvider;
}

// VizContextProviderCallback is implemented by `OpenXrRenderLoop` and is called
// by the VizContextProviderFactoryAsync when a new `viz::ContextProvider` is
// ready for the render loop thread to consume.
using VizContextProviderCallback =
    base::OnceCallback<void(scoped_refptr<viz::ContextProvider>)>;

// VizContextProviderFactoryAsync is implemented by `IsolatedXRRuntimeProvider`
// and is called when the `OpenXrRenderLoop` needs a new `viz::ContextProvider`
// to be created. No guarantees are made about the thread that the
// `VizContextProviderCallback` is called back on, and it's expected that the
// render loop will use `BindPostTask` to ensure that it is called on the
// appropriate thread.
using VizContextProviderFactoryAsync =
    base::RepeatingCallback<void(VizContextProviderCallback)>;

#endif  // DEVICE_VR_OPENXR_CONTEXT_PROVIDER_CALLBACKS_H_
