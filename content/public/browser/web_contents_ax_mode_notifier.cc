// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_ax_mode_notifier.h"

#include <vector>
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

void NotifyWebContentsToAddAXMode(ui::AXMode mode) {
  for (auto* web_contents : WebContentsImpl::GetAllWebContents()) {
    web_contents->AddAccessibilityMode(mode);
  }
}

void NotifyWebContentsToSetAXMode(ui::AXMode mode) {
  for (auto* web_contents : WebContentsImpl::GetAllWebContents()) {
    web_contents->SetAccessibilityMode(mode);
  }
}

}  // namespace content
