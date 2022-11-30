// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_helpers.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "remoting/base/name_value_map.h"
#include "remoting/host/it2me/it2me_constants.h"

namespace remoting {

namespace {

const NameMapElement<It2MeHostState> kIt2MeHostStates[] = {
    {It2MeHostState::kDisconnected, kHostStateDisconnected},
    {It2MeHostState::kStarting, kHostStateStarting},
    {It2MeHostState::kRequestedAccessCode, kHostStateRequestedAccessCode},
    {It2MeHostState::kReceivedAccessCode, kHostStateReceivedAccessCode},
    {It2MeHostState::kConnecting, kHostStateConnecting},
    {It2MeHostState::kConnected, kHostStateConnected},
    {It2MeHostState::kError, kHostStateError},
    {It2MeHostState::kInvalidDomainError, kHostStateDomainError},
};

}

std::string It2MeHostStateToString(It2MeHostState host_state) {
  return ValueToName(kIt2MeHostStates, host_state);
}

}  // namespace remoting
