// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_DELEGATE_H_

#include "extensions/common/extension_id.h"

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
}  // namespace ui

namespace extensions {

class AutomationEventRouterInterface;

class AutomationInternalApiDelegate {
 public:
  AutomationInternalApiDelegate();
  virtual ~AutomationInternalApiDelegate();

  // Returns true if the extension is permitted to use the automation
  // API for the given web contents.
  virtual bool CanRequestAutomation(const Extension* extension,
                                    const AutomationInfo* automation_info,
                                    content::WebContents* contents) = 0;
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
  // Sets the automation event router interface that should be set on the
  // automation manager.
  virtual void SetAutomationEventRouterInterface(
      AutomationEventRouterInterface* router) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_DELEGATE_H_
