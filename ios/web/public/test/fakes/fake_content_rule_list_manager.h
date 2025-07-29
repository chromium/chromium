// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_CONTENT_RULE_LIST_MANAGER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_CONTENT_RULE_LIST_MANAGER_H_

#import "base/functional/callback.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"

namespace web {

// A fake ContentRuleListManager for testing.
// This class allows tests to inspect which rule lists were updated or removed
// and to simulate the completion of asynchronous operations.
class FakeContentRuleListManager : public ContentRuleListManager {
 public:
  FakeContentRuleListManager();
  ~FakeContentRuleListManager() override;

  // ContentRuleListManager methods.
  void UpdateRuleList(const std::string& rule_list_name,
                      std::string rule_list_json,
                      base::OnceCallback<void(NSError*)> callback) override;
  void RemoveRuleList(const std::string& rule_list_name,
                      base::OnceCallback<void(NSError*)> callback) override;

  // Methods for testing.

  // Returns the key of the last rule list that was updated.
  const std::string& last_update_key() const { return last_update_key_; }
  // Returns the JSON of the last rule list that was updated.
  const std::string& last_update_json() const { return last_update_json_; }
  // Returns the key of the last rule list that was removed.
  const std::string& last_remove_key() const { return last_remove_key_; }

  // Invokes the stored completion callback with the given `error`.
  // A nil `error` simulates a successful operation.
  void InvokeCompletionCallback(NSError* error);

 private:
  std::string last_update_key_;
  std::string last_update_json_;
  std::string last_remove_key_;
  base::OnceCallback<void(NSError*)> completion_callback_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_CONTENT_RULE_LIST_MANAGER_H_
