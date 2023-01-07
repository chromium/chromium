// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_IPC_LOGGING_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_IPC_LOGGING_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_logging.h"

namespace content {

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)

// Register a logger for the given IPC message. Use
//
//   #define IPC_MESSAGE_MACROS_LOG_ENABLED
//   #define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger)
//       content::RegisterIPCLogger(msg_id, logger)
//
// to register IPC messages with the logging system.
CONTENT_EXPORT void RegisterIPCLogger(uint32_t msg_id, LogFunction logger);

#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_IPC_LOGGING_H_
