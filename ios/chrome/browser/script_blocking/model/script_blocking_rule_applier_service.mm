// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/metrics/histogram_functions.h"
#import "base/timer/elapsed_timer.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#import "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"
#import "components/privacy_sandbox/tracking_protection_settings.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"
#import "third_party/re2/src/re2/re2.h"

namespace {

std::string GetProfileTypeSuffix(bool is_incognito) {
  return is_incognito ? "Incognito" : "Regular";
}

base::Value::Dict CreateExceptionRule(const std::string& scheme,
                                      const std::string& domain) {
  // Escape any special regex characters in the scheme and domain.
  std::string escaped_scheme = RE2::QuoteMeta(scheme);
  std::string escaped_domain = RE2::QuoteMeta(domain);

  // This regex is constructed to match the given domain and all of its
  // subdomains for a specific scheme. For example, for the site
  // "example.com", it should match "https://example.com",
  // "https://sub.example.com", but not "http://example.com" or
  // "https://another-example.com".
  // - `^`: Matches the beginning of the URL.
  // - `(?:[^/.]*\\.)*`: A non-capturing group that matches any subdomain.
  //   It is optional to also match the bare domain.
  // - `(?:/.*)?`: An optional capturing group that matches the path, if any.
  // - `$`: Matches the end of the URL.
  base::Value::List top_url_list;
  top_url_list.Append("^" + escaped_scheme + "://(?:[^/.]*\\.)*" +
                      escaped_domain + "(?:/.*)?$");

  // The "trigger" dictionary specifies the conditions under which the rule's
  // action should be executed.
  base::Value::Dict trigger;
  // "if-top-url" provides a list of regular expressions to match against the
  // top-level URL of a page. If the page's URL matches any of these, the
  // condition is met.
  trigger.Set("if-top-url", std::move(top_url_list));
  // "url-filter" is a regular expression that is matched against the URL of a
  // network request. ".*" matches all requests.
  trigger.Set("url-filter", ".*");

  // The "action" dictionary defines what to do when the trigger conditions are
  // met.
  base::Value::Dict action;
  // "ignore-previous-rules" action type allows requests that would have been
  // blocked by earlier rules in the list. This effectively creates an
  // exception to the blocking rules.
  action.Set("type", "ignore-previous-rules");

  base::Value::Dict rule;
  rule.Set("trigger", std::move(trigger));
  rule.Set("action", std::move(action));
  return rule;
}

}  // namespace

using web::ContentRuleListManager;

ScriptBlockingRuleApplierService::ScriptBlockingRuleApplierService(
    ContentRuleListManager& content_rule_list_manager,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    bool is_incognito)
    : content_rule_list_manager_(content_rule_list_manager),
      tracking_protection_settings_(tracking_protection_settings),
      is_incognito_(is_incognito) {
  content_rule_list_observation_.Observe(
      &script_blocking::ContentRuleListData::GetInstance());
  tracking_protection_settings_observation_.Observe(
      tracking_protection_settings_);
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

void ScriptBlockingRuleApplierService::OnContentRuleListDataUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BuildAndApplyRules(
      FingerprintingProtectionRuleListApplyTrigger::kComponentUpdate);
}

void ScriptBlockingRuleApplierService::OnTrackingProtectionExceptionsChanged(
    const GURL& first_party_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BuildAndApplyRules(
      FingerprintingProtectionRuleListApplyTrigger::kExceptionsChanged);
}

void ScriptBlockingRuleApplierService::OnFpProtectionEnabledChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BuildAndApplyRules(
      FingerprintingProtectionRuleListApplyTrigger::kFpProtectionToggled);
}

void ScriptBlockingRuleApplierService::BuildAndApplyRules(
    FingerprintingProtectionRuleListApplyTrigger trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string suffix = GetProfileTypeSuffix(is_incognito_);
  base::UmaHistogramEnumeration(
      "IOS.FingerprintingProtection.RuleList.ApplyTrigger." + suffix, trigger);

  base::ElapsedTimer timer;
  std::optional<std::string> final_rules_json = BuildRules();
  base::UmaHistogramTimes(
      "IOS.FingerprintingProtection.RuleList.BuildTime." + suffix,
      timer.Elapsed());

  if (final_rules_json.has_value()) {
    content_rule_list_manager_->UpdateRuleList(kScriptBlockingRuleListKey,
                                               std::move(*final_rules_json),
                                               base::DoNothing());
  } else {
    content_rule_list_manager_->RemoveRuleList(kScriptBlockingRuleListKey,
                                               base::DoNothing());
  }
}

std::optional<std::string> ScriptBlockingRuleApplierService::BuildRules() {
  const std::string suffix = GetProfileTypeSuffix(is_incognito_);
  // TODO(crbug.com/436881800): Clean up the dry-run feature flag after the
  // experiment.
  bool is_dry_run = base::FeatureList::IsEnabled(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilteriOSDryRun);

  // In blocking mode (not a dry run), the feature is incognito-only.
  if (!is_dry_run && !tracking_protection_settings_->IsFpProtectionEnabled()) {
    base::UmaHistogramEnumeration(
        "IOS.FingerprintingProtection.RuleList.BuildOutcome." + suffix,
        FingerprintingProtectionRuleListBuildOutcome::kRemoveListFpDisabled);
    return std::nullopt;
  }

  const std::optional<std::string>& base_rules_json =
      script_blocking::ContentRuleListData::GetInstance().GetContentRuleList();

  // If the base rule list is not present, there are no rules to apply.
  if (!base_rules_json.has_value()) {
    base::UmaHistogramEnumeration(
        "IOS.FingerprintingProtection.RuleList.BuildOutcome." + suffix,
        FingerprintingProtectionRuleListBuildOutcome::kRemoveListNoBaseRules);
    return std::nullopt;
  }

  // Read the base anti-fingerprinting blocklist.
  std::optional<base::Value> rules_value = base::JSONReader::Read(
      *base_rules_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  // If the base rule list is empty or invalid, there are no rules to apply.
  // An exception list is meaningless without a base list.
  if (!rules_value || !rules_value->is_list() ||
      rules_value->GetList().empty()) {
    base::UmaHistogramEnumeration(
        "IOS.FingerprintingProtection.RuleList.BuildOutcome." + suffix,
        FingerprintingProtectionRuleListBuildOutcome::
            kRemoveListInvalidBaseRules);
    return std::nullopt;
  }

  // Apply exceptions to the rule list, if any.
  base::Value::List rules_list = std::move(rules_value->GetList());
  ContentSettingsForOneType exceptions =
      tracking_protection_settings_->GetTrackingProtectionExceptions();

  base::UmaHistogramCounts100(
      "IOS.FingerprintingProtection.RuleList.ExceptionCount." + suffix,
      exceptions.size());

  base::UmaHistogramEnumeration(
      "IOS.FingerprintingProtection.RuleList.BuildOutcome." + suffix,
      exceptions.empty() ? FingerprintingProtectionRuleListBuildOutcome::
                               kUpdateListNoExceptions
                         : FingerprintingProtectionRuleListBuildOutcome::
                               kUpdateListWithExceptions);
  for (const auto& exception : exceptions) {
    rules_list.Append(
        CreateExceptionRule(exception.secondary_pattern.GetScheme(),
                            exception.secondary_pattern.GetHost()));
  }

  base::UmaHistogramCounts1000(
      "IOS.FingerprintingProtection.RuleList.TotalRulesApplied." + suffix,
      rules_list.size());

  return base::WriteJson(rules_list);
}
