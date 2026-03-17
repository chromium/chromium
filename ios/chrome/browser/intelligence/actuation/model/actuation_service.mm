// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_service.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/actuation_util.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_tool_factory.h"
#import "ios/chrome/browser/intelligence/actuation/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

using ActuationCallback = ActuationTool::ActuationCallback;

ActuationService::ActuationService(ProfileIOS* profile)
    : ActuationService(profile, std::make_unique<ActuationToolFactory>()) {}

ActuationService::ActuationService(
    ProfileIOS* profile,
    std::unique_ptr<ActuationToolFactory> tool_factory)
    : profile_(profile),
      tool_factory_(std::move(tool_factory)),
      journal_(std::make_unique<AggregatedJournal>()) {
  CHECK(tool_factory_);
}

ActuationService::~ActuationService() = default;

void ActuationService::Shutdown() {}

void ActuationService::ExecuteAction(
    const optimization_guide::proto::Action& action,
    ActuationCallback callback) {
  CHECK(IsActuationEnabled());

  if (action.action_case() ==
      optimization_guide::proto::Action::ACTION_NOT_SET) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kUnsupportedAction}));
    return;
  }

  if (IsToolDisabled(action.action_case())) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kToolDisabledByFeature}));
    return;
  }

  std::string tool_name = ActuationActionCaseToToolName(action.action_case())
                              .value_or("unknown tool");

  // Log immediate attempt to create tool.
  journal_->Log(
      GURL(), TaskId{0},
      base::StringPrintf("Attempting to create tool: %s", tool_name.c_str()),
      {});

  base::expected<std::unique_ptr<ActuationTool>, ActuationError>
      create_tool_result = tool_factory_->CreateTool(action, profile_);

  if (!create_tool_result.has_value()) {
    // Log immediate failure to create tool.
    journal_->Log(
        GURL(), TaskId{0},
        base::StringPrintf("Failed to create tool: %s", tool_name.c_str()),
        {{"error", base::NumberToString(
                       static_cast<int>(create_tool_result.error().code))}});
    std::move(callback).Run(base::unexpected(create_tool_result.error()));
    return;
  }

  // TODO(crbug.com/472289603): `tool` is destroyed immediately after
  // `Execute` returns. If Execute needs to call back into this service,
  // the `tool` will no longer be available.
  std::unique_ptr<ActuationTool> tool = std::move(create_tool_result.value());

  // Start a Begin log when the Execute call starts.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> entry =
      journal_->CreatePendingAsyncEntry(
          GURL(), TaskId{0}, 0,
          base::StringPrintf("Execute Tool: %s", tool_name.c_str()), {});

  ActuationCallback wrapped_callback = base::BindOnce(
      [](std::unique_ptr<AggregatedJournal::PendingAsyncEntry> entry,
         ActuationCallback callback, ActuationTool::ActuationResult result) {
        std::vector<JournalDetails> details;
        if (!result.has_value()) {
          // Log if an error happens between Begin and End.
          details.push_back({"error", base::NumberToString(static_cast<int>(
                                          result.error().code))});
        }
        // End log when the Execute call finishes.
        entry->EndEntry(std::move(details));
        std::move(callback).Run(std::move(result));
      },
      std::move(entry), std::move(callback));

  tool->Execute(std::move(wrapped_callback));
}
