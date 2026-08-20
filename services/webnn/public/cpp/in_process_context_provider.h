// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_IN_PROCESS_CONTEXT_PROVIDER_H_
#define SERVICES_WEBNN_PUBLIC_CPP_IN_PROCESS_CONTEXT_PROVIDER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/webnn/public/cpp/webgpu_context_properties.h"

namespace webnn {

using WebGPUContextHelperCallback =
    base::RepeatingCallback<void(scoped_refptr<base::SingleThreadTaskRunner>,
                                 WebGpuContextHelperOnceCallback)>;

// Creates an in-process WebNN context provider and returns the raw message pipe
// handle for a WebNNContextProvider remote. The caller can wrap this into a
// blink-variant PendingRemote to bind a HeapMojoRemote.
//
// |weights_file_creator_pipe| is the raw message pipe handle for a
// WebNNWeightsFileCreator remote.
// |task_runner| is the task runner the provider will run on.
// |webgpu_context_helper| is an optional callback injected by callers (e.g.,
// Blink ML module) to asynchronously acquire WebGPU device/queue resources for
// in-process GPU execution. Defaults to a null callback so non-GPU callers,
// unit tests, and benchmarks can omit this parameter.
mojo::ScopedMessagePipeHandle CreateInProcessContextProvider(
    mojo::ScopedMessagePipeHandle weights_file_creator_pipe,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebGPUContextHelperCallback webgpu_context_helper = {});

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_IN_PROCESS_CONTEXT_PROVIDER_H_
