// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi/midi_message_filter.h"

#include <algorithm>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/media/midi_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"

using base::AutoLock;
using blink::WebString;
using midi::mojom::PortState;

// The maximum number of bytes which we're allowed to send to the browser
// before getting acknowledgement back from the browser that they've been
// successfully sent.
static const size_t kMaxUnacknowledgedBytesSent = 10 * 1024 * 1024;  // 10 MB.

namespace content {

MidiMessageFilter::MidiMessageFilter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : sender_(nullptr),
      io_task_runner_(io_task_runner),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      session_result_(midi::mojom::Result::NOT_INITIALIZED),
      unacknowledged_bytes_sent_(0u) {}

MidiMessageFilter::~MidiMessageFilter() {}

void MidiMessageFilter::AddClient(blink::WebMIDIAccessorClient* client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("midi", "MidiMessageFilter::AddClient");
  clients_waiting_session_queue_.push_back(client);
  if (session_result_ != midi::mojom::Result::NOT_INITIALIZED) {
    HandleClientAdded(session_result_);
  } else if (clients_waiting_session_queue_.size() == 1u) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MidiMessageFilter::StartSessionOnIOThread, this));
  }
}

void MidiMessageFilter::RemoveClient(blink::WebMIDIAccessorClient* client) {
  DCHECK(clients_.find(client) != clients_.end() ||
         base::ContainsValue(clients_waiting_session_queue_, client))
      << "RemoveClient call was not ballanced with AddClient call";
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  clients_.erase(client);
  auto it = std::find(clients_waiting_session_queue_.begin(),
                      clients_waiting_session_queue_.end(), client);
  if (it != clients_waiting_session_queue_.end())
    clients_waiting_session_queue_.erase(it);
  if (clients_.empty() && clients_waiting_session_queue_.empty()) {
    session_result_ = midi::mojom::Result::NOT_INITIALIZED;
    inputs_.clear();
    outputs_.clear();
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MidiMessageFilter::EndSessionOnIOThread, this));
  }
}

void MidiMessageFilter::SendMidiData(uint32_t port,
                                     const uint8_t* data,
                                     size_t length,
                                     base::TimeTicks timestamp) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if ((kMaxUnacknowledgedBytesSent - unacknowledged_bytes_sent_) < length) {
    // TODO(toyoshim): buffer up the data to send at a later time.
    // For now we're just dropping these bytes on the floor.
    return;
  }

  unacknowledged_bytes_sent_ += length;
  std::vector<uint8_t> v(data, data + length);
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MidiMessageFilter::SendMidiDataOnIOThread,
                                this, port, v, timestamp));
}

void MidiMessageFilter::StartSessionOnIOThread() {
  TRACE_EVENT0("midi", "MidiMessageFilter::StartSessionOnIOThread");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new MidiHostMsg_StartSession());
}

void MidiMessageFilter::SendMidiDataOnIOThread(uint32_t port,
                                               const std::vector<uint8_t>& data,
                                               base::TimeTicks timestamp) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new MidiHostMsg_SendData(port, data, timestamp));
}

void MidiMessageFilter::EndSessionOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  Send(new MidiHostMsg_EndSession());
}

void MidiMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!sender_) {
    delete message;
  } else {
    sender_->Send(message);
  }
}

bool MidiMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MidiMessageFilter, message)
    IPC_MESSAGE_HANDLER(MidiMsg_SessionStarted, OnSessionStarted)
    IPC_MESSAGE_HANDLER(MidiMsg_AddInputPort, OnAddInputPort)
    IPC_MESSAGE_HANDLER(MidiMsg_AddOutputPort, OnAddOutputPort)
    IPC_MESSAGE_HANDLER(MidiMsg_SetInputPortState, OnSetInputPortState)
    IPC_MESSAGE_HANDLER(MidiMsg_SetOutputPortState, OnSetOutputPortState)
    IPC_MESSAGE_HANDLER(MidiMsg_DataReceived, OnDataReceived)
    IPC_MESSAGE_HANDLER(MidiMsg_AcknowledgeSentData, OnAcknowledgeSentData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MidiMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = channel;
}

void MidiMessageFilter::OnFilterRemoved() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Once removed, a filter will not be used again.  At this time all
  // delegates must be notified so they release their reference.
  OnChannelClosing();
}

void MidiMessageFilter::OnChannelClosing() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = nullptr;
}

void MidiMessageFilter::OnSessionStarted(midi::mojom::Result result) {
  TRACE_EVENT0("midi", "MidiMessageFilter::OnSessionStarted");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Handle on the main JS thread.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MidiMessageFilter::HandleClientAdded, this, result));
}

void MidiMessageFilter::OnAddInputPort(midi::mojom::PortInfo info) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MidiMessageFilter::HandleAddInputPort, this, info));
}

void MidiMessageFilter::OnAddOutputPort(midi::mojom::PortInfo info) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MidiMessageFilter::HandleAddOutputPort, this, info));
}

void MidiMessageFilter::OnSetInputPortState(uint32_t port, PortState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MidiMessageFilter::HandleSetInputPortState,
                                this, port, state));
}

void MidiMessageFilter::OnSetOutputPortState(uint32_t port, PortState state) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MidiMessageFilter::HandleSetOutputPortState,
                                this, port, state));
}

void MidiMessageFilter::OnDataReceived(uint32_t port,
                                       const std::vector<uint8_t>& data,
                                       base::TimeTicks timestamp) {
  TRACE_EVENT0("midi", "MidiMessageFilter::OnDataReceived");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Handle on the main JS thread.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MidiMessageFilter::HandleDataReceived, this,
                                port, data, timestamp));
}

void MidiMessageFilter::OnAcknowledgeSentData(size_t bytes_sent) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MidiMessageFilter::HandleAckknowledgeSentData,
                                this, bytes_sent));
}

void MidiMessageFilter::HandleClientAdded(midi::mojom::Result result) {
  TRACE_EVENT0("midi", "MidiMessageFilter::HandleClientAdded");
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  session_result_ = result;

  // A for-loop using iterators does not work because |client| may touch
  // |clients_waiting_session_queue_| in callbacks.
  while (!clients_waiting_session_queue_.empty()) {
    auto* client = clients_waiting_session_queue_.back();
    clients_waiting_session_queue_.pop_back();
    if (result == midi::mojom::Result::OK) {
      // Add the client's input and output ports.
      for (const auto& info : inputs_) {
        client->DidAddInputPort(WebString::FromUTF8(info.id),
                                WebString::FromUTF8(info.manufacturer),
                                WebString::FromUTF8(info.name),
                                WebString::FromUTF8(info.version), info.state);
      }

      for (const auto& info : outputs_) {
        client->DidAddOutputPort(WebString::FromUTF8(info.id),
                                 WebString::FromUTF8(info.manufacturer),
                                 WebString::FromUTF8(info.name),
                                 WebString::FromUTF8(info.version), info.state);
      }
    }
    client->DidStartSession(result);
    clients_.insert(client);
  }
}

void MidiMessageFilter::HandleAddInputPort(midi::mojom::PortInfo info) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  inputs_.push_back(info);
  const WebString id = WebString::FromUTF8(info.id);
  const WebString manufacturer = WebString::FromUTF8(info.manufacturer);
  const WebString name = WebString::FromUTF8(info.name);
  const WebString version = WebString::FromUTF8(info.version);
  for (auto* client : clients_)
    client->DidAddInputPort(id, manufacturer, name, version, info.state);
}

void MidiMessageFilter::HandleAddOutputPort(midi::mojom::PortInfo info) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  outputs_.push_back(info);
  const WebString id = WebString::FromUTF8(info.id);
  const WebString manufacturer = WebString::FromUTF8(info.manufacturer);
  const WebString name = WebString::FromUTF8(info.name);
  const WebString version = WebString::FromUTF8(info.version);
  for (auto* client : clients_)
    client->DidAddOutputPort(id, manufacturer, name, version, info.state);
}

void MidiMessageFilter::HandleDataReceived(uint32_t port,
                                           const std::vector<uint8_t>& data,
                                           base::TimeTicks timestamp) {
  TRACE_EVENT0("midi", "MidiMessageFilter::HandleDataReceived");
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!data.empty());

  for (auto* client : clients_)
    client->DidReceiveMIDIData(port, &data[0], data.size(), timestamp);
}

void MidiMessageFilter::HandleAckknowledgeSentData(size_t bytes_sent) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_GE(unacknowledged_bytes_sent_, bytes_sent);
  if (unacknowledged_bytes_sent_ >= bytes_sent)
    unacknowledged_bytes_sent_ -= bytes_sent;
}

void MidiMessageFilter::HandleSetInputPortState(uint32_t port,
                                                PortState state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (inputs_[port].state == state)
    return;
  inputs_[port].state = state;
  for (auto* client : clients_)
    client->DidSetInputPortState(port, state);
}

void MidiMessageFilter::HandleSetOutputPortState(uint32_t port,
                                                 PortState state) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (outputs_[port].state == state)
    return;
  outputs_[port].state = state;
  for (auto* client : clients_)
    client->DidSetOutputPortState(port, state);
}

}  // namespace content
