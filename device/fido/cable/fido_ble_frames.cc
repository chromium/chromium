// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_ble_frames.h"

#include <algorithm>
#include <limits>
#include <tuple>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

FidoBleFrame::FidoBleFrame() = default;

FidoBleFrame::FidoBleFrame(FidoBleDeviceCommand command,
                           std::vector<uint8_t> data)
    : command_(command), data_(std::move(data)) {}

FidoBleFrame::FidoBleFrame(const FidoBleFrame&) = default;
FidoBleFrame& FidoBleFrame::operator=(const FidoBleFrame&) = default;

FidoBleFrame::FidoBleFrame(FidoBleFrame&&) = default;
FidoBleFrame& FidoBleFrame::operator=(FidoBleFrame&&) = default;

FidoBleFrame::~FidoBleFrame() = default;

bool FidoBleFrame::IsValid() const {
  switch (command_) {
    case FidoBleDeviceCommand::kPing:
    case FidoBleDeviceCommand::kMsg:
    case FidoBleDeviceCommand::kCancel:
    case FidoBleDeviceCommand::kControl:
      return true;
    case FidoBleDeviceCommand::kKeepAlive:
    case FidoBleDeviceCommand::kError:
      return data_.size() == 1;
  }
  NOTREACHED();
  return false;
}

FidoBleFrame::KeepaliveCode FidoBleFrame::GetKeepaliveCode() const {
  DCHECK_EQ(command_, FidoBleDeviceCommand::kKeepAlive);
  DCHECK_EQ(data_.size(), 1u);
  return static_cast<KeepaliveCode>(data_[0]);
}

FidoBleFrame::ErrorCode FidoBleFrame::GetErrorCode() const {
  DCHECK_EQ(command_, FidoBleDeviceCommand::kError);
  DCHECK_EQ(data_.size(), 1u);
  return static_cast<ErrorCode>(data_[0]);
}

std::pair<FidoBleFrameInitializationFragment,
          base::queue<FidoBleFrameContinuationFragment>>
FidoBleFrame::ToFragments(size_t max_fragment_size) const {
  DCHECK_LE(data_.size(), std::numeric_limits<uint16_t>::max());
  DCHECK_GE(max_fragment_size, 3u);

  // Cast is necessary to ignore too high bits.
  auto data_view =
      base::make_span(data_.data(), static_cast<uint16_t>(data_.size()));

  // Subtract 3 to account for CMD, HLEN and LLEN bytes.
  const size_t init_fragment_size =
      std::min(max_fragment_size - 3, data_view.size());

  FidoBleFrameInitializationFragment initial_fragment(
      command_, data_view.size(), data_view.first(init_fragment_size));

  base::queue<FidoBleFrameContinuationFragment> other_fragments;
  data_view = data_view.subspan(init_fragment_size);

  // Subtract 1 to account for SEQ byte.
  for (auto cont_data :
       fido_parsing_utils::SplitSpan(data_view, max_fragment_size - 1)) {
    // High bit must stay cleared.
    other_fragments.emplace(cont_data, other_fragments.size() & 0x7F);
  }

  return {initial_fragment, std::move(other_fragments)};
}

bool operator==(const FidoBleFrame& lhs, const FidoBleFrame& rhs) {
  return std::forward_as_tuple(lhs.command(), lhs.data()) ==
         std::forward_as_tuple(rhs.command(), rhs.data());
}

FidoBleFrameFragment::FidoBleFrameFragment() = default;

FidoBleFrameFragment::FidoBleFrameFragment(const FidoBleFrameFragment& frame) =
    default;
FidoBleFrameFragment::~FidoBleFrameFragment() = default;

FidoBleFrameFragment::FidoBleFrameFragment(base::span<const uint8_t> fragment)
    : fragment_(fragment) {}

bool FidoBleFrameInitializationFragment::Parse(
    base::span<const uint8_t> data,
    FidoBleFrameInitializationFragment* fragment) {
  if (data.size() < 3)
    return false;

  const auto command = static_cast<FidoBleDeviceCommand>(data[0]);
  const uint16_t data_length = (static_cast<uint16_t>(data[1]) << 8) + data[2];
  if (static_cast<size_t>(data_length) + 3 < data.size())
    return false;

  *fragment =
      FidoBleFrameInitializationFragment(command, data_length, data.subspan(3));
  return true;
}

size_t FidoBleFrameInitializationFragment::Serialize(
    std::vector<uint8_t>* buffer) const {
  buffer->push_back(static_cast<uint8_t>(command_));
  buffer->push_back((data_length_ >> 8) & 0xFF);
  buffer->push_back(data_length_ & 0xFF);
  buffer->insert(buffer->end(), fragment().begin(), fragment().end());
  return fragment().size() + 3;
}

bool FidoBleFrameContinuationFragment::Parse(
    base::span<const uint8_t> data,
    FidoBleFrameContinuationFragment* fragment) {
  if (data.empty())
    return false;
  const uint8_t sequence = data[0];
  *fragment = FidoBleFrameContinuationFragment(data.subspan(1), sequence);
  return true;
}

size_t FidoBleFrameContinuationFragment::Serialize(
    std::vector<uint8_t>* buffer) const {
  buffer->push_back(sequence_);
  buffer->insert(buffer->end(), fragment().begin(), fragment().end());
  return fragment().size() + 1;
}

FidoBleFrameAssembler::FidoBleFrameAssembler(
    const FidoBleFrameInitializationFragment& fragment)
    : data_length_(fragment.data_length()),
      frame_(fragment.command(),
             std::vector<uint8_t>(fragment.fragment().begin(),
                                  fragment.fragment().end())) {}

bool FidoBleFrameAssembler::AddFragment(
    const FidoBleFrameContinuationFragment& fragment) {
  if (fragment.sequence() != sequence_number_)
    return false;
  sequence_number_ = (sequence_number_ + 1) & 0x7F;

  if (static_cast<size_t>(data_length_) <
      frame_.data().size() + fragment.fragment().size()) {
    return false;
  }

  frame_.data().insert(frame_.data().end(), fragment.fragment().begin(),
                       fragment.fragment().end());
  return true;
}

bool FidoBleFrameAssembler::IsDone() const {
  return frame_.data().size() == data_length_;
}

FidoBleFrame* FidoBleFrameAssembler::GetFrame() {
  return IsDone() ? &frame_ : nullptr;
}

FidoBleFrameAssembler::~FidoBleFrameAssembler() = default;

}  // namespace device
