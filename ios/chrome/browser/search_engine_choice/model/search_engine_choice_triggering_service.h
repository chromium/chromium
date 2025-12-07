// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_H_

#import "base/memory/raw_ref.h"
#import "components/keyed_service/core/keyed_service.h"

namespace search_engines {
class SearchEngineChoiceService;
enum class SearchEngineChoiceScreenConditions;
}  // namespace search_engines
namespace policy {
class PolicyService;
}
class TemplateURLService;
class PrefService;

namespace ios {

// Service that is responsible for running dynamic checks to see if the search
// engine choice screen should be shown.
// More or less equivalent to `SearchEngineChoiceDialogService` on desktop.
class SearchEngineChoiceTriggeringService : public KeyedService {
 public:
  SearchEngineChoiceTriggeringService(
      PrefService& profile_prefs,
      const policy::PolicyService& policy_service,
      search_engines::SearchEngineChoiceService& search_engine_choice_service,
      const TemplateURLService& template_url_service);
  ~SearchEngineChoiceTriggeringService() override;

  SearchEngineChoiceTriggeringService(
      const SearchEngineChoiceTriggeringService&) = delete;
  SearchEngineChoiceTriggeringService& operator=(
      const SearchEngineChoiceTriggeringService&) = delete;

  // Returns eligibility status for newly triggering a choice screen.
  search_engines::SearchEngineChoiceScreenConditions
  EvaluateTriggeringConditions(bool is_first_run_entrypoint,
                               bool app_started_via_external_intent);

 private:
  const raw_ref<PrefService> profile_prefs_;
  const raw_ref<const policy::PolicyService> policy_service_;
  const raw_ref<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  const raw_ref<const TemplateURLService> template_url_service_;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_MODEL_SEARCH_ENGINE_CHOICE_TRIGGERING_SERVICE_H_
