// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_DELEGATE_H_

#include <memory>

#include "extensions/common/extension_id.h"
#include "extensions/common/extension_messages.h"

class ExtensionFunction;

namespace extensions {
class AutomationInternalApiDelegate;
struct AutomationInfo;
class Extension;
}  // namespace extensions

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace ui {
class AXTreeID;
class AXEventBundleSink;
}  // namespace ui

namespace extensions {

class AutomationInternalApiDelegate {
 public:
  AutomationInternalApiDelegate();
  virtual ~AutomationInternalApiDelegate();

  // Returns true if the extension is permitted to use the automation
  // API for the given web contents.
  virtual bool CanRequestAutomation(const Extension* extension,
                                    const AutomationInfo* automation_info,
                                    content::WebContents* contents) = 0;
  // Sets |contents| to point to the web contents object associated with the
  // given tab id.  Otherwise, sets |error_msg| with a reason why the
  // tab could not be found. Returns true on success.
  virtual bool GetTabById(int tab_id,
                          content::BrowserContext* browser_context,
                          bool include_incognito,
                          content::WebContents** contents,
                          std::string* error_msg) = 0;
  // Finds the tab id associated with the given web contents object.
  virtual int GetTabId(content::WebContents* contents) = 0;
  // Retrieves the active web contents.
  virtual content::WebContents* GetActiveWebContents(
      ExtensionFunction* function) = 0;
  // Enable automation nodes on the specified ax tree. Returns true if the
  // request is handled in the delegation.
  virtual bool EnableTree(const ui::AXTreeID& tree_id) = 0;
  // Starts managing automation nodes on the desktop.
  virtual void EnableDesktop() = 0;
  // Gets the ax tree id for the nodes being managed for the desktop.
  virtual ui::AXTreeID GetAXTreeID() = 0;
  // Gets the active user context, if multiple contexts are managed by
  // the delegate. Otherwise, may return null.
  virtual content::BrowserContext* GetActiveUserContext() = 0;
  // Sets the event bundle sink that should be set on the automation manager.
  virtual void SetEventBundleSink(ui::AXEventBundleSink* sink) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_DELEGATE_H_
