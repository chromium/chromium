// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/collaboration/model/ios_collaboration_flow_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

namespace collaboration {

IOSCollaborationControllerDelegate::IOSCollaborationControllerDelegate(
    std::unique_ptr<CollaborationFlowConfiguration> collaboration_flow)
    : collaboration_flow_(std::move(collaboration_flow)) {}

IOSCollaborationControllerDelegate::~IOSCollaborationControllerDelegate() {}

// CollaborationControllerDelegate.
void IOSCollaborationControllerDelegate::ShowError(ResultCallback result,
                                                   const ErrorInfo& error) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::Cancel(ResultCallback result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowAuthenticationUi(
    ResultCallback result) {
  id<ApplicationCommands> aplication_handler = HandlerForProtocol(
      collaboration_flow_->command_dispatcher(), ApplicationCommands);
  UIViewController* base_view_controller =
      collaboration_flow_->base_view_controller();
  auto completion_block = base::CallbackToBlock(std::move(result));

  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSheetSigninAndHistorySync
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_COLLABORATION_TAB_GROUP
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:^(SigninCoordinatorResult sign_in_result,
                          SigninCompletionInfo* completion_info) {
               bool completion_result =
                   sign_in_result == SigninCoordinatorResultSuccess;
               completion_block(completion_result);
             }];

  [aplication_handler showSignin:command
              baseViewController:base_view_controller];
}

void IOSCollaborationControllerDelegate::NotifySignInAndSyncStatusChange(
    ResultCallback result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowJoinDialog(ResultCallback result) {
  CHECK_EQ(collaboration_flow_->type(),
           CollaborationFlowConfiguration::Type::kJoin);
  const CollaborationFlowConfigurationJoin& join_flow =
      collaboration_flow_->As<CollaborationFlowConfigurationJoin>();

  ShareKitJoinConfiguration* configuration =
      [[ShareKitJoinConfiguration alloc] init];
  configuration.URL = join_flow.url();
  configuration.baseViewController = join_flow.base_view_controller();
  join_flow.share_kit_service()->JoinGroup(configuration);
  // TODO(crbug.com/377869115): `result` should be returned when the
  // ShareKit UI is done.
  std::move(result).Run(true);
}

void IOSCollaborationControllerDelegate::ShowShareDialog(
    ResultCallback result) {
  CHECK_EQ(collaboration_flow_->type(),
           CollaborationFlowConfiguration::Type::kShare);
  const CollaborationFlowConfigurationShare& share_flow =
      collaboration_flow_->As<CollaborationFlowConfigurationShare>();

  if (!share_flow.tab_group()) {
    std::move(result).Run(false);
    return;
  }

  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = share_flow.tab_group().get();
  config.baseViewController = share_flow.base_view_controller();
  config.applicationHandler =
      HandlerForProtocol(share_flow.command_dispatcher(), ApplicationCommands);
  share_flow.share_kit_service()->ShareGroup(config);
  // TODO(crbug.com/377869115): `result` should be returned when the
  // ShareKit UI is done.
  std::move(result).Run(true);
}

}  // namespace collaboration
