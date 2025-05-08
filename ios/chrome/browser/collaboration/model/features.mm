// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/features.h"

#import "components/collaboration/public/collaboration_service.h"
#import "components/collaboration/public/service_status.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool IsSharedTabGroupsJoinEnabled(
    collaboration::CollaborationService* collaboration_service) {
  CHECK(collaboration_service);
  if (!IsTabGroupSyncEnabled()) {
    return false;
  }
  return collaboration_service->GetServiceStatus().IsAllowedToJoin();
}

bool IsSharedTabGroupsCreateEnabled(
    collaboration::CollaborationService* collaboration_service) {
  CHECK(collaboration_service);
  if (!IsTabGroupSyncEnabled()) {
    return false;
  }
  return collaboration_service->GetServiceStatus().IsAllowedToCreate();
}
