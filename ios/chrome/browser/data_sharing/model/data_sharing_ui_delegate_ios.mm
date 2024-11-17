// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"

#import "base/notimplemented.h"
#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios_share_url_interception_context.h"
#import "url/gurl.h"

namespace data_sharing {

DataSharingUIDelegateIOS::DataSharingUIDelegateIOS(
    ShareKitService* share_kit_service)
    : share_kit_service_(share_kit_service) {}
DataSharingUIDelegateIOS::~DataSharingUIDelegateIOS() = default;

void DataSharingUIDelegateIOS::HandleShareURLIntercepted(
    const GURL& url,
    std::unique_ptr<ShareURLInterceptionContext> context) {
  if (!context) {
    return;
  }
  IOSShareURLInterceptionContext* ios_context =
      static_cast<IOSShareURLInterceptionContext*>(context.get());
  Browser* browser = ios_context->browser;
  if (!browser) {
    return;
  }

  UIViewController* baseViewController =
      browser->GetSceneState().window.rootViewController;

  while (baseViewController.presentedViewController) {
    baseViewController = baseViewController.presentedViewController;
  }

  ShareKitJoinConfiguration* configuration =
      [[ShareKitJoinConfiguration alloc] init];
  configuration.URL = url;
  configuration.baseViewController = baseViewController;
  share_kit_service_->JoinGroup(configuration);
}

}  // namespace data_sharing
