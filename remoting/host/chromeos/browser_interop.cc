// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/browser_interop.h"

#include <memory>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/policy_watcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

std::unique_ptr<ChromotingHostContext>
BrowserInterop::CreateChromotingHostContext() {
  return ChromotingHostContext::CreateForChromeOS(
      content::GetIOThreadTaskRunner({}), content::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      g_browser_process->shared_url_loader_factory());
}

std::unique_ptr<PolicyWatcher> BrowserInterop::CreatePolicyWatcher() {
  return PolicyWatcher::CreateWithPolicyService(
      g_browser_process->policy_service());
}

}  // namespace remoting
