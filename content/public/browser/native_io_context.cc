// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_io_context.h"

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

NativeIOContext::NativeIOContext()
    : base::RefCountedDeleteOnSequence<NativeIOContext>(
          GetIOThreadTaskRunner({})) {}

}  // namespace content
