// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_experiment_session_plugin.h"

#include "base/strings/string_util.h"
#include "remoting/host/host_attributes.h"
#include "remoting/protocol/jingle_messages.h"

namespace remoting {

std::optional<protocol::Attachment>
HostExperimentSessionPlugin::GetNextMessage() {
  if (attributes_sent_) {
    return std::nullopt;
  }
  attributes_sent_ = true;

  protocol::Attachment attachment;
  protocol::HostAttributesAttachment host_attributes;
  host_attributes.attribute.push_back(GetHostAttributes());
  attachment.host_attributes = std::move(host_attributes);
  return attachment;
}

void HostExperimentSessionPlugin::OnIncomingMessage(
    const protocol::Attachment& attachment) {
  if (configuration_received_ || !attachment.host_config) {
    return;
  }

  configuration_received_ = true;
  for (const auto& [key, value] : attachment.host_config->settings) {
    configuration_.Set(key, value);
  }
}

bool HostExperimentSessionPlugin::configuration_received() const {
  return configuration_received_;
}

const base::DictValue& HostExperimentSessionPlugin::configuration() const {
  return configuration_;
}

}  // namespace remoting
