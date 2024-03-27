// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_emulator.h"
#include "third_party/blink/renderer/core/inspector/locale_controller.h"
#include "third_party/blink/renderer/core/inspector/protocol/dom.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/loader_freeze_mode.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_cpu_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/public/virtual_time_controller.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"

namespace blink {
using protocol::Maybe;

InspectorEmulationAgent::InspectorEmulationAgent(
    WebLocalFrameImpl* web_local_frame_impl,
    VirtualTimeController& virtual_time_controller)
    : web_local_frame_(web_local_frame_impl),
      virtual_time_controller_(virtual_time_controller),
      default_background_color_override_rgba_(&agent_state_,
                                              /*default_value=*/{}),
      script_execution_disabled_(&agent_state_, /*default_value=*/false),
      scrollbars_hidden_(&agent_state_, /*default_value=*/false),
      document_cookie_disabled_(&agent_state_, /*default_value=*/false),
      touch_event_emulation_enabled_(&agent_state_, /*default_value=*/false),
      max_touch_points_(&agent_state_, /*default_value=*/1),
      emulated_media_(&agent_state_, /*default_value=*/WTF::String()),
      emulated_media_features_(&agent_state_, /*default_value=*/WTF::String()),
      emulated_vision_deficiency_(&agent_state_,
                                  /*default_value=*/WTF::String()),
      navigator_platform_override_(&agent_state_,
                                   /*default_value=*/WTF::String()),
      hardware_concurrency_override_(&agent_state_, /*default_value=*/0),
      user_agent_override_(&agent_state_, /*default_value=*/WTF::String()),
      serialized_ua_metadata_override_(
          &agent_state_,
          /*default_value=*/std::vector<uint8_t>()),
      accept_language_override_(&agent_state_,
                                /*default_value=*/WTF::String()),
      locale_override_(&agent_state_, /*default_value=*/WTF::String()),
      virtual_time_budget_(&agent_state_, /*default_value*/ 0.0),
      initial_virtual_time_(&agent_state_, /*default_value=*/0.0),
      virtual_time_policy_(&agent_state_, /*default_value=*/WTF::String()),
      virtual_time_task_starvation_count_(&agent_state_, /*default_value=*/0),
      emulate_focus_(&agent_state_, /*default_value=*/false),
      emulate_auto_dark_mode_(&agent_state_, /*default_value=*/false),
      auto_dark_mode_override_(&agent_state_, /*default_value=*/false),
      timezone_id_override_(&agent_state_, /*default_value=*/WTF::String()),
      disabled_image_types_(&agent_state_, /*default_value=*/false),
      cpu_throttling_rate_(&agent_state_, /*default_value=*/1),
      automation_override_(&agent_state_, /*default_value=*/false) {}

InspectorEmulationAgent::~InspectorEmulationAgent() = default;

WebViewImpl* InspectorEmulationAgent::GetWebViewImpl() {
  return web_local_frame_ ? web_local_frame_->ViewImpl() : nullptr;
}

void InspectorEmulationAgent::Restore() {
  // Since serialized_ua_metadata_override_ can't directly be converted back
  // to appropriate protocol message, we initially pass null and decode it
  // directly.
  std::vector<uint8_t> save_serialized_ua_metadata_override =
      serialized_ua_metadata_override_.Get();
  setUserAgentOverride(
      user_agent_override_.Get(), accept_language_override_.Get(),
      navigator_platform_override_.Get(),
      protocol::Maybe<protocol::Emulation::UserAgentMetadata>());
  ua_metadata_override_ = blink::UserAgentMetadata::Demarshal(std::string(
      reinterpret_cast<char*>(save_serialized_ua_metadata_override.data()),
      save_serialized_ua_metadata_override.size()));
  serialized_ua_metadata_override_.Set(save_serialized_ua_metadata_override);
  setCPUThrottlingRate(cpu_throttling_rate_.Get());

  if (int concurrency = hardware_concurrency_override_.Get())
    setHardwareConcurrencyOverride(concurrency);

  if (!locale_override_.Get().empty())
    setLocaleOverride(locale_override_.Get());
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
  if (!emulated_vision_deficiency_.Get().IsNull())
    setEmulatedVisionDeficiency(emulated_vision_deficiency_.Get());
  auto status_or_rgba = protocol::DOM::RGBA::ReadFrom(
      default_background_color_override_rgba_.Get());
  if (status_or_rgba.ok())
    setDefaultBackgroundColorOverride(std::move(status_or_rgba).value());
  setFocusEmulationEnabled(emulate_focus_.Get());
  if (emulate_auto_dark_mode_.Get())
    setAutoDarkModeOverride(auto_dark_mode_override_.Get());
  if (!timezone_id_override_.Get().IsNull())
    setTimezoneOverride(timezone_id_override_.Get());

  if (virtual_time_policy_.Get().IsNull())
    return;

  // Reinstate the stored policy.
  double virtual_time_ticks_base_ms;

  // For Pause, do not pass budget or starvation count.
  if (virtual_time_policy_.Get() ==
      protocol::Emulation::VirtualTimePolicyEnum::Pause) {
    setVirtualTimePolicy(
        protocol::Emulation::VirtualTimePolicyEnum::Pause, Maybe<double>(),
        Maybe<int>(), initial_virtual_time_.Get(), &virtual_time_ticks_base_ms);
    return;
  }

  // Calculate remaining budget for the advancing modes.
  double budget_remaining = virtual_time_budget_.Get();
  DCHECK_GE(budget_remaining, 0);

  setVirtualTimePolicy(virtual_time_policy_.Get(), budget_remaining,
                       virtual_time_task_starvation_count_.Get(),
                       initial_virtual_time_.Get(),
                       &virtual_time_ticks_base_ms);
}

protocol::Response InspectorEmulationAgent::disable() {
  if (enabled_) {
    instrumenting_agents_->RemoveInspectorEmulationAgent(this);
    enabled_ = false;
  }

  hardware_concurrency_override_.Clear();
  setUserAgentOverride(
      String(), protocol::Maybe<String>(), protocol::Maybe<String>(),
      protocol::Maybe<protocol::Emulation::UserAgentMetadata>());
  if (!locale_override_.Get().empty())
    setLocaleOverride(String());
  if (!web_local_frame_)
    return protocol::Response::Success();
  setScriptExecutionDisabled(false);
  setScrollbarsHidden(false);
  setDocumentCookieDisabled(false);
  setTouchEmulationEnabled(false, Maybe<int>());
  setAutomationOverride(false);
  // Clear emulated media features. Note that the current approach
  // doesn't work well in cases where two clients have the same set of
  // features overridden to the same value by two different clients
  // (e.g. if we allowed two different front-ends with the same
  // settings to attach to the same page). TODO: support this use case.
  setEmulatedMedia(
      String(),
      std::make_unique<protocol::Array<protocol::Emulation::MediaFeature>>());
  if (!emulated_vision_deficiency_.Get().IsNull())
    setEmulatedVisionDeficiency(String("none"));
  setCPUThrottlingRate(1);
  setFocusEmulationEnabled(false);
  if (emulate_auto_dark_mode_.Get()) {
    setAutoDarkModeOverride(Maybe<bool>());
  }
  timezone_override_.reset();
  setDefaultBackgroundColorOverride(Maybe<protocol::DOM::RGBA>());
  disabled_image_types_.Clear();
  return protocol::Response::Success();
}

protocol::Response InspectorEmulationAgent::resetPageScaleFactor() {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  GetWebViewImpl()->ResetScaleStateImmediately();
  return response;
}

protocol::Response InspectorEmulationAgent::setPageScaleFactor(
    double page_scale_factor) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  GetWebViewImpl()->SetPageScaleFactor(static_cast<float>(page_scale_factor));
  return response;
}

protocol::Response InspectorEmulationAgent::setScriptExecutionDisabled(
    bool value) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  if (script_execution_disabled_.Get() == value)
    return response;
  script_execution_disabled_.Set(value);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScriptExecutionDisabled(value);
  return response;
}

protocol::Response InspectorEmulationAgent::setScrollbarsHidden(bool hidden) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  if (scrollbars_hidden_.Get() == hidden)
    return response;
  scrollbars_hidden_.Set(hidden);
  GetWebViewImpl()->GetDevToolsEmulator()->SetScrollbarsHidden(hidden);
  return response;
}

protocol::Response InspectorEmulationAgent::setDocumentCookieDisabled(
    bool disabled) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  if (document_cookie_disabled_.Get() == disabled)
    return response;
  document_cookie_disabled_.Set(disabled);
  GetWebViewImpl()->GetDevToolsEmulator()->SetDocumentCookieDisabled(disabled);
  return response;
}

protocol::Response InspectorEmulationAgent::setTouchEmulationEnabled(
    bool enabled,
    protocol::Maybe<int> max_touch_points) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  int max_points = max_touch_points.value_or(1);
  if (max_points < 1 || max_points > WebTouchEvent::kTouchesLengthCap) {
    String msg =
        "Touch points must be between 1 and " +
        String::Number(static_cast<uint16_t>(WebTouchEvent::kTouchesLengthCap));
    return protocol::Response::InvalidParams(msg.Utf8());
  }
  touch_event_emulation_enabled_.Set(enabled);
  max_touch_points_.Set(max_points);
  GetWebViewImpl()->GetDevToolsEmulator()->SetTouchEventEmulationEnabled(
      enabled, max_points);
  return response;
}

protocol::Response InspectorEmulationAgent::setEmulatedMedia(
    Maybe<String> media,
    Maybe<protocol::Array<protocol::Emulation::MediaFeature>> features) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  String media_value = media.value_or("");
  emulated_media_.Set(media_value);
  GetWebViewImpl()->GetPage()->GetSettings().SetMediaTypeOverride(media_value);

  auto const old_emulated_media_features_keys = emulated_media_features_.Keys();
  emulated_media_features_.Clear();

  if (features.has_value()) {
    for (const auto& media_feature : features.value()) {
      String name = media_feature->getName();
      String value = media_feature->getValue();
      emulated_media_features_.Set(name, value);
    }

    auto const& forced_colors_value =
        emulated_media_features_.Get("forced-colors");
    auto const& prefers_color_scheme_value =
        emulated_media_features_.Get("prefers-color-scheme");

    if (forced_colors_value == "active") {
      if (!forced_colors_override_) {
        initial_system_forced_colors_state_ =
            GetWebViewImpl()->GetPage()->GetSettings().GetInForcedColors();
      }
      forced_colors_override_ = true;
      bool is_dark_mode = false;
      if (prefers_color_scheme_value.empty()) {
        is_dark_mode = GetWebViewImpl()
                           ->GetPage()
                           ->GetSettings()
                           .GetPreferredColorScheme() ==
                       mojom::blink::PreferredColorScheme::kDark;
      } else {
        is_dark_mode = prefers_color_scheme_value == "dark";
      }
      GetWebViewImpl()->GetPage()->EmulateForcedColors(is_dark_mode);
      GetWebViewImpl()->GetPage()->GetSettings().SetInForcedColors(true);
    } else if (forced_colors_value == "none") {
      if (!forced_colors_override_) {
        initial_system_forced_colors_state_ =
            GetWebViewImpl()->GetPage()->GetSettings().GetInForcedColors();
      }
      forced_colors_override_ = true;
      GetWebViewImpl()->GetPage()->DisableEmulatedForcedColors();
      GetWebViewImpl()->GetPage()->GetSettings().SetInForcedColors(false);
    } else if (forced_colors_override_) {
      GetWebViewImpl()->GetPage()->DisableEmulatedForcedColors();
      GetWebViewImpl()->GetPage()->GetSettings().SetInForcedColors(
          initial_system_forced_colors_state_);
    }

    for (const WTF::String& feature : emulated_media_features_.Keys()) {
      auto const& value = emulated_media_features_.Get(feature);
      GetWebViewImpl()->GetPage()->SetMediaFeatureOverride(
          AtomicString(feature), value);
    }

    if (forced_colors_override_) {
      blink::SystemColorsChanged();

      if (forced_colors_value != "none" && forced_colors_value != "active") {
        forced_colors_override_ = false;
      }
    }
  }

  for (const WTF::String& feature : old_emulated_media_features_keys) {
    auto const& value = emulated_media_features_.Get(feature);
    if (!value) {
      GetWebViewImpl()->GetPage()->SetMediaFeatureOverride(
          AtomicString(feature), "");
    }
  }

  return response;
}

protocol::Response InspectorEmulationAgent::setEmulatedVisionDeficiency(
    const String& type) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;

  VisionDeficiency vision_deficiency;
  namespace TypeEnum =
      protocol::Emulation::SetEmulatedVisionDeficiency::TypeEnum;
  if (type == TypeEnum::None)
    vision_deficiency = VisionDeficiency::kNoVisionDeficiency;
  else if (type == TypeEnum::BlurredVision)
    vision_deficiency = VisionDeficiency::kBlurredVision;
  else if (type == TypeEnum::ReducedContrast)
    vision_deficiency = VisionDeficiency::kReducedContrast;
  else if (type == TypeEnum::Achromatopsia)
    vision_deficiency = VisionDeficiency::kAchromatopsia;
  else if (type == TypeEnum::Deuteranopia)
    vision_deficiency = VisionDeficiency::kDeuteranopia;
  else if (type == TypeEnum::Protanopia)
    vision_deficiency = VisionDeficiency::kProtanopia;
  else if (type == TypeEnum::Tritanopia)
    vision_deficiency = VisionDeficiency::kTritanopia;
  else
    return protocol::Response::InvalidParams("Unknown vision deficiency type");

  emulated_vision_deficiency_.Set(type);
  GetWebViewImpl()->GetPage()->SetVisionDeficiency(vision_deficiency);
  return response;
}

protocol::Response InspectorEmulationAgent::setCPUThrottlingRate(double rate) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  cpu_throttling_rate_.Set(rate);
  scheduler::ThreadCPUThrottler::GetInstance()->SetThrottlingRate(rate);
  return response;
}

protocol::Response InspectorEmulationAgent::setFocusEmulationEnabled(
    bool enabled) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  if (enabled == emulate_focus_.Get()) {
    return response;
  }
  emulate_focus_.Set(enabled);
  GetWebViewImpl()->GetPage()->GetFocusController().SetFocusEmulationEnabled(
      enabled);
  return response;
}

protocol::Response InspectorEmulationAgent::setAutoDarkModeOverride(
    Maybe<bool> enabled) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  if (enabled.has_value()) {
    emulate_auto_dark_mode_.Set(true);
    auto_dark_mode_override_.Set(enabled.value());
    GetWebViewImpl()->GetDevToolsEmulator()->SetAutoDarkModeOverride(
        enabled.value());
  } else {
    emulate_auto_dark_mode_.Set(false);
    GetWebViewImpl()->GetDevToolsEmulator()->ResetAutoDarkModeOverride();
  }
  return response;
}

protocol::Response InspectorEmulationAgent::setVirtualTimePolicy(
    const String& policy,
    Maybe<double> virtual_time_budget_ms,
    protocol::Maybe<int> max_virtual_time_task_starvation_count,
    protocol::Maybe<double> initial_virtual_time,
    double* virtual_time_ticks_base_ms) {
  VirtualTimeController::VirtualTimePolicy scheduler_policy =
      VirtualTimeController::VirtualTimePolicy::kPause;
  if (protocol::Emulation::VirtualTimePolicyEnum::Advance == policy) {
    scheduler_policy = VirtualTimeController::VirtualTimePolicy::kAdvance;
  } else if (protocol::Emulation::VirtualTimePolicyEnum::
                 PauseIfNetworkFetchesPending == policy) {
    scheduler_policy =
        VirtualTimeController::VirtualTimePolicy::kDeterministicLoading;
  } else {
    DCHECK_EQ(scheduler_policy,
              VirtualTimeController::VirtualTimePolicy::kPause);
    if (virtual_time_budget_ms.has_value()) {
      return protocol::Response::InvalidParams(
          "Can only specify budget for non-Pause policy");
    }
    if (max_virtual_time_task_starvation_count.has_value()) {
      return protocol::Response::InvalidParams(
          "Can only specify starvation count for non-Pause policy");
    }
  }

  virtual_time_policy_.Set(policy);
  virtual_time_budget_.Set(virtual_time_budget_ms.value_or(0));
  initial_virtual_time_.Set(initial_virtual_time.value_or(0));
  virtual_time_task_starvation_count_.Set(
      max_virtual_time_task_starvation_count.value_or(0));

  InnerEnable();

  // This needs to happen before we apply virtual time.
  base::Time initial_time =
      initial_virtual_time.has_value()
          ? base::Time::FromSecondsSinceUnixEpoch(initial_virtual_time.value())
          : base::Time();
  virtual_time_base_ticks_ =
      virtual_time_controller_.EnableVirtualTime(initial_time);
  virtual_time_controller_.SetVirtualTimePolicy(scheduler_policy);
  if (virtual_time_budget_ms.value_or(0) > 0) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("renderer.scheduler", "VirtualTimeBudget",
                                      TRACE_ID_LOCAL(this), "budget",
                                      virtual_time_budget_ms.value());
    const base::TimeDelta budget_amount =
        base::Milliseconds(virtual_time_budget_ms.value());
    virtual_time_controller_.GrantVirtualTimeBudget(
        budget_amount,
        WTF::BindOnce(&InspectorEmulationAgent::VirtualTimeBudgetExpired,
                      WrapWeakPersistent(this)));
    for (DocumentLoader* loader : pending_document_loaders_)
      loader->SetDefersLoading(LoaderFreezeMode::kNone);
    pending_document_loaders_.clear();
  }

  if (max_virtual_time_task_starvation_count.value_or(0)) {
    virtual_time_controller_.SetMaxVirtualTimeTaskStarvationCount(
        max_virtual_time_task_starvation_count.value());
  }

  *virtual_time_ticks_base_ms =
      virtual_time_base_ticks_.is_null()
          ? 0
          : (virtual_time_base_ticks_ - base::TimeTicks()).InMillisecondsF();

  return protocol::Response::Success();
}

AtomicString InspectorEmulationAgent::OverrideAcceptImageHeader(
    const HashSet<String>* disabled_image_types) {
  String header(network_utils::ImageAcceptHeader());
  for (String type : *disabled_image_types) {
    // The header string is expected to be like
    // `image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8`
    // and is expected to be always ending with `image/*,*/*;q=xxx`, therefore,
    // to remove a type we replace `image/x,` with empty string. Only webp and
    // avif types can be disabled.
    header.Replace(String(type + ","), "");
  }
  return AtomicString(header);
}

void InspectorEmulationAgent::PrepareRequest(DocumentLoader* loader,
                                             ResourceRequest& request,
                                             ResourceLoaderOptions& options,
                                             ResourceType resource_type) {
  if (!accept_language_override_.Get().empty() &&
      request.HttpHeaderField(http_names::kAcceptLanguage).empty()) {
    request.SetHttpHeaderField(
        http_names::kAcceptLanguage,
        AtomicString(network_utils::GenerateAcceptLanguageHeader(
            accept_language_override_.Get())));
  }

  if (resource_type != ResourceType::kImage || disabled_image_types_.IsEmpty())
    return;

  if (!options.unsupported_image_mime_types) {
    options.unsupported_image_mime_types =
        base::MakeRefCounted<base::RefCountedData<HashSet<String>>>();
  }

  for (String type : disabled_image_types_.Keys()) {
    options.unsupported_image_mime_types->data.insert(type);
  }

  request.SetHTTPAccept(
      OverrideAcceptImageHeader(&options.unsupported_image_mime_types->data));
  // Bypassing caching to prevent the use of the previously loaded and cached
  // images.
  request.SetCacheMode(mojom::blink::FetchCacheMode::kBypassCache);
}

protocol::Response InspectorEmulationAgent::setNavigatorOverrides(
    const String& platform) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  navigator_platform_override_.Set(platform);
  GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
      platform);
  return response;
}

void InspectorEmulationAgent::VirtualTimeBudgetExpired() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("renderer.scheduler", "VirtualTimeBudget",
                                  TRACE_ID_LOCAL(this));
  // Disregard the event if the agent is disabled. Another agent may take care
  // of pausing the time in case of an in-process frame swap.
  if (!enabled_) {
    return;
  }
  virtual_time_controller_.SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kPause);
  virtual_time_policy_.Set(protocol::Emulation::VirtualTimePolicyEnum::Pause);
  // We could have been detached while VT was still running.
  // TODO(caseq): should we rather force-pause the time upon Disable()?
  if (auto* frontend = GetFrontend())
    frontend->virtualTimeBudgetExpired();
}

protocol::Response InspectorEmulationAgent::setDefaultBackgroundColorOverride(
    Maybe<protocol::DOM::RGBA> color) {
  protocol::Response response = AssertPage();
  if (!response.IsSuccess())
    return response;
  if (!color.has_value()) {
    // Clear the override and state.
    GetWebViewImpl()->SetBaseBackgroundColorOverrideForInspector(std::nullopt);
    default_background_color_override_rgba_.Clear();
    return protocol::Response::Success();
  }

  blink::protocol::DOM::RGBA* rgba = &color.value();
  default_background_color_override_rgba_.Set(rgba->Serialize());
  // Clamping of values is done by Color() constructor.
  int alpha = static_cast<int>(lroundf(255.0f * rgba->getA(1.0f)));
  GetWebViewImpl()->SetBaseBackgroundColorOverrideForInspector(
      Color(rgba->getR(), rgba->getG(), rgba->getB(), alpha).Rgb());
  return protocol::Response::Success();
}

protocol::Response InspectorEmulationAgent::setDeviceMetricsOverride(
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
    Maybe<protocol::Page::Viewport>,
    Maybe<protocol::Emulation::DisplayFeature>,
    Maybe<protocol::Emulation::DevicePosture>) {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been updated by the handling of
  // blink::mojom::FrameWidget::EnableDeviceEmulation.
  return AssertPage();
}

protocol::Response InspectorEmulationAgent::clearDeviceMetricsOverride() {
  // We don't have to do anything other than reply to the client, as the
  // emulation parameters should have already been cleared by the handling of
  // blink::mojom::FrameWidget::DisableDeviceEmulation.
  return AssertPage();
}

protocol::Response InspectorEmulationAgent::setHardwareConcurrencyOverride(
    int hardware_concurrency) {
  if (hardware_concurrency <= 0) {
    return protocol::Response::InvalidParams(
        "HardwareConcurrency must be a positive number");
  }
  InnerEnable();
  hardware_concurrency_override_.Set(hardware_concurrency);

  return protocol::Response::Success();
}

protocol::Response InspectorEmulationAgent::setUserAgentOverride(
    const String& user_agent,
    protocol::Maybe<String> accept_language,
    protocol::Maybe<String> platform,
    protocol::Maybe<protocol::Emulation::UserAgentMetadata>
        ua_metadata_override) {
  if (!user_agent.empty() || accept_language.has_value() ||
      platform.has_value()) {
    InnerEnable();
  }
  user_agent_override_.Set(user_agent);
  accept_language_override_.Set(accept_language.value_or(String()));
  navigator_platform_override_.Set(platform.value_or(String()));
  if (web_local_frame_) {
    GetWebViewImpl()->GetPage()->GetSettings().SetNavigatorPlatformOverride(
        navigator_platform_override_.Get());
  }

  if (ua_metadata_override.has_value()) {
    blink::UserAgentMetadata default_ua_metadata =
        Platform::Current()->UserAgentMetadata();

    if (user_agent.empty()) {
      ua_metadata_override_ = std::nullopt;
      serialized_ua_metadata_override_.Set(std::vector<uint8_t>());
      return protocol::Response::InvalidParams(
          "Can't specify UserAgentMetadata but no UA string");
    }
    protocol::Emulation::UserAgentMetadata& ua_metadata =
        ua_metadata_override.value();
    ua_metadata_override_.emplace();
    if (ua_metadata.hasBrands()) {
      for (const auto& bv : *ua_metadata.getBrands(nullptr)) {
        blink::UserAgentBrandVersion out_bv;
        out_bv.brand = bv->getBrand().Ascii();
        out_bv.version = bv->getVersion().Ascii();
        ua_metadata_override_->brand_version_list.push_back(std::move(out_bv));
      }
    } else {
      ua_metadata_override_->brand_version_list =
          std::move(default_ua_metadata.brand_version_list);
    }

    if (ua_metadata.hasFullVersionList()) {
      for (const auto& bv : *ua_metadata.getFullVersionList(nullptr)) {
        blink::UserAgentBrandVersion out_bv;
        out_bv.brand = bv->getBrand().Ascii();
        out_bv.version = bv->getVersion().Ascii();
        ua_metadata_override_->brand_full_version_list.push_back(
            std::move(out_bv));
      }
    } else {
      ua_metadata_override_->brand_full_version_list =
          std::move(default_ua_metadata.brand_full_version_list);
    }

    if (ua_metadata.hasFullVersion()) {
      ua_metadata_override_->full_version =
          ua_metadata.getFullVersion("").Ascii();
    } else {
      ua_metadata_override_->full_version =
          std::move(default_ua_metadata.full_version);
    }
    ua_metadata_override_->platform = ua_metadata.getPlatform().Ascii();
    ua_metadata_override_->platform_version =
        ua_metadata.getPlatformVersion().Ascii();
    ua_metadata_override_->architecture = ua_metadata.getArchitecture().Ascii();
    ua_metadata_override_->model = ua_metadata.getModel().Ascii();
    ua_metadata_override_->mobile = ua_metadata.getMobile();

    if (ua_metadata.hasBitness()) {
      ua_metadata_override_->bitness = ua_metadata.getBitness("").Ascii();
    } else {
      ua_metadata_override_->bitness = std::move(default_ua_metadata.bitness);
    }
    if (ua_metadata.hasWow64()) {
      ua_metadata_override_->wow64 = ua_metadata.getWow64(false);
    } else {
      ua_metadata_override_->wow64 = default_ua_metadata.wow64;
    }

  } else {
    ua_metadata_override_ = std::nullopt;
  }

  std::string marshalled =
      blink::UserAgentMetadata::Marshal(ua_metadata_override_)
          .value_or(std::string());
  std::vector<uint8_t> marshalled_as_bytes;
  marshalled_as_bytes.insert(marshalled_as_bytes.end(), marshalled.begin(),
                             marshalled.end());
  serialized_ua_metadata_override_.Set(std::move(marshalled_as_bytes));

  return protocol::Response::Success();
}

protocol::Response InspectorEmulationAgent::setLocaleOverride(
    protocol::Maybe<String> maybe_locale) {
  // Only allow resetting overrides set by the same agent.
  if (locale_override_.Get().empty() &&
      LocaleController::instance().has_locale_override()) {
    return protocol::Response::ServerError(
        "Another locale override is already in effect");
  }
  String locale = maybe_locale.value_or(String());
  String error = LocaleController::instance().SetLocaleOverride(locale);
  if (!error.empty())
    return protocol::Response::ServerError(error.Utf8());
  locale_override_.Set(locale);
  return protocol::Response::Success();
}

protocol::Response InspectorEmulationAgent::setTimezoneOverride(
    const String& timezone_id) {
  if (timezone_id == TimeZoneController::TimeZoneIdOverride()) {
    // Do nothing.
  } else if (timezone_id.empty()) {
    timezone_override_.reset();
  } else {
    if (timezone_override_) {
      timezone_override_->change(timezone_id);
    } else {
      timezone_override_ = TimeZoneController::SetTimeZoneOverride(timezone_id);
    }
    if (!timezone_override_) {
      return TimeZoneController::HasTimeZoneOverride()
                 ? protocol::Response::ServerError(
                       "Timezone override is already in effect")
                 : protocol::Response::InvalidParams("Invalid timezone id");
    }
  }

  timezone_id_override_.Set(timezone_id);

  return protocol::Response::Success();
}

void InspectorEmulationAgent::GetDisabledImageTypes(HashSet<String>* result) {
  if (disabled_image_types_.IsEmpty())
    return;

  for (String type : disabled_image_types_.Keys())
    result->insert(type);
}

void InspectorEmulationAgent::WillCommitLoad(LocalFrame*,
                                             DocumentLoader* loader) {
  if (virtual_time_policy_.Get() !=
      protocol::Emulation::VirtualTimePolicyEnum::Pause) {
    return;
  }
  loader->SetDefersLoading(LoaderFreezeMode::kStrict);
  pending_document_loaders_.push_back(loader);
}

void InspectorEmulationAgent::WillCreateDocumentParser(
    bool& force_sync_parsing) {
  if (virtual_time_policy_.Get().IsNull())
    return;
  force_sync_parsing = true;
}

void InspectorEmulationAgent::ApplyAcceptLanguageOverride(String* accept_lang) {
  if (!accept_language_override_.Get().empty())
    *accept_lang = accept_language_override_.Get();
}

void InspectorEmulationAgent::ApplyHardwareConcurrencyOverride(
    unsigned int& hardware_concurrency) {
  if (int concurrency = hardware_concurrency_override_.Get())
    hardware_concurrency = concurrency;
}

void InspectorEmulationAgent::ApplyUserAgentOverride(String* user_agent) {
  if (!user_agent_override_.Get().empty())
    *user_agent = user_agent_override_.Get();
}

void InspectorEmulationAgent::ApplyUserAgentMetadataOverride(
    std::optional<blink::UserAgentMetadata>* ua_metadata) {
  // This applies when UA override is set.
  if (!user_agent_override_.Get().empty()) {
    *ua_metadata = ua_metadata_override_;
  }
}

void InspectorEmulationAgent::InnerEnable() {
  if (enabled_)
    return;
  enabled_ = true;
  instrumenting_agents_->AddInspectorEmulationAgent(this);
}

void InspectorEmulationAgent::SetSystemThemeState() {}

protocol::Response InspectorEmulationAgent::AssertPage() {
  if (!web_local_frame_) {
    return protocol::Response::ServerError(
        "Operation is only supported for pages, not workers");
  }
  return protocol::Response::Success();
}

void InspectorEmulationAgent::Trace(Visitor* visitor) const {
  visitor->Trace(web_local_frame_);
  visitor->Trace(pending_document_loaders_);
  InspectorBaseAgent::Trace(visitor);
}

protocol::Response InspectorEmulationAgent::setDisabledImageTypes(
    std::unique_ptr<protocol::Array<protocol::Emulation::DisabledImageType>>
        disabled_types) {
  if (disabled_types->size() > 0 && !enabled_)
    InnerEnable();
  disabled_image_types_.Clear();
  String prefix = "image/";
  namespace DisabledImageTypeEnum = protocol::Emulation::DisabledImageTypeEnum;
  for (protocol::Emulation::DisabledImageType type : *disabled_types) {
    if (DisabledImageTypeEnum::Avif == type ||
        DisabledImageTypeEnum::Webp == type) {
      disabled_image_types_.Set(prefix + type, true);
      continue;
    }
    disabled_image_types_.Clear();
    return protocol::Response::InvalidParams("Invalid image type");
  }
  return protocol::Response::Success();
}

protocol::Response InspectorEmulationAgent::setAutomationOverride(
    bool enabled) {
  if (enabled)
    InnerEnable();
  automation_override_.Set(enabled);
  return protocol::Response::Success();
}

void InspectorEmulationAgent::ApplyAutomationOverride(bool& enabled) const {
  enabled |= automation_override_.Get();
}

}  // namespace blink
