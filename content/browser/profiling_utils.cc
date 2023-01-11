// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/profiling_utils.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/mojom/sandbox.mojom-shared.h"
#endif

namespace content {

namespace {

// A refcounted class that runs a closure once it's destroyed.
class RefCountedScopedClosureRunner
    : public base::RefCounted<RefCountedScopedClosureRunner> {
 public:
  RefCountedScopedClosureRunner(base::OnceClosure callback);

 private:
  friend class base::RefCounted<RefCountedScopedClosureRunner>;
  ~RefCountedScopedClosureRunner() = default;

  base::ScopedClosureRunner destruction_callback_;
};

RefCountedScopedClosureRunner::RefCountedScopedClosureRunner(
    base::OnceClosure callback)
    : destruction_callback_(std::move(callback)) {}

}  // namespace

void AskAllChildrenToDumpProfilingData(base::OnceClosure callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return;
  }

  auto closure_runner =
      base::MakeRefCounted<RefCountedScopedClosureRunner>(std::move(callback));

  // Ask all the renderer processes to dump their profiling data.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    DCHECK(!i.GetCurrentValue()->GetProcess().is_current());
    if (!i.GetCurrentValue()->IsInitializedAndNotDead())
      continue;
    i.GetCurrentValue()->DumpProfilingData(base::BindOnce(
        [](scoped_refptr<RefCountedScopedClosureRunner>) {}, closure_runner));
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
        [](scoped_refptr<RefCountedScopedClosureRunner>) {}, closure_runner));
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    DumpGpuProfilingData(base::BindOnce(
        [](scoped_refptr<RefCountedScopedClosureRunner>) {}, closure_runner));
  }
}

}  // namespace content
