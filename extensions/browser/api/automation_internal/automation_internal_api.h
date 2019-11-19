// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_

#include <string>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace api {
namespace automation_internal {
namespace PerformAction {
struct Params;
}  // namespace PerformAction
}  // namespace automation_internal
}  // namespace api
}  // namespace extensions

namespace ui {
struct AXActionData;
}  // namespace ui

namespace extensions {

// Implementation of the chrome.automation API.
class AutomationInternalEnableTabFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableTab",
                             AUTOMATIONINTERNAL_ENABLETAB)
 protected:
  ~AutomationInternalEnableTabFunction() override {}

  ExtensionFunction::ResponseAction Run() override;
};

class AutomationInternalPerformActionFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.performAction",
                             AUTOMATIONINTERNAL_PERFORMACTION)
 protected:
  ~AutomationInternalPerformActionFunction() override {}

  ExtensionFunction::ResponseAction Run() override;

 private:
  // Helper function to convert extension action to ax action.
  ExtensionFunction::ResponseAction ConvertToAXActionData(
      api::automation_internal::PerformAction::Params* params,
      ui::AXActionData* data);
};

class AutomationInternalEnableFrameFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableFrame",
                             AUTOMATIONINTERNAL_ENABLEFRAME)

 protected:
  ~AutomationInternalEnableFrameFunction() override {}

  ExtensionFunction::ResponseAction Run() override;
};

class AutomationInternalEnableDesktopFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableDesktop",
                             AUTOMATIONINTERNAL_ENABLEDESKTOP)
 protected:
  ~AutomationInternalEnableDesktopFunction() override {}

  ResponseAction Run() override;
};

class AutomationInternalQuerySelectorFunction : public ExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.querySelector",
                             AUTOMATIONINTERNAL_QUERYSELECTOR)

 public:
  typedef base::Callback<void(const std::string& error, int result_acc_obj_id)>
      Callback;

 protected:
  ~AutomationInternalQuerySelectorFunction() override {}

  ResponseAction Run() override;

 private:
  void OnResponse(const std::string& error, int result_acc_obj_id);

  // Used for assigning a unique ID to each request so that the response can be
  // routed appropriately.
  static int query_request_id_counter_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
