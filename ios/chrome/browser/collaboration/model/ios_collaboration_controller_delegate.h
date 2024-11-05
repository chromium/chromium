// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_

#import "components/collaboration/public/collaboration_controller_delegate.h"

namespace collaboration {

// iOS implementation of CollaborationControllerDelegate.
class IOSCollaborationControllerDelegate : CollaborationControllerDelegate {
 public:
  IOSCollaborationControllerDelegate();

  IOSCollaborationControllerDelegate(
      const IOSCollaborationControllerDelegate&) = delete;
  IOSCollaborationControllerDelegate& operator=(
      const IOSCollaborationControllerDelegate&) = delete;
  ~IOSCollaborationControllerDelegate() override;

  // CollaborationControllerDelegate.
  void ShowError(const ResultCallback& result, const ErrorInfo& error) override;
  void Cancel(const ResultCallback& result) override;
  void ShowAuthenticationUi(const ResultCallback& result) override;
  void NotifySignInAndSyncStatusChange(const ResultCallback& result) override;
  void ShowJoinDialog(const ResultCallback& result) override;
  void ShowShareDialog(const ResultCallback& result) override;
};

}  // namespace collaboration

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_IOS_COLLABORATION_CONTROLLER_DELEGATE_H_
