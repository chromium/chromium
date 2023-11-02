// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXPERIMENT_SESSION_PLUGIN_H_
#define REMOTING_HOST_HOST_EXPERIMENT_SESSION_PLUGIN_H_

#include <memory>
#include <string>

#include "remoting/protocol/session_plugin.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

// A SessionPlugin implementation to send host attributes to client, and
// receives experiment settings.
class HostExperimentSessionPlugin : public protocol::SessionPlugin {
 public:
  using SessionPlugin::SessionPlugin;

  // protocol::SessionPlug implementation.
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;

  void OnIncomingMessage(const jingle_xmpp::XmlElement& attachments) override;

  // Whether we have received configuration from client.
  bool configuration_received() const;

  // The configuration sent from client, may be empty.
  const std::string& configuration() const;

 private:
  bool attributes_sent_ = false;
  bool configuration_received_ = false;
  std::string configuration_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXPERIMENT_SESSION_PLUGIN_H_
