// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_H_
#define IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/privacy_sandbox/tracking_protection_settings_observer.h"

// LINT.IfChange(FingerprintingProtectionRuleListApplyTrigger)
enum class FingerprintingProtectionRuleListApplyTrigger {
  kComponentUpdate = 0,
  kFpProtectionToggled = 1,
  kExceptionsChanged = 2,
  kMaxValue = kExceptionsChanged,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:FingerprintingProtectionRuleListApplyTrigger)

// LINT.IfChange(FingerprintingProtectionRuleListBuildOutcome)
enum class FingerprintingProtectionRuleListBuildOutcome {
  kUpdateListWithExceptions = 0,
  kUpdateListNoExceptions = 1,
  kRemoveListFpDisabled = 2,
  kRemoveListNoBaseRules = 3,
  kRemoveListInvalidBaseRules = 4,
  kMaxValue = kRemoveListInvalidBaseRules,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:FingerprintingProtectionRuleListBuildOutcome)

namespace privacy_sandbox {
class TrackingProtectionSettings;
}
namespace web {
class ContentRuleListManager;
}
@class NSError;

// Service that applies script blocking rule lists to a profile.
class ScriptBlockingRuleApplierService
    : public KeyedService,
      public script_blocking::ContentRuleListData::Observer,
      public privacy_sandbox::TrackingProtectionSettingsObserver {
 public:
  // The unique identifier for the script blocking rule list managed by this
  // service. This key is passed to the ContentRuleListManager used by this
  // service, which is associated with a profile.
  static constexpr char kScriptBlockingRuleListKey[] = "ScriptBlockingRules";

  ScriptBlockingRuleApplierService(
      web::ContentRuleListManager& content_rule_list_manager,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      bool is_incognito);
  ~ScriptBlockingRuleApplierService() override;

  ScriptBlockingRuleApplierService(const ScriptBlockingRuleApplierService&) =
      delete;
  ScriptBlockingRuleApplierService& operator=(
      const ScriptBlockingRuleApplierService&) = delete;

 private:
  // KeyedService:
  void Shutdown() override;

  // script_blocking::ContentRuleListData::Observer:
  void OnContentRuleListDataUpdated() override;

  // privacy_sandbox::TrackingProtectionSettingsObserver:
  void OnTrackingProtectionExceptionsChanged(
      const GURL& first_party_url) override;
  void OnFpProtectionEnabledChanged() override;

  // Determines the rules to be applied based on the current state and
  // applies them.
  // - `trigger`: The reason why the rule application is being initiated, for
  //   metrics.
  void BuildAndApplyRules(FingerprintingProtectionRuleListApplyTrigger trigger);

  // Builds the content of the rule list based on the base list and exceptions.
  // Returns std::nullopt if no rules should be applied (i.e., the list
  // should be removed).
  std::optional<std::string> BuildRules();

  SEQUENCE_CHECKER(sequence_checker_);

  // The ContentRuleListManager used by this service to apply rules.
  const raw_ref<web::ContentRuleListManager> content_rule_list_manager_;

  // The TrackingProtectionSettings used to determine if fingerprinting
  // protection is enabled.
  const raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;

  // Whether the service is for an incognito profile.
  const bool is_incognito_;

  // Observation of the ContentRuleListData, which provides the rule list to
  // this service.
  base::ScopedObservation<script_blocking::ContentRuleListData,
                          script_blocking::ContentRuleListData::Observer>
      content_rule_list_observation_{this};

  // Observation of the TrackingProtectionSettings.
  base::ScopedObservation<privacy_sandbox::TrackingProtectionSettings,
                          privacy_sandbox::TrackingProtectionSettingsObserver>
      tracking_protection_settings_observation_{this};

  base::WeakPtrFactory<ScriptBlockingRuleApplierService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_H_
