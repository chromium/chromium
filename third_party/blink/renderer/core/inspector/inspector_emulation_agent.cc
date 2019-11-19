// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/inspector/protocol/DOM.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_cpu_throttler.h"

namespace blink {
using protocol::Maybe;
using protocol::Response;

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* web_local_frame_impl)
    : web_local_frame_(web_local_frame_impl),
      default_background_color_override_rgba_(&agent_state_,
                                              /*default_value=*/{}),
      script_execution_disabled_(&agent_state_, /*default_value=*/false),
      scrollbars_hidden_(&agent_state_, /*default_value=*/false),
      document_cookie_disabled_(&agent_state_, /*default_value=*/false),
      touch_event_emulation_enabled_(&agent_state_, /*default_value=*/false),
      max_touch_points_(&agent_state_, /*default_value=*/1),
      emulated_media_(&agent_state_, /*default_value=*/WTF::String()),
      emulated_media_features_(&agent_state_, /*default_value=*/WTF::String()),
      navigator_platform_override_(&agent_state_,
                                   /*default_value=*/WTF::String()),
      user_agent_override_(&agent_state_, /*default_value=*/WTF::String()),
      accept_language_override_(&agent_state_,
                                /*default_value=*/WTF::String()),
      virtual_time_budget_(&agent_state_, /*default_value*/ 0.0),
      virtual_time_budget_initial_offset_(&agent_state_, /*default_value=*/0.0),
      initial_virtual_time_(&agent_state_, /*default_value=*/0.0),
      virtual_time_offset_(&agent_state_, /*default_value=*/0.0),
      virtual_time_policy_(&agent_state_, /*default_value=*/WTF::String()),
      virtual_time_task_starvation_count_(&agent_state_, /*default_value=*/0),
      wait_for_navigation_(&agent_state_, /*default_value=*/false),
      emulate_focus_(&agent_state_, /*default_value=*/false),
      timezone_id_override_(&agent_state_, /*default_value=*/WTF::String()) {}

InspectorEmulationAgent::~InspectorEmulationAgent() = default;

WebViewImpl* InspectorEmulationAgent::GetWebViewImpl() {
  return web_local_frame_ ? web_local_frame_->ViewImpl() : nullptr;
}

namespace {
std::unique_ptr<protocol::DOM::RGBA> ParseRGBA(
    const std::vector<uint8_t>& cbor) {
  auto parsed = protocol::Value::parseBinary(cbor.data(), cbor.size());
  if (!parsed)
    return nullptr;
  blink::protocol::ErrorSupport errors;
  auto rgba = protocol::DOM::RGBA::fromValue(parsed.get(), &errors);
  if (errors.hasErrors())
    return nullptr;
  return rgba;
}
}  // namespace

void InspectorEmulationAgent::Restore() {
  setUserAgentOverride(user_agent_override_.Get(),
                       accept_language_override_.Get(),
                       navigator_platform_override_.Get());
  if (!web_local_frame_)
    return;

  // Following code only runs for pages.
  if (script_execution_disabled_.Get())
    GetWebViewImpl()->GetDevToolsEmulator()->SetScriptExecutionDisabled(true);
  if (scrollbars_hidden_.Get())
    GetWebViewImpl()->GetDevToolsEmulator()->SetScrollbarsHidden(true);
  if (document_cookie_disabled_.Get())
    GetWebViewImpl()->GetDevToolsEmulator()->SetDocumentCookieDisabled(true);
  setTouchEmulationEnabled(touch_event_emulation_enabled_.Get(),
                           max_touch_points_.Get());
  auto features =
      std::make_unique<protocol::Array<protocol::Emulation::MediaFeature>>();
  for (auto const& name : emulated_media_features_.Keys()) {
    auto const& value = emulated_media_features_.Get(name);
    features->push_back(protocol::Emulation::MediaFeature::create()
                            .setName(name)
                            .setValue(value)
                            .build());
  }
  setEmulatedMedia(emulated_media_.Get(), std::move(features));
  auto rgba = ParseRGBA(default_background_color_override_rgba_.Get());
  if (rgba)
    setDefaultBackgroundColorOverride(std::move(rgba));
  setFocusEmulationEnabled(emulate_focus_.Get());

  if (!timezone_id_override_.Get().IsNull())
    setTimezoneOverride(timezone_id_override_.Get());

  if (virtual_time_policy_.Get().IsNull())
    return;
  // Tell the scheduler about the saved virtual time progress to ensure that
  // virtual time monotonically advances despite the cross origin navigation.
  // This should be done regardless of the virtual time mode.
  web_local_frame_->View()->Scheduler()->SetInitialVirtualTimeOffset(
      base::TimeDelta::FromMillisecondsD(virtual_time_offset_.Get()));

  // Preserve wait for navigation in all modes.
  bool wait_for_navigation = wait_for_navigation_.Get();

  // Reinstate the stored policy.
  double virtual_time_ticks_base_ms;

  // For Pause, do not pass budget or starvation count.
  if (virtual_time_policy_.Get() ==
      protocol::Emulation::VirtualTimePolicyEnum::Pause) {
    setVirtualTimePolicy(protocol::Emulation::VirtualTimePolicyEnum::Pause,
                         Maybe<double>(), Maybe<int>(), wait_for_navigation,
                         initial_virtual_time_.Get(),
                         &virtual_time_ticks_base_ms);
    return;
  }

  // Calculate remaining budget for the advancing modes.
  double budget_remaining = virtual_time_budget_.Get() +
                            virtual_time_budget_initial_offset_.Get() -
                            virtual_time_offset_.Get();
  DCHECK_GE(budget_remaining, 0);

  setVirtualTimePolicy(virtual_time_policy_.Get(), budget_remaining,
                       virtual_time_task_starvation_count_.Get(),
                       wait_for_navigation, initial_virtual_time_.Get(),
                       &virtual_time_ticks_base_ms);
}

Response InspectorEmulationAgent::disable() {
  if (enabled_)
    instrumenting_agents_->RemoveInspectorEmulationAgent(this);
  setUserAgentOverride(String(), protocol::Maybe<String>(),
                       protocol::Maybe<String>());
  if (!web_local_frame_)
    return Response::OK();
  setScriptExecutionDisabled(false);
  setScrollbarsHidden(false);
  setDocumentCookieDisabled(false);
  setTouchEmulationEnabled(false, Maybe<int>());
  // Clear emulated media features. Note that the current approach
  // doesn't work well in cases where two clients have the same set of
  // features overridden to the same value by two different clients
  // (e.g. if we allowed two different front-ends with the same
  // settings to attach to the same page). TODO: support this use case.
  setEmulatedMedia(String(), {});
  setCPUThrottlingRate(1);
  setFocusEmulationEnabled(false);
  setDefaultBackgroundColorOverride(Maybe<protocol::DOM::RGBA>());
  return Response::OK();
}

Response InspectorEmulationAgent::resetPageScaleFactor() {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  GetWebViewImpl()->ResetScaleStateImmediately();
  return response;
}

Response InspectorEmulationAgent::setPageScaleFactor(double page_scale_factor) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  GetWebViewImpl()->SetPageScaleFactor(static_cast<float>(page_scale_factor));
  return response;
}

Response InspectorEmulationAgent::setScriptExecutionDisabled(bool value) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (script_execution_disabled_.Get() == value)
    return response;
  script_execution_disabled_.Set(value);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScriptExecutionDisabled(value);
  return response;
}

Response InspectorEmulationAgent::setScrollbarsHidden(bool hidden) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (scrollbars_hidden_.Get() == hidden)
    return response;
  scrollbars_hidden_.Set(hidden);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScrollbarsHidden(hidden);
  return response;
}

Response InspectorEmulationAgent::setDocumentCookieDisabled(bool disabled) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (document_cookie_disabled_.Get() == disabled)
    return response;
  document_cookie_disabled_.Set(disabled);
  GetWebViewImpl()->GetDevToolsEmulator()->SetDocumentCookieDisabled(disabled);
  return response;
}

Response InspectorEmulationAgent::setTouchEmulationEnabled(
    bool enabled,
    protocol::Maybe<int> max_touch_points) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  int max_points = max_touch_points.fromMaybe(1);
  if (max_points < 1 || max_points > WebTouchEvent::kTouchesLengthCap) {
    return Response::InvalidParams("Touch points must be between 1 and " +
                                   String::Number(static_cast<uint16_t>(
                                       WebTouchEvent::kTouchesLengthCap)));
  }
  touch_event_emulation_enabled_.Set(enabled);
  max_touch_points_.Set(max_points);
  GetWebViewImpl()->GetDevToolsEmulator()->SetTouchEventEmulationEnabled(
      enabled, max_points);
  return response;
}

Response InspectorEmulationAgent::setEmulatedMedia(
    Maybe<String> media,
    Maybe<protocol::Array<protocol::Emulation::MediaFeature>> features) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (media.isJust()) {
    auto mediaValue = media.takeJust();
    emulated_media_.Set(mediaValue);
    GetWebViewImpl()->GetPage()->GetSettings().SetMediaTypeOverride(mediaValue);
  } else {
    emulated_media_.Set("");
    GetWebViewImpl()->GetPage()->GetSettings().SetMediaTypeOverride("");
  }
  for (const WTF::String& feature : emulated_media_features_.Keys()) {
    GetWebViewImpl()->GetPage()->SetMediaFeatureOverride(AtomicString(feature),
                                                         "");
  }
  emulated_media_features_.Clear();
  if (features.isJust()) {
    auto featuresValue = features.takeJust();
    for (auto const& mediaFeature : *featuresValue.get()) {
      auto const& name = mediaFeature->getName();
      auto const& value = mediaFeature->getValue();
      emulated_media_features_.Set(name, value);
      GetWebViewImpl()->GetPage()->SetMediaFeatureOverride(AtomicString(name),
                                                           value);
    }
  }
  return response;
}

Response InspectorEmulationAgent::setCPUThrottlingRate(double rate) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  scheduler::ThreadCPUThrottler::GetInstance()->SetThrottlingRate(rate);
  return response;
}

Response InspectorEmulationAgent::setFocusEmulationEnabled(bool enabled) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  emulate_focus_.Set(enabled);
  GetWebViewImpl()->GetPage()->GetFocusController().SetFocusEmulationEnabled(
      enabled);
  return response;
}

Response InspectorEmulationAgent::setVirtualTimePolicy(
    const String& policy,
    Maybe<double> virtual_time_budget_ms,
    protocol::Maybe<int> max_virtual_time_task_starvation_count,
    protocol::Maybe<bool> wait_for_navigation,
    protocol::Maybe<double> initial_virtual_time,
    double* virtual_time_ticks_base_ms) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  virtual_time_policy_.Set(policy);

  PendingVirtualTimePolicy new_policy;
  new_policy.policy = PageScheduler::VirtualTimePolicy::kPause;
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    new_policy.policy = PageScheduler::VirtualTimePolicy::kAdvance;
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    new_policy.policy = PageScheduler::VirtualTimePolicy::kDeterministicLoading;
  }

  if (new_policy.policy == PageScheduler::VirtualTimePolicy::kPause &&
      virtual_time_budget_ms.isJust()) {
    LOG(ERROR) << "Can only specify virtual time budget for non-Pause policy";
    return Response::InvalidParams(
        "Can only specify budget for non-Pause policy");
  }
  if (new_policy.policy == PageScheduler::VirtualTimePolicy::kPause &&
      max_virtual_time_task_starvation_count.isJust()) {
    LOG(ERROR)
        << "Can only specify virtual time starvation for non-Pause policy";
    return Response::InvalidParams(
        "Can only specify starvation count for non-Pause policy");
  }

  if (virtual_time_budget_ms.isJust()) {
    new_policy.virtual_time_budget_ms = virtual_time_budget_ms.fromJust();
    virtual_time_budget_.Set(*new_policy.virtual_time_budget_ms);
    // Record the current virtual time offset so Restore can compute how much
    // budget is left.
    virtual_time_budget_initial_offset_.Set(virtual_time_offset_.Get());
  } else {
    virtual_time_budget_.Clear();
  }

  if (max_virtual_time_task_starvation_count.isJust()) {
    new_policy.max_virtual_time_task_starvation_count =
        max_virtual_time_task_starvation_count.fromJust();
    virtual_time_task_starvation_count_.Set(
        *new_policy.max_virtual_time_task_starvation_count);
  } else {
    virtual_time_task_starvation_count_.Clear();
  }

  InnerEnable();

  // This needs to happen before we apply virtual time.
  if (initial_virtual_time.isJust()) {
    initial_virtual_time_.Set(initial_virtual_time.fromJust());
    web_local_frame_->View()->Scheduler()->SetInitialVirtualTime(
        base::Time::FromDoubleT(initial_virtual_time.fromJust()));
  }

  if (wait_for_navigation.fromMaybe(false)) {
    wait_for_navigation_.Set(true);
    pending_virtual_time_policy_ = std::move(new_policy);
  } else {
    ApplyVirtualTimePolicy(new_policy);
  }

  if (virtual_time_base_ticks_.is_null()) {
    *virtual_time_ticks_base_ms = 0;
  } else {
    *virtual_time_ticks_base_ms =
        (virtual_time_base_ticks_ - base::TimeTicks()).InMillisecondsF();
  }

  return response;
}

void InspectorEmulationAgent::ApplyVirtualTimePolicy(
    const PendingVirtualTimePolicy& new_policy) {
  DCHECK(web_local_frame_);
  web_local_frame_->View()->Scheduler()->SetVirtualTimePolicy(
      new_policy.policy);
  virtual_time_base_ticks_ =
      web_local_frame_->View()->Scheduler()->EnableVirtualTime();
  if (new_policy.virtual_time_budget_ms) {
    TRACE_EVENT_ASYNC_BEGIN1("renderer.scheduler", "VirtualTimeBudget", this,
                             "budget", *new_policy.virtual_time_budget_ms);
    base::TimeDelta budget_amount =
        base::TimeDelta::FromMillisecondsD(*new_policy.virtual_time_budget_ms);
    web_local_frame_->View()->Scheduler()->GrantVirtualTimeBudget(
        budget_amount,
        WTF::Bind(&InspectorEmulationAgent::VirtualTimeBudgetExpired,
                  WrapWeakPersistent(this)));
  }
  if (new_policy.max_virtual_time_task_starvation_count) {
    web_local_frame_->View()->Scheduler()->SetMaxVirtualTimeTaskStarvationCount(
        *new_policy.max_virtual_time_task_starvation_count);
  }
}

void InspectorEmulationAgent::FrameStartedLoading(LocalFrame*) {
  if (pending_virtual_time_policy_) {
    wait_for_navigation_.Set(false);
    ApplyVirtualTimePolicy(*pending_virtual_time_policy_);
    pending_virtual_time_policy_ = base::nullopt;
  }
}

void InspectorEmulationAgent::PrepareRequest(
    DocumentLoader* loader,
    ResourceRequest& request,
    const FetchInitiatorInfo& initiator_info,
    ResourceType resource_type) {
  if (!accept_language_override_.Get().IsEmpty() &&
      request.HttpHeaderField("Accept-Language").IsEmpty()) {
    request.SetHttpHeaderField(
        "Accept-Language",
        AtomicString(network_utils::GenerateAcceptLanguageHeader(
            accept_language_override_.Get())));
  }
}

Response InspectorEmulationAgent::setNavigatorOverrides(
    const String& platform) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  navigator_platform_override_.Set(platform);
  GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
      platform);
  return response;
}

void InspectorEmulationAgent::VirtualTimeBudgetExpired() {
  TRACE_EVENT_ASYNC_END0("renderer.scheduler", "VirtualTimeBudget", this);
  WebView* view = web_local_frame_->View();
  if (!view)
    return;

  view->Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kPause);
  virtual_time_policy_.Set(protocol::Emulation::VirtualTimePolicyEnum::Pause);
  GetFrontend()->virtualTimeBudgetExpired();
}

Response InspectorEmulationAgent::setDefaultBackgroundColorOverride(
    Maybe<protocol::DOM::RGBA> color) {
  Response response = AssertPage();
  if (!response.isSuccess())
    return response;
  if (!color.isJust()) {
    // Clear the override and state.
    GetWebViewImpl()->ClearBaseBackgroundColorOverride();
    default_background_color_override_rgba_.Clear();
    return Response::OK();
  }

  blink::protocol::DOM::RGBA* rgba = color.fromJust();
  default_background_color_override_rgba_.Set(
      std::move(*rgba).TakeSerialized());
  // Clamping of values is done by Color() constructor.
  int alpha = static_cast<int>(lroundf(255.0f * rgba->getA(1.0f)));
  GetWebViewImpl()->SetBaseBackgroundColorOverride(
      Color(rgba->getR(), rgba->getG(), rgba->getB(), alpha).Rgb());
  return Response::OK();
}

Response InspectorEmulationAgent::setDeviceMetricsOverride(
    int width,
    int height,
    double device_scale_factor,
    bool mobile,
    Maybe<double> scale,
    Maybe<int> screen_width,
    Maybe<int> screen_height,
    Maybe<int> position_x,
    Maybe<int> position_y,
    Maybe<bool> dont_set_visible_size,
    Maybe<protocol::Emulation::ScreenOrientation>,
    Maybe<protocol::Page::Viewport>) {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been updated by the handling of
  // WidgetMsg_EnableDeviceEmulation.
  return AssertPage();
}

Response InspectorEmulationAgent::clearDeviceMetricsOverride() {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been cleared by the handling of
  // WidgetMsg_DisableDeviceEmulation.
  return AssertPage();
}

Response InspectorEmulationAgent::setUserAgentOverride(
    const String& user_agent,
    protocol::Maybe<String> accept_language,
    protocol::Maybe<String> platform) {
  if (!user_agent.IsEmpty() || accept_language.isJust() || platform.isJust())
    InnerEnable();
  user_agent_override_.Set(user_agent);
  accept_language_override_.Set(accept_language.fromMaybe(String()));
  navigator_platform_override_.Set(platform.fromMaybe(String()));
  if (web_local_frame_) {
    GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
        navigator_platform_override_.Get());
  }
  return Response::OK();
}

Response InspectorEmulationAgent::setTimezoneOverride(
    const String& timezone_id) {
  timezone_override_.reset();
  if (!timezone_id.IsEmpty()) {
    timezone_override_ = TimeZoneController::SetTimeZoneOverride(timezone_id);
    if (!timezone_override_) {
      return TimeZoneController::HasTimeZoneOverride()
                 ? Response::Error("Timezone override is already in effect")
                 : Response::InvalidParams("Invalid timezone id");
    }
  }

  timezone_id_override_.Set(timezone_id);

  return Response::OK();
}

void InspectorEmulationAgent::ApplyAcceptLanguageOverride(String* accept_lang) {
  if (!accept_language_override_.Get().IsEmpty())
    *accept_lang = accept_language_override_.Get();
}

void InspectorEmulationAgent::ApplyUserAgentOverride(String* user_agent) {
  if (!user_agent_override_.Get().IsEmpty())
    *user_agent = user_agent_override_.Get();
}

void InspectorEmulationAgent::InnerEnable() {
  if (enabled_)
    return;
  enabled_ = true;
  instrumenting_agents_->AddInspectorEmulationAgent(this);
}

Response InspectorEmulationAgent::AssertPage() {
  if (!web_local_frame_) {
    LOG(ERROR) << "Can only enable virtual time for pages, not workers";
    return Response::InvalidParams(
        "Can only enable virtual time for pages, not workers");
  }
  return Response::OK();
}

void InspectorEmulationAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
