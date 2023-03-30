// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_AX_MODE_NOTIFIER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_AX_MODE_NOTIFIER_H_

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

// WebContents are responsible to maintain their own AXMode value, so they need
// to be notified when the browser-level accessible state changes. This
// accessible state is set and modified based on heuristics specific to each
// platform API - outside of the content layer - so we need those helper
// functions below to notify all web contents of the changes.
CONTENT_EXPORT void NotifyWebContentsToAddAXMode(ui::AXMode mode);
CONTENT_EXPORT void NotifyWebContentsToSetAXMode(ui::AXMode mode);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_AX_MODE_NOTIFIER_H_
