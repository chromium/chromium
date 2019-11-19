// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/scoped_do_not_use_ui_default_queue_from_io.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/scheduler/browser_task_executor.h"

namespace content {

ScopedDoNotUseUIDefaultQueueFromIO::ScopedDoNotUseUIDefaultQueueFromIO(
    const base::Location& scoped_location)
    : scoped_location_(scoped_location) {
  TRACE_EVENT_BEGIN0("toplevel", "ScopedDoNotUseUIDefaultQueueFromIO");
#if DCHECK_IS_ON()
  // Only has an effect in the browser process.
  BrowserTaskExecutor::AddValidator({BrowserThread::UI}, this);
#endif
}

void ScopedDoNotUseUIDefaultQueueFromIO::ValidatePostTask(
    const base::Location& post_task_location) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO))
      << "It's prohibited by ScopedDoNotUseUIDefaultQueueFromIO at "
      << scoped_location_.ToString()
      << " to post to the UI thread's default queue at this time due to the "
         "risk of accidental task reordering. Please specify a non-default "
         "BrowserTaskType. See PostTask "
      << post_task_location.ToString();
}

ScopedDoNotUseUIDefaultQueueFromIO::~ScopedDoNotUseUIDefaultQueueFromIO() {
  TRACE_EVENT_END0("toplevel", "ScopedDoNotUseUIDefaultQueueFromIO");
#if DCHECK_IS_ON()
  BrowserTaskExecutor::RemoveValidator({BrowserThread::UI}, this);
#endif
}

}  // namespace content
