// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_tool_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/navigate_tool.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/web/public/web_state.h"

ActuationToolFactory::ActuationToolFactory() = default;
ActuationToolFactory::~ActuationToolFactory() = default;

base::expected<std::unique_ptr<ActuationTool>, ActuationError>
ActuationToolFactory::CreateTool(
    const optimization_guide::proto::Action& action,
    ProfileIOS* profile) {
  switch (action.action_case()) {
    case optimization_guide::proto::Action::kNavigate:
      return NavigateTool::Create(action.navigate(), profile);
    case optimization_guide::proto::Action::kClick:
      return ClickTool::Create(action.click(), profile);
    default:
      return base::unexpected(
          ActuationError{ActuationErrorCode::kUnsupportedAction});
  }
}
