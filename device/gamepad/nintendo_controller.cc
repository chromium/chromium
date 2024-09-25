// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/nintendo_controller.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_id_list.h"

namespace device {
namespace {
// Device IDs for the Switch Charging Grip, also used for composite devices.
const uint16_t kVendorNintendo = 0x057e;
const uint16_t kProductSwitchChargingGrip = 0x200e;

// Maximum output report sizes, used to distinguish USB and Bluetooth.
const size_t kSwitchProMaxOutputReportSizeBytesUsb = 63;
const size_t kSwitchProMaxOutputReportSizeBytesBluetooth = 48;

// Input report size.
const size_t kMaxInputReportSizeBytes = 64;

// Device name for a composite Joy-Con device.
const char kProductNameSwitchCompositeDevice[] = "Joy-Con L+R";

// Report IDs.
const uint8_t kReportIdOutput01 = 0x01;
const uint8_t kReportIdOutput10 = 0x10;
const uint8_t kReportIdInput21 = 0x21;
const uint8_t kReportIdInput30 = 0x30;
const uint8_t kUsbReportIdOutput80 = 0x80;
const uint8_t kUsbReportIdInput81 = 0x81;

// Sub-types of the 0x80 output report, used for initialization.
const uint8_t kSubTypeRequestMac = 0x01;
const uint8_t kSubTypeHandshake = 0x02;
const uint8_t kSubTypeBaudRate = 0x03;
const uint8_t kSubTypeDisableUsbTimeout = 0x04;
const uint8_t kSubTypeEnableUsbTimeout = 0x05;

// UART subcommands.
const uint8_t kSubCommandSetInputReportMode = 0x03;
const uint8_t kSubCommandReadSpi = 0x10;
const uint8_t kSubCommandSetPlayerLights = 0x30;
const uint8_t kSubCommand33 = 0x33;
const uint8_t kSubCommandEnableImu = 0x40;
const uint8_t kSubCommandSetImuSensitivity = 0x41;
const uint8_t kSubCommandEnableVibration = 0x48;

// SPI memory regions.
const uint16_t kSpiImuCalibrationAddress = 0x6020;
const size_t kSpiImuCalibrationSize = 24;
const uint16_t kSpiAnalogStickCalibrationAddress = 0x603d;
const size_t kSpiAnalogStickCalibrationSize = 18;
const uint16_t kSpiImuHorizontalOffsetsAddress = 0x6080;
const size_t kSpiImuHorizontalOffsetsSize = 6;
const uint16_t kSpiAnalogStickParametersAddress = 0x6086;
const size_t kSpiAnalogStickParametersSize = 18;

// Byte index for the first byte of subcommand data in 0x80 output reports.
const size_t kSubCommandDataOffset = 11;
// Byte index for the first byte of SPI data in SPI read responses.
const size_t kSpiDataOffset = 20;

// Values for the |device_type| field reported in the MAC reply.
const uint8_t kUsbDeviceTypeChargingGripNoDevice = 0x00;
const uint8_t kUsbDeviceTypeChargingGripJoyConL = 0x01;
const uint8_t kUsbDeviceTypeChargingGripJoyConR = 0x02;
const uint8_t kUsbDeviceTypeProController = 0x03;

// During initialization, the current initialization step will be retried if
// the client does not respond within |kTimeoutDuration|. After |kMaxRetryCount|
// retries, initialization is restarted from the first step.
//
// The timeout duration was chosen through experimentation. A shorter duration
// (~1 second) works for Pro controllers, but Joy-Cons sometimes fail to
// initialize correctly.
const base::TimeDelta kTimeoutDuration = base::Milliseconds(3000);
const size_t kMaxRetryCount = 3;

const size_t kMaxVibrationEffectDurationMillis = 100;

// Initialization parameters.
const uint8_t kGyroSensitivity2000Dps = 0x03;
const uint8_t kAccelerometerSensitivity8G = 0x00;
const uint8_t kGyroPerformance208Hz = 0x01;
const uint8_t kAccelerometerFilterBandwidth100Hz = 0x01;
const uint8_t kPlayerLightPattern1 = 0x01;

// Bogus calibration value that should be ignored.
const uint16_t kCalBogusValue = 0xfff;

// Default calibration values to use if the controller returns bogus values.
const uint16_t kCalDefaultDeadzone = 160;
const uint16_t kCalDefaultMin = 550;
const uint16_t kCalDefaultCenter = 2050;
const uint16_t kCalDefaultMax = 3550;

// Parameters for the "strong" and "weak" components of the dual-rumble effect.
const double kVibrationFrequencyStrongRumble = 141.0;
const double kVibrationFrequencyWeakRumble = 182.0;
const double kVibrationAmplitudeStrongRumbleMax = 0.9;
const double kVibrationAmplitudeWeakRumbleMax = 0.1;

const int kVibrationFrequencyHzMin = 41;
const int kVibrationFrequencyHzMax = 1253;
const int kVibrationAmplitudeMax = 1000;

// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md
struct VibrationFrequency {
  uint16_t hf;
  uint8_t lf;
  int freq_hz;  // rounded
} kVibrationFrequency[] = {
    // The linear resonant actuators (LRAs) on Switch devices are capable of
    // producing vibration effects at a wide range of frequencies, but the
    // Gamepad API assumes "dual-rumble" style vibration which is typically
    // implemented by a pair of eccentric rotating mass (ERM) actuators. To
    // simulate "dual-rumble" with Switch LRAs, the strong and weak vibration
    // magnitudes are translated into low and high frequency vibration effects.
    // Only the frequencies used for this translation are included; unused
    // frequencies have been removed.
    //
    // This list must be kept sorted.
    {0x0068, 0x3a, 141},
    {0x0098, 0x46, 182}};
const size_t kVibrationFrequencySize = std::size(kVibrationFrequency);

// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md
struct VibrationAmplitude {
  uint8_t hfa;
  uint16_t lfa;
  int amp;  // rounded, max 1000 (kVibrationAmplitudeMax)
} kVibrationAmplitude[]{
    // Only include safe amplitudes.
    {0x00, 0x0040, 0},   {0x02, 0x8040, 10},   {0x04, 0x0041, 12},
    {0x06, 0x8041, 14},  {0x08, 0x0042, 17},   {0x0a, 0x8042, 20},
    {0x0c, 0x0043, 24},  {0x0e, 0x8043, 28},   {0x10, 0x0044, 33},
    {0x12, 0x8044, 40},  {0x14, 0x0045, 47},   {0x16, 0x8045, 56},
    {0x18, 0x0046, 67},  {0x1a, 0x8046, 80},   {0x1c, 0x0047, 95},
    {0x1e, 0x8047, 112}, {0x20, 0x0048, 117},  {0x22, 0x8048, 123},
    {0x24, 0x0049, 128}, {0x26, 0x8049, 134},  {0x28, 0x004a, 140},
    {0x2a, 0x804a, 146}, {0x2c, 0x004b, 152},  {0x2e, 0x804b, 159},
    {0x30, 0x004c, 166}, {0x32, 0x804c, 173},  {0x34, 0x004d, 181},
    {0x36, 0x804d, 189}, {0x38, 0x004e, 198},  {0x3a, 0x804e, 206},
    {0x3c, 0x004f, 215}, {0x3e, 0x804f, 225},  {0x40, 0x0050, 230},
    {0x42, 0x8050, 235}, {0x44, 0x0051, 240},  {0x46, 0x8051, 245},
    {0x48, 0x0052, 251}, {0x4a, 0x8052, 256},  {0x4c, 0x0053, 262},
    {0x4e, 0x8053, 268}, {0x50, 0x0054, 273},  {0x52, 0x8054, 279},
    {0x54, 0x0055, 286}, {0x56, 0x8055, 292},  {0x58, 0x0056, 298},
    {0x5a, 0x8056, 305}, {0x5c, 0x0057, 311},  {0x5e, 0x8057, 318},
    {0x60, 0x0058, 325}, {0x62, 0x8058, 332},  {0x64, 0x0059, 340},
    {0x66, 0x8059, 347}, {0x68, 0x005a, 355},  {0x6a, 0x805a, 362},
    {0x6c, 0x005b, 370}, {0x6e, 0x805b, 378},  {0x70, 0x005c, 387},
    {0x72, 0x805c, 395}, {0x74, 0x005d, 404},  {0x76, 0x805d, 413},
    {0x78, 0x005e, 422}, {0x7a, 0x805e, 431},  {0x7c, 0x005f, 440},
    {0x7e, 0x805f, 450}, {0x80, 0x0060, 460},  {0x82, 0x8060, 470},
    {0x84, 0x0061, 480}, {0x86, 0x8061, 491},  {0x88, 0x0062, 501},
    {0x8a, 0x8062, 512}, {0x8c, 0x0063, 524},  {0x8e, 0x8063, 535},
    {0x90, 0x0064, 547}, {0x92, 0x8064, 559},  {0x94, 0x0065, 571},
    {0x96, 0x8065, 584}, {0x98, 0x0066, 596},  {0x9a, 0x8066, 609},
    {0x9c, 0x0067, 623}, {0x9e, 0x8067, 636},  {0xa0, 0x0068, 650},
    {0xa2, 0x8068, 665}, {0xa4, 0x0069, 679},  {0xa6, 0x8069, 694},
    {0xa8, 0x006a, 709}, {0xaa, 0x806a, 725},  {0xac, 0x006b, 741},
    {0xae, 0x806b, 757}, {0xb0, 0x006c, 773},  {0xb2, 0x806c, 790},
    {0xb4, 0x006d, 808}, {0xb6, 0x806d, 825},  {0xb8, 0x006e, 843},
    {0xba, 0x806e, 862}, {0xbc, 0x006f, 881},  {0xbe, 0x806f, 900},
    {0xc0, 0x0070, 920}, {0xc2, 0x8070, 940},  {0xc4, 0x0071, 960},
    {0xc6, 0x8071, 981}, {0xc8, 0x0072, 1000},
};
const size_t kVibrationAmplitudeSize = std::size(kVibrationAmplitude);

// Define indices for the additional buttons on Switch controllers.
enum SWITCH_BUTTON_INDICES {
  SWITCH_BUTTON_INDEX_CAPTURE = BUTTON_INDEX_META + 1,
  SWITCH_BUTTON_INDEX_LEFT_SL,
  SWITCH_BUTTON_INDEX_LEFT_SR,
  SWITCH_BUTTON_INDEX_RIGHT_SL,
  SWITCH_BUTTON_INDEX_RIGHT_SR,
  SWITCH_BUTTON_INDEX_COUNT
};

// Input reports with ID 0x81 are replies to commands sent during the
// initialization sequence.
#pragma pack(push, 1)
struct UsbInputReport81 {
  uint8_t subtype;
  uint8_t data[kMaxInputReportSizeBytes - 2];
};
#pragma pack(pop)
static_assert(sizeof(UsbInputReport81) == kMaxInputReportSizeBytes - 1,
              "UsbInputReport81 has incorrect size");

// When connected over USB, the initialization sequence includes a step to
// request the MAC address. The MAC is returned in an input report with ID 0x81
// and subtype 0x01.
#pragma pack(push, 1)
struct MacAddressReport {
  uint8_t subtype;  // 0x01
  uint8_t padding;
  uint8_t device_type;
  uint8_t mac_data[6];
  uint8_t padding2[kMaxInputReportSizeBytes - 10];
};
#pragma pack(pop)
static_assert(sizeof(MacAddressReport) == kMaxInputReportSizeBytes - 1,
              "MacAddressReport has incorrect size");

// When configured for standard full input report mode, controller data is
// reported at regular intervals. The data format is the same for all Switch
// devices, although some buttons are not present on all devices.
#pragma pack(push, 1)
struct ControllerData {
  uint8_t timestamp;
  uint8_t battery_level : 4;
  uint8_t connection_info : 4;
  bool button_y : 1;
  bool button_x : 1;
  bool button_b : 1;
  bool button_a : 1;
  bool button_right_sr : 1;
  bool button_right_sl : 1;
  bool button_r : 1;
  bool button_zr : 1;
  bool button_minus : 1;
  bool button_plus : 1;
  bool button_thumb_r : 1;
  bool button_thumb_l : 1;
  bool button_home : 1;
  bool button_capture : 1;
  uint8_t dummy : 1;
  bool charging_grip : 1;
  bool dpad_down : 1;
  bool dpad_up : 1;
  bool dpad_right : 1;
  bool dpad_left : 1;
  bool button_left_sr : 1;
  bool button_left_sl : 1;
  bool button_l : 1;
  bool button_zl : 1;
  uint8_t analog[6];
  uint8_t vibrator_input_report;
};
#pragma pack(pop)
static_assert(sizeof(ControllerData) == 12,
              "ControllerData has incorrect size");

// In standard full input report mode, controller data is reported with IMU data
// in reports with ID 0x30.
#pragma pack(push, 1)
struct ControllerDataReport {
  ControllerData controller_data;  // 12 bytes
  uint8_t imu_data[36];
  uint8_t padding[kMaxInputReportSizeBytes - 49];
};
#pragma pack(pop)
static_assert(sizeof(ControllerDataReport) == kMaxInputReportSizeBytes - 1,
              "ControllerDataReport has incorrect size");

// Responses to SPI read requests are sent in reports with ID 0x21. These
// reports also include controller data.
#pragma pack(push, 1)
struct SpiReadReport {
  ControllerData controller_data;  // 12 bytes
  uint8_t subcommand_ack;          // 0x90
  uint8_t subcommand;              // 0x10
  uint8_t addrl;
  uint8_t addrh;
  uint8_t padding[2];  // 0x00 0x00
  uint8_t length;
  uint8_t spi_data[kMaxInputReportSizeBytes - kSpiDataOffset];
};
#pragma pack(pop)
static_assert(sizeof(SpiReadReport) == kMaxInputReportSizeBytes - 1,
              "SpiReadReport has incorrect size");

// Unpack two packed 12-bit values.
void UnpackShorts(uint8_t byte0,
                  uint8_t byte1,
                  uint8_t byte2,
                  uint16_t* short1,
                  uint16_t* short2) {
  DCHECK(short1);
  DCHECK(short2);
  *short1 = ((byte1 << 8) & 0x0f00) | byte0;
  *short2 = (byte2 << 4) | (byte1 >> 4);
}

// Unpack a 6-byte MAC address.
uint64_t UnpackSwitchMacAddress(const uint8_t* data) {
  DCHECK(data);
  uint64_t acc = data[5];
  acc = (acc << 8) | data[4];
  acc = (acc << 8) | data[3];
  acc = (acc << 8) | data[2];
  acc = (acc << 8) | data[1];
  acc = (acc << 8) | data[0];
  return acc;
}

// Unpack the analog stick parameters into |cal|.
void UnpackSwitchAnalogStickParameters(
    const uint8_t* data,
    NintendoController::SwitchCalibrationData& cal) {
  DCHECK(data);
  // Only fetch the dead zone and range ratio. The other parameters are unknown.
  UnpackShorts(data[3], data[4], data[5], &cal.dead_zone, &cal.range_ratio);
  if (cal.dead_zone == kCalBogusValue) {
    // If the controller reports an invalid dead zone, default to something
    // reasonable.
    cal.dead_zone = kCalDefaultDeadzone;
  }
}

// Unpack the IMU calibration data into |cal|
void UnpackSwitchImuCalibration(
    const uint8_t* data,
    NintendoController::SwitchCalibrationData& cal) {
  DCHECK(data);
  // 24 bytes, as 4 groups of 3 16-bit little-endian values.
  cal.accelerometer_origin_x = (data[1] << 8) | data[0];
  cal.accelerometer_origin_y = (data[3] << 8) | data[2];
  cal.accelerometer_origin_z = (data[5] << 8) | data[4];
  cal.accelerometer_sensitivity_x = (data[7] << 8) | data[6];
  cal.accelerometer_sensitivity_y = (data[9] << 8) | data[8];
  cal.accelerometer_sensitivity_z = (data[11] << 8) | data[10];
  cal.gyro_origin_x = (data[13] << 8) | data[12];
  cal.gyro_origin_y = (data[15] << 8) | data[14];
  cal.gyro_origin_z = (data[17] << 8) | data[16];
  cal.gyro_sensitivity_x = (data[19] << 8) | data[18];
  cal.gyro_sensitivity_y = (data[21] << 8) | data[20];
  cal.gyro_sensitivity_z = (data[23] << 8) | data[22];
}

// Unpack the IMU horizontal offsets into |cal|.
void UnpackSwitchImuHorizontalOffsets(
    const uint8_t* data,
    NintendoController::SwitchCalibrationData& cal) {
  DCHECK(data);
  // 6 bytes, as 3 16-bit little-endian values.
  cal.horizontal_offset_x = (data[1] << 8) | data[0];
  cal.horizontal_offset_y = (data[3] << 8) | data[2];
  cal.horizontal_offset_z = (data[5] << 8) | data[4];
}

// Unpack the analog stick calibration data into |cal|.
void UnpackSwitchAnalogStickCalibration(
    const uint8_t* data,
    NintendoController::SwitchCalibrationData& cal) {
  DCHECK(data);
  // 18 bytes, as 2 groups of 6 packed 12-bit values.
  UnpackShorts(data[0], data[1], data[2], &cal.lx_max, &cal.ly_max);
  UnpackShorts(data[3], data[4], data[5], &cal.lx_center, &cal.ly_center);
  UnpackShorts(data[6], data[7], data[8], &cal.lx_min, &cal.ly_min);
  UnpackShorts(data[9], data[10], data[11], &cal.rx_center, &cal.ry_center);
  UnpackShorts(data[12], data[13], data[14], &cal.rx_min, &cal.ry_min);
  UnpackShorts(data[15], data[16], data[17], &cal.rx_max, &cal.ry_max);
  if (cal.lx_min == kCalBogusValue && cal.ly_max == kCalBogusValue) {
    // No valid data for the left stick, use reasonable defaults.
    cal.lx_min = kCalDefaultMin;
    cal.lx_center = kCalDefaultCenter;
    cal.lx_max = kCalDefaultMax;
    cal.ly_min = kCalDefaultMin;
    cal.ly_center = kCalDefaultCenter;
    cal.ly_max = kCalDefaultMax;
  } else {
    cal.lx_min = cal.lx_center - cal.lx_min;
    cal.lx_max = cal.lx_center + cal.lx_max;
    cal.ly_min = cal.ly_center - cal.ly_min;
    cal.ly_max = cal.ly_center + cal.ly_max;
  }

  if (cal.rx_min == kCalBogusValue && cal.ry_max == kCalBogusValue) {
    // No valid data for the right stick, use reasonable defaults.
    cal.rx_min = kCalDefaultMin;
    cal.rx_center = kCalDefaultCenter;
    cal.rx_max = kCalDefaultMax;
    cal.ry_min = kCalDefaultMin;
    cal.ry_center = kCalDefaultCenter;
    cal.ry_max = kCalDefaultMax;
  } else {
    cal.rx_min = cal.rx_center - cal.rx_min;
    cal.rx_max = cal.rx_center + cal.rx_max;
    cal.ry_min = cal.ry_center - cal.ry_min;
    cal.ry_max = cal.ry_center + cal.ry_max;
  }
}

// Unpack one frame of IMU data into |imu_data|.
void UnpackSwitchImuData(const uint8_t* data,
                         NintendoController::SwitchImuData* imu_data) {
  DCHECK(data);
  DCHECK(imu_data);
  // 12 bytes of IMU data containing 6 16-bit little-endian values.
  imu_data->accelerometer_x = (data[1] << 8) | data[0];
  imu_data->accelerometer_y = (data[3] << 8) | data[2];
  imu_data->accelerometer_z = (data[5] << 8) | data[4];
  imu_data->gyro_x = (data[7] << 8) | data[6];
  imu_data->gyro_y = (data[9] << 8) | data[8];
  imu_data->gyro_z = (data[11] << 8) | data[10];
}

// Given joystick input |x|,|y|, apply a radial deadzone with radius
// |dead_zone| centered at |x_center|,|y_center|. If the input is within the
// dead zone region, the value is snapped to the center of the dead zone.
bool ApplyDeadZone(uint16_t& x,
                   uint16_t& y,
                   uint16_t x_center,
                   uint16_t y_center,
                   uint16_t dead_zone) {
  int dx = x - x_center;
  int dy = y - y_center;
  if (dx * dx + dy * dy < dead_zone * dead_zone) {
    x = x_center;
    y = y_center;
    return true;
  }
  return false;
}

// Normalize |value| to the range [|min|,|max|]. If |value| is outside this
// range, clamp it.
double NormalizeAndClampAxis(int value, int min, int max) {
  if (value <= min)
    return -1.0;
  if (value >= max)
    return 1.0;
  return (2.0 * (value - min) / static_cast<double>(max - min)) - 1.0;
}

// Update the button and axis state in |pad| with the new controller data in
// |data|, using the calibration data |cal|. Returns true if the new data
// differs from the previous data.
bool UpdateGamepadFromControllerData(
    const ControllerData& data,
    const NintendoController::SwitchCalibrationData& cal,
    Gamepad& pad) {
  bool buttons_changed =
      pad.buttons_length != SWITCH_BUTTON_INDEX_COUNT ||
      pad.buttons[BUTTON_INDEX_PRIMARY].pressed != data.button_b ||
      pad.buttons[BUTTON_INDEX_SECONDARY].pressed != data.button_a ||
      pad.buttons[BUTTON_INDEX_TERTIARY].pressed != data.button_y ||
      pad.buttons[BUTTON_INDEX_QUATERNARY].pressed != data.button_x ||
      pad.buttons[BUTTON_INDEX_LEFT_SHOULDER].pressed != data.button_l ||
      pad.buttons[BUTTON_INDEX_RIGHT_SHOULDER].pressed != data.button_r ||
      pad.buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed != data.button_zl ||
      pad.buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed != data.button_zr ||
      pad.buttons[BUTTON_INDEX_BACK_SELECT].pressed != data.button_minus ||
      pad.buttons[BUTTON_INDEX_START].pressed != data.button_plus ||
      pad.buttons[BUTTON_INDEX_LEFT_THUMBSTICK].pressed !=
          data.button_thumb_l ||
      pad.buttons[BUTTON_INDEX_RIGHT_THUMBSTICK].pressed !=
          data.button_thumb_r ||
      pad.buttons[BUTTON_INDEX_DPAD_UP].pressed != data.dpad_up ||
      pad.buttons[BUTTON_INDEX_DPAD_DOWN].pressed != data.dpad_down ||
      pad.buttons[BUTTON_INDEX_DPAD_LEFT].pressed != data.dpad_left ||
      pad.buttons[BUTTON_INDEX_DPAD_RIGHT].pressed != data.dpad_right ||
      pad.buttons[BUTTON_INDEX_META].pressed != data.button_home ||
      pad.buttons[SWITCH_BUTTON_INDEX_CAPTURE].pressed != data.button_capture ||
      pad.buttons[SWITCH_BUTTON_INDEX_LEFT_SL].pressed != data.button_left_sl ||
      pad.buttons[SWITCH_BUTTON_INDEX_LEFT_SR].pressed != data.button_left_sr ||
      pad.buttons[SWITCH_BUTTON_INDEX_RIGHT_SL].pressed !=
          data.button_right_sl ||
      pad.buttons[SWITCH_BUTTON_INDEX_RIGHT_SR].pressed != data.button_right_sr;

  if (buttons_changed) {
    pad.buttons_length = SWITCH_BUTTON_INDEX_COUNT;
    pad.buttons[BUTTON_INDEX_PRIMARY].pressed = data.button_b;
    pad.buttons[BUTTON_INDEX_PRIMARY].value = data.button_b ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_SECONDARY].pressed = data.button_a;
    pad.buttons[BUTTON_INDEX_SECONDARY].value = data.button_a ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_TERTIARY].pressed = data.button_y;
    pad.buttons[BUTTON_INDEX_TERTIARY].value = data.button_y ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_QUATERNARY].pressed = data.button_x;
    pad.buttons[BUTTON_INDEX_QUATERNARY].value = data.button_x ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_LEFT_SHOULDER].pressed = data.button_l;
    pad.buttons[BUTTON_INDEX_LEFT_SHOULDER].value = data.button_l ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_RIGHT_SHOULDER].pressed = data.button_r;
    pad.buttons[BUTTON_INDEX_RIGHT_SHOULDER].value = data.button_r ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_LEFT_TRIGGER].pressed = data.button_zl;
    pad.buttons[BUTTON_INDEX_LEFT_TRIGGER].value = data.button_zl ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_RIGHT_TRIGGER].pressed = data.button_zr;
    pad.buttons[BUTTON_INDEX_RIGHT_TRIGGER].value = data.button_zr ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_BACK_SELECT].pressed = data.button_minus;
    pad.buttons[BUTTON_INDEX_BACK_SELECT].value = data.button_minus ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_START].pressed = data.button_plus;
    pad.buttons[BUTTON_INDEX_START].value = data.button_plus ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_LEFT_THUMBSTICK].pressed = data.button_thumb_l;
    pad.buttons[BUTTON_INDEX_LEFT_THUMBSTICK].value =
        data.button_thumb_l ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_RIGHT_THUMBSTICK].pressed = data.button_thumb_r;
    pad.buttons[BUTTON_INDEX_RIGHT_THUMBSTICK].value =
        data.button_thumb_r ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_DPAD_UP].pressed = data.dpad_up;
    pad.buttons[BUTTON_INDEX_DPAD_UP].value = data.dpad_up ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_DPAD_DOWN].pressed = data.dpad_down;
    pad.buttons[BUTTON_INDEX_DPAD_DOWN].value = data.dpad_down ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_DPAD_LEFT].pressed = data.dpad_left;
    pad.buttons[BUTTON_INDEX_DPAD_LEFT].value = data.dpad_left ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_DPAD_RIGHT].pressed = data.dpad_right;
    pad.buttons[BUTTON_INDEX_DPAD_RIGHT].value = data.dpad_right ? 1.0 : 0.0;
    pad.buttons[BUTTON_INDEX_META].pressed = data.button_home;
    pad.buttons[BUTTON_INDEX_META].value = data.button_home ? 1.0 : 0.0;
    pad.buttons[SWITCH_BUTTON_INDEX_CAPTURE].pressed = data.button_capture;
    pad.buttons[SWITCH_BUTTON_INDEX_CAPTURE].value =
        data.button_capture ? 1.0 : 0.0;
    pad.buttons[SWITCH_BUTTON_INDEX_LEFT_SL].pressed = data.button_left_sl;
    pad.buttons[SWITCH_BUTTON_INDEX_LEFT_SL].value =
        data.button_left_sl ? 1.0 : 0.0;
    pad.buttons[SWITCH_BUTTON_INDEX_LEFT_SR].pressed = data.button_left_sr;
    pad.buttons[SWITCH_BUTTON_INDEX_LEFT_SR].value =
        data.button_left_sr ? 1.0 : 0.0;
    pad.buttons[SWITCH_BUTTON_INDEX_RIGHT_SL].pressed = data.button_right_sl;
    pad.buttons[SWITCH_BUTTON_INDEX_RIGHT_SL].value =
        data.button_right_sl ? 1.0 : 0.0;
    pad.buttons[SWITCH_BUTTON_INDEX_RIGHT_SR].pressed = data.button_right_sr;
    pad.buttons[SWITCH_BUTTON_INDEX_RIGHT_SR].value =
        data.button_right_sr ? 1.0 : 0.0;
  }

  uint16_t axis_lx;
  uint16_t axis_ly;
  uint16_t axis_rx;
  uint16_t axis_ry;
  UnpackShorts(data.analog[0], data.analog[1], data.analog[2], &axis_lx,
               &axis_ly);
  UnpackShorts(data.analog[3], data.analog[4], data.analog[5], &axis_rx,
               &axis_ry);
  // Apply a radial dead zone to both sticks.
  bool ldead = ApplyDeadZone(axis_lx, axis_ly, cal.lx_center, cal.ly_center,
                             cal.dead_zone);
  bool rdead = ApplyDeadZone(axis_rx, axis_ry, cal.rx_center, cal.ry_center,
                             cal.dead_zone);
  // Normalize using calibration data.
  double lx =
      ldead ? 0.0 : NormalizeAndClampAxis(axis_lx, cal.lx_min, cal.lx_max);
  double ly =
      ldead ? 0.0 : -NormalizeAndClampAxis(axis_ly, cal.ly_min, cal.ly_max);
  double rx =
      rdead ? 0.0 : NormalizeAndClampAxis(axis_rx, cal.rx_min, cal.rx_max);
  double ry =
      rdead ? 0.0 : -NormalizeAndClampAxis(axis_ry, cal.ry_min, cal.ry_max);
  bool axes_changed = pad.axes_length != AXIS_INDEX_COUNT ||
                      pad.axes[device::AXIS_INDEX_LEFT_STICK_X] != lx ||
                      pad.axes[device::AXIS_INDEX_LEFT_STICK_Y] != ly ||
                      pad.axes[device::AXIS_INDEX_RIGHT_STICK_X] != rx ||
                      pad.axes[device::AXIS_INDEX_RIGHT_STICK_Y] != ry;
  if (axes_changed) {
    pad.axes_length = AXIS_INDEX_COUNT;
    pad.axes[device::AXIS_INDEX_LEFT_STICK_X] = lx;
    pad.axes[device::AXIS_INDEX_LEFT_STICK_Y] = ly;
    pad.axes[device::AXIS_INDEX_RIGHT_STICK_X] = rx;
    pad.axes[device::AXIS_INDEX_RIGHT_STICK_Y] = ry;
  }

  return buttons_changed || axes_changed;
}

// Update the state for a single button. The button state is taken from
// the button at index |button_index| in |src_pad|. If this is a composite
// device, |src_pad| holds the state for the left component. If |horizontal| is
// true, the button index is remapped for horizontal orientation before updating
// the state in |dst_pad|.
void UpdateButtonForLeftSide(const Gamepad& src_pad,
                             Gamepad& dst_pad,
                             size_t button_index,
                             bool horizontal) {
  size_t remapped_index = button_index;
  // The internal button mapping assumes a docked orientation for Joy-Cons. If
  // a Joy-Con is used by itself, remap the buttons so they match the Standard
  // Gamepad spec when held horizontally.
  if (horizontal) {
    switch (button_index) {
      // Map the D-pad buttons to action buttons.
      case BUTTON_INDEX_DPAD_LEFT:
        remapped_index = BUTTON_INDEX_PRIMARY;
        break;
      case BUTTON_INDEX_DPAD_DOWN:
        remapped_index = BUTTON_INDEX_SECONDARY;
        break;
      case BUTTON_INDEX_DPAD_UP:
        remapped_index = BUTTON_INDEX_TERTIARY;
        break;
      case BUTTON_INDEX_DPAD_RIGHT:
        remapped_index = BUTTON_INDEX_QUATERNARY;
        break;
      // Map L to Select.
      case BUTTON_INDEX_LEFT_SHOULDER:
        remapped_index = BUTTON_INDEX_BACK_SELECT;
        break;
      // Map Minus to Start.
      case BUTTON_INDEX_BACK_SELECT:
        remapped_index = BUTTON_INDEX_START;
        break;
      // Map Capture to Meta.
      case SWITCH_BUTTON_INDEX_CAPTURE:
        remapped_index = BUTTON_INDEX_META;
        break;
      // Map SL and SR to the left and right shoulders.
      case SWITCH_BUTTON_INDEX_LEFT_SL:
        remapped_index = BUTTON_INDEX_LEFT_SHOULDER;
        break;
      case SWITCH_BUTTON_INDEX_LEFT_SR:
        remapped_index = BUTTON_INDEX_RIGHT_SHOULDER;
        break;
      // ZL and the left thumbstick are unmodified.
      case BUTTON_INDEX_LEFT_TRIGGER:
      case BUTTON_INDEX_LEFT_THUMBSTICK:
        break;
      default:
        NOTREACHED();
    }
  }
  dst_pad.buttons[remapped_index] = src_pad.buttons[button_index];
}

// Update the state for a single button. The button state is taken from
// the button at index |button_index| in |src_pad|. If this is a composite
// device, |src_pad| holds the state for the right component. If |horizontal| is
// true, the button index is remapped for horizontal orientation before updating
// the state in |dst_pad|.
void UpdateButtonForRightSide(const Gamepad& src_pad,
                              Gamepad& dst_pad,
                              size_t button_index,
                              bool horizontal) {
  size_t remapped_index = button_index;
  // The internal button mapping assumes a docked orientation for Joy-Cons. If
  // a Joy-Con is used by itself, remap the buttons so they match the Standard
  // Gamepad spec when held horizontally.
  if (horizontal) {
    switch (button_index) {
      // Re-map the action buttons to rotate them.
      case BUTTON_INDEX_PRIMARY:
        remapped_index = BUTTON_INDEX_TERTIARY;
        break;
      case BUTTON_INDEX_TERTIARY:
        remapped_index = BUTTON_INDEX_QUATERNARY;
        break;
      case BUTTON_INDEX_QUATERNARY:
        remapped_index = BUTTON_INDEX_SECONDARY;
        break;
      case BUTTON_INDEX_SECONDARY:
        remapped_index = BUTTON_INDEX_PRIMARY;
        break;
      // Map R to Select.
      case BUTTON_INDEX_RIGHT_SHOULDER:
        remapped_index = BUTTON_INDEX_BACK_SELECT;
        break;
      // Map SL and SR to the left and right shoulders.
      case SWITCH_BUTTON_INDEX_RIGHT_SL:
        remapped_index = BUTTON_INDEX_LEFT_SHOULDER;
        break;
      case SWITCH_BUTTON_INDEX_RIGHT_SR:
        remapped_index = BUTTON_INDEX_RIGHT_SHOULDER;
        break;
      // Map right thumbstick button to left thumbstick button.
      case BUTTON_INDEX_RIGHT_THUMBSTICK:
        remapped_index = BUTTON_INDEX_LEFT_THUMBSTICK;
        break;
      // The Plus, Home, and ZR buttons are unmodified.
      case BUTTON_INDEX_START:
      case BUTTON_INDEX_META:
      case BUTTON_INDEX_RIGHT_TRIGGER:
        break;
      default:
        NOTREACHED();
    }
  }
  dst_pad.buttons[remapped_index] = src_pad.buttons[button_index];
}

// Update the state for a single axis. The axis state is taken from the axis at
// index |axis_index| in |src_pad|. If this is a composite device, |src_pad|
// holds the state for the left component. If |horizontal| is true, the axis
// index and value are remapped for horizontal orientation before updating the
// state in |dst_pad|.
void UpdateAxisForLeftSide(const Gamepad& src_pad,
                           Gamepad& dst_pad,
                           size_t axis_index,
                           bool horizontal) {
  size_t remapped_index = axis_index;
  double axis_value = src_pad.axes[axis_index];
  // The internal axis values assume a docked orientation for Joy-Cons. If a
  // Joy-Con is used by itself, remap the axis indices and adjust the sign on
  // the axis value for a horizontal orientation.
  if (horizontal) {
    switch (axis_index) {
      case AXIS_INDEX_LEFT_STICK_X:
        // Map +X to -Y.
        axis_value = -axis_value;
        remapped_index = AXIS_INDEX_LEFT_STICK_Y;
        break;
      case AXIS_INDEX_LEFT_STICK_Y:
        // Map +Y to +X.
        remapped_index = AXIS_INDEX_LEFT_STICK_X;
        break;
      default:
        NOTREACHED();
    }
  }
  dst_pad.axes[remapped_index] = axis_value;
}

// Update the state for a single axis. The axis state is taken from the axis at
// index |axis_index| in |src_pad|. If this is a composite device, |src_pad|
// holds the state for the right component. If |horizontal| is true, the axis
// index and value are remapped for horizontal orientation before updating the
// state in |dst_pad|.
void UpdateAxisForRightSide(const Gamepad& src_pad,
                            Gamepad& dst_pad,
                            size_t axis_index,
                            bool horizontal) {
  size_t remapped_index = axis_index;
  double axis_value = src_pad.axes[axis_index];
  // The internal axis values assume a docked orientation for Joy-Cons. If a
  // Joy-Con is used by itself, remap the axis indices and adjust the sign on
  // the axis value for a horizontal orientation.
  if (horizontal) {
    switch (axis_index) {
      case AXIS_INDEX_RIGHT_STICK_X:
        // Map +X to +Y.
        remapped_index = AXIS_INDEX_LEFT_STICK_Y;
        break;
      case AXIS_INDEX_RIGHT_STICK_Y:
        // Map +Y to -X.
        axis_value = -axis_value;
        remapped_index = AXIS_INDEX_LEFT_STICK_X;
        break;
      default:
        NOTREACHED();
    }
  }
  dst_pad.axes[remapped_index] = axis_value;
}

// Convert the vibration parameters |frequency| and |amplitude| into a set of
// parameters that can be sent to the vibration actuator.
void FrequencyToHex(float frequency,
                    float amplitude,
                    uint16_t* hf,
                    uint8_t* lf,
                    uint8_t* hf_amp,
                    uint16_t* lf_amp) {
  int freq = static_cast<int>(frequency);
  int amp = static_cast<int>(amplitude * kVibrationAmplitudeMax);
  // Clamp the target frequency and amplitude to a safe range.
  freq = std::clamp(freq, kVibrationFrequencyHzMin, kVibrationFrequencyHzMax);
  amp = std::clamp(amp, 0, kVibrationAmplitudeMax);
  const auto* best_vf = &kVibrationFrequency[0];
  for (size_t i = 1; i < kVibrationFrequencySize; ++i) {
    const auto* vf = &kVibrationFrequency[i];
    if (vf->freq_hz < freq) {
      best_vf = vf;
    } else {
      // The candidate frequency is higher than the target frequency. Check if
      // it is closer than the current best.
      int vf_error_above = vf->freq_hz - freq;
      int best_vf_error_below = freq - best_vf->freq_hz;
      if (vf_error_above < best_vf_error_below)
        best_vf = vf;
      break;
    }
  }
  const auto* best_va = &kVibrationAmplitude[0];
  for (size_t i = 0; i < kVibrationAmplitudeSize; ++i) {
    const auto* va = &kVibrationAmplitude[i];
    if (va->amp < amp) {
      best_va = va;
    } else {
      // The candidate amplitude is higher than the target amplitude. Check if
      // it is closer than the current best.
      int va_error_above = va->amp - amp;
      int best_va_error_below = amp - best_va->amp;
      if (va_error_above < best_va_error_below)
        best_va = va;
      break;
    }
  }
  DCHECK(best_vf);
  DCHECK(best_va);
  *hf = best_vf->hf;
  *lf = best_vf->lf;
  *hf_amp = best_va->hfa;
  *lf_amp = best_va->lfa;
}

// Return the bus type of the Switch device described by |device_info|. This is
// needed for Windows which does not report the bus type in the HID API.
GamepadBusType BusTypeFromDeviceInfo(const mojom::HidDeviceInfo* device_info) {
  DCHECK(device_info);
  // If the |device_info| indicates the device is connected over Bluetooth, it's
  // probably right. On some platforms the bus type is reported as USB
  // regardless of the actual connection.
  if (device_info->bus_type == mojom::HidBusType::kHIDBusTypeBluetooth)
    return GAMEPAD_BUS_BLUETOOTH;
  auto gamepad_id = GamepadIdList::Get().GetGamepadId(device_info->product_name,
                                                      device_info->vendor_id,
                                                      device_info->product_id);
  switch (gamepad_id) {
    case GamepadId::kNintendoProduct2009:
      // The Switch Pro Controller may be connected over USB or Bluetooth.
      // Determine which connection is in use by comparing the max output report
      // size against known values.
      switch (device_info->max_output_report_size) {
        case kSwitchProMaxOutputReportSizeBytesUsb:
          return GAMEPAD_BUS_USB;
        case kSwitchProMaxOutputReportSizeBytesBluetooth:
          return GAMEPAD_BUS_BLUETOOTH;
        default:
          break;
      }
      break;
    case GamepadId::kNintendoProduct200e:
      // The Charging Grip can only be connected over USB.
      return GAMEPAD_BUS_USB;
    case GamepadId::kNintendoProduct2006:
    case GamepadId::kNintendoProduct2007:
      // Joy Cons can only be connected over Bluetooth. When connected through
      // a Charging Grip, the grip's ID is reported instead.
      return GAMEPAD_BUS_BLUETOOTH;
    case GamepadId::kPowerALicPro:
      // The PowerA controller can only be connected over Bluetooth.
      return GAMEPAD_BUS_BLUETOOTH;
    default:
      break;
  }
  return GAMEPAD_BUS_UNKNOWN;
}
}  // namespace

NintendoController::SwitchCalibrationData::SwitchCalibrationData() = default;
NintendoController::SwitchCalibrationData::~SwitchCalibrationData() = default;

NintendoController::SwitchImuData::SwitchImuData() = default;
NintendoController::SwitchImuData::~SwitchImuData() = default;

NintendoController::NintendoController(int source_id,
                                       GamepadBusType bus_type,
                                       mojom::HidDeviceInfoPtr device_info,
                                       mojom::HidManager* hid_manager)
    : source_id_(source_id),
      is_composite_(false),
      bus_type_(bus_type),
      output_report_size_bytes_(0),
      device_info_(std::move(device_info)),
      hid_manager_(hid_manager) {
  if (device_info_) {
    output_report_size_bytes_ = device_info_->max_output_report_size;
    gamepad_id_ = GamepadIdList::Get().GetGamepadId(device_info_->product_name,
                                                    device_info_->vendor_id,
                                                    device_info_->product_id);
  } else {
    gamepad_id_ = GamepadId::kUnknownGamepad;
  }
}

NintendoController::NintendoController(
    int source_id,
    std::unique_ptr<NintendoController> composite1,
    std::unique_ptr<NintendoController> composite2,
    mojom::HidManager* hid_manager)
    : source_id_(source_id), is_composite_(true), hid_manager_(hid_manager) {
  // Require exactly one left component and one right component, but allow them
  // to be provided in either order.
  DCHECK(composite1);
  DCHECK(composite2);
  composite_left_ = std::move(composite1);
  composite_right_ = std::move(composite2);
  if (composite_left_->GetGamepadHand() != GamepadHand::kLeft)
    composite_left_.swap(composite_right_);
  DCHECK_EQ(composite_left_->GetGamepadHand(), GamepadHand::kLeft);
  DCHECK_EQ(composite_right_->GetGamepadHand(), GamepadHand::kRight);
  DCHECK_EQ(composite_left_->GetBusType(), composite_right_->GetBusType());
  bus_type_ = composite_left_->GetBusType();
}

NintendoController::~NintendoController() = default;

// static
std::unique_ptr<NintendoController> NintendoController::Create(
    int source_id,
    mojom::HidDeviceInfoPtr device_info,
    mojom::HidManager* hid_manager) {
  // Ignore if BusTypeFromDeviceInfo could not determine the bus type.
  GamepadBusType bus_type = device_info
                                ? BusTypeFromDeviceInfo(device_info.get())
                                : GAMEPAD_BUS_UNKNOWN;
  if (bus_type == GAMEPAD_BUS_UNKNOWN) {
    return nullptr;
  }

  return std::make_unique<NintendoController>(
      source_id, bus_type, std::move(device_info), hid_manager);
}

// static
std::unique_ptr<NintendoController> NintendoController::CreateComposite(
    int source_id,
    std::unique_ptr<NintendoController> composite1,
    std::unique_ptr<NintendoController> composite2,
    mojom::HidManager* hid_manager) {
  return std::make_unique<NintendoController>(
      source_id, std::move(composite1), std::move(composite2), hid_manager);
}

// static
bool NintendoController::IsNintendoController(GamepadId gamepad_id) {
  switch (gamepad_id) {
    case GamepadId::kNintendoProduct2006:
    case GamepadId::kNintendoProduct2007:
    case GamepadId::kNintendoProduct2009:
    case GamepadId::kNintendoProduct200e:
    case GamepadId::kPowerALicPro:
      return true;
    default:
      break;
  }
  return false;
}

std::vector<std::unique_ptr<NintendoController>>
NintendoController::Decompose() {
  // Stop any ongoing vibration effects before decomposing the device.
  SetZeroVibration();

  std::vector<std::unique_ptr<NintendoController>> decomposed_devices;
  if (composite_left_)
    decomposed_devices.push_back(std::move(composite_left_));
  if (composite_right_)
    decomposed_devices.push_back(std::move(composite_right_));
  return decomposed_devices;
}

void NintendoController::Open(base::OnceClosure device_ready_closure) {
  device_ready_closure_ = std::move(device_ready_closure);
  if (is_composite_) {
    StartInitSequence();
  } else {
    GamepadId gamepad_id = GamepadIdList::Get().GetGamepadId(
        device_info_->product_name, device_info_->vendor_id,
        device_info_->product_id);
    if (IsNintendoController(gamepad_id)) {
      Connect(base::BindOnce(&NintendoController::OnConnect,
                             weak_factory_.GetWeakPtr()));
    }
  }
}

GamepadHand NintendoController::GetGamepadHand() const {
  if (is_composite_)
    return GamepadHand::kNone;
  switch (gamepad_id_) {
    case GamepadId::kNintendoProduct2009:
    case GamepadId::kPowerALicPro:
      // Switch Pro and PowerA are held in both hands.
      return GamepadHand::kNone;
    case GamepadId::kNintendoProduct2006:
      // Joy-Con L is held in the left hand.
      return GamepadHand::kLeft;
    case GamepadId::kNintendoProduct2007:
      // Joy-Con R is held in the right hand.
      return GamepadHand::kRight;
    case GamepadId::kNintendoProduct200e:
      // Refer to |usb_device_type_| to determine the handedness of Joy-Cons
      // connected to a Charging Grip.
      if (state_ == kInitialized) {
        switch (usb_device_type_) {
          case kUsbDeviceTypeChargingGripJoyConL:
            return GamepadHand::kLeft;
          case kUsbDeviceTypeChargingGripJoyConR:
            return GamepadHand::kRight;
          case kUsbDeviceTypeChargingGripNoDevice:
          case kUsbDeviceTypeProController:
            return GamepadHand::kNone;
          default:
            break;
        }
      } else {
        return GamepadHand::kNone;
      }
      break;
    default:
      break;
  }
  NOTREACHED();
}

bool NintendoController::IsUsable() const {
  if (state_ != kInitialized)
    return false;
  if (is_composite_)
    return composite_left_ && composite_right_;
  switch (gamepad_id_) {
    case GamepadId::kNintendoProduct2009:
    case GamepadId::kNintendoProduct2006:
    case GamepadId::kNintendoProduct2007:
    case GamepadId::kPowerALicPro:
      return true;
    case GamepadId::kNintendoProduct200e:
      // Only usable as a composite device.
      return false;
    default:
      break;
  }
  NOTREACHED();
}

bool NintendoController::HasGuid(const std::string& guid) const {
  if (is_composite_) {
    DCHECK(composite_left_);
    DCHECK(composite_right_);
    return composite_left_->HasGuid(guid) || composite_right_->HasGuid(guid);
  }
  return device_info_->guid == guid;
}

GamepadStandardMappingFunction NintendoController::GetMappingFunction() const {
  if (is_composite_) {
    // In composite mode, we use the same mapping as the Charging Grip.
    return GetGamepadStandardMappingFunction(
        kProductNameSwitchCompositeDevice, kVendorNintendo,
        kProductSwitchChargingGrip,
        /*hid_specification_version=*/0, /*version_number=*/0, bus_type_);
  } else {
    return GetGamepadStandardMappingFunction(
        device_info_->product_name, device_info_->vendor_id,
        device_info_->product_id,

        /*hid_specification_version=*/0, /*version_number=*/0, bus_type_);
  }
}

void NintendoController::InitializeGamepadState(bool has_standard_mapping,
                                                Gamepad& pad) const {
  pad.buttons_length = SWITCH_BUTTON_INDEX_COUNT;
  pad.axes_length = device::AXIS_INDEX_COUNT;
  if (gamepad_id_ == GamepadId::kPowerALicPro) {
    pad.vibration_actuator.not_null = false;
  } else {
    pad.vibration_actuator.type = GamepadHapticActuatorType::kDualRumble;
    pad.vibration_actuator.not_null = true;
  }
  pad.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  if (is_composite_) {
    // Composite devices use the same product ID as the Switch Charging Grip.
    GamepadDataFetcher::UpdateGamepadStrings(
        kProductNameSwitchCompositeDevice, kVendorNintendo,
        kProductSwitchChargingGrip, has_standard_mapping, pad);
  } else {
    GamepadDataFetcher::UpdateGamepadStrings(
        device_info_->product_name, device_info_->vendor_id,
        device_info_->product_id, has_standard_mapping, pad);
  }
}

void NintendoController::UpdatePadConnected() {
  if (is_composite_) {
    // Composite devices are always connected.
    pad_.connected = true;
    return;
  }

  if (gamepad_id_ == GamepadId::kNintendoProduct200e &&
      usb_device_type_ == kUsbDeviceTypeChargingGripNoDevice) {
    // If the Charging Grip reports that no Joy-Con is docked, mark the gamepad
    // disconnected.
    pad_.connected = false;
    return;
  }

  // All other devices are considered connected after completing initialization.
  pad_.connected = (state_ == kInitialized);
}

void NintendoController::UpdateGamepadState(Gamepad& pad) const {
  if (is_composite_) {
    DCHECK(composite_left_);
    DCHECK(composite_right_);
    // If this is a composite device, update the gamepad state using the state
    // of the subcomponents.
    pad.connected = true;
    composite_left_->UpdateLeftGamepadState(pad, false);
    composite_right_->UpdateRightGamepadState(pad, false);
  } else {
    switch (GetGamepadHand()) {
      case GamepadHand::kLeft:
        // Update state for a Joy-Con L, remapping buttons and axes to match the
        // Standard Gamepad when the device is held horizontally.
        UpdateLeftGamepadState(pad, true);
        break;
      case GamepadHand::kRight:
        // Update state for a Joy-Con R, remapping buttons and axes to match the
        // Standard Gamepad when the device is held horizontally.
        UpdateRightGamepadState(pad, true);
        break;
      case GamepadHand::kNone:
        // Update state for a Pro Controller.
        UpdateLeftGamepadState(pad, false);
        UpdateRightGamepadState(pad, false);
        break;
      default:
        NOTREACHED();
    }
    pad.connected = pad_.connected;
  }
}

void NintendoController::UpdateLeftGamepadState(Gamepad& pad,
                                                bool horizontal) const {
  // Buttons associated with the left Joy-Con.
  const size_t kLeftButtonIndices[] = {
      BUTTON_INDEX_LEFT_SHOULDER,  // ZL button
      BUTTON_INDEX_LEFT_TRIGGER,   // L button
      BUTTON_INDEX_BACK_SELECT,    // - button
      BUTTON_INDEX_LEFT_THUMBSTICK,
      BUTTON_INDEX_DPAD_UP,    // D-pad directions for the composite gamepad
      BUTTON_INDEX_DPAD_DOWN,  // assume the Joy-Con is held in the vertical
      BUTTON_INDEX_DPAD_LEFT,  // orientation or is attached to a grip.
      BUTTON_INDEX_DPAD_RIGHT,      SWITCH_BUTTON_INDEX_CAPTURE,
      SWITCH_BUTTON_INDEX_LEFT_SL,  SWITCH_BUTTON_INDEX_LEFT_SR,
  };
  const size_t kLeftButtonIndicesSize = std::size(kLeftButtonIndices);

  // Axes associated with the left Joy-Con thumbstick.
  const size_t kLeftAxisIndices[] = {
      AXIS_INDEX_LEFT_STICK_X,  // Axes assume the Joy-Con is held vertically
      AXIS_INDEX_LEFT_STICK_Y,  // or is attached to a grip.
  };
  const size_t kLeftAxisIndicesSize = std::size(kLeftAxisIndices);

  if (pad_.buttons_length == SWITCH_BUTTON_INDEX_COUNT) {
    for (size_t i = 0; i < kLeftButtonIndicesSize; ++i)
      UpdateButtonForLeftSide(pad_, pad, kLeftButtonIndices[i], horizontal);
  }
  if (pad_.axes_length == AXIS_INDEX_COUNT) {
    for (size_t i = 0; i < kLeftAxisIndicesSize; ++i)
      UpdateAxisForLeftSide(pad_, pad, kLeftAxisIndices[i], horizontal);
  }
  pad.timestamp = std::max(pad.timestamp, pad_.timestamp);
  if (!pad_.connected)
    pad.connected = false;
}

void NintendoController::UpdateRightGamepadState(Gamepad& pad,
                                                 bool horizontal) const {
  // Buttons associated with the right Joy-Con.
  const size_t kRightButtonIndices[]{
      BUTTON_INDEX_PRIMARY,         // B button
      BUTTON_INDEX_SECONDARY,       // A button
      BUTTON_INDEX_TERTIARY,        // Y button
      BUTTON_INDEX_QUATERNARY,      // X button
      BUTTON_INDEX_RIGHT_SHOULDER,  // R button
      BUTTON_INDEX_RIGHT_TRIGGER,   // ZR button
      BUTTON_INDEX_START,           // + button
      BUTTON_INDEX_RIGHT_THUMBSTICK,
      BUTTON_INDEX_META,  // Home button
      SWITCH_BUTTON_INDEX_RIGHT_SL,
      SWITCH_BUTTON_INDEX_RIGHT_SR,
  };
  const size_t kRightButtonIndicesSize = std::size(kRightButtonIndices);

  // Axes associated with the right Joy-Con thumbstick.
  const size_t kRightAxisIndices[] = {
      AXIS_INDEX_RIGHT_STICK_X,  // Axes assume the Joy-Con is held vertically
      AXIS_INDEX_RIGHT_STICK_Y,  // or is attached to a grip.
  };
  const size_t kRightAxisIndicesSize = std::size(kRightAxisIndices);

  if (pad_.buttons_length == SWITCH_BUTTON_INDEX_COUNT) {
    for (size_t i = 0; i < kRightButtonIndicesSize; ++i)
      UpdateButtonForRightSide(pad_, pad, kRightButtonIndices[i], horizontal);
  }
  if (pad_.axes_length == AXIS_INDEX_COUNT) {
    for (size_t i = 0; i < kRightAxisIndicesSize; ++i)
      UpdateAxisForRightSide(pad_, pad, kRightAxisIndices[i], horizontal);
  }
  pad.timestamp = std::max(pad.timestamp, pad_.timestamp);
  if (!pad_.connected)
    pad.connected = false;
}

void NintendoController::Connect(mojom::HidManager::ConnectCallback callback) {
  DCHECK(!is_composite_);
  DCHECK(hid_manager_);
  hid_manager_->Connect(device_info_->guid,
                        /*connection_client=*/mojo::NullRemote(),
                        /*watcher=*/mojo::NullRemote(),
                        /*allow_protected_reports=*/false,
                        /*allow_fido_reports=*/false, std::move(callback));
}

void NintendoController::OnConnect(
    mojo::PendingRemote<mojom::HidConnection> connection) {
  if (connection) {
    connection_.Bind(std::move(connection));
    ReadInputReport();
    StartInitSequence();
  }
}

void NintendoController::StartInitSequence() {
  if (is_composite_) {
    if (composite_left_ && composite_left_->IsOpen() && composite_right_ &&
        composite_right_->IsOpen()) {
      DCHECK_EQ(composite_left_->GetGamepadHand(), GamepadHand::kLeft);
      DCHECK_EQ(composite_right_->GetGamepadHand(), GamepadHand::kRight);
      FinishInitSequence();
    } else {
      FailInitSequence();
    }
    return;
  }

  switch (bus_type_) {
    case GAMEPAD_BUS_USB:
      DCHECK(timeout_callback_.IsCancelled());
      MakeInitSequenceRequests(kPendingMacAddress);
      break;
    case GAMEPAD_BUS_BLUETOOTH:
      DCHECK(timeout_callback_.IsCancelled());
      MakeInitSequenceRequests(kPendingSetPlayerLights);
      break;
    default:
      NOTREACHED();
  }
}

void NintendoController::FinishInitSequence() {
  state_ = kInitialized;
  UpdatePadConnected();
  if (device_ready_closure_)
    std::move(device_ready_closure_).Run();
}

void NintendoController::FailInitSequence() {
  state_ = kUninitialized;
  UpdatePadConnected();
}

void NintendoController::HandleInputReport(
    uint8_t report_id,
    const std::vector<uint8_t>& report_bytes) {
  // Register to receive the next input report.
  ReadInputReport();

  // Listen for reports related to the initialization sequence or gamepad state.
  // Other reports are ignored.
  if (bus_type_ == GAMEPAD_BUS_USB && report_id == kUsbReportIdInput81)
    HandleUsbInputReport81(report_bytes);
  else if (report_id == kReportIdInput21)
    HandleInputReport21(report_bytes);
  else if (report_id == kReportIdInput30)
    HandleInputReport30(report_bytes);

  // Check whether the input report should cause us to transition to the next
  // initialization step.
  if (state_ != kInitialized && state_ != kUninitialized)
    ContinueInitSequence(report_id, report_bytes);
}

void NintendoController::HandleUsbInputReport81(
    const std::vector<uint8_t>& report_bytes) {
  const auto* ack_report =
      reinterpret_cast<const UsbInputReport81*>(report_bytes.data());
  switch (ack_report->subtype) {
    case kSubTypeRequestMac: {
      const auto* mac_report =
          reinterpret_cast<const MacAddressReport*>(report_bytes.data());
      mac_address_ = UnpackSwitchMacAddress(mac_report->mac_data);
      if (usb_device_type_ != mac_report->device_type) {
        usb_device_type_ = mac_report->device_type;
        switch (usb_device_type_) {
          case kUsbDeviceTypeChargingGripNoDevice:
            UpdatePadConnected();
            // If this was received from an initialized gamepad it means one of
            // the Joy-Cons was disconnected from the charging grip. The HID
            // device does not disconnect; de-initialize the device so the
            // composite device will be hidden.
            if (state_ == kInitialized)
              FailInitSequence();
            break;
          case kUsbDeviceTypeChargingGripJoyConL:
          case kUsbDeviceTypeChargingGripJoyConR:
            UpdatePadConnected();
            // A Joy-Con was connected to a de-initialized device. Restart the
            // initialization sequence.
            if (state_ == kUninitialized)
              StartInitSequence();
            break;
          default:
            break;
        }
      }
      break;
    }
    default:
      break;
  }
}

void NintendoController::HandleInputReport21(
    const std::vector<uint8_t>& report_bytes) {
  const auto* spi_report =
      reinterpret_cast<const SpiReadReport*>(report_bytes.data());
  if (UpdateGamepadFromControllerData(spi_report->controller_data, cal_data_,
                                      pad_)) {
    pad_.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  }
  // The input report includes the parameters for the SPI read request along
  // with the data that was read. Use the read address to determine how to
  // unpack the data.
  if (spi_report->subcommand == kSubCommandReadSpi) {
    uint16_t address = (spi_report->addrh << 8) | spi_report->addrl;
    switch (address) {
      case kSpiImuCalibrationAddress:
        UnpackSwitchImuCalibration(spi_report->spi_data, cal_data_);
        break;
      case kSpiImuHorizontalOffsetsAddress:
        UnpackSwitchImuHorizontalOffsets(spi_report->spi_data, cal_data_);
        break;
      case kSpiAnalogStickCalibrationAddress:
        UnpackSwitchAnalogStickCalibration(spi_report->spi_data, cal_data_);
        break;
      case kSpiAnalogStickParametersAddress:
        UnpackSwitchAnalogStickParameters(spi_report->spi_data, cal_data_);
        break;
      default:
        break;
    }
  }
}

void NintendoController::HandleInputReport30(
    const std::vector<uint8_t>& report_bytes) {
  const auto* controller_report =
      reinterpret_cast<const ControllerDataReport*>(report_bytes.data());
  // Each input report contains three frames of IMU data.
  UnpackSwitchImuData(&controller_report->imu_data[0], &imu_data_[0]);
  UnpackSwitchImuData(&controller_report->imu_data[12], &imu_data_[1]);
  UnpackSwitchImuData(&controller_report->imu_data[24], &imu_data_[2]);
  if (UpdateGamepadFromControllerData(controller_report->controller_data,
                                      cal_data_, pad_)) {
    pad_.timestamp = GamepadDataFetcher::CurrentTimeInMicroseconds();
  }
}

void NintendoController::ContinueInitSequence(
    uint8_t report_id,
    const std::vector<uint8_t>& report_bytes) {
  const auto* ack_report =
      reinterpret_cast<const UsbInputReport81*>(report_bytes.data());
  const auto* spi_report =
      reinterpret_cast<const SpiReadReport*>(report_bytes.data());
  const uint8_t ack_subtype =
      (report_id == kUsbReportIdInput81) ? ack_report->subtype : 0;
  const uint8_t spi_subcommand =
      (report_id == kReportIdInput21) ? spi_report->subcommand : 0;
  const bool is_spi_read =
      (report_id == kReportIdInput21 && spi_subcommand == kSubCommandReadSpi);
  const uint16_t spi_read_address =
      is_spi_read ? ((spi_report->addrh << 8) | spi_report->addrl) : 0;
  const uint16_t spi_read_length = is_spi_read ? spi_report->length : 0;

  switch (state_) {
    case kPendingMacAddress:
      if (ack_subtype == kSubTypeRequestMac) {
        CancelTimeout();
        if (mac_address_)
          MakeInitSequenceRequests(kPendingHandshake1);
        else
          FailInitSequence();
      }
      break;
    case kPendingHandshake1:
      if (ack_subtype == kSubTypeHandshake) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingBaudRate);
      }
      break;
    case kPendingBaudRate:
      if (ack_subtype == kSubTypeBaudRate) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingHandshake2);
      }
      break;
    case kPendingHandshake2:
      if (ack_subtype == kSubTypeHandshake) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingDisableUsbTimeout);
      }
      break;
    case kPendingDisableUsbTimeout:
      if (spi_subcommand == kSubCommand33) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingSetPlayerLights);
      }
      break;
    case kPendingSetPlayerLights:
      if (spi_subcommand == kSubCommandSetPlayerLights) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingEnableImu);
      }
      break;
    case kPendingEnableImu:
      if (spi_subcommand == kSubCommandEnableImu) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingSetImuSensitivity);
      }
      break;
    case kPendingSetImuSensitivity:
      if (spi_subcommand == kSubCommandSetImuSensitivity) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingReadImuCalibration);
      }
      break;
    case kPendingReadImuCalibration:
      if (spi_read_address == kSpiImuCalibrationAddress &&
          spi_read_length == kSpiImuCalibrationSize) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingReadHorizontalOffsets);
      }
      break;
    case kPendingReadHorizontalOffsets:
      if (spi_read_address == kSpiImuHorizontalOffsetsAddress &&
          spi_read_length == kSpiImuHorizontalOffsetsSize) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingReadAnalogStickCalibration);
      }
      break;
    case kPendingReadAnalogStickCalibration:
      if (spi_read_address == kSpiAnalogStickCalibrationAddress &&
          spi_read_length == kSpiAnalogStickCalibrationSize) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingReadAnalogStickParameters);
      }
      break;
    case kPendingReadAnalogStickParameters:
      if (spi_read_address == kSpiAnalogStickParametersAddress &&
          spi_read_length == kSpiAnalogStickParametersSize) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingEnableVibration);
      }
      break;
    case kPendingEnableVibration:
      if (spi_subcommand == kSubCommandEnableVibration) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingSetInputReportMode);
      }
      break;
    case kPendingSetInputReportMode:
      if (spi_subcommand == kSubCommandSetInputReportMode) {
        CancelTimeout();
        MakeInitSequenceRequests(kPendingControllerData);
      }
      break;
    case kPendingControllerData:
      if (report_id == kReportIdInput30) {
        CancelTimeout();
        FinishInitSequence();
      }
      break;
    case kInitialized:
    case kUninitialized:
      NOTREACHED();
    default:
      break;
  }
}

void NintendoController::MakeInitSequenceRequests(InitializationState state) {
  DCHECK(timeout_callback_.IsCancelled());
  state_ = state;
  switch (state_) {
    case kPendingMacAddress:
      RequestMacAddress();
      break;
    case kPendingHandshake1:
    case kPendingHandshake2:
      RequestHandshake();
      break;
    case kPendingBaudRate:
      RequestBaudRate();
      break;
    case kPendingDisableUsbTimeout:
      RequestEnableUsbTimeout(false);
      break;
    case kPendingSetPlayerLights:
      RequestSetPlayerLights(kPlayerLightPattern1);  // Player 1 indicator on.
      break;
    case kPendingEnableImu:
      RequestEnableImu(false);  // IMU disabled.
      break;
    case kPendingSetImuSensitivity:
      RequestSetImuSensitivity(
          kGyroSensitivity2000Dps, kAccelerometerSensitivity8G,
          kGyroPerformance208Hz, kAccelerometerFilterBandwidth100Hz);
      break;
    case kPendingReadImuCalibration:
      RequestImuCalibration();
      break;
    case kPendingReadHorizontalOffsets:
      RequestHorizontalOffsets();
      break;
    case kPendingReadAnalogStickCalibration:
      RequestAnalogCalibration();
      break;
    case kPendingReadAnalogStickParameters:
      RequestAnalogParameters();
      break;
    case kPendingEnableVibration:
      RequestEnableVibration(true);
      break;
    case kPendingSetInputReportMode:
      RequestSetInputReportMode(0x30);  // Standard full mode reported at 60Hz.
      break;
    case kPendingControllerData:
      ArmTimeout();
      break;
    case kInitialized:
    case kUninitialized:
    default:
      NOTREACHED();
  }
}

void NintendoController::SubCommand(uint8_t sub_command,
                                    const std::vector<uint8_t>& bytes) {
  std::vector<uint8_t> report_bytes(output_report_size_bytes_ - 1);
  // Serial subcommands also carry vibration data. Configure the vibration
  // portion of the report for a neutral vibration effect (zero amplitude).
  // https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_notes.md#output-0x12
  report_bytes[0] = static_cast<uint8_t>(output_report_counter_++ & 0xff);
  report_bytes[1] = 0x00;
  report_bytes[2] = 0x01;
  report_bytes[3] = 0x40;
  report_bytes[4] = 0x40;
  report_bytes[5] = 0x00;
  report_bytes[6] = 0x01;
  report_bytes[7] = 0x40;
  report_bytes[8] = 0x40;
  report_bytes[9] = sub_command;
  DCHECK_LT(bytes.size() + kSubCommandDataOffset, output_report_size_bytes_);
  base::ranges::copy(bytes, &report_bytes[kSubCommandDataOffset - 1]);
  WriteOutputReport(kReportIdOutput01, report_bytes, true);
}

void NintendoController::RequestMacAddress() {
  std::vector<uint8_t> report_bytes(output_report_size_bytes_ - 1);
  report_bytes[0] = kSubTypeRequestMac;
  WriteOutputReport(kUsbReportIdOutput80, report_bytes, true);
}

void NintendoController::RequestHandshake() {
  std::vector<uint8_t> report_bytes(output_report_size_bytes_ - 1);
  report_bytes[0] = kSubTypeHandshake;
  WriteOutputReport(kUsbReportIdOutput80, report_bytes, true);
}

void NintendoController::RequestBaudRate() {
  std::vector<uint8_t> report_bytes(output_report_size_bytes_ - 1);
  report_bytes[0] = kSubTypeBaudRate;
  WriteOutputReport(kUsbReportIdOutput80, report_bytes, true);
}

void NintendoController::RequestSubCommand33() {
  // Unrecognized commands do nothing, but still generate a reply.
  SubCommand(kSubCommand33, {});
}

void NintendoController::RequestVibration(double left_frequency,
                                          double left_magnitude,
                                          double right_frequency,
                                          double right_magnitude) {
  uint16_t lhf;
  uint8_t llf;
  uint8_t lhfa;
  uint16_t llfa;
  uint16_t rhf;
  uint8_t rlf;
  uint8_t rhfa;
  uint16_t rlfa;
  FrequencyToHex(left_frequency, left_magnitude, &lhf, &llf, &lhfa, &llfa);
  FrequencyToHex(right_frequency, right_magnitude, &rhf, &rlf, &rhfa, &rlfa);
  std::vector<uint8_t> report_bytes(output_report_size_bytes_ - 1);
  uint8_t counter = static_cast<uint8_t>(output_report_counter_++ & 0x0f);
  report_bytes[0] = counter;
  report_bytes[1] = lhf & 0xff;
  report_bytes[2] = lhfa + ((lhf >> 8) & 0xff);
  report_bytes[3] = llf + ((llfa >> 8) & 0xff);
  report_bytes[4] = llfa & 0xff;
  report_bytes[5] = rhf & 0xff;
  report_bytes[6] = rhfa + ((rhf >> 8) & 0xff);
  report_bytes[7] = rlf + ((rlfa >> 8) & 0xff);
  report_bytes[8] = rlfa & 0xff;
  WriteOutputReport(kReportIdOutput10, report_bytes, false);
}

void NintendoController::RequestEnableUsbTimeout(bool enable) {
  // By default, Switch Pro will revert to Bluetooth mode if it does not
  // receive any USB HID commands within a timeout window. Disabling the
  // timeout keeps the device in USB mode.
  std::vector<uint8_t> report_bytes(output_report_size_bytes_ - 1);
  report_bytes[0] =
      enable ? kSubTypeEnableUsbTimeout : kSubTypeDisableUsbTimeout;
  // This report may not be acked due to a software bug on the device.
  WriteOutputReport(kUsbReportIdOutput80, report_bytes, false);
  // Send an unused subcommand (0x33) which is acked.
  RequestSubCommand33();
}

void NintendoController::RequestEnableImu(bool enable) {
  SubCommand(kSubCommandEnableImu,
             {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

void NintendoController::RequestEnableVibration(bool enable) {
  SubCommand(kSubCommandEnableVibration,
             {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

void NintendoController::RequestSetPlayerLights(uint8_t light_pattern) {
  SubCommand(kSubCommandSetPlayerLights, {light_pattern});
}

void NintendoController::RequestSetImuSensitivity(
    uint8_t gyro_sensitivity,
    uint8_t accelerometer_sensitivity,
    uint8_t gyro_performance_rate,
    uint8_t accelerometer_filter_bandwidth) {
  SubCommand(kSubCommandSetImuSensitivity,
             {gyro_sensitivity, accelerometer_sensitivity,
              gyro_performance_rate, accelerometer_filter_bandwidth});
}

void NintendoController::RequestSetInputReportMode(uint8_t mode) {
  SubCommand(kSubCommandSetInputReportMode, {mode});
}

void NintendoController::ReadSpi(uint16_t address, size_t length) {
  DCHECK_LE(length + kSpiDataOffset, output_report_size_bytes_);
  length = std::min(length, output_report_size_bytes_ - kSpiDataOffset);
  uint8_t address_high = (address >> 8) & 0xff;
  uint8_t address_low = address & 0xff;
  SubCommand(kSubCommandReadSpi, {address_low, address_high, 0x00, 0x00,
                                  static_cast<uint8_t>(length)});
}

void NintendoController::RequestImuCalibration() {
  ReadSpi(kSpiImuCalibrationAddress, kSpiImuCalibrationSize);
}

void NintendoController::RequestHorizontalOffsets() {
  ReadSpi(kSpiImuHorizontalOffsetsAddress, kSpiImuHorizontalOffsetsSize);
}

void NintendoController::RequestAnalogCalibration() {
  ReadSpi(kSpiAnalogStickCalibrationAddress, kSpiAnalogStickCalibrationSize);
}

void NintendoController::RequestAnalogParameters() {
  ReadSpi(kSpiAnalogStickParametersAddress, kSpiAnalogStickParametersSize);
}

void NintendoController::ReadInputReport() {
  DCHECK(connection_);
  connection_->Read(base::BindOnce(&NintendoController::OnReadInputReport,
                                   weak_factory_.GetWeakPtr()));
}

void NintendoController::OnReadInputReport(
    bool success,
    uint8_t report_id,
    const std::optional<std::vector<uint8_t>>& report_bytes) {
  if (success) {
    DCHECK(report_bytes);
    HandleInputReport(report_id, *report_bytes);
  } else {
    CancelTimeout();
    FailInitSequence();
  }
}

void NintendoController::WriteOutputReport(
    uint8_t report_id,
    const std::vector<uint8_t>& report_bytes,
    bool expect_reply) {
  DCHECK(connection_);
  DCHECK(timeout_callback_.IsCancelled());
  connection_->Write(report_id, report_bytes,
                     base::BindOnce(&NintendoController::OnWriteOutputReport,
                                    weak_factory_.GetWeakPtr()));
  if (expect_reply)
    ArmTimeout();
}

void NintendoController::OnWriteOutputReport(bool success) {
  if (!success) {
    CancelTimeout();
    FailInitSequence();
  }
}

void NintendoController::DoShutdown() {
  if (composite_left_)
    composite_left_->Shutdown();
  composite_left_.reset();
  if (composite_right_)
    composite_right_->Shutdown();
  composite_right_.reset();
  connection_.reset();
  device_info_.reset();
}

void NintendoController::SetVibration(
    mojom::GamepadEffectParametersPtr params) {
  if (is_composite_) {
    // Split the vibration effect between the left and right subdevices.
    if (composite_left_ && composite_right_) {
      composite_left_->SetVibration(mojom::GamepadEffectParameters::New(
          params->duration, params->start_delay, params->strong_magnitude,
          /*weak_magnitude=*/0, /*left_trigger=*/0, /*right_trigger=*/0));
      composite_right_->SetVibration(mojom::GamepadEffectParameters::New(
          params->duration, params->start_delay, /*strong_magnitude=*/0,
          params->weak_magnitude, /*left_trigger=*/0, /*right_trigger=*/0));
    }
  } else {
    RequestVibration(
        kVibrationFrequencyStrongRumble,
        kVibrationAmplitudeStrongRumbleMax * params->strong_magnitude,
        kVibrationFrequencyWeakRumble,
        kVibrationAmplitudeWeakRumbleMax * params->weak_magnitude);
  }
}

double NintendoController::GetMaxEffectDurationMillis() {
  return kMaxVibrationEffectDurationMillis;
}

void NintendoController::ArmTimeout() {
  DCHECK(timeout_callback_.IsCancelled());
  timeout_callback_.Reset(base::BindOnce(&NintendoController::OnTimeout,
                                         weak_factory_.GetWeakPtr()));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, timeout_callback_.callback(), kTimeoutDuration);
}

void NintendoController::CancelTimeout() {
  timeout_callback_.Cancel();
  retry_count_ = 0;
}

void NintendoController::OnTimeout() {
  ++retry_count_;
  if (retry_count_ <= kMaxRetryCount)
    MakeInitSequenceRequests(state_);
  else {
    retry_count_ = 0;
    StartInitSequence();
  }
}

base::WeakPtr<AbstractHapticGamepad> NintendoController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
