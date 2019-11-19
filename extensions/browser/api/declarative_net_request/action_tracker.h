// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionPrefs;
struct WebRequestInfo;

namespace declarative_net_request {
struct RequestAction;

class ActionTracker {
 public:
  explicit ActionTracker(content::BrowserContext* browser_context);
  ~ActionTracker();

  // Called whenever a request matches with a rule.
  void OnRuleMatched(const RequestAction& request_action,
                     const WebRequestInfo& request_info);

  // Updates the action count for all tabs for the specified |extension_id|'s
  // extension action. Called when chrome.setActionCountAsBadgeText(true) is
  // called by an extension.
  void OnPreferenceEnabled(const ExtensionId& extension_id) const;

  // Clears the action count for the specified |extension_id| for all tabs.
  // Called when an extension's ruleset is removed.
  void ClearExtensionData(const ExtensionId& extension_id);

  // Clears the action count for every extension for the specified |tab_id|.
  // Called when the tab has been closed.
  void ClearTabData(int tab_id);

  // Sets the action count for every extension for the specified |tab_id| to 0
  // and notifies the extension action to set the badge text to 0 for that tab.
  // Called when the a main-frame navigation to a different document finishes on
  // the tab.
  void ResetActionCountForTab(int tab_id);

 private:
  // Called from OnRuleMatched. Dispatches a OnRuleMatchedDebug event to the
  // observer for the extension specified by |request_action.extension_id|.
  void DispatchOnRuleMatchedDebugIfNeeded(
      const RequestAction& request_action,
      api::declarative_net_request::RequestDetails request_details);

  using ExtensionTabKey = std::pair<ExtensionId, int>;

  // Maps a pair of (extension ID, tab ID) to the number of actions matched for
  // the extension and tab specified.
  std::map<ExtensionTabKey, int> actions_matched_;

  content::BrowserContext* browser_context_;

  ExtensionPrefs* extension_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ActionTracker);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_ACTION_TRACKER_H_
