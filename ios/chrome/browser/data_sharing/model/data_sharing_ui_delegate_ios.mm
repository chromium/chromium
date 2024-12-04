// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"

#import "base/notimplemented.h"
#import "components/collaboration/public/collaboration_service.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"
#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios_share_url_interception_context.h"
#import "url/gurl.h"

using collaboration::CollaborationFlowConfigurationJoin;
using collaboration::IOSCollaborationControllerDelegate;

namespace data_sharing {

DataSharingUIDelegateIOS::DataSharingUIDelegateIOS(
    ShareKitService* share_kit_service,
    collaboration::CollaborationService* collaboration_service)
    : share_kit_service_(share_kit_service),
      collaboration_service_(collaboration_service) {}
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
      browser->GetSceneState().rootViewController;

  while (baseViewController.presentedViewController) {
    baseViewController = baseViewController.presentedViewController;
  }

  std::unique_ptr<IOSCollaborationControllerDelegate> delegate =
      std::make_unique<IOSCollaborationControllerDelegate>(
          std::make_unique<CollaborationFlowConfigurationJoin>(
              share_kit_service_, url, browser->GetCommandDispatcher(),
              baseViewController));
  collaboration_service_->StartJoinFlow(std::move(delegate), url);
}

}  // namespace data_sharing
