// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PARENT_ACCESS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PARENT_ACCESS_COMMANDS_H_

#import <Foundation/Foundation.h>

#include <optional>

#include "components/supervised_user/core/common/supervised_user_constants.h"

class GURL;

namespace supervised_user {
enum class FilteringBehaviorReason;
enum class LocalApprovalResult;
}

namespace web {
class WebState;
}

// Commands related to the Parent Access UI.
@protocol ParentAccessCommands

// Shows the parent access bottom sheet for local web approvals.
- (void)
    showParentAccessBottomSheetForWebState:(web::WebState*)webState
                                 targetURL:(const GURL&)targetURL
                   filteringBehaviorReason:
                       (supervised_user::FilteringBehaviorReason)
                           filteringBehaviorReason
                                completion:
                                    (void (^)(
                                        supervised_user::LocalApprovalResult,
                                        std::optional<
                                            supervised_user::
                                                LocalWebApprovalErrorType>))
                                        completion;

// Hides the parent access bottom sheet for local web approvals.
- (void)hideParentAccessBottomSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PARENT_ACCESS_COMMANDS_H_
