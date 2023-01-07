// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_PROMPT_ENTERPRISE_PROMPT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_PROMPT_ENTERPRISE_PROMPT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_type.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// ViewController that contains enterprise prompt information.
@interface EnterprisePromptViewController : ConfirmationAlertViewController

// Initializes this alert with its `promptType`.
- (instancetype)initWithpromptType:(EnterprisePromptType)promptType;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_PROMPT_ENTERPRISE_PROMPT_VIEW_CONTROLLER_H_
