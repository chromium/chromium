// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/features.h"

#import "components/collaboration/public/collaboration_service.h"
#import "components/collaboration/public/service_status.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool IsSharedTabGroupsJoinEnabled(ProfileIOS* profile) {
  if (!IsTabGroupSyncEnabled()) {
    return false;
  }

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  return collaboration_service->GetServiceStatus().IsAllowedToJoin();
}

bool IsSharedTabGroupsCreateEnabled(ProfileIOS* profile) {
  if (!IsTabGroupSyncEnabled()) {
    return false;
  }

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  return collaboration_service->GetServiceStatus().IsAllowedToCreate();
}
