// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content_manager/content_rule_list_manager_impl.h"

#import "base/check.h"
#import "base/sequence_checker.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace {
// The key used to attach the ContentRuleListManagerImpl to the BrowserState.
const char kContentRuleListManagerKey[] = "content_rule_list_manager";

// Helper function to get the underlying rule list provider from the
// BrowserState.
web::WKContentRuleListProvider& GetRuleListProvider(
    web::BrowserState* browser_state) {
  return web::WKWebViewConfigurationProvider::FromBrowserState(browser_state)
      .GetContentRuleListProvider();
}

}  // namespace

namespace web {

// static
ContentRuleListManager& ContentRuleListManager::FromBrowserState(
    BrowserState* browser_state) {
  CHECK(browser_state);
  if (!browser_state->GetUserData(kContentRuleListManagerKey)) {
    browser_state->SetUserData(
        kContentRuleListManagerKey,
        std::make_unique<ContentRuleListManagerImpl>(browser_state));
  }
  return *static_cast<ContentRuleListManagerImpl*>(
      browser_state->GetUserData(kContentRuleListManagerKey));
}

ContentRuleListManagerImpl::ContentRuleListManagerImpl(
    BrowserState* browser_state)
    : browser_state_(browser_state) {
  CHECK(browser_state_);
}

ContentRuleListManagerImpl::~ContentRuleListManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ContentRuleListManagerImpl::UpdateRuleList(
    const RuleListKey& list_key,
    std::string rules_json,
    OperationCallback completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetRuleListProvider(browser_state_)
      .UpdateRuleList(list_key, std::move(rules_json),
                      std::move(completion_callback));
}

void ContentRuleListManagerImpl::RemoveRuleList(
    const RuleListKey& list_key,
    OperationCallback completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetRuleListProvider(browser_state_)
      .RemoveRuleList(list_key, std::move(completion_callback));
}

}  // namespace web
