// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_IOS_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_IOS_H_

#include <string>

#import "ios/web/public/webui/web_ui_ios_controller.h"

// The implementation for the chrome://autofill-internals page.
class AutofillInternalsUIIOS : public web::WebUIIOSController {
 public:
  explicit AutofillInternalsUIIOS(web::WebUIIOS* web_ui,
                                  const std::string& host);

  AutofillInternalsUIIOS(const AutofillInternalsUIIOS&) = delete;
  AutofillInternalsUIIOS& operator=(const AutofillInternalsUIIOS&) = delete;

  ~AutofillInternalsUIIOS() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_IOS_H_
