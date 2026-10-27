// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_data_channel_manager.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/host/terminal_session_manager.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {

namespace {

// The maximum number of messages to queue for a terminal data channel while
// waiting for its terminal to be created. Further messages are dropped.
constexpr size_t kMaxPendingIncomingMessages = 50;

protocol::TerminalControl::CreateTerminalResponse::Error::Reason ToProtoReason(
    TerminalError::Reason reason) {
  using ProtoError = protocol::TerminalControl::CreateTerminalResponse::Error;
  switch (reason) {
    case TerminalError::Reason::kInternalError:
      return ProtoError::INTERNAL_ERROR;
    case TerminalError::Reason::kBusy:
      return ProtoError::BUSY;
    case TerminalError::Reason::kPtyError:
      return ProtoError::PTY_ERROR;
    case TerminalError::Reason::kTmuxMissing:
      return ProtoError::TMUX_MISSING;
    case TerminalError::Reason::kLaunchFailed:
      return ProtoError::LAUNCH_FAILED;
  }
  NOTREACHED();
}

void SetCreateTerminalError(
    const TerminalError& error,
    protocol::TerminalControl::CreateTerminalResponse::Error* proto_error) {
  proto_error->set_reason(ToProtoReason(error.reason));
  if (error.system_error_code.has_value()) {
    proto_error->set_code(*error.system_error_code);
  }
  // A default-constructed base::Location has no file name (and an invalid line
  // number), so only report the location if it is valid.
  if (const char* file_name = error.location.file_name()) {
    proto_error->set_file_name(file_name);
    proto_error->set_line_number(error.location.line_number());
  }
  proto_error->set_error_message(error.message);
}

}  // namespace

// Wraps the MessagePipe for a single terminal data channel. Messages sent
// before the pipe has opened are queued and sent once it does.
class TerminalDataChannelManager::TerminalDataChannel
    : public protocol::MessagePipe::EventHandler {
 public:
  using MessageCallback =
      base::RepeatingCallback<void(std::unique_ptr<CompoundBuffer>)>;

  explicit TerminalDataChannel(std::unique_ptr<protocol::MessagePipe> pipe)
      : pipe_(std::move(pipe)) {
    pipe_->Start(this);
  }
  TerminalDataChannel(const TerminalDataChannel&) = delete;
  TerminalDataChannel& operator=(const TerminalDataChannel&) = delete;
  ~TerminalDataChannel() override = default;

  // Sends `message` on `channel`, which then deletes itself once the message
  // has been sent. If the pipe closes before the message can be sent, the
  // message is dropped and `channel` deletes itself then instead.
  static void SendAndClose(std::unique_ptr<TerminalDataChannel> channel,
                           const protocol::TerminalControl& message) {
    TerminalDataChannel* self = channel.release();
    self->self_owned_ = true;
    if (self->is_closed_) {
      self->DeleteSelfSoon();
      return;
    }
    self->Send(message, base::BindOnce(&TerminalDataChannel::DeleteSelfSoon,
                                       self->weak_factory_.GetWeakPtr()));
  }

  // Sets the callbacks to run when a message is received or the pipe closes.
  // Messages received before this is called (up to kMaxPendingIncomingMessages)
  // are queued and passed to `on_message` now, in the order they were received.
  // If the pipe has already closed, `on_closed` is run immediately, after any
  // queued messages.
  void Attach(MessageCallback on_message, base::OnceClosure on_closed) {
    on_message_ = std::move(on_message);
    for (auto& message : std::exchange(pending_incoming_messages_, {})) {
      on_message_.Run(std::move(message));
    }
    if (is_closed_) {
      std::move(on_closed).Run();
    } else {
      on_closed_ = std::move(on_closed);
    }
  }

  void Send(const protocol::TerminalControl& message,
            base::OnceClosure done = {}) {
    if (is_open_) {
      pipe_->Send(const_cast<protocol::TerminalControl*>(&message),
                  std::move(done));
    } else {
      pending_outgoing_messages_.emplace_back(message, std::move(done));
    }
  }

  bool is_closed() const { return is_closed_; }

  // protocol::MessagePipe::EventHandler
  void OnMessagePipeOpen() override {
    is_open_ = true;
    for (auto& [message, done] :
         std::exchange(pending_outgoing_messages_, {})) {
      pipe_->Send(&message, std::move(done));
    }
  }

  void OnMessageReceived(std::unique_ptr<CompoundBuffer> message) override {
    if (on_message_) {
      on_message_.Run(std::move(message));
    } else if (pending_incoming_messages_.size() <
               kMaxPendingIncomingMessages) {
      // The terminal may still be starting. Hold on to the message until
      // Attach() is called, so that early input or resizes are not lost.
      pending_incoming_messages_.push_back(std::move(message));
    } else {
      LOG(WARNING) << "Dropping terminal message received before the terminal "
                      "was created: too many pending messages.";
    }
  }

  void OnMessagePipeClosed() override {
    is_open_ = false;
    is_closed_ = true;
    pending_outgoing_messages_.clear();
    if (self_owned_) {
      DeleteSelfSoon();
    } else if (on_closed_) {
      std::move(on_closed_).Run();
    }
  }

 private:
  void DeleteSelfSoon() {
    if (deletion_pending_) {
      return;
    }
    deletion_pending_ = true;
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
  }

  std::unique_ptr<protocol::MessagePipe> pipe_;
  MessageCallback on_message_;
  base::OnceClosure on_closed_;
  bool is_open_ = false;
  bool is_closed_ = false;
  bool self_owned_ = false;
  bool deletion_pending_ = false;
  // Messages passed to Send() before the pipe opened, with their callbacks.
  std::vector<std::pair<protocol::TerminalControl, base::OnceClosure>>
      pending_outgoing_messages_;
  // Messages received before Attach() was called, up to
  // kMaxPendingIncomingMessages.
  std::vector<std::unique_ptr<CompoundBuffer>> pending_incoming_messages_;
  base::WeakPtrFactory<TerminalDataChannel> weak_factory_{this};
};

TerminalDataChannelManager::TerminalDataChannelManager(
    CreateOutgoingChannelCallback create_outgoing_channel)
    : create_outgoing_channel_(std::move(create_outgoing_channel)) {}

TerminalDataChannelManager::~TerminalDataChannelManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TerminalDataChannelManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!terminal_session_manager_) << "Start() should only be called once.";
  terminal_session_manager_ = std::make_unique<TerminalSessionManager>();
  terminal_session_manager_->Start(
      base::BindRepeating(&TerminalDataChannelManager::SendTerminalOutput,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&TerminalDataChannelManager::OnTerminalExited,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&TerminalDataChannelManager::SendTerminalProcessInfo,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&TerminalDataChannelManager::OnTerminalRestored,
                          weak_factory_.GetWeakPtr()));
}

void TerminalDataChannelManager::OnIncomingChannel(
    const std::string& channel_name,
    std::unique_ptr<protocol::MessagePipe> pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(terminal_session_manager_);

  // The terminal is created immediately; the response is queued until the
  // pipe opens. `channel` is owned by the callback until the terminal has been
  // created.
  auto channel = std::make_unique<TerminalDataChannel>(std::move(pipe));
  terminal_session_manager_->CreateTerminal(
      base::BindOnce(&TerminalDataChannelManager::OnTerminalCreated,
                     weak_factory_.GetWeakPtr(), std::move(channel)));
}

void TerminalDataChannelManager::SetAgentStatus(
    int32_t terminal_id,
    const protocol::TerminalControl::AgentInfo& agent_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(terminal_session_manager_);

  bool has_valid_agent_state =
      agent_status.has_agent_state() &&
      agent_status.agent_state() !=
          protocol::TerminalControl::AgentInfo::UNSPECIFIED;
  if (!has_valid_agent_state && !agent_status.has_conversation_title()) {
    return;
  }

  if (!terminal_session_manager_->GetTerminalSession(terminal_id)) {
    LOG(WARNING) << "Dropping SetAgentStatus for unknown terminal ID: "
                 << terminal_id;
    return;
  }

  protocol::TerminalControl message;
  auto* agent_info = message.mutable_agent_info();
  if (has_valid_agent_state) {
    agent_info->set_agent_state(agent_status.agent_state());
  }
  if (agent_status.has_conversation_title()) {
    agent_info->set_conversation_title(agent_status.conversation_title());
  }
  SendToTerminal(terminal_id, message);
}

void TerminalDataChannelManager::OnTerminalCreated(
    std::unique_ptr<TerminalDataChannel> channel,
    base::expected<int32_t, TerminalError> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    protocol::TerminalControl response;
    SetCreateTerminalError(result.error(),
                           response.mutable_create_response()->mutable_error());
    // The pipe may not have opened yet, so let the channel outlive this call
    // until the error has been sent.
    TerminalDataChannel::SendAndClose(std::move(channel), response);
    return;
  }

  int32_t terminal_id = *result;
  if (channel->is_closed()) {
    // The client closed the channel before learning the terminal ID. Leave the
    // terminal running; it will be offered to the client as a restored
    // terminal when it next connects.
    return;
  }
  AttachChannel(terminal_id, std::move(channel));
}

void TerminalDataChannelManager::OnTerminalRestored(int32_t terminal_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string channel_name =
      base::StrCat({kChannelPrefix, base::NumberToString(terminal_id)});
  std::unique_ptr<protocol::MessagePipe> pipe =
      create_outgoing_channel_.Run(channel_name);
  if (!pipe) {
    return;
  }
  AttachChannel(terminal_id,
                std::make_unique<TerminalDataChannel>(std::move(pipe)));
}

void TerminalDataChannelManager::AttachChannel(
    int32_t terminal_id,
    std::unique_ptr<TerminalDataChannel> channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TerminalDataChannel* channel_ptr = channel.get();
  // Send the response before attaching, so that it precedes any messages
  // generated by handling queued input.
  protocol::TerminalControl response;
  response.mutable_create_response()->set_terminal_id(terminal_id);
  channel_ptr->Send(response);
  // Add the channel to `channels_` before attaching, since Attach() may run
  // OnChannelMessageReceived() or OnChannelClosed() synchronously.
  channels_[terminal_id] = std::move(channel);
  channel_ptr->Attach(
      base::BindRepeating(&TerminalDataChannelManager::OnChannelMessageReceived,
                          weak_factory_.GetWeakPtr(), terminal_id),
      base::BindOnce(&TerminalDataChannelManager::OnChannelClosed,
                     weak_factory_.GetWeakPtr(), terminal_id));
}

void TerminalDataChannelManager::OnChannelMessageReceived(
    int32_t terminal_id,
    std::unique_ptr<CompoundBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<protocol::TerminalControl> control =
      protocol::ParseMessage<protocol::TerminalControl>(message.get());
  if (!control) {
    return;
  }

  if (control->has_terminal_input()) {
    terminal_session_manager_->WriteTerminal(terminal_id,
                                             control->terminal_input().input());
  } else if (control->has_resize_terminal()) {
    terminal_session_manager_->ResizeTerminal(
        terminal_id, control->resize_terminal().width(),
        control->resize_terminal().height());
  } else if (control->has_remove_request()) {
    terminal_session_manager_->CloseTerminal(terminal_id);
  }
}

void TerminalDataChannelManager::OnChannelClosed(int32_t terminal_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = channels_.find(terminal_id);
  if (it == channels_.end()) {
    return;
  }
  // This is called from the channel's own event handler, so it must not be
  // deleted synchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(it->second));
  channels_.erase(it);
}

void TerminalDataChannelManager::SendToTerminal(
    int32_t terminal_id,
    const protocol::TerminalControl& message) {
  auto it = channels_.find(terminal_id);
  if (it != channels_.end()) {
    it->second->Send(message);
  }
}

void TerminalDataChannelManager::SendTerminalOutput(int32_t terminal_id,
                                                    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol::TerminalControl message;
  message.mutable_terminal_output()->set_output(data);
  SendToTerminal(terminal_id, message);
}

void TerminalDataChannelManager::OnTerminalExited(int32_t terminal_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol::TerminalControl message;
  message.mutable_close_terminal();
  SendToTerminal(terminal_id, message);
}

void TerminalDataChannelManager::SendTerminalProcessInfo(
    int32_t terminal_id,
    bool is_active,
    std::string_view process_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  protocol::TerminalControl message;
  auto* process_info = message.mutable_process_info();
  process_info->set_process_name(std::string(process_name));
  process_info->set_is_active(is_active);
  SendToTerminal(terminal_id, message);
}

}  // namespace remoting
