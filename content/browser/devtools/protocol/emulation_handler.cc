// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/emulation_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_util.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/cpp/client_hints.h"
#include "ui/display/mojom/screen_orientation.mojom.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace content {
namespace protocol {

namespace {

constexpr char kCommandIsOnlyAvailableAtTopTarget[] =
    "Command can only be executed on top-level targets";

display::mojom::ScreenOrientation WebScreenOrientationTypeFromString(
    const std::string& type) {
  if (type == Emulation::ScreenOrientation::TypeEnum::PortraitPrimary)
    return display::mojom::ScreenOrientation::kPortraitPrimary;
  if (type == Emulation::ScreenOrientation::TypeEnum::PortraitSecondary)
    return display::mojom::ScreenOrientation::kPortraitSecondary;
  if (type == Emulation::ScreenOrientation::TypeEnum::LandscapePrimary)
    return display::mojom::ScreenOrientation::kLandscapePrimary;
  if (type == Emulation::ScreenOrientation::TypeEnum::LandscapeSecondary)
    return display::mojom::ScreenOrientation::kLandscapeSecondary;
  return display::mojom::ScreenOrientation::kUndefined;
}

absl::optional<content::DisplayFeature::Orientation>
DisplayFeatureOrientationTypeFromString(const std::string& type) {
  if (type == Emulation::DisplayFeature::OrientationEnum::Vertical)
    return content::DisplayFeature::Orientation::kVertical;
  if (type == Emulation::DisplayFeature::OrientationEnum::Horizontal)
    return content::DisplayFeature::Orientation::kHorizontal;
  return absl::nullopt;
}

ui::GestureProviderConfigType TouchEmulationConfigurationToType(
    const std::string& protocol_value) {
  ui::GestureProviderConfigType result =
      ui::GestureProviderConfigType::CURRENT_PLATFORM;
  if (protocol_value ==
      Emulation::SetEmitTouchEventsForMouse::ConfigurationEnum::Mobile) {
    result = ui::GestureProviderConfigType::GENERIC_MOBILE;
  }
  if (protocol_value ==
      Emulation::SetEmitTouchEventsForMouse::ConfigurationEnum::Desktop) {
    result = ui::GestureProviderConfigType::GENERIC_DESKTOP;
  }
  return result;
}

bool ValidateClientHintString(const std::string& s) {
  // Matches definition in structured headers:
  // https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-17#section-3.3.3
  for (char c : s) {
    if (!base::IsAsciiPrintable(c))
      return false;
  }
  return true;
}

}  // namespace

EmulationHandler::EmulationHandler()
    : DevToolsDomainHandler(Emulation::Metainfo::domainName),
      touch_emulation_enabled_(false),
      device_emulation_enabled_(false),
      focus_emulation_enabled_(false),
      host_(nullptr) {}

EmulationHandler::~EmulationHandler() = default;

// static
std::vector<EmulationHandler*> EmulationHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<EmulationHandler>(
      Emulation::Metainfo::domainName);
}

void EmulationHandler::SetRenderer(int process_host_id,
                                   RenderFrameHostImpl* frame_host) {
  if (host_ == frame_host)
    return;
  host_ = frame_host;
  if (touch_emulation_enabled_)
    UpdateTouchEventEmulationState();
  if (device_emulation_enabled_)
    UpdateDeviceEmulationState();
}

void EmulationHandler::Wire(UberDispatcher* dispatcher) {
  Emulation::Dispatcher::wire(dispatcher, this);
}

Response EmulationHandler::Disable() {
  if (touch_emulation_enabled_) {
    touch_emulation_enabled_ = false;
    UpdateTouchEventEmulationState();
  }
  user_agent_ = std::string();
  if (device_emulation_enabled_) {
    device_emulation_enabled_ = false;
    UpdateDeviceEmulationState();
  }
  if (focus_emulation_enabled_)
    SetFocusEmulationEnabled(false);
  prefers_color_scheme_ = "";
  prefers_reduced_motion_ = "";
  return Response::Success();
}

Response EmulationHandler::SetIdleOverride(bool is_user_active,
                                           bool is_screen_unlocked) {
  if (!host_)
    return Response::InternalError();
  host_->GetIdleManager()->SetIdleOverride(is_user_active, is_screen_unlocked);
  return Response::Success();
}

Response EmulationHandler::ClearIdleOverride() {
  if (!host_)
    return Response::InternalError();
  host_->GetIdleManager()->ClearIdleOverride();
  return Response::Success();
}

Response EmulationHandler::SetGeolocationOverride(Maybe<double> latitude,
                                                  Maybe<double> longitude,
                                                  Maybe<double> accuracy) {
  if (!host_)
    return Response::InternalError();

  auto* geolocation_context = GetWebContents()->GetGeolocationContext();
  device::mojom::GeopositionResultPtr override_result;
  if (latitude.has_value() && longitude.has_value() && accuracy.has_value()) {
    auto position = device::mojom::Geoposition::New();
    position->latitude = latitude.value();
    position->longitude = longitude.value();
    position->accuracy = accuracy.value();
    position->timestamp = base::Time::Now();
    if (!device::ValidateGeoposition(*position)) {
      return Response::ServerError("Invalid geolocation");
    }
    override_result =
        device::mojom::GeopositionResult::NewPosition(std::move(position));
  } else {
    override_result = device::mojom::GeopositionResult::NewError(
        device::mojom::GeopositionError::New(
            device::mojom::GeopositionErrorCode::kPositionUnavailable,
            /*error_message=*/"", /*error_technical=*/""));
  }
  geolocation_context->SetOverride(std::move(override_result));
  return Response::Success();
}

Response EmulationHandler::ClearGeolocationOverride() {
  if (!host_)
    return Response::InternalError();

  auto* geolocation_context = GetWebContents()->GetGeolocationContext();
  geolocation_context->ClearOverride();
  return Response::Success();
}

Response EmulationHandler::SetEmitTouchEventsForMouse(
    bool enabled,
    Maybe<std::string> configuration) {
  if (!host_)
    return Response::InternalError();

  if (host_->GetParentOrOuterDocument())
    return Response::ServerError(kCommandIsOnlyAvailableAtTopTarget);

  touch_emulation_enabled_ = enabled;
  touch_emulation_configuration_ = configuration.value_or("");
  UpdateTouchEventEmulationState();
  return Response::Success();
}

Response EmulationHandler::CanEmulate(bool* result) {
#if BUILDFLAG(IS_ANDROID)
  *result = false;
#else
  *result = true;
  if (host_) {
    if (GetWebContents()->GetVisibleURL().SchemeIs(kChromeDevToolsScheme) ||
        host_->GetRenderWidgetHost()->auto_resize_enabled())
      *result = false;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return Response::Success();
}

Response EmulationHandler::SetDeviceMetricsOverride(
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
    Maybe<Emulation::ScreenOrientation> screen_orientation,
    Maybe<protocol::Page::Viewport> viewport,
    Maybe<protocol::Emulation::DisplayFeature> displayFeature) {
  const static int max_size = 10000000;
  const static double max_scale = 10;
  const static int max_orientation_angle = 360;

  if (!host_)
    return Response::ServerError("Target does not support metrics override");

  if (host_->GetParentOrOuterDocument())
    return Response::ServerError(kCommandIsOnlyAvailableAtTopTarget);

  if (screen_width.value_or(0) < 0 || screen_height.value_or(0) < 0 ||
      screen_width.value_or(0) > max_size ||
      screen_height.value_or(0) > max_size) {
    return Response::InvalidParams(
        "Screen width and height values must be positive, not greater than " +
        base::NumberToString(max_size));
  }

  if (position_x.value_or(0) < 0 || position_y.value_or(0) < 0 ||
      position_x.value_or(0) > screen_width.value_or(0) ||
      position_y.value_or(0) > screen_height.value_or(0)) {
    return Response::InvalidParams("View position should be on the screen");
  }

  if (width < 0 || height < 0 || width > max_size || height > max_size) {
    return Response::InvalidParams(
        "Width and height values must be positive, not greater than " +
        base::NumberToString(max_size));
  }

  if (device_scale_factor < 0)
    return Response::InvalidParams("deviceScaleFactor must be non-negative");

  if (scale.value_or(1) <= 0 || scale.value_or(1) > max_scale) {
    return Response::InvalidParams("scale must be positive, not greater than " +
                                   base::NumberToString(max_scale));
  }

  display::mojom::ScreenOrientation orientationType =
      display::mojom::ScreenOrientation::kUndefined;
  int orientationAngle = 0;
  if (screen_orientation.has_value()) {
    Emulation::ScreenOrientation& orientation = screen_orientation.value();
    orientationType = WebScreenOrientationTypeFromString(orientation.GetType());
    if (orientationType == display::mojom::ScreenOrientation::kUndefined)
      return Response::InvalidParams("Invalid screen orientation type value");
    orientationAngle = orientation.GetAngle();
    if (orientationAngle < 0 || orientationAngle >= max_orientation_angle) {
      return Response::InvalidParams(
          "Screen orientation angle must be non-negative, less than " +
          base::NumberToString(max_orientation_angle));
    }
  }

  absl::optional<content::DisplayFeature> display_feature = absl::nullopt;
  if (displayFeature.has_value()) {
    protocol::Emulation::DisplayFeature& emu_display_feature =
        displayFeature.value();
    absl::optional<content::DisplayFeature::Orientation> disp_orientation =
        DisplayFeatureOrientationTypeFromString(
            emu_display_feature.GetOrientation());
    if (!disp_orientation) {
      return Response::InvalidParams(
          "Invalid display feature orientation type");
    }
    content::DisplayFeature::ParamErrorEnum error;
    display_feature = content::DisplayFeature::Create(
        *disp_orientation, emu_display_feature.GetOffset(),
        emu_display_feature.GetMaskLength(), width, height, &error);

    if (!display_feature) {
      switch (error) {
        case content::DisplayFeature::ParamErrorEnum::
            kDisplayFeatureWithZeroScreenSize:
          return Response::InvalidParams(
              "Cannot specify a display feature with zero width and height");
        case content::DisplayFeature::ParamErrorEnum::
            kNegativeDisplayFeatureParams:
          return Response::InvalidParams("Negative display feature parameters");
        case content::DisplayFeature::ParamErrorEnum::kOutsideScreenWidth:
          return Response::InvalidParams(
              "Display feature window segments outside screen width");
        case content::DisplayFeature::ParamErrorEnum::kOutsideScreenHeight:
          return Response::InvalidParams(
              "Display feature window segments outside screen height");
      }
    }
  }

  blink::DeviceEmulationParams params;
  params.screen_type = mobile ? blink::mojom::EmulatedScreenType::kMobile
                              : blink::mojom::EmulatedScreenType::kDesktop;
  params.screen_size =
      gfx::Size(screen_width.value_or(0), screen_height.value_or(0));
  if (position_x.has_value() && position_y.has_value()) {
    params.view_position =
        gfx::Point(position_x.value_or(0), position_y.value_or(0));
  }
  params.device_scale_factor = device_scale_factor;
  params.view_size = gfx::Size(width, height);
  params.scale = scale.value_or(1);
  params.screen_orientation_type = orientationType;
  params.screen_orientation_angle = orientationAngle;

  if (display_feature) {
    params.window_segments =
        display_feature->ComputeWindowSegments(params.view_size);
  }

  if (viewport.has_value()) {
    params.viewport_offset.SetPoint(viewport->GetX(), viewport->GetY());

    double dpfactor =
        device_scale_factor
            ? device_scale_factor /
                  host_->GetRenderWidgetHost()->GetDeviceScaleFactor()
            : 1;
    params.viewport_scale = viewport->GetScale() * dpfactor;

    // Resize the RenderWidgetHostView to the size of the overridden viewport.
    width = base::ClampRound(viewport->GetWidth() * params.viewport_scale);
    height = base::ClampRound(viewport->GetHeight() * params.viewport_scale);
  }

  bool size_changed = false;
  if (!dont_set_visible_size.value_or(false) && width > 0 && height > 0) {
    if (GetWebContents()) {
      size_changed =
          GetWebContents()->SetDeviceEmulationSize(gfx::Size(width, height));
    } else {
      return Response::ServerError("Can't find the associated web contents");
    }
  }

  if (device_emulation_enabled_ && params == device_emulation_params_) {
    // Renderer should answer after size was changed, so that the response is
    // only sent to the client once updates were applied.
    if (size_changed)
      return Response::FallThrough();
    return Response::Success();
  }

  device_emulation_enabled_ = true;
  device_emulation_params_ = params;
  UpdateDeviceEmulationState();

  // Renderer should answer after emulation params were updated, so that the
  // response is only sent to the client once updates were applied.
  // Unless the renderer has crashed.
  if (GetWebContents() && GetWebContents()->IsCrashed())
    return Response::Success();
  return Response::FallThrough();
}

Response EmulationHandler::ClearDeviceMetricsOverride() {
  if (!host_)
    return Response::ServerError("Can't find the associated web contents");
  if (host_->GetParentOrOuterDocument())
    return Response::ServerError(kCommandIsOnlyAvailableAtTopTarget);
  if (!device_emulation_enabled_)
    return Response::Success();

  GetWebContents()->ClearDeviceEmulationSize();
  device_emulation_enabled_ = false;
  device_emulation_params_ = blink::DeviceEmulationParams();
  UpdateDeviceEmulationState();
  // Renderer should answer after emulation was disabled, so that the response
  // is only sent to the client once updates were applied.
  // Unless the renderer has crashed.
  if (GetWebContents()->IsCrashed())
    return Response::Success();
  return Response::FallThrough();
}

Response EmulationHandler::SetVisibleSize(int width, int height) {
  if (width < 0 || height < 0)
    return Response::InvalidParams("Width and height must be non-negative");

  if (!host_)
    return Response::ServerError("Can't find the associated web contents");
  GetWebContents()->SetDeviceEmulationSize(gfx::Size(width, height));
  return Response::Success();
}

Response EmulationHandler::SetUserAgentOverride(
    const std::string& user_agent,
    Maybe<std::string> accept_language,
    Maybe<std::string> platform,
    Maybe<Emulation::UserAgentMetadata> ua_metadata_override) {
  if (!user_agent.empty() && !net::HttpUtil::IsValidHeaderValue(user_agent))
    return Response::InvalidParams("Invalid characters found in userAgent");
  std::string accept_lang = accept_language.value_or(std::string());
  if (!accept_lang.empty() && !net::HttpUtil::IsValidHeaderValue(accept_lang)) {
    return Response::InvalidParams(
        "Invalid characters found in acceptLanguage");
  }

  user_agent_ = user_agent;
  accept_language_ = accept_lang;

  user_agent_metadata_ = absl::nullopt;
  if (!ua_metadata_override.has_value()) {
    return Response::FallThrough();
  }

  if (user_agent.empty()) {
    return Response::InvalidParams(
        "Empty userAgent invalid with userAgentMetadata provided");
  }

  Emulation::UserAgentMetadata& ua_metadata = ua_metadata_override.value();
  blink::UserAgentMetadata new_ua_metadata;
  blink::UserAgentMetadata default_ua_metadata =
      GetContentClient()->browser()->GetUserAgentMetadata();

  if (ua_metadata.HasBrands()) {
    for (const auto& bv : *ua_metadata.GetBrands(nullptr)) {
      blink::UserAgentBrandVersion out_bv;
      if (!ValidateClientHintString(bv->GetBrand()))
        return Response::InvalidParams("Invalid brand string");
      out_bv.brand = bv->GetBrand();

      if (!ValidateClientHintString(bv->GetVersion()))
        return Response::InvalidParams("Invalid brand version string");
      out_bv.version = bv->GetVersion();

      new_ua_metadata.brand_version_list.push_back(std::move(out_bv));
    }
  } else {
    new_ua_metadata.brand_version_list =
        std::move(default_ua_metadata.brand_version_list);
  }

  if (ua_metadata.HasFullVersionList()) {
    for (const auto& bv : *ua_metadata.GetFullVersionList(nullptr)) {
      blink::UserAgentBrandVersion out_bv;
      if (!ValidateClientHintString(bv->GetBrand()))
        return Response::InvalidParams("Invalid brand string");
      out_bv.brand = bv->GetBrand();

      if (!ValidateClientHintString(bv->GetVersion()))
        return Response::InvalidParams("Invalid brand version string");
      out_bv.version = bv->GetVersion();

      new_ua_metadata.brand_full_version_list.push_back(std::move(out_bv));
    }
  } else {
    new_ua_metadata.brand_full_version_list =
        std::move(default_ua_metadata.brand_full_version_list);
  }

  if (ua_metadata.HasFullVersion()) {
    String full_version = ua_metadata.GetFullVersion("");
    if (!ValidateClientHintString(full_version))
      return Response::InvalidParams("Invalid full version string");
    new_ua_metadata.full_version = full_version;
  } else {
    new_ua_metadata.full_version = std::move(default_ua_metadata.full_version);
  }

  if (!ValidateClientHintString(ua_metadata.GetPlatform())) {
    return Response::InvalidParams("Invalid platform string");
  }
  new_ua_metadata.platform = ua_metadata.GetPlatform();

  if (!ValidateClientHintString(ua_metadata.GetPlatformVersion())) {
    return Response::InvalidParams("Invalid platform version string");
  }
  new_ua_metadata.platform_version = ua_metadata.GetPlatformVersion();

  if (!ValidateClientHintString(ua_metadata.GetArchitecture())) {
    return Response::InvalidParams("Invalid architecture string");
  }
  new_ua_metadata.architecture = ua_metadata.GetArchitecture();

  if (!ValidateClientHintString(ua_metadata.GetModel())) {
    return Response::InvalidParams("Invalid model string");
  }

  new_ua_metadata.model = ua_metadata.GetModel();
  new_ua_metadata.mobile = ua_metadata.GetMobile();

  if (ua_metadata.HasBitness()) {
    String bitness = ua_metadata.GetBitness("");
    if (!ValidateClientHintString(bitness))
      return Response::InvalidParams("Invalid bitness string");
    new_ua_metadata.bitness = std::move(bitness);
  } else {
    new_ua_metadata.bitness = std::move(default_ua_metadata.bitness);
  }
  if (ua_metadata.HasWow64()) {
    new_ua_metadata.wow64 = ua_metadata.GetWow64(false);
  } else {
    new_ua_metadata.wow64 = default_ua_metadata.wow64;
  }

  // All checks OK, can update user_agent_metadata_.
  user_agent_metadata_.emplace(std::move(new_ua_metadata));
  return Response::FallThrough();
}

Response EmulationHandler::SetFocusEmulationEnabled(bool enabled) {
  if (enabled == focus_emulation_enabled_)
    return Response::FallThrough();
  focus_emulation_enabled_ = enabled;
  if (enabled) {
    capture_handle_ =
        GetWebContents()->IncrementCapturerCount(gfx::Size(),
                                                 /*stay_hidden=*/false,
                                                 /*stay_awake=*/false);
  } else {
    capture_handle_.RunAndReset();
  }
  return Response::FallThrough();
}

Response EmulationHandler::SetEmulatedMedia(
    Maybe<std::string> media,
    Maybe<protocol::Array<protocol::Emulation::MediaFeature>> features) {
  if (!host_)
    return Response::InternalError();

  prefers_color_scheme_ = "";
  prefers_reduced_motion_ = "";
  if (features.has_value()) {
    for (auto const& mediaFeature : features.value()) {
      auto const& name = mediaFeature->GetName();
      auto const& value = mediaFeature->GetValue();
      if (name == "prefers-color-scheme") {
        prefers_color_scheme_ =
            (value == "light" || value == "dark") ? value : "";
      } else if (name == "prefers-reduced-motion") {
        prefers_reduced_motion_ = (value == "reduce") ? value : "";
      }
    }
  }

  return Response::FallThrough();
}

blink::DeviceEmulationParams EmulationHandler::GetDeviceEmulationParams() {
  return device_emulation_params_;
}

void EmulationHandler::SetDeviceEmulationParams(
    const blink::DeviceEmulationParams& params) {
  DCHECK(host_);
  // Device emulation only happens on the outermost main frame.
  DCHECK(!host_->GetParentOrOuterDocument());

  bool enabled = params != blink::DeviceEmulationParams();
  bool enable_changed = enabled != device_emulation_enabled_;
  bool params_changed = params != device_emulation_params_;
  if (!device_emulation_enabled_ && !enable_changed)
    return;  // Still disabled.
  if (!enable_changed && !params_changed)
    return;  // Nothing changed.
  device_emulation_enabled_ = enabled;
  device_emulation_params_ = params;
  UpdateDeviceEmulationState();
}

WebContentsImpl* EmulationHandler::GetWebContents() {
  DCHECK(host_);  // Only call if |host_| is set.
  return static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_));
}

void EmulationHandler::UpdateTouchEventEmulationState() {
  if (!host_)
    return;

  // We only have a single TouchEmulator for all frames, so let the main frame's
  // EmulationHandler enable/disable it.
  DCHECK(!host_->GetParentOrOuterDocument());

  if (touch_emulation_enabled_) {
    if (auto* touch_emulator =
            host_->GetRenderWidgetHost()->GetTouchEmulator()) {
      touch_emulator->Enable(
          TouchEmulator::Mode::kEmulatingTouchFromMouse,
          TouchEmulationConfigurationToType(touch_emulation_configuration_));
    }
  } else {
    if (auto* touch_emulator = host_->GetRenderWidgetHost()->GetTouchEmulator())
      touch_emulator->Disable();
  }
  GetWebContents()->SetForceDisableOverscrollContent(touch_emulation_enabled_);
}

void EmulationHandler::UpdateDeviceEmulationState() {
  if (!host_)
    return;

  // Device emulation only happens on the outermost main frame.
  DCHECK(!host_->GetParentOrOuterDocument());

  // TODO(eseckler): Once we change this to mojo, we should wait for an ack to
  // these messages from the renderer. The renderer should send the ack once the
  // emulation params were applied. That way, we can avoid having to handle
  // Set/ClearDeviceMetricsOverride in the renderer. With the old IPC system,
  // this is tricky since we'd have to track the DevTools message id with the
  // WidgetMsg and acknowledgment, as well as plump the acknowledgment back to
  // the EmulationHandler somehow. Mojo callbacks should make this much simpler.
  host_->ForEachRenderFrameHostIncludingSpeculative(
      [this](RenderFrameHostImpl* host) {
        // The main frame of nested subpages (ex. fenced frames, portals) inside
        // this page are updated as well.
        if (host->is_main_frame())
          UpdateDeviceEmulationStateForHost(host->GetRenderWidgetHost());
      });
}

void EmulationHandler::UpdateDeviceEmulationStateForHost(
    RenderWidgetHostImpl* render_widget_host) {
  auto& frame_widget = render_widget_host->GetAssociatedFrameWidget();
  if (!frame_widget)
    return;
  if (device_emulation_enabled_) {
    frame_widget->EnableDeviceEmulation(device_emulation_params_);
  } else {
    frame_widget->DisableDeviceEmulation();
  }
}

void EmulationHandler::ApplyOverrides(net::HttpRequestHeaders* headers,
                                      bool* user_agent_overridden,
                                      bool* accept_language_overridden) {
  if (!user_agent_.empty()) {
    headers->SetHeader(net::HttpRequestHeaders::kUserAgent, user_agent_);
  }
  *user_agent_overridden = !user_agent_.empty();
  if (!accept_language_.empty()) {
    headers->SetHeader(
        net::HttpRequestHeaders::kAcceptLanguage,
        net::HttpUtil::GenerateAcceptLanguageHeader(accept_language_));
  }
  *accept_language_overridden = !accept_language_.empty();
  if (!prefers_color_scheme_.empty()) {
    const auto& prefers_color_scheme_client_hint_name =
        network::GetClientHintToNameMap().at(
            network::mojom::WebClientHintsType::kPrefersColorScheme);
    if (headers->HasHeader(prefers_color_scheme_client_hint_name)) {
      headers->SetHeader(prefers_color_scheme_client_hint_name,
                         prefers_color_scheme_);
    }
  }
  if (!prefers_reduced_motion_.empty()) {
    const auto& prefers_reduced_motion_client_hint_name =
        network::GetClientHintToNameMap().at(
            network::mojom::WebClientHintsType::kPrefersReducedMotion);
    if (headers->HasHeader(prefers_reduced_motion_client_hint_name)) {
      headers->SetHeader(prefers_reduced_motion_client_hint_name,
                         prefers_reduced_motion_);
    }
  }
}

bool EmulationHandler::ApplyUserAgentMetadataOverrides(
    absl::optional<blink::UserAgentMetadata>* override_out) {
  // This is conditional on basic user agent override being on; this helps us
  // emulate a device not sending any UA client hints.
  if (user_agent_.empty())
    return false;
  *override_out = user_agent_metadata_;
  return true;
}

void EmulationHandler::ApplyNetworkOverridesForDownload(
    download::DownloadUrlParameters* parameters) {
  net::HttpRequestHeaders headers;
  bool user_agent_overridden;
  bool accept_language_overridden;
  ApplyOverrides(&headers, &user_agent_overridden, &accept_language_overridden);
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    parameters->add_request_header(it.name(), it.value());
  }
}

}  // namespace protocol
}  // namespace content
