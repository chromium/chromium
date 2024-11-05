// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

namespace collaboration {

IOSCollaborationControllerDelegate::IOSCollaborationControllerDelegate() {
  // TODO(crbug.com/377306986): Implement this.
}

IOSCollaborationControllerDelegate::~IOSCollaborationControllerDelegate() {}

// CollaborationControllerDelegate.
void IOSCollaborationControllerDelegate::ShowError(const ResultCallback& result,
                                                   const ErrorInfo& error) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::Cancel(const ResultCallback& result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowAuthenticationUi(
    const ResultCallback& result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::NotifySignInAndSyncStatusChange(
    const ResultCallback& result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowJoinDialog(
    const ResultCallback& result) {
  // TODO(crbug.com/377306986): Implement this.
}

void IOSCollaborationControllerDelegate::ShowShareDialog(
    const ResultCallback& result) {
  // TODO(crbug.com/377306986): Implement this.
}

}  // namespace collaboration
