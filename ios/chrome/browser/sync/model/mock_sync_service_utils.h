// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_MOCK_SYNC_SERVICE_UTILS_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_MOCK_SYNC_SERVICE_UTILS_H_

#include "components/sync/test/mock_sync_service.h"
#include "ios/web/public/browser_state.h"

// Returns a basic MockSyncService.
std::unique_ptr<KeyedService> CreateMockSyncService(web::BrowserState* context);

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_MOCK_SYNC_SERVICE_UTILS_H_
