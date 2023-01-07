// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "ipc/ipc_buildflags.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
    content::RegisterIPCLogger(msg_id, logger)
#include "content/common/all_messages.h"


#include "base/lazy_instance.h"

namespace content {

namespace {

base::LazyInstance<std::unordered_map<uint32_t, LogFunction>>::Leaky
    g_log_function_mapping = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void RegisterIPCLogger(uint32_t msg_id, LogFunction logger) {
  if (!g_log_function_mapping.IsCreated())
    IPC::Logging::set_log_function_map(g_log_function_mapping.Pointer());
  g_log_function_mapping.Get()[msg_id] = logger;
}

}  // content

#endif  // BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
