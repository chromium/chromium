// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"

class Browser;

// Provides access to toolbar size information for the browser.
class ToolbarsSizeBrowserAgent
    : public BrowserUserData<ToolbarsSizeBrowserAgent> {
 public:
  // Not copiable or assignable.
  ToolbarsSizeBrowserAgent(const ToolbarsSizeBrowserAgent&) = delete;
  ToolbarsSizeBrowserAgent& operator=(const ToolbarsSizeBrowserAgent&) = delete;
  ~ToolbarsSizeBrowserAgent() override;

  // Sets the `ToolbarsSize` object that this agent will manage.
  void SetToolbarsSize(ToolbarsSize* toolbars_size);
  // Returns the `ToolbarsSize` object managed by this agent.
  ToolbarsSize* GetToolbarsSize();

 private:
  // The ToolbarsSize object providing toolbars size.
  ToolbarsSize* toolbars_size_ = nil;

  friend class BrowserUserData<ToolbarsSizeBrowserAgent>;

  // Constructs the agent, associating it with the given `browser`.
  explicit ToolbarsSizeBrowserAgent(Browser* browser);
};

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_BROWSER_AGENT_H_
