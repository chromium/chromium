// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TERMINAL_DATA_CHANNEL_MANAGER_H_
#define REMOTING_HOST_TERMINAL_DATA_CHANNEL_MANAGER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/host/terminal_error.h"
#include "remoting/proto/terminal.pb.h"

namespace remoting {

class CompoundBuffer;
class TerminalSessionManager;

namespace protocol {
class MessagePipe;
}  // namespace protocol

// Owns the terminal sessions for a single client connection, and the
// per-terminal data channels (named with kChannelPrefix) used to communicate
// with the client about them.
class TerminalDataChannelManager {
 public:
  // Prefix of the names of terminal data channels.
  static constexpr char kChannelPrefix[] = "terminal-";

  // Creates an outgoing data channel with the given name. May return nullptr
  // if the channel cannot be created.
  using CreateOutgoingChannelCallback =
      base::RepeatingCallback<std::unique_ptr<protocol::MessagePipe>(
          const std::string& name)>;

  explicit TerminalDataChannelManager(
      CreateOutgoingChannelCallback create_outgoing_channel);
  TerminalDataChannelManager(const TerminalDataChannelManager&) = delete;
  TerminalDataChannelManager& operator=(const TerminalDataChannelManager&) =
      delete;
  ~TerminalDataChannelManager();

  // Restores any persistent terminals, opening an outgoing data channel for
  // each one. Must be called once, before any other method.
  void Start();

  // Called when the client opens a terminal data channel. Creates a new
  // terminal session for the channel.
  void OnIncomingChannel(const std::string& channel_name,
                         std::unique_ptr<protocol::MessagePipe> pipe);

  // Forwards `agent_status` to the client on the data channel for
  // `terminal_id`. Ignored if the terminal is unknown, or if `agent_status`
  // contains no valid fields.
  void SetAgentStatus(int32_t terminal_id,
                      const protocol::TerminalControl::AgentInfo& agent_status);

  TerminalSessionManager* terminal_session_manager_for_tests() const {
    return terminal_session_manager_.get();
  }

  base::WeakPtr<TerminalDataChannelManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  class TerminalDataChannel;

  // Sends the response to a create-terminal request on `channel` and, on
  // success, associates `channel` with the new terminal.
  void OnTerminalCreated(std::unique_ptr<TerminalDataChannel> channel,
                         base::expected<int32_t, TerminalError> result);

  // Called when a persistent terminal has been restored. Opens a data channel
  // to the client for it.
  void OnTerminalRestored(int32_t terminal_id);

  // Associates `channel` with `terminal_id` and sends the create-terminal
  // response for it.
  void AttachChannel(int32_t terminal_id,
                     std::unique_ptr<TerminalDataChannel> channel);

  void OnChannelMessageReceived(int32_t terminal_id,
                                std::unique_ptr<CompoundBuffer> message);
  void OnChannelClosed(int32_t terminal_id);

  // Sends `message` on the data channel for `terminal_id`, if there is one.
  void SendToTerminal(int32_t terminal_id,
                      const protocol::TerminalControl& message);

  void SendTerminalOutput(int32_t terminal_id, const std::string& data);
  void OnTerminalExited(int32_t terminal_id);
  void SendTerminalProcessInfo(int32_t terminal_id,
                               bool is_active,
                               std::string_view process_name);

  SEQUENCE_CHECKER(sequence_checker_);

  CreateOutgoingChannelCallback create_outgoing_channel_;

  // Declared before `channels_` so that the channels are destroyed first.
  std::unique_ptr<TerminalSessionManager> terminal_session_manager_;
  std::map<int32_t, std::unique_ptr<TerminalDataChannel>> channels_;

  base::WeakPtrFactory<TerminalDataChannelManager> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_DATA_CHANNEL_MANAGER_H_
