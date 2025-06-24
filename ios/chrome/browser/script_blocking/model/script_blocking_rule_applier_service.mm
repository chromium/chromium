// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

using web::BrowserState;
using web::ContentRuleListManager;

ScriptBlockingRuleApplierService::ScriptBlockingRuleApplierService(
    ContentRuleListManager& content_rule_list_manager)
    : content_rule_list_manager_(content_rule_list_manager) {
  content_rule_list_observation_.Observe(
      &script_blocking::ContentRuleListData::GetInstance());
}

ScriptBlockingRuleApplierService::~ScriptBlockingRuleApplierService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ScriptBlockingRuleApplierService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content_rule_list_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void ScriptBlockingRuleApplierService::OnScriptBlockingRuleListUpdated(
    const std::string& rules_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Use the appropriate method on the manager based on whether the rules
  // string is empty.
  if (rules_json.empty()) {
    content_rule_list_manager_->RemoveRuleList(
        kScriptBlockingRuleListKey,
        base::BindOnce(&ScriptBlockingRuleApplierService::OnRuleUpdateCompleted,
                       weak_factory_.GetWeakPtr()));
  } else {
    content_rule_list_manager_->UpdateRuleList(
        kScriptBlockingRuleListKey, rules_json,
        base::BindOnce(&ScriptBlockingRuleApplierService::OnRuleUpdateCompleted,
                       weak_factory_.GetWeakPtr()));
  }
}

void ScriptBlockingRuleApplierService::OnRuleUpdateCompleted(NSError* error) {
  if (error) {
    SCOPED_CRASH_KEY_STRING256("SBRuleApplierService", "error",
                               base::SysNSStringToUTF8(error.description));
    base::debug::DumpWithoutCrashing();
  }
}
