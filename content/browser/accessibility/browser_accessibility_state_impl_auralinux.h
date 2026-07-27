// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_AURALINUX_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_AURALINUX_H_

#include <string_view>

#include "content/common/content_export.h"

namespace content::internal {

// Returns true if the contents of a process's /proc/<pid>/cmdline or
// /proc/<pid>/comm files identify it as Orca.
CONTENT_EXPORT bool IsOrcaProcess(std::string_view cmdline,
                                  std::string_view comm);

}  // namespace content::internal

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_AURALINUX_H_
