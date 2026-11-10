// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/test_sync_service_utils.h"

#include "components/sync/test/test_sync_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

std::unique_ptr<KeyedService> CreateTestSyncService(ProfileIOS& profile) {
  return std::make_unique<syncer::TestSyncService>();
}
