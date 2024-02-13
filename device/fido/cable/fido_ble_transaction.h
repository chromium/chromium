// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_BLE_TRANSACTION_H_
#define DEVICE_FIDO_CABLE_FIDO_BLE_TRANSACTION_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "device/fido/cable/fido_ble_frames.h"

namespace device {

class FidoBleConnection;

// This class encapsulates logic related to a single U2F BLE request and
// response. FidoBleTransaction is owned by FidoBleDevice, which is the only
// class that should make use of this class.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleTransaction {
 public:
  using FrameCallback = base::OnceCallback<void(std::optional<FidoBleFrame>)>;

  FidoBleTransaction(FidoBleConnection* connection,
                     uint16_t control_point_length);

  FidoBleTransaction(const FidoBleTransaction&) = delete;
  FidoBleTransaction& operator=(const FidoBleTransaction&) = delete;

  ~FidoBleTransaction();

  void WriteRequestFrame(FidoBleFrame request_frame, FrameCallback callback);
  void OnResponseFragment(std::vector<uint8_t> data);

  // Cancel requests that a cancelation command be sent if possible.
  void Cancel();

 private:
  void WriteRequestFragment(const FidoBleFrameFragment& fragment);
  void OnRequestFragmentWritten(bool success);
  void ProcessResponseFrame();

  void StartTimeout();
  void StopTimeout();
  void OnError(std::optional<FidoBleFrame> response_frame);

  raw_ptr<FidoBleConnection> connection_;
  uint16_t control_point_length_;

  std::optional<FidoBleFrame> request_frame_;
  FrameCallback callback_;

  base::queue<FidoBleFrameContinuationFragment> request_cont_fragments_;
  std::optional<FidoBleFrameAssembler> response_frame_assembler_;

  std::vector<uint8_t> buffer_;
  base::OneShotTimer timer_;

  bool has_pending_request_fragment_write_ = false;
  // cancel_pending_ is true if a cancelation should be sent after the current
  // set of frames has finished transmitting.
  bool cancel_pending_ = false;
  // cancel_sent_ records whether a cancel message has already been sent.
  bool cancel_sent_ = false;

  base::WeakPtrFactory<FidoBleTransaction> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_BLE_TRANSACTION_H_
