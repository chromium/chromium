// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_browser_agent.h"

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"

ToolbarsSizeBrowserAgent::ToolbarsSizeBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

void ToolbarsSizeBrowserAgent::SetToolbarsSize(ToolbarsSize* toolbars_size) {
  toolbars_size_ = toolbars_size;
}

ToolbarsSize* ToolbarsSizeBrowserAgent::GetToolbarsSize() {
  return toolbars_size_;
}

ToolbarsSizeBrowserAgent::~ToolbarsSizeBrowserAgent() = default;
