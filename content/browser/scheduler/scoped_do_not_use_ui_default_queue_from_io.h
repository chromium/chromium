// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_SCOPED_DO_NOT_USE_UI_DEFAULT_QUEUE_FROM_IO_H_
#define CONTENT_BROWSER_SCHEDULER_SCOPED_DO_NOT_USE_UI_DEFAULT_QUEUE_FROM_IO_H_

#include "base/location.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace content {

// When ScopedDoNotUseUIDefaultQueueFromIO exists, it's prohibited to post to
// the UI thread's default task runner from the IO thread.
class CONTENT_EXPORT ScopedDoNotUseUIDefaultQueueFromIO
    : public BrowserTaskQueues::Validator {
 public:
  explicit ScopedDoNotUseUIDefaultQueueFromIO(const base::Location& location);

  ~ScopedDoNotUseUIDefaultQueueFromIO() override;

 private:
  void ValidatePostTask(const base::Location& post_task_location) override;

  const base::Location scoped_location_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_SCOPED_DO_NOT_USE_UI_DEFAULT_QUEUE_FROM_IO_H_
