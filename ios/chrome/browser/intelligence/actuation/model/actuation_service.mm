// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_service.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_tool_factory.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

using ActuationCallback = ActuationTool::ActuationCallback;
using ActuationError = ActuationTool::ActuationError;
using ActuationErrorCode = ActuationTool::ActuationErrorCode;

ActuationService::ActuationService(ProfileIOS* profile)
    : profile_(profile),
      tool_factory_(std::make_unique<ActuationToolFactory>()) {}

ActuationService::~ActuationService() = default;

void ActuationService::Shutdown() {}

void ActuationService::ExecuteAction(
    const optimization_guide::proto::Action& action,
    ActuationCallback callback) {
  CHECK(IsActuationEnabled());

  if (action.action_case() ==
      optimization_guide::proto::Action::ACTION_NOT_SET) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kUnsupportedAction,
                       "There isn't a tool to support this action."}));
    return;
  }

  if (IsToolDisabled(action.action_case())) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kToolDisabled,
                       "Tool is disabled via feature parameter."}));
    return;
  }

  base::expected<std::unique_ptr<ActuationTool>, ActuationError> result =
      tool_factory_->CreateTool(action, profile_);
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(result.error()));
    return;
  }

  // TODO(crbug.com/472289603): `command` is destroyed immediately after
  // `Execute` returns. If Execute needs to call back into this service,
  // the `command` will no longer be available.
  std::unique_ptr<ActuationTool> command = std::move(result.value());
  command->Execute(std::move(callback));
}
