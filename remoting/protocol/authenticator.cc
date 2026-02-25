// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authenticator.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"

namespace remoting::protocol {

Authenticator::RejectionDetails::RejectionDetails() = default;
Authenticator::RejectionDetails::RejectionDetails(RejectionDetails&&) = default;
Authenticator::RejectionDetails::RejectionDetails(const RejectionDetails&) =
    default;

Authenticator::RejectionDetails::RejectionDetails(
    std::string_view message,
    const base::Location& location)
    : message(std::string(message)), location(location) {}

Authenticator::RejectionDetails::~RejectionDetails() = default;

Authenticator::RejectionDetails& Authenticator::RejectionDetails::operator=(
    RejectionDetails&&) = default;
Authenticator::RejectionDetails& Authenticator::RejectionDetails::operator=(
    const RejectionDetails&) = default;

Authenticator::Authenticator() = default;
Authenticator::~Authenticator() = default;

void Authenticator::NotifyStateChangeAfterAccepted() {
  if (on_state_change_after_accepted_) {
    on_state_change_after_accepted_.Run();
  } else {
    LOG(WARNING)
        << "State change notification ignored because callback is not set.";
  }
}

void Authenticator::ChainStateChangeAfterAcceptedWithUnderlying(
    Authenticator& underlying) {
  underlying.set_state_change_after_accepted_callback(base::BindRepeating(
      &Authenticator::NotifyStateChangeAfterAccepted, base::Unretained(this)));
}

}  // namespace remoting::protocol
