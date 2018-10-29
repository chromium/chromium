// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/midi_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/common/media/midi_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "media/midi/message_util.h"
#include "media/midi/midi_message_queue.h"
#include "media/midi/midi_service.h"

namespace content {
namespace {

// The total number of bytes which we're allowed to send to the OS
// before knowing that they have been successfully sent.
const size_t kMaxInFlightBytes = 10 * 1024 * 1024;  // 10 MB.

// We keep track of the number of bytes successfully sent to
// the hardware.  Every once in a while we report back to the renderer
// the number of bytes sent since the last report. This threshold determines
// how many bytes will be sent before reporting back to the renderer.
const size_t kAcknowledgementThresholdBytes = 1024 * 1024;  // 1 MB.

}  // namespace

using midi::IsDataByte;
using midi::IsSystemRealTimeMessage;
using midi::IsValidWebMIDIData;
using midi::kSysExByte;
using midi::kEndOfSysExByte;
using midi::mojom::PortState;
using midi::mojom::Result;

MidiHost::MidiHost(int renderer_process_id, midi::MidiService* midi_service)
    : BrowserMessageFilter(MidiMsgStart),
      renderer_process_id_(renderer_process_id),
      has_sys_ex_permission_(false),
      is_session_requested_(false),
      midi_service_(midi_service),
      sent_bytes_in_flight_(0),
      bytes_sent_since_last_acknowledgement_(0),
      output_port_count_(0) {
  DCHECK(midi_service_);
}

MidiHost::~MidiHost() = default;

void MidiHost::OnChannelClosing() {
  // If we get here the MidiHost is going to be destroyed soon.  Prevent any
  // subsequent calls from MidiService by closing our session.
  // If Send() is called from a different thread (e.g. a separate thread owned
  // by the MidiService implementation), it will get posted to the IO thread.
  // There is a race condition here if our refcount is 0 and we're about to or
  // have already entered OnDestruct().
  if (is_session_requested_ && midi_service_) {
    midi_service_->EndSession(this);
    is_session_requested_ = false;
  }
}

void MidiHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

// IPC Messages handler
bool MidiHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MidiHost, message)
    IPC_MESSAGE_HANDLER(MidiHostMsg_StartSession, OnStartSession)
    IPC_MESSAGE_HANDLER(MidiHostMsg_SendData, OnSendData)
    IPC_MESSAGE_HANDLER(MidiHostMsg_EndSession, OnEndSession)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void MidiHost::OnStartSession() {
  is_session_requested_ = true;
  if (midi_service_)
    midi_service_->StartSession(this);
}

void MidiHost::OnSendData(uint32_t port,
                          const std::vector<uint8_t>& data,
                          base::TimeTicks timestamp) {
  {
    base::AutoLock auto_lock(output_port_count_lock_);
    if (output_port_count_ <= port) {
      bad_message::ReceivedBadMessage(this, bad_message::MH_INVALID_MIDI_PORT);
      return;
    }
  }

  if (data.empty())
    return;

  // Blink running in a renderer checks permission to raise a SecurityError
  // in JavaScript. The actual permission check for security purposes
  // happens here in the browser process.
  if (!has_sys_ex_permission_ && base::ContainsValue(data, kSysExByte)) {
    bad_message::ReceivedBadMessage(this, bad_message::MH_SYS_EX_PERMISSION);
    return;
  }

  if (!IsValidWebMIDIData(data))
    return;

  {
    base::AutoLock auto_lock(in_flight_lock_);
    // Sanity check that we won't send too much data.
    // TODO(yukawa): Consider to send an error event back to the renderer
    // after some future discussion in W3C.
    if (data.size() + sent_bytes_in_flight_ > kMaxInFlightBytes)
      return;
    sent_bytes_in_flight_ += data.size();
  }
  if (midi_service_)
    midi_service_->DispatchSendMidiData(this, port, data, timestamp);
}

void MidiHost::OnEndSession() {
  is_session_requested_ = false;
  if (midi_service_)
    midi_service_->EndSession(this);
}

void MidiHost::CompleteStartSession(Result result) {
  DCHECK(is_session_requested_);
  if (result == Result::OK) {
    // ChildSecurityPolicy is set just before OnStartSession by
    // MidiDispatcherHost. So we can safely cache the policy.
    has_sys_ex_permission_ = ChildProcessSecurityPolicyImpl::GetInstance()->
        CanSendMidiSysExMessage(renderer_process_id_);
  }
  Send(new MidiMsg_SessionStarted(result));
}

void MidiHost::AddInputPort(const midi::mojom::PortInfo& info) {
  base::AutoLock auto_lock(messages_queues_lock_);
  // MidiMessageQueue is created later in ReceiveMidiData().
  received_messages_queues_.push_back(nullptr);
  Send(new MidiMsg_AddInputPort(info));
}

void MidiHost::AddOutputPort(const midi::mojom::PortInfo& info) {
  base::AutoLock auto_lock(output_port_count_lock_);
  output_port_count_++;
  Send(new MidiMsg_AddOutputPort(info));
}

void MidiHost::SetInputPortState(uint32_t port, PortState state) {
  Send(new MidiMsg_SetInputPortState(port, state));
}

void MidiHost::SetOutputPortState(uint32_t port, PortState state) {
  Send(new MidiMsg_SetOutputPortState(port, state));
}

void MidiHost::ReceiveMidiData(uint32_t port,
                               const uint8_t* data,
                               size_t length,
                               base::TimeTicks timestamp) {
  TRACE_EVENT0("midi", "MidiHost::ReceiveMidiData");

  base::AutoLock auto_lock(messages_queues_lock_);
  if (received_messages_queues_.size() <= port)
    return;

  // Lazy initialization
  if (received_messages_queues_[port] == nullptr)
    received_messages_queues_[port] =
        std::make_unique<midi::MidiMessageQueue>(true);

  received_messages_queues_[port]->Add(data, length);
  std::vector<uint8_t> message;
  while (true) {
    received_messages_queues_[port]->Get(&message);
    if (message.empty())
      break;

    // MIDI devices may send a system exclusive messages even if the renderer
    // doesn't have a permission to receive it. Don't kill the renderer as
    // OnSendData() does.
    if (message[0] == kSysExByte && !has_sys_ex_permission_)
      continue;

    // Send to the renderer.
    Send(new MidiMsg_DataReceived(port, message, timestamp));
  }
}

void MidiHost::AccumulateMidiBytesSent(size_t n) {
  {
    base::AutoLock auto_lock(in_flight_lock_);
    if (n <= sent_bytes_in_flight_)
      sent_bytes_in_flight_ -= n;
  }

  if (bytes_sent_since_last_acknowledgement_ + n >=
      bytes_sent_since_last_acknowledgement_)
    bytes_sent_since_last_acknowledgement_ += n;

  if (bytes_sent_since_last_acknowledgement_ >=
      kAcknowledgementThresholdBytes) {
    Send(new MidiMsg_AcknowledgeSentData(
        bytes_sent_since_last_acknowledgement_));
    bytes_sent_since_last_acknowledgement_ = 0;
  }
}

void MidiHost::Detach() {
  midi_service_ = nullptr;
}

}  // namespace content
