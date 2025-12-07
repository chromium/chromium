// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"

#import "base/functional/callback_helpers.h"
#import "base/notimplemented.h"
#import "components/collaboration/public/collaboration_flow_type.h"
#import "components/collaboration/public/collaboration_service.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"
#import "ios/chrome/browser/data_sharing/model/ios_share_url_interception_context.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios_share_url_interception_context.h"
#import "url/gurl.h"

using collaboration::FlowType;
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
  Browser* browser = ios_context->weak_browser.get();
  if (!browser) {
    return;
  }

  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  [applicationHandler
      dismissModalDialogsWithCompletion:
          base::CallbackToBlock(base::BindOnce(
              &DataSharingUIDelegateIOS::OnJoinFlowReadyToBePresented,
              weak_ptr_factory_.GetWeakPtr(), url,
              std::move(ios_context->weak_browser)))];
}

void DataSharingUIDelegateIOS::OnJoinFlowReadyToBePresented(
    GURL url,
    base::WeakPtr<Browser> weak_browser) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  UIViewController* base_view_controller =
      browser->GetSceneState().window.rootViewController;

  std::unique_ptr<IOSCollaborationControllerDelegate> delegate =
      std::make_unique<IOSCollaborationControllerDelegate>(
          browser,
          CreateControllerDelegateParamsFromProfile(
              browser->GetProfile(), base_view_controller, FlowType::kJoin));
  collaboration_service_->StartJoinFlow(std::move(delegate), url);
}

}  // namespace data_sharing
