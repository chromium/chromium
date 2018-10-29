// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
#define CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START LayoutTestMsgStart

IPC_SYNC_MESSAGE_ROUTED1_1(LayoutTestHostMsg_ReadFileToString,
                           base::FilePath /* local path */,
                           std::string /* contents */)
IPC_SYNC_MESSAGE_ROUTED1_1(LayoutTestHostMsg_RegisterIsolatedFileSystem,
                           std::vector<base::FilePath> /* absolute_filenames */,
                           std::string /* filesystem_id */)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_ClearAllDatabases)
IPC_MESSAGE_ROUTED1(LayoutTestHostMsg_SetDatabaseQuota,
                    int /* quota */)
IPC_MESSAGE_ROUTED3(LayoutTestHostMsg_SimulateWebNotificationClick,
                    std::string /* title */,
                    base::Optional<int> /* action_index */,
                    base::Optional<base::string16> /* reply */)
IPC_MESSAGE_ROUTED2(LayoutTestHostMsg_SimulateWebNotificationClose,
                    std::string /* title */,
                    bool /* by_user */)
IPC_MESSAGE_ROUTED1(LayoutTestHostMsg_BlockThirdPartyCookies,
                    bool /* block */)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_DeleteAllCookies)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_DeleteAllCookiesForNetworkService)
IPC_MESSAGE_ROUTED4(LayoutTestHostMsg_SetPermission,
                    std::string /* name */,
                    blink::mojom::PermissionStatus /* status */,
                    GURL /* origin */,
                    GURL /* embedding_origin */)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_ResetPermissions)
IPC_MESSAGE_ROUTED0(LayoutTestHostMsg_InspectSecondaryWindow)
IPC_MESSAGE_ROUTED2(LayoutTestHostMsg_InitiateCaptureDump,
                    bool /* should dump navigation history */,
                    bool /* should dump pixels */)

// Notifies the browser that one of renderers has changed layout test runtime
// flags (i.e. has set dump_as_text).
IPC_MESSAGE_CONTROL1(
    LayoutTestHostMsg_LayoutTestRuntimeFlagsChanged,
    base::DictionaryValue /* changed_layout_test_runtime_flags */)

// Used send flag changes to renderers - either when
// 1) broadcasting change happening in one renderer to all other renderers, or
// 2) sending accumulated changes to a single new renderer.
IPC_MESSAGE_CONTROL1(
    LayoutTestMsg_ReplicateLayoutTestRuntimeFlagsChanges,
    base::DictionaryValue /* changed_layout_test_runtime_flags */)

// Sent by secondary test window to notify the test has finished.
IPC_MESSAGE_CONTROL0(LayoutTestHostMsg_TestFinishedInSecondaryRenderer)

#endif  // CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_MESSAGES_H_
