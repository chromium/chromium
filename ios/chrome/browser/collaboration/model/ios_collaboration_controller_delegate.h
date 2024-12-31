// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "components/collaboration/public/collaboration_controller_delegate.h"

class Browser;
class ShareKitService;

namespace collaboration {

class CollaborationFlowConfiguration;

// iOS implementation of CollaborationControllerDelegate.
class IOSCollaborationControllerDelegate
    : public CollaborationControllerDelegate {
 public:
  IOSCollaborationControllerDelegate(
      Browser* browser,
      UIViewController* base_view_controller,
      std::unique_ptr<CollaborationFlowConfiguration> collaboration_flow);

  IOSCollaborationControllerDelegate(
      const IOSCollaborationControllerDelegate&) = delete;
  IOSCollaborationControllerDelegate& operator=(
      const IOSCollaborationControllerDelegate&) = delete;
  ~IOSCollaborationControllerDelegate() override;

  // CollaborationControllerDelegate.
  void PrepareFlowUI(ResultCallback result) override;
  void ShowError(const ErrorInfo& error, ResultCallback result) override;
  void Cancel(ResultCallback result) override;
  void ShowAuthenticationUi(ResultCallback result) override;
  void NotifySignInAndSyncStatusChange() override;
  void ShowJoinDialog(const data_sharing::GroupToken& token,
                      const data_sharing::SharedDataPreview& preview_data,
                      ResultCallback result) override;
  void ShowShareDialog(const tab_groups::EitherGroupID& either_id,
                       ResultCallback result) override;
  void ShowManageDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void PromoteTabGroup(const data_sharing::GroupId& group_id,
                       ResultCallback result) override;
  void PromoteCurrentScreen() override;

 private:
  raw_ptr<ShareKitService> share_kit_service_;
  raw_ptr<Browser> browser_;
  __weak UIViewController* base_view_controller_;
  std::unique_ptr<CollaborationFlowConfiguration> flow_config_;
  NSString* session_id_ = nil;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
