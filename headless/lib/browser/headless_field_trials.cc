// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_field_trials.h"

#include <functional>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace headless {

namespace {

// A simple concrete implementation of the EnabledStateProvider interface.
class HeadlessEnabledStateProvider : public metrics::EnabledStateProvider {
 public:
  HeadlessEnabledStateProvider(bool consent, bool enabled)
      : consent_(consent), enabled_(enabled) {}

  HeadlessEnabledStateProvider(const HeadlessEnabledStateProvider&) = delete;
  HeadlessEnabledStateProvider& operator=(const HeadlessEnabledStateProvider&) =
      delete;

  ~HeadlessEnabledStateProvider() override = default;

  // metrics::EnabledStateProvider
  bool IsConsentGiven() const override { return consent_; }
  bool IsReportingEnabled() const override { return enabled_; }

 private:
  bool consent_;
  bool enabled_;
};

class HeadlessVariationsServiceClient
    : public variations::VariationsServiceClient {
 public:
  HeadlessVariationsServiceClient() = default;
  ~HeadlessVariationsServiceClient() override = default;

  // variations::VariationsServiceClient:
  base::Version GetVersionForSimulation() override { return base::Version(); }
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    return false;
  }
  bool IsEnterprise() override { return false; }

  void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) override {}
};

}  // namespace

bool ShouldEnableFieldTrials() {
  static bool should_enable_field_trials =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableFieldTrialTestingConfig);
  return should_enable_field_trials;
}

void SetUpFieldTrials(PrefService* local_state,
                      const base::FilePath& user_data_dir) {
  CHECK(local_state);

  HeadlessEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                      /*enabled=*/false);

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(local_state, &enabled_state_provider,
                                           std::wstring(), user_data_dir);

  metrics_state_manager->InstantiateFieldTrialList();

  HeadlessVariationsServiceClient variations_service_client;
  variations::VariationsFieldTrialCreator field_trial_creator(
      &variations_service_client,
      std::make_unique<variations::VariationsSeedStore>(
          local_state, /*initial_seed=*/nullptr,
          /*signature_verification_enabled=*/true,
          std::make_unique<variations::VariationsSafeSeedStoreLocalState>(
              local_state)),
      variations::UIStringOverrider(),
      /*limited_entropy_synthetic_trial=*/nullptr);

  variations::SafeSeedManager safe_seed_manager(local_state);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Overrides for content/common and lower layers' switches.
  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      content::GetSwitchDependentFeatureOverrides(command_line);

  // Setup field trials directly without help of the variations service.
  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();
  variations::PlatformFieldTrials platform_field_trials;
  variations::SyntheticTrialRegistry synthetic_trial_registry;
  field_trial_creator.SetUpFieldTrials(
      variation_ids,
      command_line.GetSwitchValueASCII(
          variations::switches::kForceVariationIds),
      feature_overrides, std::move(feature_list), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/false);
}

}  // namespace headless
