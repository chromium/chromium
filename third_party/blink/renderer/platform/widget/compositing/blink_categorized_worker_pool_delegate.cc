// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/blink_categorized_worker_pool_delegate.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

BlinkCategorizedWorkerPoolDelegate::BlinkCategorizedWorkerPoolDelegate() =
    default;

BlinkCategorizedWorkerPoolDelegate::~BlinkCategorizedWorkerPoolDelegate() =
    default;

// static
BlinkCategorizedWorkerPoolDelegate& BlinkCategorizedWorkerPoolDelegate::Get() {
  static base::NoDestructor<BlinkCategorizedWorkerPoolDelegate> delegate;
  return *delegate;
}

void BlinkCategorizedWorkerPoolDelegate::NotifyThreadWillRun(
    base::PlatformThreadId tid) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  scoped_refptr<base::TaskRunner> task_runner =
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
  task_runner->PostTask(FROM_HERE, base::BindOnce(
                                       [](base::PlatformThreadId tid) {
                                         Platform::Current()->SetThreadType(
                                             tid,
                                             base::ThreadType::kBackground);
                                       },
                                       tid));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace blink
