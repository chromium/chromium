// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SITE_SETTINGS_COUNTER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SITE_SETTINGS_COUNTER_H_

#import "base/memory/scoped_refptr.h"
#import "components/browsing_data/core/counters/browsing_data_counter.h"

class HostContentSettingsMap;

// `SiteSettingsCounter` is a `BrowsingDataCounter` that counts the number of
// sites with user-set permission exceptions modified within the deletion
// period.
class SiteSettingsCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit SiteSettingsCounter(
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);

  SiteSettingsCounter(const SiteSettingsCounter&) = delete;
  SiteSettingsCounter& operator=(const SiteSettingsCounter&) = delete;

  ~SiteSettingsCounter() override;

  // browsing_data::BrowsingDataCounter implementation.
  const char* GetPrefName() const override;
  void Count() override;

 private:
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SITE_SETTINGS_COUNTER_H_
