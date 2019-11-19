// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace autofill {
class LogRouter;
}

namespace web {
class WebUIIOSDataSource;
}

namespace autofill {

web::WebUIIOSDataSource* CreateInternalsHTMLSource(
    const std::string& source_name);

// The implementation for chrome://password-manager-internals and
// chrome://autofill-internals. Use the GetLogRouterFunction parameter of the
// constructor to inject the corresponding LogRouter.
class InternalsUIHandler : public web::WebUIIOSMessageHandler,
                           public autofill::LogReceiver {
 public:
  using GetLogRouterFunction =
      base::RepeatingCallback<LogRouter*(ios::ChromeBrowserState*)>;

  explicit InternalsUIHandler(std::string call_on_load,
                              GetLogRouterFunction get_log_router_function);
  ~InternalsUIHandler() override;

 private:
  void RegisterMessages() override;

  // LogReceiver implementation.
  void LogEntry(const base::Value& entry) override;

  void StartSubscription();
  void EndSubscription();

  // JavaScript call handler.
  void OnLoaded(const base::ListValue* args);

 private:
  // JavaScript function to be called on load.
  std::string call_on_load_;
  GetLogRouterFunction get_log_router_function_;

  // Whether |this| is registered as a log receiver with the LogRouter.
  bool registered_with_log_router_ = false;

  DISALLOW_COPY_AND_ASSIGN(InternalsUIHandler);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
