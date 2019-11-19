// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_thread.h"

#include <utility>

#include "base/callback.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

void RunOrPostTaskOnThread(const base::Location& location,
                           BrowserThread::ID thread_id,
                           base::OnceClosure task) {
  if (BrowserThread::CurrentlyOn(thread_id)) {
    std::move(task).Run();
    return;
  }
  base::PostTask(location, {thread_id}, std::move(task));
}

}  // namespace content
