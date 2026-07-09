// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/web_ui_metrics.h"

#import "base/metrics/histogram_functions.h"

namespace web {

void RecordWebUIMojoActionOutcome(std::string_view action_name,
                                  WebUIMojoActions outcome) {
  std::string histogram_name = "IOS.WebUI.";
  histogram_name.append(action_name);
  base::UmaHistogramEnumeration(histogram_name, outcome);
}

}  // namespace web
