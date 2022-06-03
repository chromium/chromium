// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include "base/task/lazy_thread_pool_task_runner.h"

namespace content {

namespace {

base::LazyThreadPoolSequencedTaskRunner g_font_list_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE));

}  // namespace

scoped_refptr<base::SequencedTaskRunner> GetFontListTaskRunner() {
  return g_font_list_task_runner.Get();
}

}  // content
