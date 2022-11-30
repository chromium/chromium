// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_CONTEXT_PROVIDER_CALLBACKS_H_
#define DEVICE_VR_OPENXR_CONTEXT_PROVIDER_CALLBACKS_H_

namespace viz {
class ContextProvider;
}

namespace base {
class SingleThreadTaskRunner;
}

// VizContextProviderCallback is implemented by OpenXrRenderLoop and is called
// by the IsolatedXRRuntimeProvider when a new viz::ContextProvider is ready for
// the render loop thread to consume.
using VizContextProviderCallback =
    base::OnceCallback<void(scoped_refptr<viz::ContextProvider>)>;

// VizContextProviderFactoryAsync is implemented by IsolatedXRRuntimeProvider
// and is called when the OpenXrRenderLoop needs a new viz::ContextProvider to
// be created. The base::SingleThreadTaskRunner parameter is the destination
// task runner for the context provider, the render loop thread.
using VizContextProviderFactoryAsync =
    base::RepeatingCallback<void(VizContextProviderCallback,
                                 scoped_refptr<base::SingleThreadTaskRunner>)>;

#endif  // DEVICE_VR_OPENXR_CONTEXT_PROVIDER_CALLBACKS_H_
