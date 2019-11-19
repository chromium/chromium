// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_IOS_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_IOS_H_

#include "base/macros.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"

// The implementation for the chrome://autofill-internals page.
class AutofillInternalsUIIOS : public web::WebUIIOSController {
 public:
  explicit AutofillInternalsUIIOS(web::WebUIIOS* web_ui);
  ~AutofillInternalsUIIOS() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsUIIOS);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_IOS_H_
