// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_MESSAGES_H_
#define GPU_IPC_COMMON_GPU_MESSAGES_H_

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

#define IPC_MESSAGE_START GpuChannelMsgStart

// This space intentionally left blank, as this header will be removed very
// soon along with several stale references to it.

#endif  // GPU_IPC_COMMON_GPU_MESSAGES_H_
