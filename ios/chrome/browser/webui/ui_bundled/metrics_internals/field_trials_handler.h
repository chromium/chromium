// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/metrics/debug/field_trials_handler_base.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

class ProfileIOS;

// LINT.IfChange(field_trials_handler)

// UI Handler for the Field Trials tab of chrome://metrics-internals.
class FieldTrialsHandler : public web::WebUIIOSMessageHandler,
                           public metrics::FieldTrialsHandlerBase::Delegate {
 public:
  explicit FieldTrialsHandler(ProfileIOS* profile);
  ~FieldTrialsHandler() override;

  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // metrics::FieldTrialsHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

 private:
  void HandleFetchState(const base::ListValue& args);
  void HandleSetEnrollState(const base::ListValue& args);
  void HandleRestart(const base::ListValue& args);
  void HandleLookupTrialOrGroupName(const base::ListValue& args);

  bool GetShowNames();

  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<metrics::FieldTrialsHandlerBase> base_handler_;
};

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/field_trials_handler.h)

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_FIELD_TRIALS_HANDLER_H_
