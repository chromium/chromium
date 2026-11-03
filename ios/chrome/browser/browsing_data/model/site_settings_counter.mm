// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/site_settings_counter.h"

#import <set>
#import <string>
#import <utility>

#import "base/check.h"
#import "base/time/time.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/browser/permission_settings_info.h"
#import "components/content_settings/core/browser/permission_settings_registry.h"
#import "components/content_settings/core/browser/website_settings_info.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/translate/core/browser/translate_prefs.h"

SiteSettingsCounter::SiteSettingsCounter(
    scoped_refptr<HostContentSettingsMap> host_content_settings_map,
    PrefService* pref_service)
    : host_content_settings_map_(std::move(host_content_settings_map)),
      pref_service_(pref_service) {
  CHECK(host_content_settings_map_);
  CHECK(pref_service_);
}

SiteSettingsCounter::~SiteSettingsCounter() = default;

const char* SiteSettingsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteSiteSettings;
}

void SiteSettingsCounter::Count() {
  // Keep in sync with Desktop and Android's `SiteSettingsCounter`
  // (chrome/browser/browsing_data/counters/site_settings_counter.cc) so the
  // same site settings are counted on all platforms.
  const base::Time period_start = GetPeriodStart();
  const base::Time period_end = GetPeriodEnd();

  // Exceptions are counted per host so that a site with several permissions
  // set only counts once. Patterns without a host (e.g. wildcards) can't be
  // deduplicated and are counted individually.
  std::set<std::string> hosts;
  ResultInt empty_host_pattern_count = 0;

  for (const content_settings::PermissionSettingsInfo* info :
       *content_settings::PermissionSettingsRegistry::GetInstance()) {
    for (const ContentSettingPatternSource& setting :
         host_content_settings_map_->GetSettingsForOneType(
             info->website_settings_info()->type())) {
      if (content_settings::GetSettingSourceFromProviderType(setting.source) !=
              content_settings::SettingSource::kUser ||
          setting.source == content_settings::ProviderType::kDefaultProvider) {
        continue;
      }

      const base::Time last_modified = setting.metadata.last_modified();
      if (last_modified < period_start || last_modified >= period_end) {
        continue;
      }

      const std::string host = setting.primary_pattern.GetHost();
      if (host.empty()) {
        ++empty_host_pattern_count;
      } else {
        hosts.insert(host);
      }
    }
  }

  for (const std::string& site :
       translate::TranslatePrefs(pref_service_)
           .GetNeverPromptSitesBetween(period_start, period_end)) {
    hosts.insert(site);
  }

  ReportResult(static_cast<ResultInt>(hosts.size()) + empty_host_pattern_count);
}
