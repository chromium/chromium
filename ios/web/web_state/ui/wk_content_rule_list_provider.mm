// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"

namespace web {

WKContentRuleListProvider::WKContentRuleListProvider(
    const base::FilePath& state_path) {
  rule_list_store_ = [WKContentRuleListStore
      storeWithURL:base::apple::FilePathToNSURL(state_path)];
  CHECK(rule_list_store_);
}

WKContentRuleListProvider::~WKContentRuleListProvider() = default;

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

  NSString* identifier = base::SysUTF8ToNSString(key);
  NSString* rules = base::SysUTF8ToNSString(json_rules);

  void (^completion_handler)(WKContentRuleList*, NSError*) =
      base::CallbackToBlock(
          base::BindOnce(&WKContentRuleListProvider::OnRuleListCompiled,
                         weak_ptr_factory_.GetWeakPtr(), key,
                         std::move(callback), base::TimeTicks::Now()));

  [rule_list_store_ compileContentRuleListForIdentifier:identifier
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
  NSString* identifier = base::SysUTF8ToNSString(key);
  [rule_list_store_
      removeContentRuleListForIdentifier:identifier
                       completionHandler:
                           base::CallbackToBlock(base::BindOnce(
                               &WKContentRuleListProvider::OnRuleListRemoved,
                               weak_ptr_factory_.GetWeakPtr(), key,
                               std::move(callback)))];
}

// Private methods

void WKContentRuleListProvider::OnRuleListCompiled(RuleListKey key,
                                                   OperationCallback callback,
                                                   base::TimeTicks start_time,
                                                   WKContentRuleList* rule_list,
                                                   NSError* error) {
  const base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  // This case is not expected. WebKit should return either a list or an
  // error.
  if (!rule_list && !error) {
    error = [NSError
        errorWithDomain:WKErrorDomain
                   code:WKErrorUnknown
               userInfo:@{
                 NSLocalizedDescriptionKey :
                     @"Rule list compilation returned no list and no error."
               }];
  }

  base::UmaHistogramBoolean(
      "IOS.ContentRuleListProvider.Compile.Success." + key, error == nil);

  if (error) {
    base::UmaHistogramSparse("IOS.ContentRuleListProvider.Compile.Error." + key,
                             error.code);
  } else {
    base::UmaHistogramTimes("IOS.ContentRuleListProvider.Compile.Time." + key,
                            duration);

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

  // Notify the original caller of the result.
  std::move(callback).Run(error);
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

  base::UmaHistogramBoolean("IOS.ContentRuleListProvider.Remove.Success." + key,
                            error == nil);

  // Only remove from the internal map on success.
  if (!error) {
    compiled_lists_.erase(key);
  }
  std::move(callback).Run(error);
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

}  // namespace web
