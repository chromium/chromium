// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_

#include <memory>

#include "base/values.h"
#include "components/metrics/debug/runtime_mutable_features_handler_base.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// UI Handler for the Runtime-Mutable-Features tab in
// chrome://metrics-internals.
// LINT.IfChange(runtime_mutable_features_handler)
class RuntimeMutableFeaturesHandler
    : public web::WebUIIOSMessageHandler,
      public metrics::RuntimeMutableFeaturesHandlerBase::Delegate {
 public:
  RuntimeMutableFeaturesHandler();

  RuntimeMutableFeaturesHandler(const RuntimeMutableFeaturesHandler&) = delete;
  RuntimeMutableFeaturesHandler& operator=(
      const RuntimeMutableFeaturesHandler&) = delete;

  ~RuntimeMutableFeaturesHandler() override;

  // web::WebUIIOSMessageHandler:
  void RegisterMessages() override;

  // metrics::RuntimeMutableFeaturesHandlerBase::Delegate:
  void ResolvePageCallback(const base::ValueView callback_id,
                           const base::ValueView response) override;

 private:
  void HandleFetchRuntimeMutableFeatures(const base::ListValue& args);
  void HandleIsSeedFetchingPaused(const base::ListValue& args);
  void HandleSetSeedFetchingPaused(const base::ListValue& args);
  void HandleUploadSeed(const base::ListValue& args);

  std::unique_ptr<metrics::RuntimeMutableFeaturesHandlerBase> base_handler_;
};
// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/runtime_mutable_features_handler.h)

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_METRICS_INTERNALS_RUNTIME_MUTABLE_FEATURES_HANDLER_H_
