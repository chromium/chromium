// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/host_experiment_sender.h"

#include "remoting/base/constants.h"

namespace remoting {

HostExperimentSender::HostExperimentSender(const std::string& experiment_config)
    : experiment_config_(experiment_config) {}

std::unique_ptr<jingle_xmpp::XmlElement> HostExperimentSender::GetNextMessage() {
  if (message_sent_ || experiment_config_.empty()) {
    return nullptr;
  }
  message_sent_ = true;
  std::unique_ptr<jingle_xmpp::XmlElement> configuration(new jingle_xmpp::XmlElement(
      jingle_xmpp::QName(kChromotingXmlNamespace, "host-configuration")));
  configuration->SetBodyText(experiment_config_);
  return configuration;
}

void HostExperimentSender::OnIncomingMessage(
    const jingle_xmpp::XmlElement& attachments) {}

}  // namespace remoting
