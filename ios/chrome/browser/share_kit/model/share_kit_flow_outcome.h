// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FLOW_OUTCOME_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FLOW_OUTCOME_H_

#import "components/collaboration/public/collaboration_controller_delegate.h"

// The outcome of a share kit flow.
enum class ShareKitFlowOutcome {
  kSuccess,
  kFailure,
  kCancel,
};

// Converts `outcome` between the two enums.
collaboration::CollaborationControllerDelegate::Outcome
ConvertShareKitFlowOutcome(ShareKitFlowOutcome outcome);

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FLOW_OUTCOME_H_
