// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/runtime_mutable_features_handler.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/webui/web_ui_ios.h"

// LINT.IfChange(runtime_mutable_features_handler)

RuntimeMutableFeaturesHandler::RuntimeMutableFeaturesHandler()
    : base_handler_(
          std::make_unique<metrics::RuntimeMutableFeaturesHandlerBase>(
              this,
              GetApplicationContext()->GetVariationsService())) {}

RuntimeMutableFeaturesHandler::~RuntimeMutableFeaturesHandler() = default;

void RuntimeMutableFeaturesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchRuntimeMutableFeatures",
      base::BindRepeating(
          &RuntimeMutableFeaturesHandler::HandleFetchRuntimeMutableFeatures,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isSeedFetchingPaused",
      base::BindRepeating(
          &RuntimeMutableFeaturesHandler::HandleIsSeedFetchingPaused,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setSeedFetchingPaused",
      base::BindRepeating(
          &RuntimeMutableFeaturesHandler::HandleSetSeedFetchingPaused,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "uploadSeed",
      base::BindRepeating(&RuntimeMutableFeaturesHandler::HandleUploadSeed,
                          base::Unretained(this)));
}

void RuntimeMutableFeaturesHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  web_ui()->ResolveJavascriptCallback(callback_id, response);
}

void RuntimeMutableFeaturesHandler::HandleFetchRuntimeMutableFeatures(
    const base::ListValue& args) {
  // args[0]: Callback ID.
  CHECK_EQ(args.size(), 1U);
  base_handler_->HandleFetchRuntimeMutableFeatures(args[0]);
}

void RuntimeMutableFeaturesHandler::HandleIsSeedFetchingPaused(
    const base::ListValue& args) {
  // args[0]: Callback ID.
  CHECK_EQ(args.size(), 1U);
  base_handler_->HandleIsSeedFetchingPaused(args[0]);
}

void RuntimeMutableFeaturesHandler::HandleSetSeedFetchingPaused(
    const base::ListValue& args) {
  // args[0]: Callback ID.
  // args[1]: Whether to pause seed fetching (bool).
  CHECK_EQ(args.size(), 2U);
  base_handler_->HandleSetSeedFetchingPaused(args[0], args[1].GetBool());
}

void RuntimeMutableFeaturesHandler::HandleUploadSeed(
    const base::ListValue& args) {
  // args[0]: Callback ID.
  // args[1]: The seed bytes (binary blob).
  CHECK_EQ(args.size(), 2U);
  base_handler_->HandleUploadSeed(args[0], args[1]);
}

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/runtime_mutable_features_handler.cc)
