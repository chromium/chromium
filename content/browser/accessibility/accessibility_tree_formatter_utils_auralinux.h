// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT const char* ATSPIStateToString(AtspiStateType state);
CONTENT_EXPORT const char* ATSPIRoleToString(AtspiRole role);
CONTENT_EXPORT const char* AtkRoleToString(AtkRole role);

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_AURALINUX_H_
