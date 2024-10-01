// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/automation_internal/automation_internal_api.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api_delegate.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/api/automation.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/automation.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_updates_and_events.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace extensions {
class AutomationWebContentsObserver;
}  // namespace extensions

namespace extensions {

namespace {

const char kCannotRequestAutomationOnPage[] =
    "Failed request of automation on a page";

// Helper function to convert extension action to ax action.
// |extension_id| can be the empty string.
// |data| is an out param.
AutomationInternalPerformActionFunction::Result ConvertToAXActionData(
    const ui::AXTreeID& tree_id,
    int32_t automation_node_id,
    const std::string& action_type_string,
    int request_id,
    const base::Value::Dict& additional_properties,
    const ExtensionId& extension_id,
    ui::AXActionData* action) {
  AutomationInternalPerformActionFunction::Result validation_error_result;
  validation_error_result.validation_success = false;
  AutomationInternalPerformActionFunction::Result success_result;
  success_result.validation_success = true;
  action->target_tree_id = tree_id;
  action->source_extension_id = extension_id;
  action->target_node_id = automation_node_id;
  action->request_id = request_id;
  api::automation::ActionType action_type =
      api::automation::ParseActionType(action_type_string);
  switch (action_type) {
    case api::automation::ActionType::kBlur:
      action->action = ax::mojom::Action::kBlur;
      break;
    case api::automation::ActionType::kClearAccessibilityFocus:
      action->action = ax::mojom::Action::kClearAccessibilityFocus;
      break;
    case api::automation::ActionType::kDecrement:
      action->action = ax::mojom::Action::kDecrement;
      break;
    case api::automation::ActionType::kDoDefault:
      action->action = ax::mojom::Action::kDoDefault;
      break;
    case api::automation::ActionType::kIncrement:
      action->action = ax::mojom::Action::kIncrement;
      break;
    case api::automation::ActionType::kFocus:
      action->action = ax::mojom::Action::kFocus;
      break;
    case api::automation::ActionType::kGetImageData: {
      auto get_image_data_params =
          api::automation_internal::GetImageDataParams::FromValue(
              additional_properties);
      if (!get_image_data_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kGetImageData;
      action->target_rect = gfx::Rect(0, 0, get_image_data_params->max_width,
                                      get_image_data_params->max_height);
      break;
    }
    case api::automation::ActionType::kHitTest: {
      auto hit_test_params = api::automation_internal::HitTestParams::FromValue(
          additional_properties);
      if (!hit_test_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kHitTest;
      action->target_point = gfx::Point(hit_test_params->x, hit_test_params->y);
      action->hit_test_event_to_fire = ui::ParseAXEnum<ax::mojom::Event>(
          hit_test_params->event_to_fire.c_str());
      if (action->hit_test_event_to_fire == ax::mojom::Event::kNone) {
        return success_result;
      }
      break;
    }
    case api::automation::ActionType::kLoadInlineTextBoxes:
      action->action = ax::mojom::Action::kLoadInlineTextBoxes;
      break;
    case api::automation::ActionType::kSetAccessibilityFocus:
      action->action = ax::mojom::Action::kSetAccessibilityFocus;
      break;
    case api::automation::ActionType::kScrollToMakeVisible:
      action->action = ax::mojom::Action::kScrollToMakeVisible;
      action->horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
      action->vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
      action->scroll_behavior =
          ax::mojom::ScrollBehavior::kDoNotScrollIfVisible;
      break;
    case api::automation::ActionType::kScrollBackward:
      action->action = ax::mojom::Action::kScrollBackward;
      break;
    case api::automation::ActionType::kScrollForward:
      action->action = ax::mojom::Action::kScrollForward;
      break;
    case api::automation::ActionType::kScrollUp:
      action->action = ax::mojom::Action::kScrollUp;
      break;
    case api::automation::ActionType::kScrollDown:
      action->action = ax::mojom::Action::kScrollDown;
      break;
    case api::automation::ActionType::kScrollLeft:
      action->action = ax::mojom::Action::kScrollLeft;
      break;
    case api::automation::ActionType::kScrollRight:
      action->action = ax::mojom::Action::kScrollRight;
      break;
    case api::automation::ActionType::kSetSelection: {
      auto selection_params =
          api::automation_internal::SetSelectionParams::FromValue(
              additional_properties);
      if (!selection_params) {
        return validation_error_result;
      }
      action->anchor_node_id = automation_node_id;
      action->anchor_offset = selection_params->anchor_offset;
      action->focus_node_id = selection_params->focus_node_id;
      action->focus_offset = selection_params->focus_offset;
      action->action = ax::mojom::Action::kSetSelection;
      break;
    }
    case api::automation::ActionType::kShowContextMenu: {
      action->action = ax::mojom::Action::kShowContextMenu;
      break;
    }
    case api::automation::ActionType::
        kSetSequentialFocusNavigationStartingPoint: {
      action->action =
          ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
      break;
    }
    case api::automation::ActionType::kCustomAction: {
      auto perform_custom_action_params =
          api::automation_internal::PerformCustomActionParams::FromValue(
              additional_properties);
      if (!perform_custom_action_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kCustomAction;
      action->custom_action_id = perform_custom_action_params->custom_action_id;
      break;
    }
    case api::automation::ActionType::kReplaceSelectedText: {
      auto replace_selected_text_params =
          api::automation_internal::ReplaceSelectedTextParams::FromValue(
              additional_properties);
      if (!replace_selected_text_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kReplaceSelectedText;
      action->value = replace_selected_text_params->value;
      break;
    }
    case api::automation::ActionType::kSetValue: {
      auto set_value_params =
          api::automation_internal::SetValueParams::FromValue(
              additional_properties);
      if (!set_value_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kSetValue;
      action->value = set_value_params->value;
      break;
    }
    case api::automation::ActionType::kScrollToPoint: {
      auto scroll_to_point_params =
          api::automation_internal::ScrollToPointParams::FromValue(
              additional_properties);
      if (!scroll_to_point_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kScrollToPoint;
      action->target_point =
          gfx::Point(scroll_to_point_params->x, scroll_to_point_params->y);
      break;
    }
    case api::automation::ActionType::kScrollToPositionAtRowColumn: {
      auto params = api::automation_internal::
          ScrollToPositionAtRowColumnParams::FromValue(additional_properties);
      if (!params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kScrollToPositionAtRowColumn;
      action->row_column = std::pair(params->row, params->column);
      break;
    }
    case api::automation::ActionType::kSetScrollOffset: {
      auto set_scroll_offset_params =
          api::automation_internal::SetScrollOffsetParams::FromValue(
              additional_properties);
      if (!set_scroll_offset_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kSetScrollOffset;
      action->target_point =
          gfx::Point(set_scroll_offset_params->x, set_scroll_offset_params->y);
      break;
    }
    case api::automation::ActionType::kGetTextLocation: {
      auto get_text_location_params =
          api::automation_internal::GetTextLocationDataParams::FromValue(
              additional_properties);
      if (!get_text_location_params) {
        return validation_error_result;
      }
      action->action = ax::mojom::Action::kGetTextLocation;
      action->start_index = get_text_location_params->start_index;
      action->end_index = get_text_location_params->end_index;
      break;
    }
    case api::automation::ActionType::kShowTooltip:
      action->action = ax::mojom::Action::kShowTooltip;
      break;
    case api::automation::ActionType::kHideTooltip:
      action->action = ax::mojom::Action::kHideTooltip;
      break;
    case api::automation::ActionType::kCollapse:
      action->action = ax::mojom::Action::kCollapse;
      break;
    case api::automation::ActionType::kExpand:
      action->action = ax::mojom::Action::kExpand;
      break;
    case api::automation::ActionType::kResumeMedia:
      action->action = ax::mojom::Action::kResumeMedia;
      break;
    case api::automation::ActionType::kStartDuckingMedia:
      action->action = ax::mojom::Action::kStartDuckingMedia;
      break;
    case api::automation::ActionType::kStopDuckingMedia:
      action->action = ax::mojom::Action::kStopDuckingMedia;
      break;
    case api::automation::ActionType::kSuspendMedia:
      action->action = ax::mojom::Action::kSuspendMedia;
      break;
    case api::automation::ActionType::kLongClick:
      action->action = ax::mojom::Action::kLongClick;
      break;
    case api::automation::ActionType::kAnnotatePageImages:
    case api::automation::ActionType::kSignalEndOfTest:
    case api::automation::ActionType::kStitchChildTree:
    case api::automation::ActionType::kInternalInvalidateTree:
    case api::automation::ActionType::kNone:
      break;  // Actions that are only internally used.
  }
  return success_result;
}

}  // namespace

// Helper class that receives accessibility data from |WebContents|.
class AutomationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutomationWebContentsObserver>,
      public AutomationEventRouterObserver {
 public:
  AutomationWebContentsObserver(const AutomationWebContentsObserver&) = delete;
  AutomationWebContentsObserver& operator=(
      const AutomationWebContentsObserver&) = delete;

  ~AutomationWebContentsObserver() override {
    automation_event_router_observer_.Reset();
  }

  // Set the accessibility mode for the observed WebContents so that
  // accessibility events are forwarded to each WebContentsObserver. A reset is
  // performed if the WebContents already has web accessibility enabled.
  void EnableOrResetWebContentsAccessibility() {
    // Force a reset if web accessibility is already enabled to ensure that new
    // observers of accessibility events get the full accessibility tree from
    // scratch.
    const bool need_reset = web_contents()->GetAccessibilityMode().has_mode(
        ui::AXMode::kWebContents);

    if (!scoped_accessibility_mode_) {
      scoped_accessibility_mode_ =
          content::BrowserAccessibilityState::GetInstance()
              ->CreateScopedModeForWebContents(web_contents(),
                                               ui::kAXModeWebContentsOnly);
    }

    if (need_reset) {
      web_contents()->ResetAccessibility();
    }
  }

  // content::WebContentsObserver overrides.
  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& content_event_bundle) override {
    gfx::Point mouse_location;
#if defined(USE_AURA)
    mouse_location = aura::Env::GetInstance()->last_mouse_location();
#endif

    AutomationEventRouter* router = AutomationEventRouter::GetInstance();
    router->DispatchAccessibilityEvents(
        content_event_bundle.ax_tree_id, content_event_bundle.updates,
        mouse_location, content_event_bundle.events);
  }

  void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details) override {
    AutomationEventRouter* router = AutomationEventRouter::GetInstance();
    for (const auto& src : details.location_changes) {
      router->DispatchAccessibilityLocationChange(tree_id, src);
    }
  }

  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override {
    auto* render_frame_host =
        content::RenderFrameHost::FromID(id.frame_routing_id);
    if (!render_frame_host)
      return;

    ui::AXUpdatesAndEvents content_event_bundle;
    content_event_bundle.ax_tree_id = render_frame_host->GetAXTreeID();
    content_event_bundle.events.resize(1);
    content_event_bundle.events[0].event_type =
        ax::mojom::Event::kMediaStartedPlaying;
    AccessibilityEventReceived(content_event_bundle);
  }

  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override {
    auto* render_frame_host =
        content::RenderFrameHost::FromID(id.frame_routing_id);
    if (!render_frame_host)
      return;

    ui::AXUpdatesAndEvents content_event_bundle;
    content_event_bundle.ax_tree_id = render_frame_host->GetAXTreeID();
    content_event_bundle.events.resize(1);
    content_event_bundle.events[0].event_type =
        ax::mojom::Event::kMediaStoppedPlaying;
    AccessibilityEventReceived(content_event_bundle);
  }

  // AutomationEventRouterObserver overrides.
  void AllAutomationExtensionsGone() override {
    scoped_accessibility_mode_.reset();
  }

  void ExtensionListenerAdded() override {
    if (web_contents()) {
      EnableOrResetWebContentsAccessibility();

      // On ChromeOS Ash, the automation api is the native accessibility api.
      // For the purposes of tracking web contents accessibility like other
      // desktop platforms, record the same UMA metric as those platforms.
#if BUILDFLAG(IS_CHROMEOS)
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_INLINE_TEXT_BOXES,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_SCREEN_READER,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML_METADATA,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_LABEL_IMAGES,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
      UMA_HISTOGRAM_ENUMERATION(
          "Accessibility.ModeFlag",
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_PDF,
          ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_MAX);
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
  }

 private:
  friend class content::WebContentsUserData<AutomationWebContentsObserver>;

  explicit AutomationWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        content::WebContentsUserData<AutomationWebContentsObserver>(
            *web_contents),
        browser_context_(web_contents->GetBrowserContext()) {
    if (web_contents->IsCurrentlyAudible()) {
      content::RenderFrameHost* render_frame_host =
          web_contents->GetPrimaryMainFrame();
      if (!render_frame_host) {
        return;
      }

      ui::AXUpdatesAndEvents content_event_bundle;
      content_event_bundle.ax_tree_id = render_frame_host->GetAXTreeID();
      content_event_bundle.events.resize(1);
      content_event_bundle.events[0].event_type =
          ax::mojom::Event::kMediaStartedPlaying;
      AccessibilityEventReceived(content_event_bundle);
    }

    automation_event_router_observer_.Observe(
        AutomationEventRouter::GetInstance());
  }

  raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<extensions::AutomationEventRouter,
                          extensions::AutomationEventRouterObserver>
      automation_event_router_observer_{this};

  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutomationWebContentsObserver);

std::optional<std::string> AutomationInternalEnableTreeFunction::EnableTree(
    const ui::AXTreeID& ax_tree_id,
    const ExtensionId& extension_id) {
  AutomationInternalApiDelegate* automation_api_delegate =
      ExtensionsAPIClient::Get()->GetAutomationInternalApiDelegate();
  if (automation_api_delegate->EnableTree(ax_tree_id))
    return std::nullopt;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromAXTreeID(ax_tree_id);
  if (!render_frame_host) {
    return std::nullopt;
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  AutomationWebContentsObserver::CreateForWebContents(contents);

  // Only call this if this is the root of a frame tree, to avoid resetting
  // the accessibility state multiple times.
  if (render_frame_host->IsInPrimaryMainFrame()) {
    AutomationWebContentsObserver::FromWebContents(contents)
        ->EnableOrResetWebContentsAccessibility();
  }

  return std::nullopt;
}

ExtensionFunction::ResponseAction AutomationInternalEnableTreeFunction::Run() {
  using api::automation_internal::EnableTree::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ui::AXTreeID ax_tree_id = ui::AXTreeID::FromString(params->tree_id);
  std::optional<std::string> error = EnableTree(ax_tree_id, extension_id());
  if (error) {
    return RespondNow(Error(error.value()));
  } else {
    return RespondNow(NoArguments());
  }
}

AutomationInternalPerformActionFunction::Result::Result() = default;
AutomationInternalPerformActionFunction::Result::Result(const Result&) =
    default;
AutomationInternalPerformActionFunction::Result::~Result() = default;

// static
AutomationInternalPerformActionFunction::Result
AutomationInternalPerformActionFunction::PerformAction(
    const ui::AXActionData& data,
    const Extension* extension,
    const AutomationInfo* automation_info) {
  Result result;
  result.validation_success = true;

  // The ash implementation of crosapi registers itself as an action observer.
  // This allows it to forward actions in parallel to Lacros.
  ui::AXActionHandlerRegistry* registry =
      ui::AXActionHandlerRegistry::GetInstance();
  registry->PerformAction(data);

  ui::AXActionHandlerBase* action_handler =
      registry->GetActionHandler(data.target_tree_id);
  if (action_handler) {
    // Handle an AXActionHandler with a render frame host first. Some actions
    // require a render frame host -> web contents and this api requires web
    // contents to perform a permissions check.
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromAXTreeID(data.target_tree_id);
    if (render_frame_host) {
      content::WebContents* contents =
          content::WebContents::FromRenderFrameHost(render_frame_host);
      if (extension && automation_info) {
        if (!ExtensionsAPIClient::Get()
                 ->GetAutomationInternalApiDelegate()
                 ->CanRequestAutomation(extension, automation_info, contents)) {
          result.automation_error = kCannotRequestAutomationOnPage;
          return result;
        }
      } else {
        // If |extension| is nullptr, then Lacros is receiving a crosapi request
        // from ash to perform an action. We make the assumption this this is
        // allowed.
        // TODO(crbug.com/40753344): Confirm whether this assumption is
        // valid.
      }

      // Handle internal actions.
      content::MediaSession* session = content::MediaSession::Get(contents);
      switch (data.action) {
        case ax::mojom::Action::kStartDuckingMedia:
          session->StartDucking();
          return result;
        case ax::mojom::Action::kStopDuckingMedia:
          session->StopDucking();
          return result;
        case ax::mojom::Action::kResumeMedia:
          session->Resume(content::MediaSession::SuspendType::kSystem);
          return result;
        case ax::mojom::Action::kSuspendMedia:
          session->Suspend(content::MediaSession::SuspendType::kSystem);
          return result;
        default:
          break;
      }
    }

    action_handler->PerformAction(data);
  }

  return result;
}

ExtensionFunction::ResponseAction
AutomationInternalPerformActionFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info && automation_info->desktop);

  using api::automation_internal::PerformAction::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int request_id = params->args.request_id.value_or(-1);

  ui::AXActionData data;
  Result result = ConvertToAXActionData(
      ui::AXTreeID::FromString(params->args.tree_id),
      params->args.automation_node_id, params->args.action_type, request_id,
      params->opt_args.additional_properties, extension_id(), &data);

  if (!result.validation_success) {
    // This macro has a built in |return|.
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  result = PerformAction(data, extension(), automation_info);

  if (!result.validation_success) {
    // This macro has a built in |return|.
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  if (result.automation_error) {
    return RespondNow(Error(result.automation_error.value()));
  } else {
    return RespondNow(NoArguments());
  }
}

ExtensionFunction::ResponseAction
AutomationInternalEnableDesktopFunction::Run() {
#if defined(USE_AURA)
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  if (!automation_info || !automation_info->desktop)
    return RespondNow(Error("desktop permission must be requested"));

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerWithDesktopPermission(
      extension_id(), source_process_id(), GetSenderWebContents());

  AutomationInternalApiDelegate* automation_api_delegate =
      ExtensionsAPIClient::Get()->GetAutomationInternalApiDelegate();
  automation_api_delegate->EnableDesktop();
  ui::AXTreeID ax_tree_id = automation_api_delegate->GetAXTreeID();
  return RespondNow(
      ArgumentList(api::automation_internal::EnableDesktop::Results::Create(
          ax_tree_id.ToString())));
#else
  return RespondNow(Error("getDesktop is unsupported by this platform"));
#endif  // defined(USE_AURA)
}

ExtensionFunction::ResponseAction
AutomationInternalDisableDesktopFunction::Run() {
#if defined(USE_AURA)
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  if (!automation_info || !automation_info->desktop)
    return RespondNow(Error("desktop permission must be requested"));

  AutomationEventRouter::GetInstance()->UnregisterListenerWithDesktopPermission(
      source_process_id());
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("getDesktop is unsupported by this platform"));
#endif  // defined(USE_AURA)
}

}  // namespace extensions
