// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_transaction.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/ble/fido_ble_connection.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoBleTransaction::FidoBleTransaction(FidoBleConnection* connection,
                                       uint16_t control_point_length)
    : connection_(connection), control_point_length_(control_point_length) {
  buffer_.reserve(control_point_length_);
}

FidoBleTransaction::~FidoBleTransaction() = default;

void FidoBleTransaction::WriteRequestFrame(FidoBleFrame request_frame,
                                           FrameCallback callback) {
  if (control_point_length_ < 3u) {
    FIDO_LOG(DEBUG) << "Control Point Length is too short: "
                    << control_point_length_;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  DCHECK(!request_frame_ && callback_.is_null());
  request_frame_ = std::move(request_frame);
  callback_ = std::move(callback);

  FidoBleFrameInitializationFragment request_init_fragment;
  std::tie(request_init_fragment, request_cont_fragments_) =
      request_frame_->ToFragments(control_point_length_);
  WriteRequestFragment(request_init_fragment);
}

void FidoBleTransaction::WriteRequestFragment(
    const FidoBleFrameFragment& fragment) {
  buffer_.clear();
  fragment.Serialize(&buffer_);
  DCHECK(!has_pending_request_fragment_write_);
  has_pending_request_fragment_write_ = true;
  FIDO_LOG(DEBUG) << "Writing request fragment: " +
                         base::HexEncode(buffer_.data(), buffer_.size());
  // A weak pointer is required, since this call might time out. If that
  // happens, the current FidoBleTransaction could be destroyed.
  connection_->WriteControlPoint(
      buffer_, base::BindOnce(&FidoBleTransaction::OnRequestFragmentWritten,
                              weak_factory_.GetWeakPtr()));
  // WriteRequestFragment() expects an invocation of OnRequestFragmentWritten()
  // soon after.
  StartTimeout();
}

static void WriteCancel(FidoBleConnection* connection) {
  FIDO_LOG(DEBUG) << "Writing control point 'Cancel'";
  connection->WriteControlPoint(
      {static_cast<uint8_t>(FidoBleDeviceCommand::kCancel), 0, 0},
      base::DoNothing());
}

void FidoBleTransaction::OnRequestFragmentWritten(bool success) {
  DCHECK(has_pending_request_fragment_write_);
  has_pending_request_fragment_write_ = false;
  StopTimeout();
  if (!success) {
    OnError(base::nullopt);
    return;
  }

  if (!request_cont_fragments_.empty()) {
    auto next_request_fragment = std::move(request_cont_fragments_.front());
    request_cont_fragments_.pop();
    WriteRequestFragment(next_request_fragment);
    return;
  }

  if (cancel_pending_) {
    cancel_pending_ = false;
    cancel_sent_ = true;
    WriteCancel(connection_);
  }

  // The transaction wrote the full request frame. It is possible that the full
  // response frame was already received, at which point we process it and run
  // the completim callback.
  if (response_frame_assembler_ && response_frame_assembler_->IsDone()) {
    ProcessResponseFrame();
    return;
  }

  // Otherwise, a response should follow soon after.
  StartTimeout();
}

void FidoBleTransaction::OnResponseFragment(std::vector<uint8_t> data) {
  StopTimeout();
  if (!response_frame_assembler_) {
    FidoBleFrameInitializationFragment fragment;
    if (!FidoBleFrameInitializationFragment::Parse(data, &fragment)) {
      FIDO_LOG(ERROR) << "Malformed Frame Initialization Fragment";
      OnError(base::nullopt);
      return;
    }

    response_frame_assembler_.emplace(fragment);
  } else {
    FidoBleFrameContinuationFragment fragment;
    if (!FidoBleFrameContinuationFragment::Parse(data, &fragment) ||
        !response_frame_assembler_->AddFragment(fragment)) {
      FIDO_LOG(ERROR) << "Malformed Frame Continuation Fragment";
      OnError(base::nullopt);
      return;
    }
  }

  if (!response_frame_assembler_->IsDone()) {
    // Expect the next reponse fragment to arrive soon.
    StartTimeout();
    return;
  }

  // It is possible to receive the last response fragment before the write of
  // the last request fragment has been acknowledged. If this is the case, do
  // not run the completion callback yet.
  // It is OK to process keep alive frames before the request frame is
  // acknowledged.
  if (!has_pending_request_fragment_write_ ||
      response_frame_assembler_->GetFrame()->command() ==
          FidoBleDeviceCommand::kKeepAlive) {
    ProcessResponseFrame();
  }
}

void FidoBleTransaction::Cancel() {
  if (cancel_sent_) {
    return;
  }

  if (has_pending_request_fragment_write_) {
    // A mesasge is still being written. Signal that the cancelation should be
    // written once complete.
    cancel_pending_ = true;
  } else {
    cancel_sent_ = true;
    WriteCancel(connection_);
  }
}

void FidoBleTransaction::ProcessResponseFrame() {
  DCHECK(response_frame_assembler_ && response_frame_assembler_->IsDone());
  auto response_frame = std::move(*response_frame_assembler_->GetFrame());
  response_frame_assembler_.reset();

  DCHECK(request_frame_.has_value());
  if (response_frame.command() == request_frame_->command()) {
    request_frame_.reset();
    std::move(callback_).Run(std::move(response_frame));
    return;
  }

  if (response_frame.command() == FidoBleDeviceCommand::kKeepAlive) {
    if (!response_frame.IsValid()) {
      FIDO_LOG(ERROR) << "Got invalid KeepAlive Command.";
      OnError(base::nullopt);
      return;
    }

    FIDO_LOG(DEBUG) << "CMD_KEEPALIVE: "
                    << static_cast<int>(response_frame.GetKeepaliveCode());
    // Expect another reponse frame soon.
    StartTimeout();
    return;
  }

  if (response_frame.command() == FidoBleDeviceCommand::kError) {
    if (!response_frame.IsValid()) {
      FIDO_LOG(ERROR) << "Got invald Error Command.";
      OnError(base::nullopt);
      return;
    }

    FIDO_LOG(ERROR) << "CMD_ERROR: "
                    << static_cast<int>(response_frame.GetErrorCode());
    OnError(std::move(response_frame));
    return;
  }

  FIDO_LOG(ERROR) << "Got unexpected Command: "
                  << static_cast<int>(response_frame.command());
  OnError(base::nullopt);
}

void FidoBleTransaction::StartTimeout() {
  timer_.Start(FROM_HERE, kDeviceTimeout,
               base::BindOnce(&FidoBleTransaction::OnError,
                              base::Unretained(this), base::nullopt));
}

void FidoBleTransaction::StopTimeout() {
  timer_.Stop();
}

void FidoBleTransaction::OnError(base::Optional<FidoBleFrame> response_frame) {
  request_frame_.reset();
  request_cont_fragments_ = base::queue<FidoBleFrameContinuationFragment>();
  response_frame_assembler_.reset();
  // |callback_| might have been run due to a previous error.
  if (callback_)
    std::move(callback_).Run(std::move(response_frame));
}

}  // namespace device
