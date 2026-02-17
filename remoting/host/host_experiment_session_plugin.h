// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXPERIMENT_SESSION_PLUGIN_H_
#define REMOTING_HOST_HOST_EXPERIMENT_SESSION_PLUGIN_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "remoting/protocol/session_plugin.h"

namespace remoting {

// A SessionPlugin implementation to send host attributes to client, and
// receives experiment settings.
class HostExperimentSessionPlugin : public protocol::SessionPlugin {
 public:
  using SessionPlugin::SessionPlugin;

  // protocol::SessionPlug implementation.
  std::optional<Attachment> GetNextMessage() override;

  void OnIncomingMessage(const Attachment& attachment) override;

  // Whether we have received configuration from client.
  bool configuration_received() const;

  // The configuration sent from client, may be empty.
  const base::DictValue& configuration() const;

 private:
  bool attributes_sent_ = false;
  bool configuration_received_ = false;
  base::DictValue configuration_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXPERIMENT_SESSION_PLUGIN_H_
