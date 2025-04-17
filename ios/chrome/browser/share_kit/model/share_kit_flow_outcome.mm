// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_flow_outcome.h"

collaboration::CollaborationControllerDelegate::Outcome
ConvertShareKitFlowOutcome(ShareKitFlowOutcome outcome) {
  switch (outcome) {
    case ShareKitFlowOutcome::kSuccess:
      return collaboration::CollaborationControllerDelegate::Outcome::kSuccess;
    case ShareKitFlowOutcome::kFailure:
      return collaboration::CollaborationControllerDelegate::Outcome::kFailure;
    case ShareKitFlowOutcome::kCancel:
      return collaboration::CollaborationControllerDelegate::Outcome::kCancel;
  }
}
