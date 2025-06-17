// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"

namespace web {

WKContentRuleListProvider::WKContentRuleListProvider() = default;

WKContentRuleListProvider::~WKContentRuleListProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Remove all managed rule lists from the web view.
  UninstallAllRuleLists();
}

void WKContentRuleListProvider::SetUserContentController(
    WKUserContentController* user_content_controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Remove all managed rule lists from the existing controller.
  UninstallAllRuleLists();
  user_content_controller_ = user_content_controller;
  // Install all rule lists into the new controller.
  InstallAllRuleLists();
}

void WKContentRuleListProvider::UpdateRuleList(RuleListKey key,
                                               std::string json_rules,
                                               OperationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementPendingOperations();

  NSString* identifier = base::SysUTF8ToNSString(key);
  NSString* rules = base::SysUTF8ToNSString(json_rules);

  // The completion handler is now bound to a private member function. The
  // WeakPtr ensures that if `this` is destroyed, the callback is not run.
  void (^completion_handler)(WKContentRuleList*, NSError*) =
      base::CallbackToBlock(base::BindOnce(
          &WKContentRuleListProvider::OnRuleListCompiled,
          weak_ptr_factory_.GetWeakPtr(), key, std::move(callback)));

  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:identifier
                   encodedContentRuleList:rules
                        completionHandler:completion_handler];
}

void WKContentRuleListProvider::RemoveRuleList(RuleListKey key,
                                               OperationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = compiled_lists_.find(key);
  if (it == compiled_lists_.end()) {
    // If the list is not tracked, assume it's not in the store either.
    // Just run the callback with success.
    std::move(callback).Run(nil);
    return;
  }
  // If the list is tracked, remove it from the content controller to
  // immediately stop its application.
  [user_content_controller_ removeContentRuleList:it->second];

  // Now, asynchronously remove it from the persistent WKContentRuleListStore.
  IncrementPendingOperations();
  NSString* identifier = base::SysUTF8ToNSString(key);
  [WKContentRuleListStore.defaultStore
      removeContentRuleListForIdentifier:identifier
                       completionHandler:
                           base::CallbackToBlock(base::BindOnce(
                               &WKContentRuleListProvider::OnRuleListRemoved,
                               weak_ptr_factory_.GetWeakPtr(), key,
                               std::move(callback)))];
}

void WKContentRuleListProvider::SetIdleCallbackForTesting(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_callback_for_testing_ = std::move(callback);

  // If the provider is already idle at the time the callback is set,
  // the caller should be notified.
  if (pending_operations_count_ == 0u && idle_callback_for_testing_) {
    // Post as a task to the current message loop to avoid re-entrancy and
    // maintain a consistent asynchronous feel for the callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, idle_callback_for_testing_);
  }
}

// Private methods

void WKContentRuleListProvider::OnRuleListCompiled(RuleListKey key,
                                                   OperationCallback callback,
                                                   WKContentRuleList* rule_list,
                                                   NSError* error) {
  if (error || !rule_list) {
    SCOPED_CRASH_KEY_BOOL("WKContentRuleListProvider", "rule_list",
                          rule_list != nullptr);
    SCOPED_CRASH_KEY_STRING64("WKContentRuleListProvider", "key", key);
    SCOPED_CRASH_KEY_STRING256("WKContentRuleListProvider", "error",
                               base::SysNSStringToUTF8(error.description));
    base::debug::DumpWithoutCrashing();
  } else {
    // If a list for this key already exists, it's an update. Remove the
    // old one from the controller before adding the new one.
    auto it = compiled_lists_.find(key);
    if (it != compiled_lists_.end()) {
      [user_content_controller_ removeContentRuleList:it->second];
    }

    // Store the key for future operations.
    compiled_lists_[key] = rule_list;

    // Install the newly compiled list into the content controller.
    [user_content_controller_ addContentRuleList:rule_list];
  }

  // Notify the original caller of the result and decrement the counter.
  std::move(callback).Run(error);
  DecrementPendingOperations();
}

void WKContentRuleListProvider::OnRuleListRemoved(RuleListKey key,
                                                  OperationCallback callback,
                                                  NSError* error) {
  // A "not found" error is not a failure for a removal operation. The goal is
  // to ensure the list is gone from the store.
  if (error && ([error.domain isEqualToString:WKErrorDomain] &&
                error.code == WKErrorContentRuleListStoreLookUpFailed)) {
    error = nil;
  }
  // Only remove from the internal map on success.
  if (!error) {
    compiled_lists_.erase(key);
  }
  std::move(callback).Run(error);
  DecrementPendingOperations();
}

void WKContentRuleListProvider::InstallAllRuleLists() {
  for (const auto& [key, rule_list] : compiled_lists_) {
    [user_content_controller_ addContentRuleList:rule_list];
  }
}

void WKContentRuleListProvider::UninstallAllRuleLists() {
  for (const auto& [key, rule_list] : compiled_lists_) {
    [user_content_controller_ removeContentRuleList:rule_list];
  }
}

void WKContentRuleListProvider::IncrementPendingOperations() {
  pending_operations_count_++;
}

void WKContentRuleListProvider::DecrementPendingOperations() {
  DCHECK_GT(pending_operations_count_, 0u);
  pending_operations_count_--;
  if (pending_operations_count_ == 0u && idle_callback_for_testing_) {
    idle_callback_for_testing_.Run();
  }
}

}  // namespace web
