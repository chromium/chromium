// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/emulation_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/browser/device_posture/device_posture_provider_impl.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/generic_sensor/web_contents_sensor_provider_proxy.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/renderer_host/input/touch_emulator_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_util.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/geolocation/geoposition.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider.mojom.h"
#include "ui/display/mojom/screen_orientation.mojom.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace content {
namespace protocol {

namespace {

constexpr char kCommandIsOnlyAvailableAtTopTarget[] =
    "Command can only be executed on top-level targets";
#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
constexpr char kPressureSourceIsAlreadyOverridden[] =
    "The specified pressure source is already overridden";
constexpr char kPressureSourceIsNotOverridden[] =
    "The specified pressure source is not being overridden";
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
constexpr char kSensorIsAlreadyOverridden[] =
    "The specified sensor type is already overridden";
constexpr char kSensorIsNotOverridden[] =
    "This sensor type is not being overridden with a virtual sensor";

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

std::optional<content::DisplayFeature::Orientation>
DisplayFeatureOrientationTypeFromString(const std::string& type) {
  if (type == Emulation::DisplayFeature::OrientationEnum::Vertical)
    return content::DisplayFeature::Orientation::kVertical;
  if (type == Emulation::DisplayFeature::OrientationEnum::Horizontal)
    return content::DisplayFeature::Orientation::kHorizontal;
  return std::nullopt;
}

base::expected<blink::mojom::DevicePostureType, protocol::Response>
DevicePostureTypeFromString(const std::string& type) {
  if (type == Emulation::DevicePosture::TypeEnum::Continuous) {
    return blink::mojom::DevicePostureType::kContinuous;
  } else if (type == Emulation::DevicePosture::TypeEnum::Folded) {
    return blink::mojom::DevicePostureType::kFolded;
  } else {
    return base::unexpected(
        protocol::Response::InvalidParams("Invalid posture type"));
  }
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
  if (!frame_host) {
    sensor_overrides_.clear();
#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
    pressure_overrides_.clear();
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  }
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
  prefers_reduced_transparency_ = "";
  sensor_overrides_.clear();
#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  pressure_overrides_.clear();
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  ClearDevicePostureOverride();
  return Response::Success();
}

namespace {

Response ConvertSensorType(const Emulation::SensorType& type,
                           device::mojom::SensorType* out_type) {
  if (type == Emulation::SensorTypeEnum::AbsoluteOrientation) {
    *out_type = device::mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION;
  } else if (type == Emulation::SensorTypeEnum::Accelerometer) {
    *out_type = device::mojom::SensorType::ACCELEROMETER;
  } else if (type == Emulation::SensorTypeEnum::AmbientLight) {
    *out_type = device::mojom::SensorType::AMBIENT_LIGHT;
  } else if (type == Emulation::SensorTypeEnum::Gravity) {
    *out_type = device::mojom::SensorType::GRAVITY;
  } else if (type == Emulation::SensorTypeEnum::Gyroscope) {
    *out_type = device::mojom::SensorType::GYROSCOPE;
  } else if (type == Emulation::SensorTypeEnum::LinearAcceleration) {
    *out_type = device::mojom::SensorType::LINEAR_ACCELERATION;
  } else if (type == Emulation::SensorTypeEnum::Magnetometer) {
    *out_type = device::mojom::SensorType::MAGNETOMETER;
  } else if (type == Emulation::SensorTypeEnum::RelativeOrientation) {
    *out_type = device::mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION;
  } else {
    return Response::InvalidParams("Invalid sensor type: " + type);
  }

  return Response::Success();
}

Response ConvertSensorReading(device::mojom::SensorType type,
                              Emulation::SensorReading* const reading,
                              device::SensorReading* out_reading) {
  switch (type) {
    case device::mojom::SensorType::AMBIENT_LIGHT: {
      if (!reading->HasSingle()) {
        return Response::InvalidParams(
            "This sensor type requires a 'single' parameter");
      }
      auto* single_value = reading->GetSingle(nullptr);
      out_reading->als.value = single_value->GetValue();
      break;
    }
    case device::mojom::SensorType::ACCELEROMETER:
    case device::mojom::SensorType::GRAVITY:
    case device::mojom::SensorType::GYROSCOPE:
    case device::mojom::SensorType::LINEAR_ACCELERATION:
    case device::mojom::SensorType::MAGNETOMETER: {
      if (!reading->HasXyz()) {
        return Response::InvalidParams(
            "This sensor type requires an 'xyz' parameter");
      }
      auto* xyz = reading->GetXyz(nullptr);
      out_reading->accel.x = xyz->GetX();
      out_reading->accel.y = xyz->GetY();
      out_reading->accel.z = xyz->GetZ();
      break;
    }
    case device::mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case device::mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION: {
      if (!reading->HasQuaternion()) {
        return Response::InvalidParams(
            "This sensor type requires a 'quaternion' parameter");
      }
      auto* quaternion = reading->GetQuaternion(nullptr);
      out_reading->orientation_quat.x = quaternion->GetX();
      out_reading->orientation_quat.y = quaternion->GetY();
      out_reading->orientation_quat.z = quaternion->GetZ();
      out_reading->orientation_quat.w = quaternion->GetW();
      break;
    }
    case device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      return Response::InvalidParams("Unsupported sensor type");
  }
  out_reading->raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  return Response::Success();
}

base::expected<device::mojom::VirtualSensorMetadataPtr, Response>
ParseSensorMetadata(Maybe<Emulation::SensorMetadata>& metadata) {
  if (!metadata.has_value()) {
    return device::mojom::VirtualSensorMetadata::New();
  }

  if (metadata->HasMinimumFrequency() && metadata->HasMaximumFrequency() &&
      metadata->GetMinimumFrequency(0) > metadata->GetMaximumFrequency(0)) {
    return base::unexpected(
        Response::InvalidParams("The specified minimum frequency is higher "
                                "than the maximum frequency"));
  }

  auto virtual_sensor_metadata = device::mojom::VirtualSensorMetadata::New();
  if (metadata->HasAvailable()) {
    virtual_sensor_metadata->available = metadata->GetAvailable(true);
  }
  if (metadata->HasMinimumFrequency()) {
    virtual_sensor_metadata->minimum_frequency =
        metadata->GetMinimumFrequency(0);
  }
  if (metadata->HasMaximumFrequency()) {
    virtual_sensor_metadata->maximum_frequency =
        metadata->GetMaximumFrequency(0);
  }
  return virtual_sensor_metadata;
}

}  // namespace

void EmulationHandler::GetOverriddenSensorInformation(
    const Emulation::SensorType& type,
    std::unique_ptr<GetOverriddenSensorInformationCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  device::mojom::SensorType sensor_type;
  if (auto response = ConvertSensorType(type, &sensor_type);
      !response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  auto it = sensor_overrides_.find(sensor_type);
  if (it == sensor_overrides_.end()) {
    callback->sendFailure(Response::InvalidParams(kSensorIsNotOverridden));
    return;
  }

  it->second->GetVirtualSensorInformation(base::BindOnce(
      [](std::unique_ptr<GetOverriddenSensorInformationCallback> callback,
         device::mojom::GetVirtualSensorInformationResultPtr result) {
        if (result->is_error()) {
          switch (result->get_error()) {
            case device::mojom::GetVirtualSensorInformationError::
                kSensorTypeNotOverridden:
              callback->sendFailure(
                  Response::InvalidParams(kSensorIsNotOverridden));
              return;
          }
        }
        CHECK(result->is_info());
        callback->sendSuccess(result->get_info()->sampling_frequency);
      },
      std::move(callback)));
}

Response EmulationHandler::SetSensorOverrideEnabled(
    bool enabled,
    const Emulation::SensorType& type,
    Maybe<Emulation::SensorMetadata> metadata) {
  if (!host_) {
    return Response::InternalError();
  }

  device::mojom::SensorType sensor_type;
  if (auto response = ConvertSensorType(type, &sensor_type);
      !response.IsSuccess()) {
    return response;
  }

  if (enabled) {
    auto virtual_sensor_metadata = ParseSensorMetadata(metadata);
    if (!virtual_sensor_metadata.has_value()) {
      return virtual_sensor_metadata.error();
    }

    if (sensor_overrides_.contains(sensor_type)) {
      return Response::InvalidParams(kSensorIsAlreadyOverridden);
    }

    auto virtual_sensor =
        WebContentsSensorProviderProxy::GetOrCreate(GetWebContents())
            ->CreateVirtualSensorForDevTools(
                sensor_type, std::move(virtual_sensor_metadata.value()));
    if (!virtual_sensor) {
      return Response::InvalidParams(kSensorIsAlreadyOverridden);
    }
    sensor_overrides_[sensor_type] = std::move(virtual_sensor);
  } else {
    sensor_overrides_.erase(sensor_type);
  }
  return Response::Success();
}

void EmulationHandler::SetSensorOverrideReadings(
    const Emulation::SensorType& type,
    std::unique_ptr<Emulation::SensorReading> reading,
    std::unique_ptr<SetSensorOverrideReadingsCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  device::mojom::SensorType sensor_type;
  if (auto response = ConvertSensorType(type, &sensor_type);
      !response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  device::SensorReading device_reading;
  if (auto response =
          ConvertSensorReading(sensor_type, reading.get(), &device_reading);
      !response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  auto it = sensor_overrides_.find(sensor_type);
  if (it == sensor_overrides_.end()) {
    callback->sendFailure(Response::InvalidParams(kSensorIsNotOverridden));
    return;
  }

  it->second->UpdateVirtualSensor(
      device_reading,
      base::BindOnce(
          [](std::unique_ptr<SetSensorOverrideReadingsCallback> callback,
             device::mojom::UpdateVirtualSensorResult result) {
            switch (result) {
              case device::mojom::UpdateVirtualSensorResult::
                  kSensorTypeNotOverridden:
                callback->sendFailure(
                    Response::InvalidParams(kSensorIsNotOverridden));
                break;
              case device::mojom::UpdateVirtualSensorResult::kSuccess:
                callback->sendSuccess();
                break;
            }
          },
          std::move(callback)));
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
namespace {

device::mojom::VirtualPressureSourceMetadataPtr ConvertPressureMetadata(
    Maybe<Emulation::PressureMetadata>& metadata) {
  auto pressure_metadata = device::mojom::VirtualPressureSourceMetadata::New();
  if (metadata.has_value()) {
    pressure_metadata->available = metadata->GetAvailable(true);
  }
  return pressure_metadata;
}

Response ConvertPressureSource(const Emulation::PressureSource& source,
                               device::mojom::PressureSource* out_type) {
  if (source == Emulation::PressureSourceEnum::Cpu) {
    *out_type = device::mojom::PressureSource::kCpu;
  } else {
    return Response::InvalidParams("Invalid pressure source: " + source);
  }
  return Response::Success();
}

Response ConvertPressureState(const Emulation::PressureState& state,
                              device::mojom::PressureState* out_type) {
  if (state == Emulation::PressureStateEnum::Nominal) {
    *out_type = device::mojom::PressureState::kNominal;
  } else if (state == Emulation::PressureStateEnum::Fair) {
    *out_type = device::mojom::PressureState::kFair;
  } else if (state == Emulation::PressureStateEnum::Serious) {
    *out_type = device::mojom::PressureState::kSerious;
  } else if (state == Emulation::PressureStateEnum::Critical) {
    *out_type = device::mojom::PressureState::kCritical;
  } else {
    return Response::InvalidParams("Invalid pressure state: " + state);
  }
  return Response::Success();
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

Response EmulationHandler::SetPressureSourceOverrideEnabled(
    bool enabled,
    const Emulation::PressureSource& source,
    Maybe<Emulation::PressureMetadata> metadata) {
#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  if (!host_) {
    return Response::InternalError();
  }
  device::mojom::PressureSource mojo_source;
  if (auto response = ConvertPressureSource(source, &mojo_source);
      !response.IsSuccess()) {
    return response;
  }
  if (enabled) {
    if (pressure_overrides_.contains(mojo_source)) {
      return Response::InvalidParams(kPressureSourceIsAlreadyOverridden);
    }
    auto virtual_pressure_source =
        WebContentsPressureManagerProxy::GetOrCreate(GetWebContents())
            ->CreateVirtualPressureSourceForDevTools(
                mojo_source, ConvertPressureMetadata(metadata));
    if (!virtual_pressure_source) {
      return Response::InvalidParams(kPressureSourceIsAlreadyOverridden);
    }
    pressure_overrides_[mojo_source] = std::move(virtual_pressure_source);
  } else {
    pressure_overrides_.erase(mojo_source);
  }
  return Response::Success();
#else
  return Response::InternalError();
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
}

void EmulationHandler::SetPressureStateOverride(
    const Emulation::PressureSource& source,
    const Emulation::PressureState& state,
    std::unique_ptr<SetPressureStateOverrideCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  device::mojom::PressureSource mojo_source;
  if (auto response = ConvertPressureSource(source, &mojo_source);
      !response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  device::mojom::PressureState mojo_state;
  if (auto response = ConvertPressureState(state, &mojo_state);
      !response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }
  auto it = pressure_overrides_.find(mojo_source);
  if (it == pressure_overrides_.end()) {
    callback->sendFailure(
        Response::InvalidParams(kPressureSourceIsNotOverridden));
    return;
  }
  it->second->UpdateVirtualPressureSourceState(
      mojo_state, base::BindOnce(&SetPressureStateOverrideCallback::sendSuccess,
                                 std::move(callback)));
#else
  callback->sendFailure(Response::InternalError());
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
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
    Maybe<protocol::Emulation::DisplayFeature> display_feature,
    Maybe<protocol::Emulation::DevicePosture> device_posture) {
  const static int max_size = 10000000;
  const static double max_scale = 10;
  const static int max_orientation_angle = 360;

  if (!host_ || host_->GetRenderWidgetHost()->auto_resize_enabled()) {
    return Response::ServerError("Target does not support metrics override");
  }

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

  std::optional<content::DisplayFeature> content_display_feature = std::nullopt;
  if (display_feature.has_value()) {
    protocol::Emulation::DisplayFeature& emu_display_feature =
        display_feature.value();
    std::optional<content::DisplayFeature::Orientation> disp_orientation =
        DisplayFeatureOrientationTypeFromString(
            emu_display_feature.GetOrientation());
    if (!disp_orientation) {
      return Response::InvalidParams(
          "Invalid display feature orientation type");
    }
    content::DisplayFeature::ParamErrorEnum error;
    content_display_feature = content::DisplayFeature::Create(
        *disp_orientation, emu_display_feature.GetOffset(),
        emu_display_feature.GetMaskLength(), width, height, &error);

    if (!content_display_feature) {
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
              "Display feature viewport segments outside screen width");
        case content::DisplayFeature::ParamErrorEnum::kOutsideScreenHeight:
          return Response::InvalidParams(
              "Display feature viewport segments outside screen height");
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

  if (content_display_feature) {
    params.viewport_segments =
        content_display_feature->ComputeViewportSegments(params.view_size);
  }

  if (device_posture.has_value()) {
    params.device_posture =
        DevicePostureTypeFromString(device_posture.value().GetType()).value();
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

  user_agent_metadata_ = std::nullopt;
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
    capture_handle_ = GetWebContents()->IncrementCapturerCount(
        gfx::Size(),
        /*stay_hidden=*/false,
        /*stay_awake=*/false, /*is_activity=*/true);
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
  prefers_reduced_transparency_ = "";
  if (features.has_value()) {
    for (auto const& mediaFeature : features.value()) {
      auto const& name = mediaFeature->GetName();
      auto const& value = mediaFeature->GetValue();
      if (name == "prefers-color-scheme") {
        prefers_color_scheme_ = (value == network::kPrefersColorSchemeLight ||
                                 value == network::kPrefersColorSchemeDark)
                                    ? value
                                    : "";
      } else if (name == "prefers-reduced-motion") {
        prefers_reduced_motion_ =
            (value == network::kPrefersReducedMotionReduce) ? value : "";
      } else if (name == "prefers-reduced-transparency") {
        prefers_reduced_transparency_ =
            (value == network::kPrefersReducedTransparencyReduce) ? value : "";
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
    if (auto* touch_emulator = host_->GetRenderWidgetHost()->GetTouchEmulator(
            /*create_if_necessary=*/true)) {
      touch_emulator->Enable(
          input::TouchEmulator::Mode::kEmulatingTouchFromMouse,
          TouchEmulationConfigurationToType(touch_emulation_configuration_));
    }
  } else {
    if (auto* touch_emulator = host_->GetRenderWidgetHost()->GetTouchEmulator(
            /*create_if_necessary=*/true)) {
      touch_emulator->Disable();
    }
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
        // The main frame of nested subpages (ex. fenced frames) inside this
        // page are updated as well.
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

Response EmulationHandler::SetDevicePostureOverride(
    std::unique_ptr<protocol::Emulation::DevicePosture> posture) {
  ASSIGN_OR_RETURN(blink::mojom::DevicePostureType posture_type,
                   DevicePostureTypeFromString(posture->GetType()));
  device_posture_emulation_enabled_ = true;
  GetWebContents()
      ->GetDevicePostureProvider()
      ->OverrideDevicePostureForEmulation(posture_type);
  return Response::Success();
}

Response EmulationHandler::ClearDevicePostureOverride() {
  if (device_posture_emulation_enabled_) {
    device_posture_emulation_enabled_ = false;
    GetWebContents()
        ->GetDevicePostureProvider()
        ->DisableDevicePostureOverrideForEmulation();
  }
  return Response::Success();
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
  if (!prefers_reduced_transparency_.empty()) {
    const auto& prefers_reduced_transparency_client_hint_name =
        network::GetClientHintToNameMap().at(
            network::mojom::WebClientHintsType::kPrefersReducedTransparency);
    if (headers->HasHeader(prefers_reduced_transparency_client_hint_name)) {
      headers->SetHeader(prefers_reduced_transparency_client_hint_name,
                         prefers_reduced_transparency_);
    }
  }
}

bool EmulationHandler::ApplyUserAgentMetadataOverrides(
    std::optional<blink::UserAgentMetadata>* override_out) {
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
