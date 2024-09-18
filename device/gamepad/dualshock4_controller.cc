// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/dualshock4_controller.h"

#include <algorithm>
#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/metrics/crc32.h"
#include "base/numerics/safe_conversions.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/hid_writer.h"
#include "device/gamepad/public/cpp/gamepad_features.h"

namespace device {

namespace {
// Expected values for |version_number| when connected over USB and Bluetooth.
const uint16_t kDualshock4VersionUsb = 0x0100;
const uint16_t kDualshock4VersionBluetooth = 0;

// Report IDs.
constexpr uint8_t kReportId01 = 0x01;
constexpr uint8_t kReportId05 = 0x05;
constexpr uint8_t kReportId11 = 0x11;

// Maximum in-range values for HID report fields.
const uint8_t kRumbleMagnitudeMax = 0xff;
const float kAxisMax = 255.0f;
const float kDpadMax = 7.0f;

// Dualshock 4 touchpad absolute dimension.
constexpr uint16_t kTouchDimensionX = 1920;
constexpr uint16_t kTouchDimensionY = 942;

struct PACKED_OBJ ControllerData {
  uint8_t axis_left_x;
  uint8_t axis_left_y;
  uint8_t axis_right_x;
  uint8_t axis_right_y;
  uint8_t axis_dpad : 4;
  bool button_square : 1;
  bool button_cross : 1;
  bool button_circle : 1;
  bool button_triangle : 1;
  bool button_left_1 : 1;
  bool button_right_1 : 1;
  bool button_left_2 : 1;
  bool button_right_2 : 1;
  bool button_share : 1;
  bool button_options : 1;
  bool button_left_3 : 1;
  bool button_right_3 : 1;
  bool button_ps : 1;
  bool button_touch : 1;
  uint8_t sequence_number : 6;
  uint8_t axis_left_2;
  uint8_t axis_right_2;
  uint16_t timestamp;
  uint8_t sensor_temperature;
  uint16_t axis_gyro_pitch;
  uint16_t axis_gyro_yaw;
  uint16_t axis_gyro_roll;
  uint16_t axis_accelerometer_x;
  uint16_t axis_accelerometer_y;
  uint16_t axis_accelerometer_z;
  uint8_t padding1[5];
  uint8_t battery_info : 5;
  uint8_t padding2 : 2;
  bool extension_detection : 1;
};

static_assert(sizeof(ControllerData) == 30,
              "ControllerData has incorrect size");

struct TouchData {
  uint8_t id : 7;
  bool is_invalid : 1;
  uint8_t data[3];
};

static_assert(sizeof(TouchData) == 4, "TouchPadData has incorrect size");

struct TouchPadData {
  uint8_t touch_data_timestamp;
  TouchData touch[2];
};

static_assert(sizeof(TouchPadData) == 9, "TouchPadData has incorrect size");

struct Dualshock4InputReportUsb {
  ControllerData controller_data;
  uint8_t padding1[2];
  uint8_t touches_count;
  TouchPadData touches[3];
  uint8_t padding2[4];
};

static_assert(sizeof(Dualshock4InputReportUsb) == 64,
              "Dualshock4InputReportUsb has incorrect size");

struct PACKED_OBJ Dualshock4InputReportBluetooth {
  uint8_t padding1[2];
  ControllerData controller_data;
  uint8_t padding2[2];
  uint8_t touches_count;
  TouchPadData touches[4];
  uint8_t padding3[2];
  uint32_t crc32;
};

static_assert(sizeof(Dualshock4InputReportBluetooth) == 77,
              "Dualshock4InputReportBluetooth has incorrect size");

// Returns the CRC32 checksum for a Dualshock4 Bluetooth output report.
// |report_data| is the report data excluding the bytes where the checksum will
// be written.
uint32_t ComputeDualshock4Checksum(base::span<const uint8_t> report_data) {
  // The Bluetooth report checksum includes a constant header byte not contained
  // in the report data.
  constexpr uint8_t bt_header = 0xa2;
  uint32_t crc = base::Crc32(0xffffffff, base::span_from_ref(bt_header));
  // Extend the checksum with the contents of the report.
  return ~base::Crc32(crc, report_data);
}

// Scales |value| with range [0,255] to a float within [-1.0,+1.0].
static float NormalizeAxis(uint8_t value) {
  return (2.0f * value / kAxisMax) - 1.0f;
}

// Scales a D-pad value from a Dualshock4 report to an axis value. The D-pad
// report value has a logical range from 0 to 7, and uses an out-of-bounds value
// of 8 to indicate null input (no interaction). In-bounds values are scaled to
// the range [-1.0,+1.0]. The null input value returns +9/7 (about 1.29).
static float NormalizeDpad(uint8_t value) {
  return (2.0f * value / kDpadMax) - 1.0f;
}

// Scales the Dualshock4 touch absolute coordinates to a float in within the
// range [-1.0,+1.0].
float NormalizeTouch(uint32_t value, uint32_t min, uint32_t max) {
  DCHECK_LT(min, max);

  uint32_t clamped_value = std::min(max, std::max(min, value));
  return (2.0f * (clamped_value - min) / static_cast<float>(max - min)) - 1.0f;
}

// Reads the 12 bits coordinates given by `ds4_touch_data` into `touch`
// position.
void ReadTouchCoordinates(base::span<const uint8_t> ds4_touch_data_span,
                          GamepadTouch& touch) {
  uint16_t touch_data_x_axis =
      ((ds4_touch_data_span[1] & 0x0f) << 8) | ds4_touch_data_span[0];
  uint16_t touch_data_y_axis =
      (ds4_touch_data_span[2] << 4) | ((ds4_touch_data_span[1] & 0xf0) >> 4);

  touch.x = NormalizeTouch(touch_data_x_axis, 0, (kTouchDimensionX - 1));
  touch.y = NormalizeTouch(touch_data_y_axis, 0, (kTouchDimensionY - 1));
  touch.surface_width = kTouchDimensionX;
  touch.surface_height = kTouchDimensionY;
  touch.has_surface_dimensions = true;
}

// Reads the touchpad information given by `touchpad_data` and `touches_count`
// into `pad`.
// TODO(crbug.com/40155307): Make a member of Dualshock4Controller
template <typename Transform>
void ProcessTouchData(base::span<const TouchPadData> touchpad_data,
                      Transform& id_transform,
                      std::optional<uint32_t>& initial_touch_id,
                      Gamepad* pad) {
  pad->touch_events_length = 0;
  GamepadTouch* touches = pad->touch_events;

  for (const auto& touchpad_data_entry : touchpad_data) {
    auto [touch_id_0, touch_id_1] = id_transform(
        touchpad_data_entry.touch[0].id, touchpad_data_entry.touch[1].id);
    // 2 touches per touch pad data entry
    for (auto j = 0u; j < 2; ++j) {
      auto& raw_touch = touchpad_data_entry.touch[j];

      if (!raw_touch.is_invalid) {
        if (!initial_touch_id.has_value()) {
          initial_touch_id = j == 0 ? touch_id_0 : touch_id_1;
        }
        auto& touch = touches[pad->touch_events_length++];
        touch.touch_id =
            (j == 0 ? touch_id_0 : touch_id_1) - initial_touch_id.value();
        touch.surface_id = 0;
        // x and y coordinates stored in 3 bytes (12bits each)
        ReadTouchCoordinates(base::make_span(raw_touch.data, 3u), touch);
      }
    }
  }
}

// Reads the Axis and button information given by |controller_data| into
// |pad|.
void ProcessAxisButtonData(const ControllerData& controller_data,
                           Gamepad* pad) {
  // Button and axis indices must match the ordering expected by the
  // Dualshock4 mapping function.
  pad->axes[0] = NormalizeAxis(controller_data.axis_left_x);
  pad->axes[1] = NormalizeAxis(controller_data.axis_left_y);
  pad->axes[2] = NormalizeAxis(controller_data.axis_right_x);
  pad->axes[3] = NormalizeAxis(controller_data.axis_left_2);
  pad->axes[4] = NormalizeAxis(controller_data.axis_right_2);
  pad->axes[5] = NormalizeAxis(controller_data.axis_right_y);
  pad->axes[9] = NormalizeDpad(controller_data.axis_dpad);
  const bool button_values[] = {
      controller_data.button_square, controller_data.button_cross,
      controller_data.button_circle, controller_data.button_triangle,
      controller_data.button_left_1, controller_data.button_right_1,
      controller_data.button_left_2, controller_data.button_right_2,
      controller_data.button_share,  controller_data.button_options,
      controller_data.button_left_3, controller_data.button_right_3,
      controller_data.button_ps,     controller_data.button_touch,
  };
  for (size_t i = 0; i < std::size(button_values); ++i) {
    pad->buttons[i].pressed = button_values[i];
    pad->buttons[i].touched = button_values[i];
    pad->buttons[i].value = button_values[i] ? 1.0 : 0.0;
  }
}

}  // namespace

template <typename ExtendedType, typename BaseType>
ExtendedType
Dualshock4Controller::ExtendedCounter<ExtendedType, BaseType>::operator()(
    BaseType num,
    ExtendedCounter const* other) {
  if (other && prefix < other->prefix && last != num) {
    last = kLastMax;
    ++prefix;
  }

  auto pre = prefix;
  if (num == last && num == 127) {
    pre -= 1;
  } else if (num == 127) {
    ++prefix;
  }
  last = num;
  return (pre << 7) | num;
}

Dualshock4Controller::Dualshock4Controller(GamepadId gamepad_id,
                                           GamepadBusType bus_type,
                                           std::unique_ptr<HidWriter> writer)
    : gamepad_id_(gamepad_id),
      bus_type_(bus_type),
      writer_(std::move(writer)) {}

Dualshock4Controller::~Dualshock4Controller() = default;

// static
bool Dualshock4Controller::IsDualshock4(GamepadId gamepad_id) {
  return gamepad_id == GamepadId::kSonyProduct05c4 ||
         gamepad_id == GamepadId::kSonyProduct09cc ||
         gamepad_id == GamepadId::kScufProduct7725;
}

// static
GamepadBusType Dualshock4Controller::BusTypeFromVersionNumber(
    uint32_t version_number) {
  // RawInputDataFetcher on Windows does not distinguish devices by bus type.
  // Detect the transport in use by inspecting the version number reported by
  // the device.
  if (version_number == kDualshock4VersionUsb)
    return GAMEPAD_BUS_USB;
  if (version_number == kDualshock4VersionBluetooth)
    return GAMEPAD_BUS_BLUETOOTH;
  return GAMEPAD_BUS_UNKNOWN;
}

void Dualshock4Controller::DoShutdown() {
  writer_.reset();
}

bool Dualshock4Controller::ProcessInputReport(uint8_t report_id,
                                              base::span<const uint8_t> report,
                                              Gamepad* pad,
                                              bool ignore_button_axis,
                                              bool is_multitouch_enabled) {
  DCHECK(pad);

  const ControllerData* controller_data = nullptr;
  const TouchPadData* touches = nullptr;
  uint8_t touches_count = 0;

  auto set_controller_and_touch_data =
      [&controller_data, &touches, &touches_count,
       is_multitouch_enabled](const auto& data) {
        controller_data = &data->controller_data;
        if (is_multitouch_enabled) {
          touches = data->touches;
          touches_count = data->touches_count;
        }
      };

  if (bus_type_ == GAMEPAD_BUS_USB &&
      report_id == kReportId01 /*USB feature report*/ &&
      report.size_bytes() >= sizeof(Dualshock4InputReportUsb) &&
      is_multitouch_enabled) {
    const auto* data =
        reinterpret_cast<const Dualshock4InputReportUsb*>(report.data());
    set_controller_and_touch_data(data);
  } else if (report_id == kReportId11 /*Bluetooth feature report*/ &&
             report.size_bytes() >= sizeof(Dualshock4InputReportBluetooth)) {
    const auto* data =
        reinterpret_cast<const Dualshock4InputReportBluetooth*>(report.data());
    set_controller_and_touch_data(data);
  } else {
    return false;
  }

  if (!ignore_button_axis) {
    ProcessAxisButtonData(*controller_data, pad);
  }

  if (is_multitouch_enabled) {
    pad->supports_touch_events_ = true;
    ProcessTouchData(base::make_span(touches, touches_count),
                     transform_touch_id_, initial_touch_id_, pad);
  }

  pad->timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  return true;
}

void Dualshock4Controller::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  // Genuine DualShock 4 gamepads use an alternate output report when
  // connected over Bluetooth. Always send USB-mode reports to SCUF Vantage
  // gamepads.
  if (bus_type_ == GAMEPAD_BUS_BLUETOOTH &&
      gamepad_id_ != GamepadId::kScufProduct7725) {
    SetVibrationBluetooth(params->strong_magnitude, params->weak_magnitude);
    return;
  }

  SetVibrationUsb(params->strong_magnitude, params->weak_magnitude);
}

void Dualshock4Controller::SetVibrationUsb(double strong_magnitude,
                                           double weak_magnitude) {
  DCHECK(writer_);
  // Construct a USB output report with report ID 0x05. In USB mode, the 0x05
  // report is used to control vibration, LEDs, and audio volume.
  // https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c#L2105
  std::array<uint8_t, 32> control_report;
  control_report.fill(0);
  control_report[0] = kReportId05;
  control_report[1] = 0x01;  // motor only, don't update LEDs
  control_report[4] =
      base::ClampRound<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);
  control_report[5] =
      base::ClampRound<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);

  writer_->WriteOutputReport(control_report);
}

void Dualshock4Controller::SetVibrationBluetooth(double strong_magnitude,
                                                 double weak_magnitude) {
  DCHECK(writer_);
  // Construct a Bluetooth output report with report ID 0x11. In Bluetooth
  // mode, the 0x11 report is used to control vibration, LEDs, and audio
  // volume. https://www.psdevwiki.com/ps4/DS4-BT#0x11_2
  std::array<uint8_t, 78> control_report;
  control_report.fill(0);
  control_report[0] = kReportId11;
  control_report[1] = 0xc0;  // unknown
  control_report[2] = 0x20;  // unknown
  control_report[3] = 0xf1;  // motor only, don't update LEDs
  control_report[4] = 0x04;  // unknown
  control_report[6] =
      base::ClampRound<uint8_t>(weak_magnitude * kRumbleMagnitudeMax);
  control_report[7] =
      base::ClampRound<uint8_t>(strong_magnitude * kRumbleMagnitudeMax);
  control_report[21] = 0x43;  // volume left
  control_report[22] = 0x43;  // volume right
  control_report[24] = 0x4d;  // volume speaker
  control_report[25] = 0x85;  // unknown

  // The last four bytes of the report hold a CRC32 checksum. Compute the
  // checksum and store it in little-endian byte order.
  uint32_t crc = ComputeDualshock4Checksum(
      base::span(control_report).first(control_report.size() - 4));
  control_report[control_report.size() - 4] = crc & 0xff;
  control_report[control_report.size() - 3] = (crc >> 8) & 0xff;
  control_report[control_report.size() - 2] = (crc >> 16) & 0xff;
  control_report[control_report.size() - 1] = (crc >> 24) & 0xff;

  writer_->WriteOutputReport(control_report);
}

base::WeakPtr<AbstractHapticGamepad> Dualshock4Controller::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
