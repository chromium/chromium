// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_PIP_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_PIP_UTILS_H_

#include <optional>

#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"

namespace content::desktop_capture {

// Returns the ID of the PiP window that should be excluded from the
// capture of the specified `desktop_id` by the application owning the
// PiP window (e.g., to prevent feedback loops).
//
// Returns `std::nullopt` if no such PiP window exists, or if it does not
// need to be excluded.
//
// Must only be called on the UI thread.
CONTENT_EXPORT std::optional<DesktopMediaID::Id>
GetPipWindowToExcludeFromScreenCapture(DesktopMediaID::Id desktop_id);

}  // namespace content::desktop_capture

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_PIP_UTILS_H_
