// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/features.h"

#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/service_status.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool IsSharedTabGroupsJoinEnabled(ProfileIOS* profile) {
  if (!IsTabGroupSyncEnabled()) {
    return false;
  }

  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  return data_sharing_service->GetServiceStatus().IsAllowedToJoin();
}

bool IsSharedTabGroupsCreateEnabled(ProfileIOS* profile) {
  if (!IsTabGroupSyncEnabled()) {
    return false;
  }

  data_sharing::DataSharingService* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  return data_sharing_service->GetServiceStatus().IsAllowedToCreate();
}
