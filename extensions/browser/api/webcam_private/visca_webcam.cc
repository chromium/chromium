// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/webcam_private/visca_webcam.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "extensions/common/extension_id.h"

using content::BrowserThread;

namespace {

// Message terminator:
const char kViscaTerminator = 0xFF;

// Response types:
const char kViscaResponseNetworkChange = 0x38;
const char kViscaResponseAck = 0x40;
const char kViscaResponseError = 0x60;

// The default pan speed is kMaxPanSpeed /2 and the default tilt speed is
// kMaxTiltSpeed / 2.
const int kMaxPanSpeed = 0x18;
const int kMaxTiltSpeed = 0x14;
const int kDefaultPanSpeed = 0x18 / 2;
const int kDefaultTiltSpeed = 0x14 / 2;

// Pan-Tilt-Zoom movement comands from http://www.manualslib.com/manual/...
// 557364/Cisco-Precisionhd-1080p12x.html?page=31#manual

// Reset the address of each device in the VISCA chain (broadcast). This is used
// when resetting the VISCA network.
const uint8_t kSetAddressCommand[] = {0x88, 0x30, 0x01, 0xFF};

// Clear all of the devices, halting any pending commands in the VISCA chain
// (broadcast). This is used when resetting the VISCA network.
const uint8_t kClearAllCommand[] = {0x88, 0x01, 0x00, 0x01, 0xFF};

// Command: {0x8X, 0x09, 0x06, 0x12, 0xFF}, X = 1 to 7: target device address.
// Response: {0xY0, 0x50, 0x0p, 0x0q, 0x0r, 0x0s, 0x0t, 0x0u, 0x0v, 0x0w, 0xFF},
// Y = socket number; pqrs: pan position; tuvw: tilt position.
const uint8_t kGetPanTiltCommand[] = {0x81, 0x09, 0x06, 0x12, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x02, 0x0p, 0x0t, 0x0q, 0x0r, 0x0s, 0x0u, 0x0v,
// 0x0w, 0x0y, 0x0z, 0xFF}, X = 1 to 7: target device address; p = pan speed;
// t = tilt speed; qrsu = pan position; vwyz = tilt position.
const uint8_t kSetPanTiltCommand[] = {0x81, 0x01, 0x06, 0x02, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x05, 0xFF}, X = 1 to 7: target device address.
const uint8_t kResetPanTiltCommand[] = {0x81, 0x01, 0x06, 0x05, 0xFF};

// Command: {0x8X, 0x09, 0x04, 0x47, 0xFF}, X = 1 to 7: target device address.
// Response: {0xY0, 0x50, 0x0p, 0x0q, 0x0r, 0x0s, 0xFF}, Y = socket number;
// pqrs: zoom position.
const uint8_t kGetZoomCommand[] = {0x81, 0x09, 0x04, 0x47, 0xFF};

// Command: {0x8X, 0x01, 0x04, 0x47, 0x0p, 0x0q, 0x0r, 0x0s, 0xFF}, X = 1 to 7:
// target device address; pqrs: zoom position;
const uint8_t kSetZoomCommand[] = {0x81, 0x01, 0x04, 0x47, 0x00,
                                   0x00, 0x00, 0x00, 0xFF};

// Command: {0x8X, 0x01, 0x04, 0x38, 0x02, 0xFF}, X = 1 to 7: target device
// address.
const uint8_t kSetAutoFocusCommand[] = {0x81, 0x01, 0x04, 0x38, 0x02, 0xFF};

// Command: {0x8X, 0x01, 0x04, 0x38, 0x03, 0xFF}, X = 1 to 7: target device
// address.
const uint8_t kSetManualFocusCommand[] = {0x81, 0x01, 0x04, 0x38, 0x03, 0xFF};

// Command: {0x8X, 0x09, 0x04, 0x48, 0xFF}, X = 1 to 7: target device address.
// Response: {0xY0, 0x50, 0x0p, 0x0q, 0x0r, 0x0s, 0xFF}, Y = socket number;
// pqrs: focus position.
const uint8_t kGetFocusCommand[] = {0x81, 0x09, 0x04, 0x48, 0xFF};

// Command: {0x8X, 0x01, 0x04, 0x48, 0x0p, 0x0q, 0x0r, 0x0s, 0xFF}, X = 1 to 7:
// target device address; pqrs: focus position;
const uint8_t kSetFocusCommand[] = {0x81, 0x01, 0x04, 0x48, 0x00,
                                    0x00, 0x00, 0x00, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x03, 0x01, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const uint8_t kPTUpCommand[] = {0x81, 0x01, 0x06, 0x01, 0x00,
                                0x00, 0x03, 0x01, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x03, 0x02, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const uint8_t kPTDownCommand[] = {0x81, 0x01, 0x06, 0x01, 0x00,
                                  0x00, 0x03, 0x02, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x0, 0x03, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const uint8_t kPTLeftCommand[] = {0x81, 0x01, 0x06, 0x01, 0x00,
                                  0x00, 0x01, 0x03, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x02, 0x03, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const uint8_t kPTRightCommand[] = {0x81, 0x01, 0x06, 0x01, 0x00,
                                   0x00, 0x02, 0x03, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x03, 0x03, 0x03, 0x03, 0xFF}, X = 1 to 7:
// target device address.
const uint8_t kPTStopCommand[] = {0x81, 0x01, 0x06, 0x01, 0x03,
                                  0x03, 0x03, 0x03, 0xFF};

#define CHAR_VECTOR_FROM_ARRAY(array)                     \
  std::vector<char>(reinterpret_cast<const char*>(array), \
                    reinterpret_cast<const char*>(array + std::size(array)))

int ShiftResponseLowerBits(char c, size_t shift) {
  return static_cast<int>(c & 0x0F) << shift;
}

bool CanBuildResponseInt(const std::vector<char>& response,
                         size_t start_index) {
  return response.size() >= start_index + 4;
}

int BuildResponseInt(const std::vector<char>& response, size_t start_index) {
  return ShiftResponseLowerBits(response[start_index], 12) +
         ShiftResponseLowerBits(response[start_index + 1], 8) +
         ShiftResponseLowerBits(response[start_index + 2], 4) +
         ShiftResponseLowerBits(response[start_index + 3], 0);
}

void ResponseToCommand(std::vector<char>* command,
                       size_t start_index,
                       uint16_t response) {
  DCHECK(command);
  std::vector<char>& command_ref = *command;
  command_ref[start_index] |= ((response >> 12) & 0x0F);
  command_ref[start_index + 1] |= ((response >> 8) & 0x0F);
  command_ref[start_index + 2] |= ((response >> 4 & 0x0F));
  command_ref[start_index + 3] |= (response & 0x0F);
}

int CalculateSpeed(int desired_speed, int max_speed, int default_speed) {
  int speed = std::min(desired_speed, max_speed);
  return speed > 0 ? speed : default_speed;
}

int GetPositiveValue(int value) {
  return value < 0x8000 ? value : value - 0xFFFF;
}

}  // namespace

namespace extensions {

ViscaWebcam::ViscaWebcam() = default;

ViscaWebcam::~ViscaWebcam() = default;

void ViscaWebcam::Open(const ExtensionId& extension_id,
                       api::SerialPortManager* port_manager,
                       const std::string& path,
                       const OpenCompleteCallback& open_callback) {
  api::serial::ConnectionOptions options;

  // Set the receive buffer size to receive the response data 1 by 1.
  options.buffer_size = 1;
  options.persistent = false;
  options.bitrate = 9600;
  options.cts_flow_control = false;
  // Enable send and receive timeout error.
  options.receive_timeout = 3000;
  options.send_timeout = 3000;
  options.data_bits = api::serial::DataBits::kEight;
  options.parity_bit = api::serial::ParityBit::kNo;
  options.stop_bits = api::serial::StopBits::kOne;

  serial_connection_ = std::make_unique<SerialConnection>(extension_id);
  serial_connection_->Open(
      port_manager, path, options,
      base::BindOnce(&ViscaWebcam::OnConnected, base::Unretained(this),
                     open_callback));
}

void ViscaWebcam::OnConnected(const OpenCompleteCallback& open_callback,
                              bool success) {
  if (!success) {
    open_callback.Run(success);
    return;
  }

  Send(CHAR_VECTOR_FROM_ARRAY(kSetAddressCommand),
       base::BindRepeating(&ViscaWebcam::OnAddressSetCompleted,
                           base::Unretained(this), open_callback));
}

void ViscaWebcam::OnAddressSetCompleted(
    const OpenCompleteCallback& open_callback,
    bool success,
    const std::vector<char>& response) {
  commands_.pop_front();
  if (!success) {
    open_callback.Run(success);
    return;
  }

  Send(CHAR_VECTOR_FROM_ARRAY(kClearAllCommand),
       base::BindRepeating(&ViscaWebcam::OnClearAllCompleted,
                           base::Unretained(this), open_callback));
}

void ViscaWebcam::OnClearAllCompleted(const OpenCompleteCallback& open_callback,
                                      bool success,
                                      const std::vector<char>& response) {
  commands_.pop_front();
  open_callback.Run(success);
}

void ViscaWebcam::Send(const std::vector<char>& command,
                       const CommandCompleteCallback& callback) {
  commands_.push_back(std::make_pair(command, callback));
  // If this is the only command in the queue, send it now.
  if (commands_.size() == 1) {
    serial_connection_->Send(
        std::vector<uint8_t>(command.begin(), command.end()),
        base::BindOnce(&ViscaWebcam::OnSendCompleted, base::Unretained(this),
                       callback));
  }
}

void ViscaWebcam::OnSendCompleted(const CommandCompleteCallback& callback,
                                  uint32_t bytes_sent,
                                  api::serial::SendError error) {
  // TODO(xdai): Check |bytes_sent|?
  if (error == api::serial::SendError::kNone) {
    serial_connection_->StartPolling(base::BindRepeating(
        &ViscaWebcam::OnReceiveEvent, base::Unretained(this), callback));
  } else {
    callback.Run(false, std::vector<char>());
  }
}

void ViscaWebcam::OnReceiveEvent(const CommandCompleteCallback& callback,
                                 std::vector<uint8_t> data,
                                 api::serial::ReceiveError error) {
  data_buffer_.insert(data_buffer_.end(), data.begin(), data.end());

  if (error != api::serial::ReceiveError::kNone || data_buffer_.empty()) {
    // Clear |data_buffer_|.
    std::vector<char> response;
    response.swap(data_buffer_);
    serial_connection_->SetPaused(true);
    callback.Run(false, response);
    return;
  }

  // Success case. If waiting for more data, then loop until encounter the
  // terminator.
  if (data_buffer_.back() != kViscaTerminator)
    return;

  // Success case, and a complete response has been received.
  // Clear |data_buffer_|.
  std::vector<char> response;
  response.swap(data_buffer_);

  if (response.size() < 2 ||
      (static_cast<int>(response[1]) & 0xF0) == kViscaResponseError) {
    serial_connection_->SetPaused(true);
    callback.Run(false, response);
  } else if ((static_cast<int>(response[1]) & 0xF0) != kViscaResponseAck &&
             (static_cast<int>(response[1]) & 0xFF) !=
                 kViscaResponseNetworkChange) {
    serial_connection_->SetPaused(true);
    callback.Run(true, response);
  }
}

void ViscaWebcam::OnCommandCompleted(const SetPTZCompleteCallback& callback,
                                     bool success,
                                     const std::vector<char>& response) {
  // TODO(xdai): Error handling according to |response|.
  callback.Run(success);
  ProcessNextCommand();
}

void ViscaWebcam::OnInquiryCompleted(InquiryType type,
                                     const GetPTZCompleteCallback& callback,
                                     bool success,
                                     const std::vector<char>& response) {
  if (!success) {
    callback.Run(false, 0 /* value */, 0 /* min_value */, 0 /* max_value */);
    ProcessNextCommand();
    return;
  }

  bool is_valid_response = false;
  switch (type) {
    case INQUIRY_PAN:
      is_valid_response = CanBuildResponseInt(response, 2);
      break;
    case INQUIRY_TILT:
      is_valid_response = CanBuildResponseInt(response, 6);
      break;
    case INQUIRY_ZOOM:
      is_valid_response = CanBuildResponseInt(response, 2);
      break;
    case INQUIRY_FOCUS:
      is_valid_response = CanBuildResponseInt(response, 2);
      break;
  }
  if (!is_valid_response) {
    callback.Run(false, 0 /* value */, 0 /* min_value */, 0 /* max_value */);
    ProcessNextCommand();
    return;
  }

  int value = 0;
  switch (type) {
    case INQUIRY_PAN:
      // See kGetPanTiltCommand for the format of response.
      pan_ = BuildResponseInt(response, 2);
      value = GetPositiveValue(pan_);
      break;
    case INQUIRY_TILT:
      // See kGetPanTiltCommand for the format of response.
      tilt_ = BuildResponseInt(response, 6);
      value = GetPositiveValue(tilt_);
      break;
    case INQUIRY_ZOOM:
      // See kGetZoomCommand for the format of response.
      value = BuildResponseInt(response, 2);
      break;
    case INQUIRY_FOCUS:
      // See kGetFocusCommand for the format of response.
      value = BuildResponseInt(response, 2);
      break;
  }
  // TODO(pbos): Add support for valid ranges.
  callback.Run(true, value, 0, 0);
  ProcessNextCommand();
}

void ViscaWebcam::ProcessNextCommand() {
  commands_.pop_front();

  if (commands_.empty())
    return;

  // If there are pending commands, process the next one.
  const std::vector<char> next_command = commands_.front().first;
  const CommandCompleteCallback next_callback = commands_.front().second;
  serial_connection_->Send(
      std::vector<uint8_t>(next_command.begin(), next_command.end()),
      base::BindOnce(&ViscaWebcam::OnSendCompleted, base::Unretained(this),
                     next_callback));
}

void ViscaWebcam::GetPan(const GetPTZCompleteCallback& callback) {
  Send(CHAR_VECTOR_FROM_ARRAY(kGetPanTiltCommand),
       base::BindRepeating(&ViscaWebcam::OnInquiryCompleted,
                           base::Unretained(this), INQUIRY_PAN, callback));
}

void ViscaWebcam::GetTilt(const GetPTZCompleteCallback& callback) {
  Send(CHAR_VECTOR_FROM_ARRAY(kGetPanTiltCommand),
       base::BindRepeating(&ViscaWebcam::OnInquiryCompleted,
                           base::Unretained(this), INQUIRY_TILT, callback));
}

void ViscaWebcam::GetZoom(const GetPTZCompleteCallback& callback) {
  Send(CHAR_VECTOR_FROM_ARRAY(kGetZoomCommand),
       base::BindRepeating(&ViscaWebcam::OnInquiryCompleted,
                           base::Unretained(this), INQUIRY_ZOOM, callback));
}

void ViscaWebcam::GetFocus(const GetPTZCompleteCallback& callback) {
  Send(CHAR_VECTOR_FROM_ARRAY(kGetFocusCommand),
       base::BindRepeating(&ViscaWebcam::OnInquiryCompleted,
                           base::Unretained(this), INQUIRY_FOCUS, callback));
}

void ViscaWebcam::SetPan(int value,
                         int pan_speed,
                         const SetPTZCompleteCallback& callback) {
  int actual_pan_speed =
      CalculateSpeed(pan_speed, kMaxPanSpeed, kDefaultPanSpeed);
  pan_ = value;

  std::vector<char> command = CHAR_VECTOR_FROM_ARRAY(kSetPanTiltCommand);
  command[4] |= actual_pan_speed;
  command[5] |= kDefaultTiltSpeed;
  ResponseToCommand(&command, 6, static_cast<uint16_t>(pan_));
  ResponseToCommand(&command, 10, static_cast<uint16_t>(tilt_));
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::SetTilt(int value,
                          int tilt_speed,
                          const SetPTZCompleteCallback& callback) {
  int actual_tilt_speed =
      CalculateSpeed(tilt_speed, kMaxTiltSpeed, kDefaultTiltSpeed);
  tilt_ = value;

  std::vector<char> command = CHAR_VECTOR_FROM_ARRAY(kSetPanTiltCommand);
  command[4] |= kDefaultPanSpeed;
  command[5] |= actual_tilt_speed;
  ResponseToCommand(&command, 6, static_cast<uint16_t>(pan_));
  ResponseToCommand(&command, 10, static_cast<uint16_t>(tilt_));
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::SetZoom(int value, const SetPTZCompleteCallback& callback) {
  int actual_value = std::max(value, 0);
  std::vector<char> command = CHAR_VECTOR_FROM_ARRAY(kSetZoomCommand);
  ResponseToCommand(&command, 4, actual_value);
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::SetFocus(int value, const SetPTZCompleteCallback& callback) {
  int actual_value = std::max(value, 0);
  std::vector<char> command = CHAR_VECTOR_FROM_ARRAY(kSetFocusCommand);
  ResponseToCommand(&command, 4, actual_value);
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::SetAutofocusState(AutofocusState state,
                                    const SetPTZCompleteCallback& callback) {
  std::vector<char> command;
  if (state == AUTOFOCUS_ON) {
    command = CHAR_VECTOR_FROM_ARRAY(kSetAutoFocusCommand);
  } else {
    command = CHAR_VECTOR_FROM_ARRAY(kSetManualFocusCommand);
  }
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::SetPanDirection(PanDirection direction,
                                  int pan_speed,
                                  const SetPTZCompleteCallback& callback) {
  int actual_pan_speed =
      CalculateSpeed(pan_speed, kMaxPanSpeed, kDefaultPanSpeed);
  std::vector<char> command = CHAR_VECTOR_FROM_ARRAY(kPTStopCommand);
  switch (direction) {
    case PAN_STOP:
      break;
    case PAN_RIGHT:
      command = CHAR_VECTOR_FROM_ARRAY(kPTRightCommand);
      command[4] |= actual_pan_speed;
      command[5] |= kDefaultTiltSpeed;
      break;
    case PAN_LEFT:
      command = CHAR_VECTOR_FROM_ARRAY(kPTLeftCommand);
      command[4] |= actual_pan_speed;
      command[5] |= kDefaultTiltSpeed;
      break;
  }
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::SetTiltDirection(TiltDirection direction,
                                   int tilt_speed,
                                   const SetPTZCompleteCallback& callback) {
  int actual_tilt_speed =
      CalculateSpeed(tilt_speed, kMaxTiltSpeed, kDefaultTiltSpeed);
  std::vector<char> command = CHAR_VECTOR_FROM_ARRAY(kPTStopCommand);
  switch (direction) {
    case TILT_STOP:
      break;
    case TILT_UP:
      command = CHAR_VECTOR_FROM_ARRAY(kPTUpCommand);
      command[4] |= kDefaultPanSpeed;
      command[5] |= actual_tilt_speed;
      break;
    case TILT_DOWN:
      command = CHAR_VECTOR_FROM_ARRAY(kPTDownCommand);
      command[4] |= kDefaultPanSpeed;
      command[5] |= actual_tilt_speed;
      break;
  }
  Send(command, base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                                    base::Unretained(this), callback));
}

void ViscaWebcam::Reset(bool pan,
                        bool tilt,
                        bool zoom,
                        const SetPTZCompleteCallback& callback) {
  // pan and tilt are always reset together in Visca Webcams.
  if (pan || tilt) {
    Send(CHAR_VECTOR_FROM_ARRAY(kResetPanTiltCommand),
         base::BindRepeating(&ViscaWebcam::OnCommandCompleted,
                             base::Unretained(this), callback));
  }
  if (zoom) {
    // Set the default zoom value to 100 to be consistent with V4l2 webcam.
    const int kDefaultZoom = 100;
    SetZoom(kDefaultZoom, callback);
  }
}

void ViscaWebcam::SetHome(const SetPTZCompleteCallback& callback) {}

void ViscaWebcam::RestoreCameraPreset(int preset_number,
                                      const SetPTZCompleteCallback& callback) {}
void ViscaWebcam::SetCameraPreset(int preset_number,
                                  const SetPTZCompleteCallback& callback) {}

void ViscaWebcam::OpenForTesting(
    std::unique_ptr<SerialConnection> serial_connection) {
  serial_connection_ = std::move(serial_connection);
}

SerialConnection* ViscaWebcam::GetSerialConnectionForTesting() {
  return serial_connection_.get();
}

}  // namespace extensions
