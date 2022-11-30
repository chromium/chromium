// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/shared_image_trace_utils.h"

#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace gpu {

base::trace_event::MemoryAllocatorDumpGuid GetSharedImageGUIDForTracing(
    const Mailbox& mailbox) {
  return base::trace_event::MemoryAllocatorDumpGuid(base::StringPrintf(
      "gpu-shared-image/%s", mailbox.ToDebugString().c_str()));
}

}  // namespace gpu
