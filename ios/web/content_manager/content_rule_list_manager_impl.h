// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_MANAGER_CONTENT_RULE_LIST_MANAGER_IMPL_H_
#define IOS_WEB_CONTENT_MANAGER_CONTENT_RULE_LIST_MANAGER_IMPL_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/supports_user_data.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"

namespace web {

class BrowserState;

// Implementation of the ContentRuleListManager that uses SupportsUserData to
// attach its lifetime to a BrowserState.
class ContentRuleListManagerImpl : public ContentRuleListManager,
                                   public base::SupportsUserData::Data {
 public:
  explicit ContentRuleListManagerImpl(BrowserState* browser_state);
  ~ContentRuleListManagerImpl() override;

  ContentRuleListManagerImpl(const ContentRuleListManagerImpl&) = delete;
  ContentRuleListManagerImpl& operator=(const ContentRuleListManagerImpl&) =
      delete;

  // ContentRuleListManager implementation:
  void UpdateRuleList(const RuleListKey& list_key,
                      std::string rules_json,
                      OperationCallback completion_callback) override;
  void RemoveRuleList(const RuleListKey& list_key,
                      OperationCallback completion_callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // The BrowserState this service is associated with. Not owned.
  const raw_ptr<BrowserState> browser_state_;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_MANAGER_CONTENT_RULE_LIST_MANAGER_IMPL_H_
