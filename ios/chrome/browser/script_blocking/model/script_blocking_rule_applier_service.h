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
#import "ios/web/public/content_manager/content_rule_list_manager.h"

@class NSError;

// Service that applies script blocking rule lists to a profile.
class ScriptBlockingRuleApplierService
    : public KeyedService,
      public script_blocking::ContentRuleListData::Observer {
 public:
  // The unique identifier for the script blocking rule list managed by this
  // service. This key is passed to the ContentRuleListManager used by this
  // service, which is associated with a profile.
  static constexpr char kScriptBlockingRuleListKey[] = "script_blocking_rules";

  explicit ScriptBlockingRuleApplierService(
      web::ContentRuleListManager& content_rule_list_manager);
  ~ScriptBlockingRuleApplierService() override;

  ScriptBlockingRuleApplierService(const ScriptBlockingRuleApplierService&) =
      delete;
  ScriptBlockingRuleApplierService& operator=(
      const ScriptBlockingRuleApplierService&) = delete;

 private:
  // KeyedService:
  void Shutdown() override;

  // script_blocking::ContentRuleListData::Observer:
  void OnScriptBlockingRuleListUpdated(const std::string& rules_json) override;

 private:
  // Applies the given rules to the profile.
  void ApplyRules(const std::string& rules_json);

  // Handles the completion of the rule update operation.
  void OnRuleUpdateCompleted(NSError* error);

  SEQUENCE_CHECKER(sequence_checker_);

  // The ContentRuleListManager used by this service to apply rules.
  const raw_ref<web::ContentRuleListManager> content_rule_list_manager_;

  // Observation of the ContentRuleListData, which provides the rule list to
  // this service.
  base::ScopedObservation<script_blocking::ContentRuleListData,
                          script_blocking::ContentRuleListData::Observer>
      content_rule_list_observation_{this};

  base::WeakPtrFactory<ScriptBlockingRuleApplierService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SCRIPT_BLOCKING_MODEL_SCRIPT_BLOCKING_RULE_APPLIER_SERVICE_H_
