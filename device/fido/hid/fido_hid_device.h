// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FIDO_HID_DEVICE_H_
#define DEVICE_FIDO_HID_FIDO_HID_DEVICE_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "device/fido/fido_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

class FidoHidMessage;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidDevice : public FidoDevice {
 public:
  FidoHidDevice(device::mojom::HidDeviceInfoPtr device_info,
                device::mojom::HidManager* hid_manager);
  ~FidoHidDevice() final;

  // Returns FidoDevice::GetId() for a given HidDeviceInfo.
  static std::string GetIdForDevice(
      const device::mojom::HidDeviceInfo& device_info);

  // FidoDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) final;
  void TryWink(base::OnceClosure callback) final;
  void Cancel(CancelToken token) final;
  std::string GetId() const final;
  FidoTransportProtocol DeviceTransport() const final;
  void DiscoverSupportedProtocolAndDeviceInfo(base::OnceClosure done) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestConnectionFailure);
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestDeviceError);
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestRetryChannelAllocation);
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestCancel);

  // BusyState enumerates a sub-state-machine of the main state machine. This
  // is separate from |FidoDevice::state_| because that is shared between all
  // types of device, but HID and BLE devices are quite different in the way
  // that they handle transactions: HID drivers read when they wish to, but BLE
  // receives messages whenever the device sends one.
  enum class BusyState {
    // kWriting means that a request is being written. If a cancelation occurs
    // for this request, this will change to |kWritingPendingCancel|.
    kWriting,
    // kWritingPendingCancel means that a request is being written, but it has
    // already been canceled. Therefore a cancel request will be sent
    // immediately afterwards. Then the state will move to |kReading|.
    kWritingPendingCancel,
    // kWaiting means that a request was written, has not yet been canceled, and
    // the first packet of the reply is still pending. If the request is
    // cancelled the state will move to |kReading|. Otherwise, that'll happen
    // when the first packet of the reply is received.
    kWaiting,
    // kReading means that, while the full response has not yet been received,
    // there's also no point in sending a cancel message. This is either because
    // the first frame of the response has been received, or else because a
    // cancel request has already been sent.
    kReading,
  };

  struct COMPONENT_EXPORT(DEVICE_FIDO) PendingTransaction {
    PendingTransaction(FidoHidDeviceCommand command_type,
                       std::vector<uint8_t> command,
                       DeviceCallback callback,
                       CancelToken token);
    ~PendingTransaction();

    FidoHidDeviceCommand command_type;
    std::vector<uint8_t> command;
    DeviceCallback callback;
    CancelToken token;
  };

  void Transition(base::Optional<State> next_state = base::nullopt);

  // Open a connection to this device.
  void Connect(device::mojom::HidManager::ConnectCallback callback);
  void OnConnect(mojo::PendingRemote<device::mojom::HidConnection> connection);
  void OnInitWriteComplete(std::vector<uint8_t> nonce, bool success);
  // Ask device to allocate a unique channel id for this connection.
  void OnAllocateChannel(std::vector<uint8_t> nonce,
                         base::Optional<FidoHidMessage> message);
  base::Optional<uint32_t> ParseInitReply(const std::vector<uint8_t>& nonce,
                                          const std::vector<uint8_t>& buf);
  void OnPotentialInitReply(std::vector<uint8_t> nonce,
                            bool success,
                            uint8_t report_id,
                            const base::Optional<std::vector<uint8_t>>& buf);
  // Write all message packets to device, and read response if expected.
  void WriteMessage(FidoHidMessage message);
  void PacketWritten(FidoHidMessage message, bool success);
  // Read all response message packets from device.
  void ReadMessage();
  void OnRead(bool success,
              uint8_t report_id,
              const base::Optional<std::vector<uint8_t>>& buf);
  void OnReadContinuation(FidoHidMessage message,
                          bool success,
                          uint8_t report_id,
                          const base::Optional<std::vector<uint8_t>>& buf);
  void MessageReceived(FidoHidMessage message);
  void ArmTimeout();
  void OnTimeout();
  void WriteCancel();

  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  uint8_t capabilities_ = 0;

  // |output_report_size_| is the size of the packets that will be sent to the
  // device. (For HID devices, these are called reports.)
  const uint8_t output_report_size_;

  // busy_state_ is valid iff |state_| (from the parent class) is |kBusy|.
  BusyState busy_state_;

  uint32_t channel_id_;
  base::CancelableOnceClosure timeout_callback_;
  // pending_transactions_ includes both the current transaction, and
  // transactions that have not yet been sent.
  std::list<PendingTransaction> pending_transactions_;
  // current_token_ is valid if |state_| is |kBusy| and |busy_state_| is not
  // |kInit|.
  CancelToken current_token_;

  // All the FidoHidDevice instances are owned by U2fRequest. So it is safe to
  // let the FidoHidDevice share the device::mojo::HidManager raw pointer from
  // U2fRequest.
  device::mojom::HidManager* hid_manager_;
  device::mojom::HidDeviceInfoPtr device_info_;
  mojo::Remote<device::mojom::HidConnection> connection_;
  base::WeakPtrFactory<FidoHidDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoHidDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FIDO_HID_DEVICE_H_
