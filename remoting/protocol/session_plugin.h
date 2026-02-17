// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_PLUGIN_H_
#define REMOTING_PROTOCOL_SESSION_PLUGIN_H_

#include <memory>
#include <optional>

namespace remoting {
struct Attachment;
}  // namespace remoting

namespace remoting::protocol {

// Interface for Session plugins. Plugins allow to send and receive optional
// information that is not essential for session handshake. Messages generated
// by the plugins on one end of a connection are attached to the session
// handshake messages and passed to the plugins on the other end. Plugins are
// optional, i.e. Session doesn't need any plugins to connect successfully.
class SessionPlugin {
 public:
  SessionPlugin() = default;
  virtual ~SessionPlugin() = default;

  // Returns an Attachment if the SessionPlugin has something to attach to the
  // outgoing message.
  // TODO: crbug.com/359620500 - Use a specific attachment type for plugins.
  virtual std::optional<Attachment> GetNextMessage() = 0;

  // Handles messages in |attachment|.
  virtual void OnIncomingMessage(const Attachment& attachment) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_SESSION_PLUGIN_H_
