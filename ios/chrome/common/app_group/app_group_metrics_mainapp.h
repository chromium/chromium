// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_MAINAPP_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_MAINAPP_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

#include "ios/chrome/common/app_group/app_group_constants.h"

namespace app_group {

typedef void (^ProceduralBlockWithData)(NSData*);

// These methods must be called from the Chrome app.
namespace main_app {

// Iterates through the extensions pending logs and deletes them.
// Calls `callback` on each log before deleting.
// TODO(crbug.com/40548746): remove function.
void ProcessPendingLogs(ProceduralBlockWithData callback);

// Enables the metrics collecting in extensions. The extensions will
// use `clientID` as client ID, and `brandCode` as brand code in the logs.
// TODO(crbug.com/40548746): remove function.
void EnableMetrics(NSString* client_id,
                   NSString* brand_code,
                   int64_t installDate,
                   int64_t enableMetricsDate);

// Disables the metrics collecting in extensions.
// TODO(crbug.com/40548746): remove function.
void DisableMetrics();

// Report the metrics collected from the Open extension.
void LogOpenExtensionMetrics();

}  // namespace main_app
}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_MAINAPP_H_
