// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/set_up_list_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"

namespace set_up_list_metrics {

void RecordDisplayed() {
  base::UmaHistogramBoolean("IOS.SetUpList.Displayed", true);
}

void RecordItemDisplayed(SetUpListItemType type) {
  base::UmaHistogramEnumeration("IOS.SetUpList.ItemDisplayed", type);
}

void RecordItemSelected(SetUpListItemType type) {
  base::UmaHistogramEnumeration("IOS.SetUpList.ItemSelected", type);
}

void RecordItemCompleted(SetUpListItemType type) {
  base::UmaHistogramEnumeration("IOS.SetUpList.ItemCompleted", type);
}

void RecordAllItemsCompleted() {
  base::UmaHistogramBoolean("IOS.SetUpList.AllItemsCompleted", true);
}

}  // namespace set_up_list_metrics
