// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_content_rule_list_util.h"

namespace web {
namespace {
NSString* const kScriptBlockingListIdentifier = @"script-blocking-list";
NSString* const kBlockLocalListIdentifier = @"block-local";
NSString* const kMixedContentAutoupgradeListIdentifier =
    @"mixed-content-autoupgrade";

// Number of static rule lists compiled in the constructor.
const int kInitialStaticListCompilationCount = 2;

}  // namespace

WKContentRuleListProvider::WKContentRuleListProvider()
    : pending_static_compilations_(kInitialStaticListCompilationCount),
      weak_ptr_factory_(this) {
  base::WeakPtr<WKContentRuleListProvider> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  // Compile "block-local" rules
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:kBlockLocalListIdentifier
                   encodedContentRuleList:CreateLocalBlockingJsonRuleList()
                        completionHandler:^(WKContentRuleList* rule_list,
                                            NSError* compile_error) {
                          if (!weak_this.get()) {
                            return;
                          }
                          if (compile_error) {
                            NSLog(@"Error compiling list '%@': %@",
                                  kBlockLocalListIdentifier, compile_error);
                          }
                          block_local_rule_list_ = rule_list;
                          InstallContentRuleLists();
                          MaybeSignalStaticListsCompiled();
                        }];

  // Compile "mixed-content-autoupgrade" rules
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:kMixedContentAutoupgradeListIdentifier
                   encodedContentRuleList:
                       CreateMixedContentAutoUpgradeJsonRuleList()
                        completionHandler:^(WKContentRuleList* rule_list,
                                            NSError* compile_error) {
                          if (!weak_this.get()) {
                            return;
                          }
                          if (compile_error) {
                            NSLog(@"Error compiling list '%@': %@",
                                  kMixedContentAutoupgradeListIdentifier,
                                  compile_error);
                          }
                          mixed_content_autoupgrade_rule_list_ = rule_list;
                          InstallContentRuleLists();
                          MaybeSignalStaticListsCompiled();
                        }];

  // script_blocking_rule_list_ is initialized to nullptr and added later.
}

WKContentRuleListProvider::~WKContentRuleListProvider() {}

void WKContentRuleListProvider::SetOnAllStaticListsCompiledCallback(
    base::OnceClosure callback) {
  // If compilations are already done, run the callback immediately.
  // Otherwise, store it.
  if (pending_static_compilations_ == 0) {
    if (callback) {
      std::move(callback).Run();
    }
  } else {
    on_all_static_lists_compiled_callback_ = std::move(callback);
  }
}

void WKContentRuleListProvider::MaybeSignalStaticListsCompiled() {
  // This method is called from the completion handler of each static list
  // compilation.
  if (pending_static_compilations_ > 0) {
    pending_static_compilations_--;
  }

  if (pending_static_compilations_ == 0 &&
      on_all_static_lists_compiled_callback_) {
    std::move(on_all_static_lists_compiled_callback_).Run();
  }
}

void WKContentRuleListProvider::UpdateContentRuleLists(
    base::OnceCallback<void(bool)> callback) {
  if (update_callback_) {
    std::move(update_callback_).Run(false);
  }
  update_callback_ = std::move(callback);
  // Re-apply all currently configured rules.
  InstallContentRuleLists();

  if (update_callback_) {
    std::move(update_callback_).Run(true);
  }
}

void WKContentRuleListProvider::UpdateScriptBlockingRuleList(
    NSString* json_rules,
    base::OnceCallback<void(bool success, NSError* error)> callback) {
  base::WeakPtr<WKContentRuleListProvider> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  __block base::OnceCallback<void(bool success, NSError* error)>
      block_callback = std::move(callback);

  // Handle removal
  if (!json_rules) {
    if (!script_blocking_rule_list_) {  // List already nil, considered success.
      std::move(block_callback).Run(true, nil);
      return;
    }

    if (user_content_controller_) {
      [user_content_controller_
          removeContentRuleList:script_blocking_rule_list_];
    }

    [WKContentRuleListStore.defaultStore
        removeContentRuleListForIdentifier:kScriptBlockingListIdentifier
                         completionHandler:^(NSError* remove_error) {
                           if (!weak_this.get()) {
                             return;
                           }
                           bool success = (remove_error == nil);
                           if (!success) {
                             NSLog(@"Error removing list '%@' from store: %@",
                                   kScriptBlockingListIdentifier, remove_error);
                           } else {
                             script_blocking_rule_list_ = nil;
                           }
                           std::move(block_callback).Run(success, remove_error);
                         }];
    return;
  }

  // Handle compilation and installation
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:kScriptBlockingListIdentifier
                   encodedContentRuleList:json_rules
                        completionHandler:^(WKContentRuleList* new_rule_list,
                                            NSError* compile_error) {
                          if (!weak_this.get()) {
                            return;
                          }

                          if (compile_error) {
                            NSLog(@"Error compiling list '%@': %@. Keeping "
                                  @"existing list if list was present.",
                                  kScriptBlockingListIdentifier, compile_error);
                            std::move(block_callback).Run(false, compile_error);
                          } else {
                            UninstallContentRuleLists();
                            script_blocking_rule_list_ = new_rule_list;
                            InstallContentRuleLists();
                            std::move(block_callback).Run(true, nil);
                          }
                        }];
}

void WKContentRuleListProvider::InstallContentRuleLists() {
  // Uninstall all lists managed by this provider to ensure a clean slate.
  // This prevents adding the same list multiple times if Install is called
  // repeatedly.
  UninstallContentRuleLists();

  // Add the current lists.
  if (block_local_rule_list_) {
    [user_content_controller_ addContentRuleList:block_local_rule_list_];
  }
  if (mixed_content_autoupgrade_rule_list_) {
    [user_content_controller_
        addContentRuleList:mixed_content_autoupgrade_rule_list_];
  }
  if (script_blocking_rule_list_) {
    [user_content_controller_ addContentRuleList:script_blocking_rule_list_];
  }
}

void WKContentRuleListProvider::UninstallContentRuleLists() {
  // It's safe to call remove even if the list wasn't added or is nil.
  if (block_local_rule_list_) {
    [user_content_controller_ removeContentRuleList:block_local_rule_list_];
  }
  if (mixed_content_autoupgrade_rule_list_) {
    [user_content_controller_
        removeContentRuleList:mixed_content_autoupgrade_rule_list_];
  }
  if (script_blocking_rule_list_) {
    [user_content_controller_ removeContentRuleList:script_blocking_rule_list_];
  }
}

void WKContentRuleListProvider::SetUserContentController(
    WKUserContentController* user_content_controller) {
  if (user_content_controller_ == user_content_controller) {
    return;
  }

  // If there was a previous controller, remove rules from it first.
  if (user_content_controller_) {
    UninstallContentRuleLists();  // Acts on the old controller.
  }

  // Update the controller reference.
  user_content_controller_ = user_content_controller;

  // If a new controller is set, install the current rules into it.
  if (user_content_controller_) {
    InstallContentRuleLists();
  }
}

}  // namespace web
