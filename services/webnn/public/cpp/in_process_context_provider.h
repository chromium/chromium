// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_IN_PROCESS_CONTEXT_PROVIDER_H_
#define SERVICES_WEBNN_PUBLIC_CPP_IN_PROCESS_CONTEXT_PROVIDER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace webnn::tflite {

// Creates an in-process TFLite context provider and returns the raw message
// pipe handle for a WebNNContextProvider remote. The caller can wrap this
// into a blink-variant PendingRemote to bind a HeapMojoRemote.
//
// |task_runner| is the task runner the provider will run on.
mojo::ScopedMessagePipeHandle CreateInProcessContextProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_PUBLIC_CPP_IN_PROCESS_CONTEXT_PROVIDER_H_
