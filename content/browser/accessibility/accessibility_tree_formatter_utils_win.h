// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_WIN_H_

#include <stdint.h>
#include <wtypes.h>
#include <vector>

#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ui/gfx/win/hwnd_util.h"

namespace content {

CONTENT_EXPORT base::string16 IAccessibleRoleToString(int32_t ia_role);
CONTENT_EXPORT base::string16 IAccessible2RoleToString(int32_t ia_role);
CONTENT_EXPORT base::string16 IAccessibleStateToString(int32_t ia_state);
CONTENT_EXPORT void IAccessibleStateToStringVector(
    int32_t ia_state,
    std::vector<base::string16>* result);
CONTENT_EXPORT base::string16 IAccessible2StateToString(int32_t ia2_state);
CONTENT_EXPORT void IAccessible2StateToStringVector(
    int32_t ia_state,
    std::vector<base::string16>* result);

// Handles both IAccessible/MSAA events and IAccessible2 events.
CONTENT_EXPORT base::string16 AccessibilityEventToString(int32_t event_id);

CONTENT_EXPORT base::string16 UiaIdentifierToString(int32_t identifier);
CONTENT_EXPORT base::string16 UiaOrientationToString(int32_t identifier);
CONTENT_EXPORT base::string16 UiaLiveSettingToString(int32_t identifier);

CONTENT_EXPORT std::string BstrToUTF8(BSTR bstr);
CONTENT_EXPORT std::string UiaIdentifierToStringUTF8(int32_t id);

CONTENT_EXPORT HWND GetHwndForProcess(base::ProcessId pid);
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_WIN_H_
