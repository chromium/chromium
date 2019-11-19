// Copyright 2019 The Chromium Authors. All rights reserved.
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
void ExposeChildInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders);

}  // namespace content

#endif  // CONTENT_CHILD_BROWSER_EXPOSED_CHILD_INTERFACES_H_
