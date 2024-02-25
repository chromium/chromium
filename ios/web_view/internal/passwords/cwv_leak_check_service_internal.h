// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_LEAK_CHECK_SERVICE_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_LEAK_CHECK_SERVICE_INTERNAL_H_

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#import "ios/web_view/public/cwv_leak_check_service.h"

@interface CWVLeakCheckService ()

- (instancetype)initWithBulkLeakCheckService:
    (password_manager::BulkLeakCheckServiceInterface*)service
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_LEAK_CHECK_SERVICE_INTERNAL_H_
