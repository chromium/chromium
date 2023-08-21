// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter.h"

#import "base/values.h"
#import "components/feature_engagement/public/configuration.h"
#import "components/feature_engagement/public/feature_configurations.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

PromosManagerEventExporter::PromosManagerEventExporter(PrefService* local_state)
    : local_state_(local_state) {}

PromosManagerEventExporter::~PromosManagerEventExporter() = default;

void PromosManagerEventExporter::InitializePromoConfigs(
    PromoConfigsSet promo_configs) {
  promo_configs_ = std::move(promo_configs);
}

void PromosManagerEventExporter::ExportEvents(ExportEventsCallback callback) {
  std::vector<EventData> events_to_migrate;

  // Load events directly from prefs so they can be updated as well.
  ScopedListPrefUpdate update(local_state_,
                              prefs::kIosPromosManagerImpressions);
  for (auto& single_impression : update.Get()) {
    absl::optional<promos_manager::Impression> impression =
        promos_manager::ImpressionFromDict(single_impression.GetDict());
    if (!impression || impression->feature_engagement_migration_completed) {
      continue;
    }
    auto it = promo_configs_.find(impression->promo);
    if (it == promo_configs_.end() || !it->feature_engagement_feature) {
      continue;
    }

    // Get the feature engagement tracker config for this feature to find the
    // correct event name to export.
    absl::optional<feature_engagement::FeatureConfig> feature_config =
        feature_engagement::GetClientSideFeatureConfig(
            it->feature_engagement_feature);
    if (!feature_config) {
      continue;
    }

    events_to_migrate.emplace_back(feature_config->trigger.name,
                                   impression->day);
    single_impression.GetDict().Set(
        promos_manager::kImpressionFeatureEngagementMigrationCompletedKey,
        true);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(events_to_migrate)));
}

base::WeakPtr<PromosManagerEventExporter>
PromosManagerEventExporter::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
