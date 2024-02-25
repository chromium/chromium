// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BROWSER_EXPOSED_CHILD_INTERFACES_H_
#define CONTENT_CHILD_BROWSER_EXPOSED_CHILD_INTERFACES_H_

#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace mojo {
class BinderMap;
}

namespace content {

// Populates |*binders| with callbacks to expose interface to the browser
// process from all child processes (including renderers, GPU, service
// processes, etc.). Interfaces exposed here can be acquired in the browser via
// |RenderProcessHost::BindReceiver()| or |ChildProcessHost::BindReceiver()|.
//
// |in_browser_process| is true if the child process is running in the browser
// process. For example, single-process mode, or in-process gpu mode (forced
// by low-end device mode) makes all child processes or gpu process run in
// the browser process. If the services depend on whether the child process
// is running in the browser process or not (e.g. if using a process-wide
// global variables and |in_browser_process| is true, the browser process and
// the child process will use the same global variables.
void ExposeChildInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    bool in_browser_process,
    mojo::BinderMap* binders);

}  // namespace content

#endif  // CONTENT_CHILD_BROWSER_EXPOSED_CHILD_INTERFACES_H_
