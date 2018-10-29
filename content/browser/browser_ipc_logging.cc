// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_ipc_logging.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/common/child_control.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/child_process_host.h"
#include "ipc/ipc_logging.h"

namespace content {

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

void EnableIPCLoggingForChildProcesses(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserChildProcessHostIterator i;  // default constr references a singleton
  while (!i.Done()) {
    mojom::ChildControlPtr child_control;
    content::BindInterface(i.GetHost(), &child_control);
    child_control->SetIPCLoggingEnabled(enabled);
    ++i;
  }
}

void EnableIPCLogging(bool enable) {
  // First enable myself.
  if (enable)
    IPC::Logging::GetInstance()->Enable();
  else
    IPC::Logging::GetInstance()->Disable();

  // Now tell subprocesses.  Messages to ChildProcess-derived
  // processes must be done on the IO thread.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(EnableIPCLoggingForChildProcesses, enable));

  // Finally, tell the renderers which don't derive from ChildProcess.
  // Messages to the renderers must be done on the UI (main) thread.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    mojom::ChildControlPtr child_control;
    content::BindInterface(i.GetCurrentValue(), &child_control);
    child_control->SetIPCLoggingEnabled(enable);
  }
}

#endif  // BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

}  // namespace content
