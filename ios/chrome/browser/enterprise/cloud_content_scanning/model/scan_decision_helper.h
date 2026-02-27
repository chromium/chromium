// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_SCAN_DECISION_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_SCAN_DECISION_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "components/enterprise/connectors/core/common.h"

namespace web {
class WebState;
}

namespace enterprise_connectors {

// Represents the download type that triggers the file download.
enum TriggerType { kSavePrompt, kShareSheet };

// Handles the scan decision and shows the Warning Dialog or Snackbar Message to
// the user. A callback `download_proceed` will run at the end to indicate if
// the download should proceed.
void HandleScanDecision(web::WebState* web_state,
                        RequestHandlerResult result,
                        TriggerType trigger_type,
                        base::OnceCallback<void(bool)> download_proceed);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_SCAN_DECISION_HELPER_H_
