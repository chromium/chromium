// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_CLIENT_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_CLIENT_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/common/app_group/app_group_constants.h"

namespace app_group {
// TODO(crbug.com/782685): remove all functions.
namespace client_app {

// Adds a closed log to the pending log directory. The log will be processed the
// next time Chrome is launched.
void AddPendingLog(NSData* log, AppGroupApplications application);

// Deletes oldest pending logs.
void CleanOldPendingLogs();

}  // namespace client_app
}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_CLIENT_H_
