// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Files that want to export their ipc messages should do
//   #undef IPC_MESSAGE_EXPORT
//   #define IPC_MESSAGE_EXPORT VISIBILITY_MACRO
// after including this header, but before using any of the macros below.
// (This needs to be before the include guard.)
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#ifndef IPC_IPC_MESSAGE_MACROS_H_
#define IPC_IPC_MESSAGE_MACROS_H_

// TODO(tsepez): Fix IWYU and remove these includes.
#include <stdint.h>

#include <tuple>

#include "base/export_template.h"
#include "base/task/common/task_annotator.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/param_traits_macros.h"

#endif  // IPC_IPC_MESSAGE_MACROS_H_

