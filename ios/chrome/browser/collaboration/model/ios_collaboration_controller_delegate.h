// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_

#import "components/collaboration/public/collaboration_controller_delegate.h"

namespace collaboration {

class CollaborationFlowConfiguration;

// iOS implementation of CollaborationControllerDelegate.
class IOSCollaborationControllerDelegate : CollaborationControllerDelegate {
 public:
  IOSCollaborationControllerDelegate(
      std::unique_ptr<CollaborationFlowConfiguration> collaboration_flow);

  IOSCollaborationControllerDelegate(
      const IOSCollaborationControllerDelegate&) = delete;
  IOSCollaborationControllerDelegate& operator=(
      const IOSCollaborationControllerDelegate&) = delete;
  ~IOSCollaborationControllerDelegate() override;

  // CollaborationControllerDelegate.
  void ShowError(ResultCallback result, const ErrorInfo& error) override;
  void Cancel(ResultCallback result) override;
  void ShowAuthenticationUi(ResultCallback result) override;
  void NotifySignInAndSyncStatusChange(ResultCallback result) override;
  void ShowJoinDialog(ResultCallback result) override;
  void ShowShareDialog(ResultCallback result) override;

 private:
  std::unique_ptr<CollaborationFlowConfiguration> collaboration_flow_;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
