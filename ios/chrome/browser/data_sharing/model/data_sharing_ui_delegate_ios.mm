// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"

#import "base/notimplemented.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "url/gurl.h"

namespace data_sharing {

DataSharingUIDelegateIOS::DataSharingUIDelegateIOS(
    ShareKitService* share_kit_service)
    : share_kit_service_(share_kit_service) {}
DataSharingUIDelegateIOS::~DataSharingUIDelegateIOS() = default;

void DataSharingUIDelegateIOS::HandleShareURLIntercepted(const GURL& url) {
  ShareKitJoinConfiguration* configuration =
      [[ShareKitJoinConfiguration alloc] init];
  configuration.URL = url;
  // TODO(crbug.com/373825718): get the correct ViewController.
  configuration.baseViewController = nil;
  share_kit_service_->JoinGroup(nil);
}

}  // namespace data_sharing
