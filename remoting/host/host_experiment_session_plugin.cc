// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_experiment_session_plugin.h"

#include "remoting/base/constants.h"
#include "remoting/host/host_attributes.h"

namespace remoting {

using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

std::unique_ptr<XmlElement> HostExperimentSessionPlugin::GetNextMessage() {
  if (attributes_sent_) {
    return nullptr;
  }
  attributes_sent_ = true;
  std::unique_ptr<XmlElement> attributes(
      new XmlElement(QName(kChromotingXmlNamespace, "host-attributes")));
  attributes->SetBodyText(GetHostAttributes());
  return attributes;
}

void HostExperimentSessionPlugin::OnIncomingMessage(
    const XmlElement& attachments) {
  if (configuration_received_) {
    return;
  }

  const XmlElement* configuration = attachments.FirstNamed(
      QName(kChromotingXmlNamespace, "host-configuration"));
  if (!configuration) {
    return;
  }

  configuration_received_ = true;
  configuration_ = configuration->BodyText();
}

bool HostExperimentSessionPlugin::configuration_received() const {
  return configuration_received_;
}

const std::string& HostExperimentSessionPlugin::configuration() const {
  return configuration_;
}

}  // namespace remoting
