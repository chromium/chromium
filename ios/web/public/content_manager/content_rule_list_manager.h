// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_CONTENT_MANAGER_CONTENT_RULE_LIST_MANAGER_H_
#define IOS_WEB_PUBLIC_CONTENT_MANAGER_CONTENT_RULE_LIST_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"

@class NSError;

namespace web {

class BrowserState;

// A generic service for managing WebKit content rule lists for a specific
// BrowserState.
class ContentRuleListManager {
 public:
  // A unique identifier for a content rule list.
  using RuleListKey = std::string;

  // Callback for async operations.
  using OperationCallback = base::OnceCallback<void(NSError* error)>;

  // Returns the ContentRuleListManager for the given `browser_state`.
  // The returned service's lifetime is tied to the BrowserState and it is
  // created on first access.
  // BrowserState must not be null.
  static ContentRuleListManager& FromBrowserState(BrowserState* browser_state);

  virtual ~ContentRuleListManager() = default;

  // Asynchronously updates or creates a content rule list identified by
  // `list_key`.
  // - `list_key`: The unique, stable identifier for the rule list to update.
  // - `rules_json`: The JSON string containing the rules.
  // - `completion_callback`: Invoked when the operation is complete.
  virtual void UpdateRuleList(const RuleListKey& list_key,
                              std::string rules_json,
                              OperationCallback completion_callback) = 0;

  // Asynchronously removes the content rule list for `list_key`.
  // - `list_key`: The unique, stable identifier for the rule list to remove.
  // - `completion_callback`: Invoked when the operation is complete.
  virtual void RemoveRuleList(const std::string& list_key,
                              OperationCallback completion_callback) = 0;

 protected:
  ContentRuleListManager() = default;

  ContentRuleListManager(const ContentRuleListManager&) = delete;
  ContentRuleListManager& operator=(const ContentRuleListManager&) = delete;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_CONTENT_MANAGER_CONTENT_RULE_LIST_MANAGER_H_
