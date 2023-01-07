// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_HOST_EXPERIMENT_SENDER_H_
#define REMOTING_CLIENT_HOST_EXPERIMENT_SENDER_H_

#include <memory>
#include <string>

#include "remoting/protocol/session_plugin.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

// A SessionPlugin implementation to send host configuration to the host.
// Currently only WebApp sets the experiment configuration.
// This is a temporary solution until we have more permanent approach
// implemented, which should take host attributes into account.
class HostExperimentSender : public protocol::SessionPlugin {
 public:
  HostExperimentSender(const std::string& experiment_config);

  // protocol::SessionPlugin implementation.
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

  void OnIncomingMessage(const jingle_xmpp::XmlElement& attachments) override;
 private:
  const std::string experiment_config_;
  bool message_sent_ = false;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_HOST_EXPERIMENT_SENDER_H_
