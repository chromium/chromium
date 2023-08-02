// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_metrics.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

namespace web {

void RecordSearchStartedAction() {
  base::RecordAction(base::UserMetricsAction("IOS.FindInPage.SearchStarted"));
}

void RecordFindNextAction() {
  base::RecordAction(base::UserMetricsAction("IOS.FindInPage.FindNext"));
}

void RecordFindPreviousAction() {
  base::RecordAction(base::UserMetricsAction("IOS.FindInPage.FindPrevious"));
}

}  // namespace web
