// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_EVENT_EXPORTER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_EVENT_EXPORTER_H_

#import <Foundation/Foundation.h>

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"

class PrefService;

// A class to export saved PromosManager impression events to the Feature
// Engagement Tracker as part of an ongoing migration.
class PromosManagerEventExporter
    : public KeyedService,
      public feature_engagement::TrackerEventExporter {
 public:
  PromosManagerEventExporter(PrefService* local_state);
  ~PromosManagerEventExporter() override;

  // Ingests promo-specific impression limits and stores them in-memory for
  // later reference.
  void InitializePromoConfigs(PromoConfigsSet promo_configs);

  // feature_engagement::TrackerEventExporter implementation
  void ExportEvents(ExportEventsCallback callback) override;

  // Returns a weak pointer to the current instance.
  base::WeakPtr<PromosManagerEventExporter> AsWeakPtr();

  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // Promo-specific configuration. This must be provided to the exporter for
  // it export any events.
  PromoConfigsSet promo_configs_;

  base::WeakPtrFactory<PromosManagerEventExporter> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_EVENT_EXPORTER_H_
