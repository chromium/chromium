// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_REGION_DATA_LOADER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_REGION_DATA_LOADER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "components/autofill/core/browser/ui/region_combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"

namespace autofill {
class RegionDataLoader;
}  // namespace autofill

namespace ui {
class ComboboxModel;
}  // namespace ui

@interface RegionData : NSObject

@property(nonatomic, copy, readonly) NSString* regionCode;
@property(nonatomic, copy, readonly) NSString* regionName;

- (instancetype)initWithRegionCode:(NSString*)regionCode
                        regionName:(NSString*)regionName
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@protocol RegionDataLoaderConsumer

// To be called with a list of regions when region data for the given country
// code is loaded asynchronously.
- (void)regionDataLoaderDidSucceedWithRegions:(NSArray<RegionData*>*)regions;

@end

class RegionDataLoader : public ui::ComboboxModelObserver {
 public:
  explicit RegionDataLoader(id<RegionDataLoaderConsumer> consumer);
  ~RegionDataLoader() override;

  void LoadRegionData(const std::string& country_code,
                      autofill::RegionDataLoader* autofill_region_data_loader);

  // ui::ComboboxModelObserver
  void OnComboboxModelChanged(ui::ComboboxModel* model) override;

 private:
  __weak id<RegionDataLoaderConsumer> consumer_;
  autofill::RegionComboboxModel region_model_;

  DISALLOW_COPY_AND_ASSIGN(RegionDataLoader);
};

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_REGION_DATA_LOADER_H_
