// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_IPC_LOGGING_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_IPC_LOGGING_H_

#include "content/common/content_export.h"
#include "ipc/ipc_buildflags.h"

namespace content {

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

// Enable or disable IPC logging for the browser, all processes
// derived from ChildProcess (plugin etc), and all
// renderers.
CONTENT_EXPORT void EnableIPCLogging(bool enable);

#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_IPC_LOGGING_H_
