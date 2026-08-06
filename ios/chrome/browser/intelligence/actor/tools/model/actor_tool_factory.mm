// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_login_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/history_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/navigate_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_to_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/scroll_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/select_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tab_management_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/wait_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state_id.h"

namespace actor {
namespace {

// Returns whether the given tool requires a tab_id field.
bool RequiresTabId(const ActorToolRequest& request) {
  switch (request.action().action_case()) {
    case optimization_guide::proto::Action::kNavigate:
    case optimization_guide::proto::Action::kClick:
    case optimization_guide::proto::Action::kBack:
    case optimization_guide::proto::Action::kForward:
    case optimization_guide::proto::Action::kType:
    case optimization_guide::proto::Action::kScroll:
    case optimization_guide::proto::Action::kScrollTo:
    case optimization_guide::proto::Action::kSelect:
    case optimization_guide::proto::Action::kAttemptLogin:
    case optimization_guide::proto::Action::kAttemptFormFilling:
    case optimization_guide::proto::Action::kCloseTab:
    case optimization_guide::proto::Action::kActivateTab:
      return true;
    default:
      return false;
  }
}

}  // namespace

ActorToolFactory::ActorToolFactory(ProfileIOS* profile)
    : profile_context_resolver_(profile) {}
ActorToolFactory::~ActorToolFactory() = default;

base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
ActorToolFactory::CreateTool(const ActorToolRequest& request,
                             ToolDelegate* tool_delegate) {
  if (IsToolDisabled(request.action().action_case())) {
    return base::unexpected(
        ToolExecutionResult(InternalToolErrorCode::kToolDisabledByFeature));
  }

  base::WeakPtr<web::WebState> target_web_state = nullptr;
  base::WeakPtr<WebStateList> target_web_state_list = nullptr;
  std::optional<base::expected<ProfileContextResolver::TabResolutionResult,
                               ToolExecutionResult>>
      tab_resolution = std::nullopt;

  // Get the target objects if required by the requests.
  if (request.GetTargetWebStateId().valid()) {
    tab_resolution = profile_context_resolver_.ResolveTab(
        request.GetTargetWebStateId().identifier());
    if (!tab_resolution->has_value()) {
      return base::unexpected(tab_resolution->error());
    }
    target_web_state = tab_resolution->value().web_state;
    target_web_state_list = tab_resolution->value().web_state_list;
  } else if (RequiresTabId(request)) {
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
  }

  // LINT.IfChange(CreateTool)
  switch (request.action().action_case()) {
    case optimization_guide::proto::Action::kNavigate: {
      // TODO(crbug.com/515423965): grab this from a new
      // ProfileContextResolver::GetUrlLoadingAgent.
      base::WeakPtr<UrlLoadingBrowserAgent> url_loader =
          tab_resolution && tab_resolution->has_value()
              ? tab_resolution->value().url_loader
              : nullptr;
      return NavigateTool::Create(target_web_state, request.action().navigate(),
                                  url_loader);
    }
    case optimization_guide::proto::Action::kClick:
      return ClickTool::Create(target_web_state, request.action().click());
    case optimization_guide::proto::Action::kBack:
      return HistoryTool::Create(target_web_state, request.action().back());
    case optimization_guide::proto::Action::kForward:
      return HistoryTool::Create(target_web_state, request.action().forward());
    case optimization_guide::proto::Action::kSelect:
      return SelectTool::Create(target_web_state, request.action().select());
    case optimization_guide::proto::Action::kType:
      return TypeTool::Create(target_web_state, request.action().type());
    case optimization_guide::proto::Action::kWait:
      return WaitTool::Create(target_web_state, request.action().wait());
    case optimization_guide::proto::Action::kScroll:
      return ScrollTool::Create(target_web_state, request.action().scroll());
    case optimization_guide::proto::Action::kScrollTo:
      return ScrollToTool::Create(target_web_state,
                                  request.action().scroll_to());
    case optimization_guide::proto::Action::kAttemptLogin:
      return AttemptLoginTool::Create(
          target_web_state, request.action().attempt_login(), tool_delegate);
    case optimization_guide::proto::Action::kAttemptFormFilling:
      return AttemptFormFillingTool::Create(
          target_web_state, request.action().attempt_form_filling(),
          tool_delegate);
    case optimization_guide::proto::Action::kCloseTab:
      return TabManagementTool::CreateCloseTabTool(target_web_state,
                                                   target_web_state_list);
    case optimization_guide::proto::Action::kCreateTab:
      return TabManagementTool::CreateTabTool(request.action().create_tab(),
                                              tool_delegate);
    case optimization_guide::proto::Action::kActivateTab:
      return TabManagementTool::CreateActivateTabTool(target_web_state,
                                                      target_web_state_list);
    default:
      return base::unexpected(
          ToolExecutionResult(InternalToolErrorCode::kUnsupportedAction));
  }
  // LINT.ThenChange(
  //   //ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:SupportedCapabilities,
  //   //ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.mm:InjectDataIntoAction,
  //   //ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.mm:GetToolType,
  //   //ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.mm:GetTargetWebStateId
  // )
}

std::vector<optimization_guide::proto::Action::ActionCase>
ActorToolFactory::GetSupportedCapabilities() const {
  // LINT.IfChange(SupportedCapabilities)
  const optimization_guide::proto::Action::ActionCase kCandidates[] = {
      optimization_guide::proto::Action::kNavigate,
      optimization_guide::proto::Action::kClick,
      optimization_guide::proto::Action::kBack,
      optimization_guide::proto::Action::kForward,
      optimization_guide::proto::Action::kType,
      optimization_guide::proto::Action::kWait,
      optimization_guide::proto::Action::kScroll,
      optimization_guide::proto::Action::kScrollTo,
      optimization_guide::proto::Action::kSelect,
      optimization_guide::proto::Action::kAttemptLogin,
      optimization_guide::proto::Action::kAttemptFormFilling,
      optimization_guide::proto::Action::kCloseTab,
      optimization_guide::proto::Action::kCreateTab,
      optimization_guide::proto::Action::kActivateTab,
  };
  // LINT.ThenChange(//ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.mm:CreateTool)

  std::vector<optimization_guide::proto::Action::ActionCase> capabilities;
  for (const optimization_guide::proto::Action::ActionCase& tool :
       kCandidates) {
    if (!IsToolDisabled(tool)) {
      capabilities.push_back(tool);
    }
  }
  return capabilities;
}

}  // namespace actor
