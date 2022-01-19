// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/profiling_utils.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/mojom/sandbox.mojom-shared.h"
#endif

namespace content {

// Serves WaitableEvent that should be used by the child processes to signal
// that they have finished dumping the profiling data.
class WaitForProcessesToDumpProfilingInfo {
 public:
  WaitForProcessesToDumpProfilingInfo();
  ~WaitForProcessesToDumpProfilingInfo();
  WaitForProcessesToDumpProfilingInfo(
      const WaitForProcessesToDumpProfilingInfo& other) = delete;
  WaitForProcessesToDumpProfilingInfo& operator=(
      const WaitForProcessesToDumpProfilingInfo&) = delete;

  // Wait for all the events served by |GetNewWaitableEvent| to signal.
  void WaitForAll();

  // Return a new waitable event. Calling |WaitForAll| will wait for this event
  // to be signaled.
  // The returned WaitableEvent is owned by this
  // WaitForProcessesToDumpProfilingInfo instance.
  base::WaitableEvent* GetNewWaitableEvent();

 private:
  // Implementation of WaitForAll that will run on the thread pool.
  void WaitForAllOnThreadPool();

  std::vector<std::unique_ptr<base::WaitableEvent>> events_;
};

WaitForProcessesToDumpProfilingInfo::WaitForProcessesToDumpProfilingInfo() =
    default;
WaitForProcessesToDumpProfilingInfo::~WaitForProcessesToDumpProfilingInfo() =
    default;

void WaitForProcessesToDumpProfilingInfo::WaitForAll() {
  base::RunLoop nested_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Some of the waitable events will be signaled on the main thread, use a
  // nested run loop to ensure we're not preventing them from signaling.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(
          &WaitForProcessesToDumpProfilingInfo::WaitForAllOnThreadPool,
          base::Unretained(this)),
      nested_run_loop.QuitClosure());
  nested_run_loop.Run();
}

void WaitForProcessesToDumpProfilingInfo::WaitForAllOnThreadPool() {
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_blocking;

  std::vector<base::WaitableEvent*> events_raw;
  events_raw.reserve(events_.size());
  for (const auto& iter : events_)
    events_raw.push_back(iter.get());

  // Wait for all the events to be signaled.
  while (events_raw.size()) {
    size_t index =
        base::WaitableEvent::WaitMany(events_raw.data(), events_raw.size());
    events_raw.erase(events_raw.begin() + index);
  }
}

base::WaitableEvent*
WaitForProcessesToDumpProfilingInfo::GetNewWaitableEvent() {
  events_.push_back(std::make_unique<base::WaitableEvent>());
  return events_.back().get();
}

void WaitForAllChildrenToDumpProfilingData() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return;
  }
  WaitForProcessesToDumpProfilingInfo wait_for_profiling_data;

  // Ask all the renderer processes to dump their profiling data.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    DCHECK(!i.GetCurrentValue()->GetProcess().is_current());
    if (!i.GetCurrentValue()->IsInitializedAndNotDead())
      continue;
    i.GetCurrentValue()->DumpProfilingData(base::BindOnce(
        &base::WaitableEvent::Signal,
        base::Unretained(wait_for_profiling_data.GetNewWaitableEvent())));
  }

  // Ask all the other child processes to dump their profiling data
  for (content::BrowserChildProcessHostIterator browser_child_iter;
       !browser_child_iter.Done(); ++browser_child_iter) {
#if BUILDFLAG(IS_WIN)
    // On Windows, elevated processes are never passed the profiling data file
    // so cannot dump their data.
    if (browser_child_iter.GetData().sandbox_type ==
        sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges) {
      continue;
    }
#endif
    browser_child_iter.GetHost()->DumpProfilingData(base::BindOnce(
        &base::WaitableEvent::Signal,
        base::Unretained(wait_for_profiling_data.GetNewWaitableEvent())));
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    DumpGpuProfilingData(base::BindOnce(
        &base::WaitableEvent::Signal,
        base::Unretained(wait_for_profiling_data.GetNewWaitableEvent())));
  }

  // This will block until all the child processes have saved their profiling
  // data to disk.
  wait_for_profiling_data.WaitForAll();
}

}  // namespace content
