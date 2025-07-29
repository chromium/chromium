// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_content_rule_list_manager.h"

namespace web {

FakeContentRuleListManager::FakeContentRuleListManager() = default;
FakeContentRuleListManager::~FakeContentRuleListManager() = default;

void FakeContentRuleListManager::UpdateRuleList(
    const std::string& rule_list_name,
    std::string rule_list_json,
    base::OnceCallback<void(NSError*)> callback) {
  last_update_key_ = rule_list_name;
  last_update_json_ = rule_list_json;
  completion_callback_ = std::move(callback);
}

void FakeContentRuleListManager::RemoveRuleList(
    const std::string& rule_list_name,
    base::OnceCallback<void(NSError*)> callback) {
  last_remove_key_ = rule_list_name;
  completion_callback_ = std::move(callback);
}

void FakeContentRuleListManager::InvokeCompletionCallback(NSError* error) {
  if (completion_callback_) {
    std::move(completion_callback_).Run(error);
  }
}

}  // namespace web
