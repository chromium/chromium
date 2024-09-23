// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_

#include <string>

#include "base/functional/bind.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
  using GetLogRouterFunction = base::RepeatingCallback<LogRouter*(ProfileIOS*)>;

  explicit InternalsUIHandler(std::string call_on_load,
                              GetLogRouterFunction get_log_router_function);

  InternalsUIHandler(const InternalsUIHandler&) = delete;
  InternalsUIHandler& operator=(const InternalsUIHandler&) = delete;

  ~InternalsUIHandler() override;

 private:
  void RegisterMessages() override;

  // LogReceiver implementation.
  void LogEntry(const base::Value::Dict& entry) override;

  void StartSubscription();
  void EndSubscription();

  // JavaScript call handler.
  void OnLoaded(const base::Value::List& args);

 private:
  // JavaScript function to be called on load.
  std::string call_on_load_;
  GetLogRouterFunction get_log_router_function_;

  // Whether `this` is registered as a log receiver with the LogRouter.
  bool registered_with_log_router_ = false;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_INTERNALS_UI_HANDLER_H_
