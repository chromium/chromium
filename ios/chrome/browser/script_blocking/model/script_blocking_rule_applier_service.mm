// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"
#import "components/privacy_sandbox/tracking_protection_settings.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"

namespace {

base::Value::Dict CreateExceptionRule(const std::string& domain) {
  base::Value::Dict rule;
  rule.SetByDottedPath("action.type", "ignore-previous-rules");
  rule.SetByDottedPath("trigger.if-domain",
                       base::Value::List().Append("*" + domain));
  rule.SetByDottedPath("trigger.url-filter", ".*");
  return rule;
}

}  // namespace

using web::ContentRuleListManager;

ScriptBlockingRuleApplierService::ScriptBlockingRuleApplierService(
    ContentRuleListManager& content_rule_list_manager,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings)
    : content_rule_list_manager_(content_rule_list_manager),
      tracking_protection_settings_(tracking_protection_settings) {
  content_rule_list_observation_.Observe(
      &script_blocking::ContentRuleListData::GetInstance());
  tracking_protection_settings_observation_.Observe(
      tracking_protection_settings_);
  const std::optional<std::string>& rules =
      script_blocking::ContentRuleListData::GetInstance().GetContentRuleList();
  ApplyRules(rules.has_value() ? *rules : "");
}

ScriptBlockingRuleApplierService::~ScriptBlockingRuleApplierService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ScriptBlockingRuleApplierService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content_rule_list_observation_.Reset();
  tracking_protection_settings_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
}

void ScriptBlockingRuleApplierService::OnScriptBlockingRuleListUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::optional<std::string>& rules =
      script_blocking::ContentRuleListData::GetInstance().GetContentRuleList();
  ApplyRules(rules.has_value() ? *rules : "");
}

void ScriptBlockingRuleApplierService::OnTrackingProtectionExceptionsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::optional<std::string>& rules =
      script_blocking::ContentRuleListData::GetInstance().GetContentRuleList();
  ApplyRules(rules.has_value() ? *rules : "");
}

void ScriptBlockingRuleApplierService::OnFpProtectionEnabledChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::optional<std::string>& rules =
      script_blocking::ContentRuleListData::GetInstance().GetContentRuleList();
  ApplyRules(rules.has_value() ? *rules : "");
}

void ScriptBlockingRuleApplierService::ApplyRules(
    const std::string& base_rules_json) {
  if (!tracking_protection_settings_->IsFpProtectionEnabled()) {
    content_rule_list_manager_->RemoveRuleList(
        kScriptBlockingRuleListKey,
        base::BindOnce(&ScriptBlockingRuleApplierService::OnRuleUpdateCompleted,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Read the base anti-fingerprinting blocklist.
  std::optional<base::Value> rules_value =
      base::JSONReader::Read(base_rules_json);

  // If the base rule list is empty or invalid, there are no rules to apply,
  // so any existing list should be removed. An exception list is meaningless
  // without a base list.
  if (!rules_value || !rules_value->is_list() ||
      rules_value->GetList().empty()) {
    content_rule_list_manager_->RemoveRuleList(
        kScriptBlockingRuleListKey,
        base::BindOnce(&ScriptBlockingRuleApplierService::OnRuleUpdateCompleted,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Apply exceptions to the rule list, if any.
  base::Value::List rules_list = std::move(rules_value->GetList());
  ContentSettingsForOneType exceptions =
      tracking_protection_settings_->GetTrackingProtectionExceptions();
  for (const auto& exception : exceptions) {
    rules_list.Append(
        CreateExceptionRule(exception.secondary_pattern.GetHost()));
  }

  std::string rules_json;
  base::JSONWriter::Write(rules_list, &rules_json);

  content_rule_list_manager_->UpdateRuleList(
      kScriptBlockingRuleListKey, rules_json,
      base::BindOnce(&ScriptBlockingRuleApplierService::OnRuleUpdateCompleted,
                     weak_factory_.GetWeakPtr()));
}

void ScriptBlockingRuleApplierService::OnRuleUpdateCompleted(NSError* error) {
  if (error) {
    SCOPED_CRASH_KEY_STRING256("SBRuleApplierService", "error",
                               base::SysNSStringToUTF8(error.description));
    base::debug::DumpWithoutCrashing();
  }
}
