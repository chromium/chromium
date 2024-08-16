// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_NINTENDO_CONTROLLER_H_
#define DEVICE_GAMEPAD_NINTENDO_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// NintendoController represents a gaming input for a Nintendo console. In some
// cases, multiple discrete devices can be associated to form one logical
// input. A single NintendoController instance may represent a discrete device,
// or may represent two devices associated to form a composite input. (For
// instance, a pair of matched Joy-Cons may be treated as a single gamepad.)
//
// Switch devices must be initialized in order to provide a good experience.
// Devices that connect over Bluetooth (Joy-Cons and the Pro Controller) default
// to a HID interface that exposes only partial functionality. Devices that
// connect over USB (Pro Controller and Charging Grip) send no controller data
// reports until the initialization sequence is started. In both cases,
// initialization is necessary in order to configure device LEDs, enable and
// disable device features, and fetch calibration data.
//
// After initialization, the Joy-Con or Pro Controller should be in the
// following state:
// * Faster baud rate, if connected over USB.
// * Player indicator light 1 is lit, others are unlit.
// * Home light is 100% on.
// * Accelerometer and gyroscope inputs are enabled with default sensitivity.
// * Vibration is enabled.
// * NFC is disabled.
// * Configured to send controller data reports with report ID 0x30.
//
// Joy-Cons and the Pro Controller provide uncalibrated joystick input. The
// devices store factory calibration data for scaling the joystick axes and
// applying deadzones.
//
// Dual-rumble vibration effects are supported for both discrete devices and for
// composite devices. Joy-Cons and the Pro Controller use linear resonant
// actuators (LRAs) instead of the traditional eccentric rotating mass (ERM)
// vibration actuators. The LRAs are controlled using frequency and amplitude
// pairs rather than a single magnitude value. To simulate a dual-rumble effect,
// the left and right LRAs are set to vibrate at different frequencies as though
// they were the strong and weak ERM actuators. The amplitudes are set to the
// strong and weak effect magnitudes. When a vibration effect is played on a
// composite device, the effect is split so that each component receives one
// channel of the dual-rumble effect.
class NintendoController final : public AbstractHapticGamepad {
 public:
  struct SwitchCalibrationData {
    SwitchCalibrationData();
    ~SwitchCalibrationData();

    // Analog stick calibration data.
    uint16_t lx_center = 0;
    uint16_t lx_min = 0;
    uint16_t lx_max = 0;
    uint16_t ly_center = 0;
    uint16_t ly_min = 0;
    uint16_t ly_max = 0;
    uint16_t rx_center = 0;
    uint16_t rx_min = 0;
    uint16_t rx_max = 0;
    uint16_t ry_center = 0;
    uint16_t ry_min = 0;
    uint16_t ry_max = 0;
    uint16_t dead_zone = 0;
    uint16_t range_ratio = 0;

    // IMU calibration data.
    uint16_t accelerometer_origin_x = 0;
    uint16_t accelerometer_origin_y = 0;
    uint16_t accelerometer_origin_z = 0;
    uint16_t accelerometer_sensitivity_x = 0;
    uint16_t accelerometer_sensitivity_y = 0;
    uint16_t accelerometer_sensitivity_z = 0;
    uint16_t gyro_origin_x = 0;
    uint16_t gyro_origin_y = 0;
    uint16_t gyro_origin_z = 0;
    uint16_t gyro_sensitivity_x = 0;
    uint16_t gyro_sensitivity_y = 0;
    uint16_t gyro_sensitivity_z = 0;
    uint16_t horizontal_offset_x = 0;
    uint16_t horizontal_offset_y = 0;
    uint16_t horizontal_offset_z = 0;
  };

  // One frame of accerometer and gyroscope data.
  struct SwitchImuData {
    SwitchImuData();
    ~SwitchImuData();

    uint16_t accelerometer_x;
    uint16_t accelerometer_y;
    uint16_t accelerometer_z;
    uint16_t gyro_x;
    uint16_t gyro_y;
    uint16_t gyro_z;
  };

  ~NintendoController() override;

  // Create a NintendoController for a newly-connected HID device. It may return
  // a nullptr if `device_info`, does not represent a compatible device.
  static std::unique_ptr<NintendoController> Create(
      int source_id,
      mojom::HidDeviceInfoPtr device_info,
      mojom::HidManager* hid_manager);

  // Create a composite NintendoController from two already-connected HID
  // devices.
  static std::unique_ptr<NintendoController> CreateComposite(
      int source_id,
      std::unique_ptr<NintendoController> composite1,
      std::unique_ptr<NintendoController> composite2,
      mojom::HidManager* hid_manager);

  // Return true if |gamepad_id| describes a Nintendo controller.
  static bool IsNintendoController(GamepadId gamepad_id);

  // Decompose a composite device and return a vector of its subcomponents.
  // Return an empty vector if the device is non-composite.
  std::vector<std::unique_ptr<NintendoController>> Decompose();

  // Begin the initialization sequence. When the device is ready to report
  // controller data, |device_ready_closure| is called.
  void Open(base::OnceClosure device_ready_closure);

  // Return true if the device has completed initialization and is ready to
  // report controller data.
  bool IsOpen() const { return is_composite_ || connection_.is_bound(); }

  // Return true if the device is ready to be assigned a gamepad slot.
  bool IsUsable() const;

  // Return true if the device is composed of multiple subdevices.
  bool IsComposite() const { return is_composite_; }

  // Return the source ID assigned to this device.
  int GetSourceId() const { return source_id_; }

  // Return the bus type for this controller.
  GamepadBusType GetBusType() const { return bus_type_; }

  // Return the mapping function for this controller.
  GamepadStandardMappingFunction GetMappingFunction() const;

  // Return true if |guid| is the device GUID for any of the HID devices
  // opened for this controller.
  bool HasGuid(const std::string& guid) const;

  // Perform one-time initialization for the gamepad data in |pad|.
  void InitializeGamepadState(bool has_standard_mapping, Gamepad& pad) const;

  // Update the button and axis state in |pad|.
  void UpdateGamepadState(Gamepad& pad) const;
  // Update the button and axis state in |pad| for a Joy-Con L or for the left
  // side of a Pro controller. If |horizontal| is true, also remap buttons and
  // axes for a horizontal orientation.
  void UpdateLeftGamepadState(Gamepad& pad, bool horizontal) const;
  // Update the button and axis state in |pad| for a Joy-Con R or for the right
  // side of a Pro controller. If |horizontal| is true, also remap buttons and
  // axes for a horizontal orientation.
  void UpdateRightGamepadState(Gamepad& pad, bool horizontal) const;

  // Return the handedness of the device, or GamepadHand::kNone if the device
  // is not intended to be used in a specific hand.
  GamepadHand GetGamepadHand() const;

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  double GetMaxEffectDurationMillis() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  NintendoController(int source_id,
                     GamepadBusType bus_type,
                     mojom::HidDeviceInfoPtr device_info,
                     mojom::HidManager* hid_manager);
  NintendoController(int source_id,
                     std::unique_ptr<NintendoController> composite1,
                     std::unique_ptr<NintendoController> composite2,
                     mojom::HidManager* hid_manager);

 private:
  enum InitializationState {
    // Switch Pro requires initialization to configure the device for USB mode
    // and to read calibration data.
    kUninitialized = 0,
    // Fetch the MAC address. This allows us to identify when the device is
    // double-connected through USB and Bluetooth.
    kPendingMacAddress,
    // Increase the baud rate to improve latency. This requires a handshake
    // before and after the change.
    kPendingHandshake1,
    kPendingBaudRate,
    kPendingHandshake2,
    // Disable the USB timeout. This subcommand is not acked, so also send an
    // unused subcommand (0x33) which is acked.
    kPendingDisableUsbTimeout,
    // Set the player lights to the default (player 1).
    kPendingSetPlayerLights,
    // Disable the accelerometer and gyro.
    kPendingEnableImu,
    // Configure accelerometer and gyro sensitivity.
    kPendingSetImuSensitivity,
    // Read the calibration settings for the accelerometer and gyro.
    kPendingReadImuCalibration,
    // Read the dead zone and range ratio for the analog sticks.
    kPendingReadAnalogStickParameters,
    // Read the calibration settings for the horizontal orientation.
    kPendingReadHorizontalOffsets,
    // Read the calibration settings for the analog sticks.
    kPendingReadAnalogStickCalibration,
    // Enable vibration.
    kPendingEnableVibration,
    // Set standard full mode (60 Hz).
    kPendingSetInputReportMode,
    // Wait for controller data to be received.
    kPendingControllerData,
    // Fully initialized.
    kInitialized,
  };

  // Initiate a connection request to the HID device.
  void Connect(mojom::HidManager::ConnectCallback callback);

  // Completion callback for the HID connection request.
  void OnConnect(mojo::PendingRemote<mojom::HidConnection> connection);

  // Initiate the sequence of exchanges to prepare the device to provide
  // controller data.
  void StartInitSequence();

  // Transition to |state| and makes the request(s) associated with the state.
  // May be called repeatedly to retry the current initialization step.
  void MakeInitSequenceRequests(InitializationState state);

  // Mark the device as initialized.
  void FinishInitSequence();

  // Mark the device as uninitialized.
  void FailInitSequence();

  // Handle an input report sent by the device. The first byte of the report
  // (the report ID) has been extracted to |report_id| and the remaining bytes
  // are in |report_bytes|.
  void HandleInputReport(uint8_t report_id,
                         const std::vector<uint8_t>& report_bytes);

  // Handle a USB input report with report ID 0x81. These reports are used
  // during device initialization.
  void HandleUsbInputReport81(const std::vector<uint8_t>& report_bytes);

  // Handle a USB or Bluetooth input report with ID 0x21. These reports carry
  // controller data and subcommand responses.
  void HandleInputReport21(const std::vector<uint8_t>& report_bytes);

  // Handle a USB or Bluetooth input report with ID 0x30. These reports carry
  // controller data and IMU data.
  void HandleInputReport30(const std::vector<uint8_t>& report_bytes);

  // Check the result of a received input report and decide whether to
  // transition to the next step in the initialization sequence.
  void ContinueInitSequence(uint8_t report_id,
                            const std::vector<uint8_t>& report_bytes);

  // Update |pad_.connected| based on the current device state.
  void UpdatePadConnected();

  // Register to receive the next input report from the underlying HID device.
  void ReadInputReport();

  // Callback to be called when an input report is received, or when the read
  // has failed.
  void OnReadInputReport(
      bool success,
      uint8_t report_id,
      const std::optional<std::vector<uint8_t>>& report_bytes);

  // Request to send an output report to the underlying HID device. If
  // |expect_reply| is true, a timeout is armed that will retry the current
  // initialization step if no reply is received within the timeout window.
  void WriteOutputReport(uint8_t report_id,
                         const std::vector<uint8_t>& report_bytes,
                         bool expect_reply);

  // Callback to be called when an output report request is complete or has
  // failed.
  void OnWriteOutputReport(bool success);

  // Output reports sent to the device.
  void SubCommand(uint8_t sub_command, const std::vector<uint8_t>& bytes);
  void RequestMacAddress();
  void RequestHandshake();
  void RequestBaudRate();
  void RequestSubCommand33();
  void RequestVibration(double left_frequency,
                        double left_magnitude,
                        double right_frequency,
                        double right_magnitude);
  void RequestEnableUsbTimeout(bool enable);
  void RequestEnableVibration(bool enable);
  void RequestEnableImu(bool enable);
  void RequestSetPlayerLights(uint8_t light_pattern);
  void RequestSetImuSensitivity(uint8_t gyro_sensitivity,
                                uint8_t accelerometer_sensitivity,
                                uint8_t gyro_performance_rate,
                                uint8_t accelerometer_filter_bandwidth);
  void RequestSetInputReportMode(uint8_t mode);
  void ReadSpi(uint16_t address, size_t length);
  void RequestImuCalibration();
  void RequestHorizontalOffsets();
  void RequestAnalogCalibration();
  void RequestAnalogParameters();

  // Schedule a callback to retry a step during the initialization sequence.
  void ArmTimeout();

  // Cancel the current timeout, if there is one.
  void CancelTimeout();

  // Timeout expiration callback.
  void OnTimeout();

  // An ID value to identify this device among other devices enumerated by the
  // data fetcher.
  const int source_id_;

  // The current step of the initialization sequence, or kInitialized if the
  // device is already initialized. Set to kUninitialized if the device is
  // in a temporary de-initialized state but is still connected.
  InitializationState state_ = kUninitialized;

  // The number of times the current initialization step has been retried.
  size_t retry_count_ = 0;

  // A composite device contains up to two Joy-Cons as sub-devices.
  bool is_composite_ = false;

  // Left and right sub-devices for a composite device.
  std::unique_ptr<NintendoController> composite_left_;
  std::unique_ptr<NintendoController> composite_right_;

  // Global output report counter. Increments by 1 for each report sent.
  uint8_t output_report_counter_ = 0;

  // The Bluetooth MAC address of the device.
  uint64_t mac_address_ = 0;

  // The bus type for the underlying HID device.
  GamepadBusType bus_type_ = GAMEPAD_BUS_UNKNOWN;

  // The maximum size of an output report for the underlying HID device.
  size_t output_report_size_bytes_ = 0;

  // 8-bit value representing the device type, as reported by the device when
  // connected over USB.
  uint8_t usb_device_type_ = 0;

  // Calibration data read from the device.
  SwitchCalibrationData cal_data_;

  // The last collection of IMU data. The device reports three frames of data in
  // each update.
  SwitchImuData imu_data_[3];

  // The most recent gamepad state.
  Gamepad pad_;

  // A callback to be called once the initialization timeout has expired.
  base::CancelableOnceClosure timeout_callback_;

  // Information about the underlying HID device.
  mojom::HidDeviceInfoPtr device_info_;

  GamepadId gamepad_id_;

  // HID service manager.
  const raw_ptr<mojom::HidManager> hid_manager_;

  // The open connection to the underlying HID device.
  mojo::Remote<mojom::HidConnection> connection_;

  // A closure, provided in the call to Open, to be called once the device
  // becomes ready.
  base::OnceClosure device_ready_closure_;

  base::WeakPtrFactory<NintendoController> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_NINTENDO_CONTROLLER_H_
