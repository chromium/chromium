// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/fake_ftl_signal_strategy.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"

namespace remoting {

FakeFtlSignalStrategy::FakeFtlSignalStrategy(const SignalingAddress& address)
    : address_(address) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeFtlSignalStrategy::~FakeFtlSignalStrategy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeFtlSignalStrategy::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(CONNECTED);
}

void FakeFtlSignalStrategy::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(DISCONNECTED);
}

SignalStrategy::State FakeFtlSignalStrategy::GetState() const {
  return state_;
}

SignalStrategy::Error FakeFtlSignalStrategy::GetError() const {
  return error_;
}

const SignalingAddress& FakeFtlSignalStrategy::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return address_;
}

void FakeFtlSignalStrategy::AddListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.AddObserver(listener);
}

void FakeFtlSignalStrategy::RemoveListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

bool FakeFtlSignalStrategy::SendMessage(JingleMessage&& message) {
  return Send(std::move(message));
}

bool FakeFtlSignalStrategy::SendReply(JingleMessageReply&& message) {
  return Send(std::move(message));
}

template <typename T>
bool FakeFtlSignalStrategy::Send(T&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

std::string FakeFtlSignalStrategy::GetNextId() {
  ++last_id_;
  return base::NumberToString(last_id_);
}

bool FakeFtlSignalStrategy::IsSignInError() const {
  return false;
}

bool FakeFtlSignalStrategy::SendFtlMessage(
    const SignalingAddress& destination_address,
    ftl::ChromotingMessage&& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void FakeFtlSignalStrategy::AddFtlListener(FtlListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_listeners_.AddObserver(listener);
}

void FakeFtlSignalStrategy::RemoveFtlListener(FtlListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ftl_listeners_.RemoveObserver(listener);
}

void FakeFtlSignalStrategy::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state == state_) {
    return;
  }
  state_ = state;
  for (auto& observer : listeners_) {
    observer.OnSignalingStateChanged(state_);
  }
}

}  // namespace remoting
