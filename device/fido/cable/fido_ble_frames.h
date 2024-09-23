// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_BLE_FRAMES_H_
#define DEVICE_FIDO_CABLE_FIDO_BLE_FRAMES_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "device/fido/fido_constants.h"

namespace device {

class FidoBleFrameInitializationFragment;
class FidoBleFrameContinuationFragment;

// Encapsulates a frame, i.e., a single request to or response from a FIDO
// compliant authenticator, designed to be transported via BLE. The frame is
// further split into fragments (see FidoBleFrameFragment class).
//
// The specification of what constitues a frame can be found here:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-bt-protocol-v1.2-ps-20170411.html#h2_framing
//
// TODO(crbug.com/40539129): Consider refactoring U2fMessage to support BLE
// frames.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleFrame {
 public:
  // The values which can be carried in the |data| section of a KEEPALIVE
  // message sent from an authenticator.
  enum class KeepaliveCode : uint8_t {
    // The request is still being processed. The authenticator will be sending
    // this message every |kKeepAliveMillis| milliseconds until completion.
    PROCESSING = 0x01,
    // The authenticator is waiting for the Test of User Presence to complete.
    TUP_NEEDED = 0x02,
  };

  // The types of errors an authenticator can return to the client. Carried in
  // the |data| section of an ERROR command.
  enum class ErrorCode : uint8_t {
    INVALID_CMD = 0x01,  // The command in the request is unknown/invalid.
    INVALID_PAR = 0x02,  // The parameters of the command are invalid/missing.
    INVALID_LEN = 0x03,  // The length of the request is invalid.
    INVALID_SEQ = 0x04,  // The sequence number is invalid.
    REQ_TIMEOUT = 0x05,  // The request timed out.
    NA_1 = 0x06,         // Value reserved (HID).
    NA_2 = 0x0A,         // Value reserved (HID).
    NA_3 = 0x0B,         // Value reserved (HID).
    ENCRYPTION_FAILED = 0x0C,  // Encryption failed for the request.
    OTHER = 0x7F,              // Other, unspecified error.
  };

  FidoBleFrame();
  FidoBleFrame(FidoBleDeviceCommand command, std::vector<uint8_t> data);

  FidoBleFrame(const FidoBleFrame&);
  FidoBleFrame& operator=(const FidoBleFrame&);

  FidoBleFrame(FidoBleFrame&&);
  FidoBleFrame& operator=(FidoBleFrame&&);

  ~FidoBleFrame();

  FidoBleDeviceCommand command() const { return command_; }

  bool IsValid() const;
  KeepaliveCode GetKeepaliveCode() const;
  ErrorCode GetErrorCode() const;

  const std::vector<uint8_t>& data() const { return data_; }
  std::vector<uint8_t>& data() { return data_; }

  // Splits the frame into fragments suitable for sending over BLE. Returns the
  // first fragment via |initial_fragment|, and pushes the remaining ones back
  // to the |other_fragments| vector.
  //
  // The |max_fragment_size| parameter ought to be at least 3. The resulting
  // fragments' binary sizes will not exceed this value.
  std::pair<FidoBleFrameInitializationFragment,
            base::queue<FidoBleFrameContinuationFragment>>
  ToFragments(size_t max_fragment_size) const;

 private:
  FidoBleDeviceCommand command_ = FidoBleDeviceCommand::kMsg;
  std::vector<uint8_t> data_;
};

COMPONENT_EXPORT(DEVICE_FIDO)
bool operator==(const FidoBleFrame& lhs, const FidoBleFrame& rhs);

// A single frame sent over BLE may be split over multiple writes and
// notifications because the technology was not designed for large messages.
// This class represents a single fragment. Not to be used directly.
//
// A frame is divided into an initialization fragment and zero, one or more
// continuation fragments. See the below section of the spec for the details:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-bt-protocol-v1.2-ps-20170411.html#h2_framing-fragmentation
//
// Note: This class and its subclasses don't own the |data|.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleFrameFragment {
 public:
  base::span<const uint8_t> fragment() const { return fragment_; }
  virtual size_t Serialize(std::vector<uint8_t>* buffer) const = 0;

 protected:
  FidoBleFrameFragment();
  explicit FidoBleFrameFragment(base::span<const uint8_t> fragment);
  FidoBleFrameFragment(const FidoBleFrameFragment& frame);
  virtual ~FidoBleFrameFragment();

 private:
  base::raw_span<const uint8_t> fragment_;
};

// An initialization fragment of a frame.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleFrameInitializationFragment
    : public FidoBleFrameFragment {
 public:
  static bool Parse(base::span<const uint8_t> data,
                    FidoBleFrameInitializationFragment* fragment);

  FidoBleFrameInitializationFragment() = default;
  FidoBleFrameInitializationFragment(FidoBleDeviceCommand command,
                                     uint16_t data_length,
                                     base::span<const uint8_t> fragment)
      : FidoBleFrameFragment(fragment),
        command_(command),
        data_length_(data_length) {}

  FidoBleDeviceCommand command() const { return command_; }
  uint16_t data_length() const { return data_length_; }

  size_t Serialize(std::vector<uint8_t>* buffer) const override;

 private:
  FidoBleDeviceCommand command_ = FidoBleDeviceCommand::kMsg;
  uint16_t data_length_ = 0;
};

// A continuation fragment of a frame.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleFrameContinuationFragment
    : public FidoBleFrameFragment {
 public:
  static bool Parse(base::span<const uint8_t> data,
                    FidoBleFrameContinuationFragment* fragment);

  FidoBleFrameContinuationFragment() = default;
  FidoBleFrameContinuationFragment(base::span<const uint8_t> fragment,
                                   uint8_t sequence)
      : FidoBleFrameFragment(fragment), sequence_(sequence) {}

  uint8_t sequence() const { return sequence_; }

  size_t Serialize(std::vector<uint8_t>* buffer) const override;

 private:
  uint8_t sequence_ = 0;
};

// The helper used to construct a FidoBleFrame from a sequence of its fragments.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleFrameAssembler {
 public:
  explicit FidoBleFrameAssembler(
      const FidoBleFrameInitializationFragment& fragment);

  FidoBleFrameAssembler(const FidoBleFrameAssembler&) = delete;
  FidoBleFrameAssembler& operator=(const FidoBleFrameAssembler&) = delete;

  ~FidoBleFrameAssembler();

  bool IsDone() const;

  bool AddFragment(const FidoBleFrameContinuationFragment& fragment);
  FidoBleFrame* GetFrame();

 private:
  uint16_t data_length_ = 0;
  uint8_t sequence_number_ = 0;
  FidoBleFrame frame_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_BLE_FRAMES_H_
