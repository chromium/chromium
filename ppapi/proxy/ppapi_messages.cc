// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#undef PPAPI_PROXY_PPAPI_MESSAGES_H_
#include "ppapi/proxy/ppapi_messages.h"
#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#error "Failed to include ppapi/proxy/ppapi_messages.h"
#endif

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#undef PPAPI_PROXY_PPAPI_MESSAGES_H_
#include "ppapi/proxy/ppapi_messages.h"
#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#error "Failed to include ppapi/proxy/ppapi_messages.h"
#endif

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef PPAPI_PROXY_PPAPI_MESSAGES_H_
#include "ppapi/proxy/ppapi_messages.h"
#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#error "Failed to include ppapi/proxy/ppapi_messages.h"
#endif
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef PPAPI_PROXY_PPAPI_MESSAGES_H_
#include "ppapi/proxy/ppapi_messages.h"
#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#error "Failed to include ppapi/proxy/ppapi_messages.h"
#endif
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef PPAPI_PROXY_PPAPI_MESSAGES_H_
#include "ppapi/proxy/ppapi_messages.h"
#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#error "Failed to include ppapi/proxy/ppapi_messages.h"
#endif
}  // namespace IPC
