// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_content_rule_list_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

WKContentRuleListProvider::WKContentRuleListProvider()
    : weak_ptr_factory_(this) {
  base::WeakPtr<WKContentRuleListProvider> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:@"block-local"
                   encodedContentRuleList:CreateLocalBlockingJsonRuleList()
                        completionHandler:^(WKContentRuleList* rule_list,
                                            NSError* error) {
                          if (!weak_this.get()) {
                            return;
                          }
                          block_local_rule_list_ = rule_list;
                          InstallContentRuleLists();
                        }];

  // Auto-upgrade mixed content.
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:@"mixed-content-autoupgrade"
                   encodedContentRuleList:
                       CreateMixedContentAutoUpgradeJsonRuleList()
                        completionHandler:^(WKContentRuleList* rule_list,
                                            NSError* error) {
                          if (!weak_this.get()) {
                            return;
                          }
                          mixed_content_autoupgrade_rule_list_ = rule_list;
                          InstallContentRuleLists();
                        }];
}

WKContentRuleListProvider::~WKContentRuleListProvider() {}

void WKContentRuleListProvider::UpdateContentRuleLists(
    base::OnceCallback<void(bool)> callback) {
  if (update_callback_) {
    std::move(update_callback_).Run(false);
  }
  update_callback_ = std::move(callback);
  InstallContentRuleLists();
}

void WKContentRuleListProvider::InstallContentRuleLists() {
  UninstallContentRuleLists();

  if (block_local_rule_list_) {
    [user_content_controller_ addContentRuleList:block_local_rule_list_];
  }
  if (mixed_content_autoupgrade_rule_list_) {
    [user_content_controller_
        addContentRuleList:mixed_content_autoupgrade_rule_list_];
  }
}

void WKContentRuleListProvider::UninstallContentRuleLists() {
  if (block_local_rule_list_) {
    [user_content_controller_ removeContentRuleList:block_local_rule_list_];
  }
  if (mixed_content_autoupgrade_rule_list_) {
    [user_content_controller_
        removeContentRuleList:mixed_content_autoupgrade_rule_list_];
  }
}

void WKContentRuleListProvider::SetUserContentController(
    WKUserContentController* user_content_controller) {
  user_content_controller_ = user_content_controller;
  InstallContentRuleLists();
}

}  // namespace web
