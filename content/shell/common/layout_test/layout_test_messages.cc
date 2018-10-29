// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#undef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#include "content/shell/common/layout_test/layout_test_messages.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#undef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#include "content/shell/common/layout_test/layout_test_messages.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#undef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#include "content/shell/common/layout_test/layout_test_messages.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#include "content/shell/common/layout_test/layout_test_messages.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#include "content/shell/common/layout_test/layout_test_messages.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#include "content/shell/common/layout_test/layout_test_messages.h"
}  // namespace IPC
