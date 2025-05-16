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

  WKContentRuleListProvider(const WKContentRuleListProvider&) = delete;
  WKContentRuleListProvider& operator=(const WKContentRuleListProvider&) =
      delete;

  // Sets the WKUserContentController that this provider will install its rules
  // on. This should be called before rules are expected to be applied.
  // Calling this triggers installation of any already-compiled rules.
  void SetUserContentController(
      WKUserContentController* user_content_controller);

  // Sets a callback to be invoked when all initial static content rule lists
  // (e.g., block-local, mixed-content) have finished their compilation
  // attempts. If compilations are already complete when this is called,
  // the callback will be run immediately.
  void SetOnAllStaticListsCompiledCallback(base::OnceClosure callback);

  // Updates and re-installs the Content Blocker rules using any new state.
  // This may be asynchronous if a rule list hasn't been compiled yet, so
  // `callback` will be called after the mode is set. It will be called with
  // true if the update is successful and false otherwise (most likely because
  // rules were updated again before the first set of rules was fully
  // installed).
  void UpdateContentRuleLists(base::OnceCallback<void(bool)> callback);

  // Updates or clears the Script Blocking rule list.
  // - `json_rules`: An NSString containing the JSON rules for script
  //   blocking. Passing nil will uninstall the managed list from the controller
  //   and remove it from the list store.
  // - `callback`: Called with `success` (true if compilation succeeded) and an
  //   `error` if one occurred during compilation.
  void UpdateScriptBlockingRuleList(
      NSString* json_rules,
      base::OnceCallback<void(bool success, NSError* error)> callback);

 private:
  // Installs all compiled content rule lists managed by this provider onto the
  // `user_content_controller_`.
  void InstallContentRuleLists();

  // Uninstalls all content rule lists managed by this provider from the
  // `user_content_controller_`.
  void UninstallContentRuleLists();

  // Called after each static list compilation attempt. Decrements the pending
  // count and invokes `on_all_static_lists_compiled_callback_` if all have
  // completed.
  void MaybeSignalStaticListsCompiled();

  // Controller where rules are installed. Weak reference as the controller's
  // lifecycle might be managed elsewhere (e.g., by WKWebViewConfiguration).
  __weak WKUserContentController* user_content_controller_ = nullptr;

  // Compiled rule lists.
  WKContentRuleList* block_local_rule_list_ = nullptr;
  WKContentRuleList* mixed_content_autoupgrade_rule_list_ = nullptr;
  WKContentRuleList* script_blocking_rule_list_ = nullptr;

  // Callback for the UpdateContentRuleLists method.
  base::OnceCallback<void(bool)> update_callback_;

  // Counter for pending static list compilations initiated by the constructor.
  int pending_static_compilations_;

  // Callback to be invoked once all initial static lists have finished
  // compiling.
  base::OnceClosure on_all_static_lists_compiled_callback_;

  // Factory for creating weak pointers to this object, used for async
  // callbacks.
  base::WeakPtrFactory<WKContentRuleListProvider> weak_ptr_factory_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_
