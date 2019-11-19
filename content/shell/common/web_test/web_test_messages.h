// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_
#define CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START WebTestMsgStart

IPC_SYNC_MESSAGE_ROUTED1_1(WebTestHostMsg_ReadFileToString,
                           base::FilePath /* local path */,
                           std::string /* contents */)
IPC_SYNC_MESSAGE_ROUTED1_1(WebTestHostMsg_RegisterIsolatedFileSystem,
                           std::vector<base::FilePath> /* absolute_filenames */,
                           std::string /* filesystem_id */)

IPC_MESSAGE_ROUTED0(WebTestHostMsg_ClearAllDatabases)
IPC_MESSAGE_ROUTED1(WebTestHostMsg_SetDatabaseQuota, int /* quota */)
IPC_MESSAGE_ROUTED3(WebTestHostMsg_SimulateWebNotificationClick,
                    std::string /* title */,
                    base::Optional<int> /* action_index */,
                    base::Optional<base::string16> /* reply */)
IPC_MESSAGE_ROUTED2(WebTestHostMsg_SimulateWebNotificationClose,
                    std::string /* title */,
                    bool /* by_user */)
IPC_MESSAGE_ROUTED1(WebTestHostMsg_SimulateWebContentIndexDelete,
                    std::string /* id */)
IPC_MESSAGE_ROUTED1(WebTestHostMsg_BlockThirdPartyCookies, bool /* block */)
IPC_MESSAGE_ROUTED0(WebTestHostMsg_DeleteAllCookies)
IPC_MESSAGE_ROUTED4(WebTestHostMsg_SetPermission,
                    std::string /* name */,
                    blink::mojom::PermissionStatus /* status */,
                    GURL /* origin */,
                    GURL /* embedding_origin */)
IPC_MESSAGE_ROUTED0(WebTestHostMsg_ResetPermissions)
IPC_MESSAGE_ROUTED0(WebTestHostMsg_InspectSecondaryWindow)
IPC_MESSAGE_ROUTED2(WebTestHostMsg_InitiateCaptureDump,
                    bool /* should dump navigation history */,
                    bool /* should dump pixels */)

// Notifies the browser that one of renderers has changed web test runtime
// flags (i.e. has set dump_as_text).
IPC_MESSAGE_CONTROL1(WebTestHostMsg_WebTestRuntimeFlagsChanged,
                     base::DictionaryValue /* changed_web_test_runtime_flags */)

// Used send flag changes to renderers - either when
// 1) broadcasting change happening in one renderer to all other renderers, or
// 2) sending accumulated changes to a single new renderer.
IPC_MESSAGE_CONTROL1(WebTestMsg_ReplicateWebTestRuntimeFlagsChanges,
                     base::DictionaryValue /* changed_web_test_runtime_flags */)

// Sent by secondary test window to notify the test has finished.
IPC_MESSAGE_CONTROL0(WebTestHostMsg_TestFinishedInSecondaryRenderer)

#endif  // CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_
