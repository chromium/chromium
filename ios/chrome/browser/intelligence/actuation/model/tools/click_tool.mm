// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool_java_script_feature.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

ClickTool::~ClickTool() = default;

// static
base::expected<std::unique_ptr<ClickTool>, ActuationError> ClickTool::Create(
    const optimization_guide::proto::ClickAction& action,
    ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  BrowserAndIndex browser_and_index = FindBrowserAndIndex(
      web::WebStateID::FromSerializedValue(action.tab_id()),
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular));
  Browser* browser = browser_and_index.browser;

  if (browser_and_index.tab_index == WebStateList::kInvalidIndex || !browser) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationTargetTabNotFound});
  }

  if (!action.has_click_count() || !action.has_click_type()) {
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  if (!action.has_target() || !action.target().has_coordinate()) {
    // Bling currently only uses the Coordinate field of the ActionTarget.
    // TODO(crbug.com/476461762): add support for (document_identifier,
    // dom_node_id).
    return base::unexpected(
        ActuationError{ActuationErrorCode::kCreationMissingRequiredFields});
  }

  return std::unique_ptr<ClickTool>(
      new ClickTool(action, browser->GetWebStateList()
                                ->GetWebStateAt(browser_and_index.tab_index)
                                ->GetWeakPtr()));
}

void ClickTool::Execute(ActuationCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }
  web::WebState* web_state = web_state_.get();
  web::WebFramesManager* frames_manager =
      js_feature_->GetWebFramesManager(web_state);
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    std::move(callback).Run(base::unexpected(
        ActuationError{ActuationErrorCode::kExecutionMissingDependencies}));
    return;
  }

  js_feature_->Click(frames_manager->GetMainWebFrame(), action_,
                     std::move(callback));
}

ClickTool::ClickTool(const optimization_guide::proto::ClickAction& action,
                     base::WeakPtr<web::WebState> web_state)
    : action_(action),
      web_state_(web_state),
      js_feature_(ClickToolJavaScriptFeature::GetInstance()) {}
