// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_device.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/hid/fido_hid_message.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {

// U2F devices only provide a single report so specify a report ID of 0 here.
static constexpr uint8_t kReportId = 0x00;
static constexpr uint8_t kWinkCapability = 0x01;

FidoHidDevice::FidoHidDevice(device::mojom::HidDeviceInfoPtr device_info,
                             device::mojom::HidManager* hid_manager)
    : FidoDevice(),
      output_report_size_(device_info->max_output_report_size),
      hid_manager_(hid_manager),
      device_info_(std::move(device_info)) {
  DCHECK_GE(std::numeric_limits<decltype(output_report_size_)>::max(),
            device_info_->max_output_report_size);
  // These limits on the report size are enforced in fido_hid_discovery.cc.
  DCHECK_LT(kHidInitPacketHeaderSize, output_report_size_);
  DCHECK_GE(kHidMaxPacketSize, output_report_size_);
}

FidoHidDevice::~FidoHidDevice() = default;

FidoDevice::CancelToken FidoHidDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  const CancelToken token = next_cancel_token_++;
  const auto command_type = supported_protocol() == ProtocolVersion::kCtap2
                                ? FidoHidDeviceCommand::kCbor
                                : FidoHidDeviceCommand::kMsg;
  pending_transactions_.emplace_back(command_type, std::move(command),
                                     std::move(callback), token);
  Transition();
  return token;
}

void FidoHidDevice::Cancel(CancelToken token) {
  if (state_ == State::kBusy && current_token_ == token) {
    // Sending a Cancel request should cause the outstanding request to return
    // with CTAP2_ERR_KEEPALIVE_CANCEL if the device is CTAP2. That error will
    // cause the request to complete in the usual way. U2F doesn't have a cancel
    // message, but U2F devices are not expected to block on requests and also
    // no U2F command alters state in a meaningful way, as CTAP2 commands do.
    if (supported_protocol() != ProtocolVersion::kCtap2) {
      return;
    }

    switch (busy_state_) {
      case BusyState::kWriting:
        // Send a cancelation message once the transmission is complete.
        busy_state_ = BusyState::kWritingPendingCancel;
        break;
      case BusyState::kWritingPendingCancel:
        // A cancelation message is already scheduled.
        break;
      case BusyState::kWaiting:
        // Waiting for reply. Send cancelation message.
        busy_state_ = BusyState::kReading;
        WriteCancel();
        break;
      case BusyState::kReading:
        // Have either already sent a cancel message, or else have started
        // reading the response.
        break;
    }
    return;
  }

  // The request with the given |token| isn't the current request. Remove it
  // from the list of pending requests if found.
  for (auto it = pending_transactions_.begin();
       it != pending_transactions_.end(); it++) {
    if (it->token != token) {
      continue;
    }

    auto callback = std::move(it->callback);
    pending_transactions_.erase(it);
    std::vector<uint8_t> cancel_reply = {
        static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel)};
    std::move(callback).Run(std::move(cancel_reply));
    break;
  }
}

void FidoHidDevice::Transition(base::Optional<State> next_state) {
  if (next_state) {
    state_ = *next_state;
  }

  switch (state_) {
    case State::kInit:
      state_ = State::kConnecting;
      ArmTimeout();
      Connect(base::BindOnce(&FidoHidDevice::OnConnect,
                             weak_factory_.GetWeakPtr()));
      break;
    case State::kReady: {
      DCHECK(!pending_transactions_.empty());

      // Some devices fail when sent a wink command immediately followed by a
      // CBOR command. Only try to wink if device claims support and it is
      // required to signal user presence.
      if (pending_transactions_.front().command_type ==
              FidoHidDeviceCommand::kWink &&
          !(capabilities_ & kWinkCapability && needs_explicit_wink_)) {
        DeviceCallback pending_cb =
            std::move(pending_transactions_.front().callback);
        pending_transactions_.pop_front();
        std::move(pending_cb).Run(base::nullopt);
        break;
      }

      state_ = State::kBusy;
      busy_state_ = BusyState::kWriting;
      ArmTimeout();

      // Write message to the device.
      current_token_ = pending_transactions_.front().token;
      auto maybe_message(FidoHidMessage::Create(
          channel_id_, pending_transactions_.front().command_type,
          output_report_size_,
          std::move(pending_transactions_.front().command)));
      DCHECK(maybe_message);
      WriteMessage(std::move(*maybe_message));
      break;
    }
    case State::kConnecting:
    case State::kBusy:
      break;
    case State::kDeviceError:
    case State::kMsgError:
      base::WeakPtr<FidoHidDevice> self = weak_factory_.GetWeakPtr();
      // Executing callbacks may free |this|. Check |self| first.
      while (self && !pending_transactions_.empty()) {
        // Respond to any pending requests.
        DeviceCallback pending_cb =
            std::move(pending_transactions_.front().callback);
        pending_transactions_.pop_front();
        std::move(pending_cb).Run(base::nullopt);
      }
      break;
  }
}

FidoHidDevice::PendingTransaction::PendingTransaction(
    FidoHidDeviceCommand command_type,
    std::vector<uint8_t> in_command,
    DeviceCallback in_callback,
    CancelToken in_token)
    : command_type(command_type),
      command(std::move(in_command)),
      callback(std::move(in_callback)),
      token(in_token) {}

FidoHidDevice::PendingTransaction::~PendingTransaction() = default;

void FidoHidDevice::Connect(
    device::mojom::HidManager::ConnectCallback callback) {
  DCHECK(hid_manager_);
  hid_manager_->Connect(device_info_->guid,
                        /*connection_client=*/mojo::NullRemote(),
                        /*watcher=*/mojo::NullRemote(), std::move(callback));
}

void FidoHidDevice::OnConnect(
    mojo::PendingRemote<device::mojom::HidConnection> connection) {
  timeout_callback_.Cancel();

  if (!connection) {
    Transition(State::kDeviceError);
    return;
  }

  connection_.Bind(std::move(connection));
  // Send random nonce to device to verify received message.
  std::vector<uint8_t> nonce(8);
  crypto::RandBytes(nonce.data(), nonce.size());

  DCHECK_EQ(State::kConnecting, state_);
  ArmTimeout();

  FidoHidInitPacket init(kHidBroadcastChannel, FidoHidDeviceCommand::kInit,
                         nonce, nonce.size());
  std::vector<uint8_t> init_packet = init.GetSerializedData();
  init_packet.resize(output_report_size_, 0);
  connection_->Write(
      kReportId, std::move(init_packet),
      base::BindOnce(&FidoHidDevice::OnInitWriteComplete,
                     weak_factory_.GetWeakPtr(), std::move(nonce)));
}

void FidoHidDevice::OnInitWriteComplete(std::vector<uint8_t> nonce,
                                        bool success) {
  if (state_ == State::kDeviceError) {
    return;
  }

  if (!success) {
    Transition(State::kDeviceError);
  }

  connection_->Read(base::BindOnce(&FidoHidDevice::OnPotentialInitReply,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(nonce)));
}

// ParseInitReply parses a potential reply to a U2FHID_INIT message. If the
// reply matches the given nonce then the assigned channel ID is returned.
base::Optional<uint32_t> FidoHidDevice::ParseInitReply(
    const std::vector<uint8_t>& nonce,
    const std::vector<uint8_t>& buf) {
  auto message = FidoHidMessage::CreateFromSerializedData(buf);
  if (!message ||
      // Any reply will be sent to the broadcast channel.
      message->channel_id() != kHidBroadcastChannel ||
      // Init replies must fit in a single frame.
      !message->MessageComplete() ||
      message->cmd() != FidoHidDeviceCommand::kInit) {
    return base::nullopt;
  }

  auto payload = message->GetMessagePayload();
  // The channel allocation response is defined as:
  // 0: 8 byte nonce
  // 8: 4 byte channel id
  // 12: Protocol version id
  // 13: Major device version
  // 14: Minor device version
  // 15: Build device version
  // 16: Capabilities
  DCHECK_EQ(8u, nonce.size());
  if (payload.size() != 17 || memcmp(nonce.data(), payload.data(), 8) != 0) {
    return base::nullopt;
  }

  capabilities_ = payload[16];

  return static_cast<uint32_t>(payload[8]) << 24 |
         static_cast<uint32_t>(payload[9]) << 16 |
         static_cast<uint32_t>(payload[10]) << 8 |
         static_cast<uint32_t>(payload[11]);
}

void FidoHidDevice::OnPotentialInitReply(
    std::vector<uint8_t> nonce,
    bool success,
    uint8_t report_id,
    const base::Optional<std::vector<uint8_t>>& buf) {
  if (state_ == State::kDeviceError) {
    return;
  }

  if (!success) {
    Transition(State::kDeviceError);
    return;
  }
  DCHECK(buf);

  base::Optional<uint32_t> maybe_channel_id = ParseInitReply(nonce, *buf);
  if (!maybe_channel_id) {
    // This instance of Chromium may not be the only process communicating with
    // this HID device, but all processes will see all the messages from the
    // device. Thus it is not an error to observe unexpected messages from the
    // device and they are ignored.
    connection_->Read(base::BindOnce(&FidoHidDevice::OnPotentialInitReply,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(nonce)));
    return;
  }

  timeout_callback_.Cancel();
  channel_id_ = *maybe_channel_id;
  Transition(State::kReady);
}

void FidoHidDevice::WriteMessage(FidoHidMessage message) {
  DCHECK_EQ(State::kBusy, state_);
  DCHECK(message.NumPackets() > 0);

  auto packet = message.PopNextPacket();
  DCHECK_LE(packet.size(), output_report_size_);
  packet.resize(output_report_size_, 0);
  connection_->Write(
      kReportId, packet,
      base::BindOnce(&FidoHidDevice::PacketWritten, weak_factory_.GetWeakPtr(),
                     std::move(message)));
}

void FidoHidDevice::PacketWritten(FidoHidMessage message, bool success) {
  if (state_ == State::kDeviceError) {
    return;
  }

  DCHECK_EQ(State::kBusy, state_);
  if (!success) {
    Transition(State::kDeviceError);
    return;
  }

  if (message.NumPackets() > 0) {
    WriteMessage(std::move(message));
    return;
  }

  switch (busy_state_) {
    case BusyState::kWriting:
      busy_state_ = BusyState::kWaiting;
      ReadMessage();
      break;
    case BusyState::kWritingPendingCancel:
      busy_state_ = BusyState::kReading;
      WriteCancel();
      ReadMessage();
      break;
    default:
      NOTREACHED();
  }
}

void FidoHidDevice::ReadMessage() {
  connection_->Read(
      base::BindOnce(&FidoHidDevice::OnRead, weak_factory_.GetWeakPtr()));
}

void FidoHidDevice::OnRead(bool success,
                           uint8_t report_id,
                           const base::Optional<std::vector<uint8_t>>& buf) {
  if (state_ == State::kDeviceError) {
    return;
  }

  DCHECK_EQ(State::kBusy, state_);

  if (!success) {
    Transition(State::kDeviceError);
    return;
  }
  DCHECK(buf);

  auto message = FidoHidMessage::CreateFromSerializedData(*buf);
  if (!message) {
    Transition(State::kDeviceError);
    return;
  }

  if (!message->MessageComplete()) {
    // Continue reading additional packets.
    connection_->Read(base::BindOnce(&FidoHidDevice::OnReadContinuation,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(*message)));
    return;
  }

  // Received a message from a different channel, so try again.
  if (channel_id_ != message->channel_id()) {
    ReadMessage();
    return;
  }

  // If received HID packet is a keep-alive message then reset the timeout and
  // read again.
  if (supported_protocol() == ProtocolVersion::kCtap2 &&
      message->cmd() == FidoHidDeviceCommand::kKeepAlive) {
    timeout_callback_.Cancel();
    ArmTimeout();
    ReadMessage();
    return;
  }

  switch (busy_state_) {
    case BusyState::kWaiting:
      busy_state_ = BusyState::kReading;
      break;
    case BusyState::kReading:
      break;
    default:
      NOTREACHED();
  }

  MessageReceived(std::move(*message));
}

void FidoHidDevice::OnReadContinuation(
    FidoHidMessage message,
    bool success,
    uint8_t report_id,
    const base::Optional<std::vector<uint8_t>>& buf) {
  if (state_ == State::kDeviceError) {
    return;
  }

  if (!success) {
    Transition(State::kDeviceError);
    return;
  }
  DCHECK(buf);

  if (!message.AddContinuationPacket(*buf)) {
    Transition(State::kDeviceError);
    return;
  }

  if (!message.MessageComplete()) {
    connection_->Read(base::BindOnce(&FidoHidDevice::OnReadContinuation,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(message)));
    return;
  }

  // Received a message from a different channel, so try again.
  if (channel_id_ != message.channel_id()) {
    ReadMessage();
    return;
  }

  MessageReceived(std::move(message));
}

void FidoHidDevice::MessageReceived(FidoHidMessage message) {
  timeout_callback_.Cancel();

  const auto cmd = message.cmd();
  auto response = message.GetMessagePayload();
  if (cmd != FidoHidDeviceCommand::kMsg && cmd != FidoHidDeviceCommand::kCbor &&
      cmd != FidoHidDeviceCommand::kWink) {
    if (cmd != FidoHidDeviceCommand::kError || response.size() != 1) {
      FIDO_LOG(ERROR) << "Unknown HID message received: "
                      << static_cast<int>(cmd) << " "
                      << base::HexEncode(response.data(), response.size());
      Transition(State::kDeviceError);
      return;
    }

    // HID transport layer error constants that are returned to the client.
    // https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#ctaphid-commands
    enum class HidErrorConstant : uint8_t {
      kInvalidCommand = 0x01,
      kInvalidParameter = 0x02,
      kInvalidLength = 0x03,
      // (Other errors omitted.)
    };

    switch (static_cast<HidErrorConstant>(response[0])) {
      case HidErrorConstant::kInvalidCommand:
      case HidErrorConstant::kInvalidParameter:
      case HidErrorConstant::kInvalidLength:
        Transition(State::kMsgError);
        break;
      default:
        FIDO_LOG(ERROR) << "HID error received: "
                        << static_cast<int>(response[0]);
        Transition(State::kDeviceError);
    }

    return;
  }

  DCHECK(!pending_transactions_.empty());
  auto callback = std::move(pending_transactions_.front().callback);
  pending_transactions_.pop_front();
  current_token_ = FidoDevice::kInvalidCancelToken;

  base::WeakPtr<FidoHidDevice> self = weak_factory_.GetWeakPtr();
  // The callback may call back into this object thus |state_| is set ahead of
  // time.
  state_ = State::kReady;
  std::move(callback).Run(std::move(response));

  // Executing |callback| may have freed |this|. Check |self| first.
  if (self && !pending_transactions_.empty()) {
    Transition();
  }
}

void FidoHidDevice::TryWink(base::OnceClosure callback) {
  const CancelToken token = next_cancel_token_++;
  pending_transactions_.emplace_back(
      FidoHidDeviceCommand::kWink, std::vector<uint8_t>(),
      base::BindOnce(
          [](base::OnceClosure cb, base::Optional<std::vector<uint8_t>> data) {
            std::move(cb).Run();
          },
          std::move(callback)),
      token);
  Transition();
}

void FidoHidDevice::ArmTimeout() {
  DCHECK(timeout_callback_.IsCancelled());
  timeout_callback_.Reset(
      base::BindOnce(&FidoHidDevice::OnTimeout, weak_factory_.GetWeakPtr()));
  // Setup timeout task for 3 seconds.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_callback_.callback(), kDeviceTimeout);
}

void FidoHidDevice::OnTimeout() {
  FIDO_LOG(ERROR) << "FIDO HID device timeout for " << GetId();
  Transition(State::kDeviceError);
}

void FidoHidDevice::WriteCancel() {
  FidoHidInitPacket cancel(channel_id_, FidoHidDeviceCommand::kCancel, {},
                           /*payload_length=*/0);
  std::vector<uint8_t> cancel_packet = cancel.GetSerializedData();
  DCHECK_LE(cancel_packet.size(), output_report_size_);
  cancel_packet.resize(output_report_size_, 0);
  connection_->Write(kReportId, std::move(cancel_packet), base::DoNothing());
}

std::string FidoHidDevice::GetId() const {
  return GetIdForDevice(*device_info_);
}

FidoTransportProtocol FidoHidDevice::DeviceTransport() const {
  return FidoTransportProtocol::kUsbHumanInterfaceDevice;
}

// VidPidToString returns the device's vendor and product IDs as formatted by
// the lsusb utility.
static std::string VidPidToString(const mojom::HidDeviceInfoPtr& device_info) {
  static_assert(sizeof(device_info->vendor_id) == 2,
                "vendor_id must be uint16_t");
  static_assert(sizeof(device_info->product_id) == 2,
                "product_id must be uint16_t");
  uint16_t vendor_id = ((device_info->vendor_id & 0xff) << 8) |
                       ((device_info->vendor_id & 0xff00) >> 8);
  uint16_t product_id = ((device_info->product_id & 0xff) << 8) |
                        ((device_info->product_id & 0xff00) >> 8);
  return base::ToLowerASCII(base::HexEncode(&vendor_id, 2) + ":" +
                            base::HexEncode(&product_id, 2));
}

void FidoHidDevice::DiscoverSupportedProtocolAndDeviceInfo(
    base::OnceClosure done) {
  // The following devices cannot handle GetInfo messages.
  static const base::flat_set<std::string> kForceU2fCompatibilitySet({
      "10c4:8acf",  // U2F Zero
      "20a0:4287",  // Nitrokey FIDO U2F
  });

  if (base::Contains(kForceU2fCompatibilitySet, VidPidToString(device_info_))) {
    supported_protocol_ = ProtocolVersion::kU2f;
    DCHECK(SupportedProtocolIsInitialized());
    std::move(done).Run();
    return;
  }
  FidoDevice::DiscoverSupportedProtocolAndDeviceInfo(std::move(done));
}

// static
std::string FidoHidDevice::GetIdForDevice(
    const device::mojom::HidDeviceInfo& device_info) {
  return "hid:" + device_info.guid;
}

base::WeakPtr<FidoDevice> FidoHidDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
