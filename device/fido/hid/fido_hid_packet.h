// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FIDO_HID_PACKET_H_
#define DEVICE_FIDO_HID_FIDO_HID_PACKET_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "device/fido/fido_constants.h"

namespace device {

// HID Packets are defined by the specification at
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#message-and-packet-structure
// Packets are one of two types, initialization packets and continuation
// packets. HID Packets have header information and a payload. If a
// FidoHidInitPacket cannot store the entire payload, further payload
// information is stored in HidContinuationPackets.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidPacket {
 public:
  FidoHidPacket(std::vector<uint8_t> data, uint32_t channel_id);

  FidoHidPacket(const FidoHidPacket&) = delete;
  FidoHidPacket& operator=(const FidoHidPacket&) = delete;

  virtual ~FidoHidPacket();

  virtual std::vector<uint8_t> GetSerializedData() const = 0;
  const std::vector<uint8_t>& GetPacketPayload() const { return data_; }
  uint32_t channel_id() const { return channel_id_; }

 protected:
  FidoHidPacket();

  std::vector<uint8_t> data_;
  uint32_t channel_id_ = kHidBroadcastChannel;

 private:
  friend class HidMessage;
};

// FidoHidInitPacket, based on the CTAP specification consists of a header with
// data that is serialized into a IOBuffer. A channel identifier is allocated by
// the CTAP device to ensure its system-wide uniqueness. Command identifiers
// determine the type of message the packet corresponds to. Payload length
// is the length of the entire message payload, and the data is only the portion
// of the payload that will fit into the HidInitPacket.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidInitPacket final
    : public FidoHidPacket {
 public:
  // Creates a packet from the serialized data of an initialization packet. As
  // this is the first packet, the payload length of the entire message will be
  // included within the serialized data. Remaining size will be returned to
  // inform the callee how many additional packets to expect.
  static std::unique_ptr<FidoHidInitPacket> CreateFromSerializedData(
      base::span<const uint8_t> serialized,
      size_t* remaining_size);

  FidoHidInitPacket(uint32_t channel_id,
                    FidoHidDeviceCommand cmd,
                    std::vector<uint8_t> data,
                    uint16_t payload_length);

  FidoHidInitPacket(const FidoHidInitPacket&) = delete;
  FidoHidInitPacket& operator=(const FidoHidInitPacket&) = delete;

  ~FidoHidInitPacket() final;

  std::vector<uint8_t> GetSerializedData() const final;
  FidoHidDeviceCommand command() const { return command_; }
  uint16_t payload_length() const { return payload_length_; }

 private:
  FidoHidDeviceCommand command_;
  uint16_t payload_length_;
};

// FidoHidContinuationPacket, based on the CTAP Specification consists of a
// header with data that is serialized into an IOBuffer. The channel identifier
// will be identical to the identifier in all other packets of the message. The
// packet sequence will be the sequence number of this particular packet, from
// 0x00 to 0x7f.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidContinuationPacket final
    : public FidoHidPacket {
 public:
  // Creates a packet from the serialized data of a continuation packet. As an
  // HidInitPacket would have arrived earlier with the total payload size,
  // the remaining size should be passed to inform the packet of how much data
  // to expect.
  static std::unique_ptr<FidoHidContinuationPacket> CreateFromSerializedData(
      base::span<const uint8_t> serialized,
      size_t* remaining_size);

  FidoHidContinuationPacket(uint32_t channel_id,
                            uint8_t sequence,
                            std::vector<uint8_t> data);

  FidoHidContinuationPacket(const FidoHidContinuationPacket&) = delete;
  FidoHidContinuationPacket& operator=(const FidoHidContinuationPacket&) =
      delete;

  ~FidoHidContinuationPacket() final;

  std::vector<uint8_t> GetSerializedData() const final;
  uint8_t sequence() const { return sequence_; }

 private:
  uint8_t sequence_;
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FIDO_HID_PACKET_H_
