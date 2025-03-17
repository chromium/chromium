// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"

class BrowserList;
class PrefService;

// Service that listens to Inactive Tabs pref and moves tabs between active and
// inactive browsers accordingly.
class InactiveTabsService : public KeyedService {
 public:
  InactiveTabsService(PrefService* pref_service, BrowserList* browser_list);
  ~InactiveTabsService() override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  // Notification for prefs::kInactiveTabsTimeThreshold.
  void OnPrefChanged(const std::string& name);

  // The pref service to observe for Inactive Tabs pref changes.
  raw_ptr<PrefService> pref_service_;
  // The keyed service used.
  raw_ptr<BrowserList> browser_list_;
  // Registrar for prefs::kInactiveTabsTimeThreshold.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<InactiveTabsService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_H_
