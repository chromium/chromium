// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_
#define IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

@class NSError;
@class WKContentRuleList;
@class WKContentRuleListStore;
@class WKUserContentController;

namespace base {
class FilePath;
}

namespace web {

// A provider that handles compiling, storing, and applying WKContentRuleLists
// to a WKUserContentController.
//
// This class is not thread-safe and should only be accessed on the UI thread.
class WKContentRuleListProvider {
 public:
  // A unique identifier for a content rule list.
  using RuleListKey = std::string;

  // Callback invoked after an asynchronous operation completes. `error` will
  // be nil on success, and non-nil if compilation or removal failed.
  using OperationCallback = base::OnceCallback<void(NSError* error)>;

  // `state_path` - The path where the BrowserState data is stored, in which to
  // store content rule lists.
  explicit WKContentRuleListProvider(const base::FilePath& state_path);
  virtual ~WKContentRuleListProvider();

  WKContentRuleListProvider(const WKContentRuleListProvider&) = delete;
  WKContentRuleListProvider& operator=(const WKContentRuleListProvider&) =
      delete;

  // Sets the WKUserContentController that this provider will install its rules
  // on. This must be called before any rules can be applied.
  void SetUserContentController(
      WKUserContentController* user_content_controller);

  // Asynchronously creates or updates a content rule list identified by `key`.
  // The `callback` is invoked upon completion.
  virtual void UpdateRuleList(RuleListKey key,
                              std::string json_rules,
                              OperationCallback callback);

  // Asynchronously removes an existing content rule list identified by `key`.
  // The `callback` is invoked upon completion.
  virtual void RemoveRuleList(RuleListKey key, OperationCallback callback);

 private:
  // Installs all compiled content rule lists from `compiled_lists_` onto the
  // `user_content_controller_`.
  void InstallAllRuleLists();

  // Uninstalls all content rule lists currently tracked by this provider from
  // the `user_content_controller_`.
  void UninstallAllRuleLists();

  // Callback invoked when a rule list is compiled by the
  // WKContentRuleListStore.
  void OnRuleListCompiled(RuleListKey key,
                          OperationCallback callback,
                          base::TimeTicks start_time,
                          WKContentRuleList* rule_list,
                          NSError* error);

  // Callback invoked when a rule list is removed from the
  // WKContentRuleListStore.
  void OnRuleListRemoved(RuleListKey key,
                         OperationCallback callback,
                         NSError* error);

  SEQUENCE_CHECKER(sequence_checker_);

  // The user content controller that this provider will install its rules on.
  // Weak reference as the controller's lifecycle might be managed elsewhere
  // (e.g., by WKWebViewConfiguration).
  __weak WKUserContentController* user_content_controller_ = nullptr;
  // A map of all compiled lists, keyed by their identifier.
  std::map<RuleListKey, WKContentRuleList*> compiled_lists_;
  // The persistent store where content rule lists are stored.
  WKContentRuleListStore* __strong rule_list_store_ = nil;

  base::WeakPtrFactory<WKContentRuleListProvider> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_CONTENT_RULE_LIST_PROVIDER_H_
