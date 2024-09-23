// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_
#define IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

@class WKContentRuleList;
@class WKUserContentController;

namespace web {

// A provider class that handles compiling and configuring Content Blocker
// rules.
class WKContentRuleListProvider {
 public:
  WKContentRuleListProvider();
  ~WKContentRuleListProvider();

  // Sets the WKUserContentController that this provider will install its rules
  // on.
  void SetUserContentController(
      WKUserContentController* user_content_controller);

  // Updates and re-installs the Content Blocker rules using any new state.
  // This may be asynchronous if a rule list hasn't been compiled yet, so
  // `callback` will be called after the mode is set. It will be called with
  // true if the update is successful and false otherwise (most likely because
  // rules were updated again before the first set of rules was fully
  // installed).
  void UpdateContentRuleLists(base::OnceCallback<void(bool)> callback);

 private:
  WKContentRuleListProvider(const WKContentRuleListProvider&) = delete;
  WKContentRuleListProvider& operator=(const WKContentRuleListProvider&) =
      delete;

  // Installs the content rule list that should be installed given the current
  // block setting.
  void InstallContentRuleLists();

  // Uninstalls all content rule lists installed by this provider.
  void UninstallContentRuleLists();

  __weak WKUserContentController* user_content_controller_;
  WKContentRuleList* block_local_rule_list_;
  WKContentRuleList* mixed_content_autoupgrade_rule_list_;

  base::OnceCallback<void(bool)> update_callback_;

  base::WeakPtrFactory<WKContentRuleListProvider> weak_ptr_factory_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_
