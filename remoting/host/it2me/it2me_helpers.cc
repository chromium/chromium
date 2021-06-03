// Copyright 2021 The Chromium Authors. All rights reserved.
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

bool ParseIt2MeNativeMessageJson(const std::string& message,
                                 std::string& message_type,
                                 base::Value& dictionary_value) {
  auto opt_message = base::JSONReader::Read(message);
  if (!opt_message.has_value()) {
    LOG(ERROR) << "Received a message that's not valid JSON.";
    return false;
  }

  auto message_value = std::move(opt_message.value());
  if (!message_value.is_dict()) {
    LOG(ERROR) << "Received a message that's not a dictionary.";
    return false;
  }

  const std::string* message_type_value =
      message_value.FindStringPath(kMessageType);
  if (message_type_value) {
    message_type = *message_type_value;
  }

  dictionary_value = std::move(message_value);

  return true;
}

std::string It2MeHostStateToString(It2MeHostState host_state) {
  return ValueToName(kIt2MeHostStates, host_state);
}

}  // namespace remoting
