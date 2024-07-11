// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_BLOCK_METRICS_DATA_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_BLOCK_METRICS_DATA_H_

#import "base/time/time.h"

// Holds any data related to displaying a panel block that must be tracked for
// metrics purposes.
@interface PanelBlockMetricsData : NSObject

@property(nonatomic, assign) base::TimeDelta timeVisible;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_BLOCK_METRICS_DATA_H_
