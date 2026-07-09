// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_WEB_UI_METRICS_H_
#define IOS_WEB_WEBUI_WEB_UI_METRICS_H_

#include <string_view>

namespace web {

// Enum for the IOS.WebUI.{Action} histograms.
// LINT.IfChange(WebUIMojoActions)
enum class WebUIMojoActions {
  kSuccess = 0,
  kFailure = 1,
  kMaxValue = kFailure,
};
// LINT.ThenChange(tools/metrics/histograms/metadata/ios/enums.xml:WebUIMojoActions)

// Records the success/failure outcome of a Mojo bridge action.
void RecordWebUIMojoActionOutcome(std::string_view action_name,
                                  WebUIMojoActions outcome);

}  // namespace web

#endif  // IOS_WEB_WEBUI_WEB_UI_METRICS_H_
