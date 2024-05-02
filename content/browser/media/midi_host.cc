// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/midi_host.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "media/midi/message_util.h"
#include "media/midi/midi_message_queue.h"
#include "media/midi/midi_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/features.h"

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
using midi::kEndOfSysExByte;
using midi::kSysExByte;
using midi::mojom::PortState;
using midi::mojom::Result;

MidiHost::MidiHost(int renderer_process_id, midi::MidiService* midi_service)
    : renderer_process_id_(renderer_process_id),
      has_midi_permission_(false),
      has_midi_sysex_permission_(false),
      midi_service_(midi_service),
      sent_bytes_in_flight_(0),
      bytes_sent_since_last_acknowledgement_(0),
      output_port_count_(0) {
  DCHECK(midi_service_);
}

MidiHost::~MidiHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (midi_client_ && midi_service_)
    EndSession();
}

// static
void MidiHost::BindReceiver(
    int render_process_id,
    midi::MidiService* midi_service,
    mojo::PendingReceiver<midi::mojom::MidiSessionProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new MidiHost(render_process_id, midi_service)),
      std::move(receiver));
}

void MidiHost::CompleteStartSession(Result result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(midi_client_);
  if (result == Result::OK)
    midi_session_.Bind(std::move(pending_session_receiver_));
  midi_client_->SessionStarted(result);
}

void MidiHost::AddInputPort(const midi::mojom::PortInfo& info) {
  base::AutoLock auto_lock(messages_queues_lock_);
  // MidiMessageQueue is created later in ReceiveMidiData().
  received_messages_queues_.push_back(nullptr);
  CallClient(&midi::mojom::MidiSessionClient::AddInputPort,
             midi::mojom::PortInfo::New(info));
}

void MidiHost::AddOutputPort(const midi::mojom::PortInfo& info) {
  base::AutoLock auto_lock(output_port_count_lock_);
  output_port_count_++;
  CallClient(&midi::mojom::MidiSessionClient::AddOutputPort,
             midi::mojom::PortInfo::New(info));
}

void MidiHost::SetInputPortState(uint32_t port, PortState state) {
  CallClient(&midi::mojom::MidiSessionClient::SetInputPortState, port, state);
}

void MidiHost::SetOutputPortState(uint32_t port, PortState state) {
  CallClient(&midi::mojom::MidiSessionClient::SetOutputPortState, port, state);
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

    if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
      // MIDI devices may send messages even if the renderer doesn't have
      // permission to receive them. Don't kill the renderer as SendData() does.
      if (!has_midi_permission_) {
        // TODO(crbug.com/40637524): This should check permission with the Frame
        // and not the Process.
        has_midi_permission_ =
            ChildProcessSecurityPolicyImpl::GetInstance()->CanSendMidiMessage(
                renderer_process_id_);
        if (!has_midi_permission_) {
          continue;
        }
      }
    }

    // MIDI devices may send a system exclusive messages even if the renderer
    // doesn't have a permission to receive it. Don't kill the renderer as
    // SendData() does.
    if (message[0] == kSysExByte) {
      if (!has_midi_sysex_permission_) {
        // TODO(crbug.com/40637524): This should check permission with the Frame
        // and not the Process.
        has_midi_sysex_permission_ =
            ChildProcessSecurityPolicyImpl::GetInstance()
                ->CanSendMidiSysExMessage(renderer_process_id_);
        if (!has_midi_sysex_permission_) {
          continue;
        }
      }
    }

    // Send to the renderer.
    CallClient(&midi::mojom::MidiSessionClient::DataReceived, port, message,
               timestamp);
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
    CallClient(&midi::mojom::MidiSessionClient::AcknowledgeSentData,
               bytes_sent_since_last_acknowledgement_);
    bytes_sent_since_last_acknowledgement_ = 0;
  }
}

void MidiHost::Detach() {
  midi_service_ = nullptr;
}

void MidiHost::StartSession(
    mojo::PendingReceiver<midi::mojom::MidiSession> session_receiver,
    mojo::PendingRemote<midi::mojom::MidiSessionClient> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!pending_session_receiver_);
  // Checks to see if |midi_session_| isn't already bound to another
  // MidiSessionRequest.
  pending_session_receiver_ = std::move(session_receiver);

  DCHECK(!midi_client_);
  midi_client_.Bind(std::move(client));
  midi_client_.set_disconnect_handler(
      base::BindOnce(&MidiHost::EndSession, base::Unretained(this)));

  if (midi_service_)
    midi_service_->StartSession(this);
}

void MidiHost::SendData(uint32_t port,
                        const std::vector<uint8_t>& data,
                        base::TimeTicks timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  {
    base::AutoLock auto_lock(output_port_count_lock_);
    if (output_port_count_ <= port) {
      bad_message::ReceivedBadMessage(renderer_process_id_,
                                      bad_message::MH_INVALID_MIDI_PORT);
      return;
    }
  }

  if (data.empty())
    return;

  // Blink running in a renderer checks permission to raise a SecurityError
  // in JavaScript. The actual permission check for security purposes
  // happens here in the browser process.
  if (base::FeatureList::IsEnabled(blink::features::kBlockMidiByDefault)) {
    if (!has_midi_permission_ && !base::Contains(data, kSysExByte)) {
      has_midi_permission_ =
          ChildProcessSecurityPolicyImpl::GetInstance()->CanSendMidiMessage(
              renderer_process_id_);
      if (!has_midi_permission_) {
        bad_message::ReceivedBadMessage(renderer_process_id_,
                                        bad_message::MH_MIDI_PERMISSION);
        return;
      }
    }
  }

  // Check `has_midi_sysex_permission_` here to avoid searching kSysExByte in
  // large bulk data transfers for correct uses.
  if (!has_midi_sysex_permission_ && base::Contains(data, kSysExByte)) {
    has_midi_sysex_permission_ =
        ChildProcessSecurityPolicyImpl::GetInstance()->CanSendMidiSysExMessage(
            renderer_process_id_);
    if (!has_midi_sysex_permission_) {
      bad_message::ReceivedBadMessage(renderer_process_id_,
                                      bad_message::MH_MIDI_SYSEX_PERMISSION);
      return;
    }
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

template <typename Method, typename... Params>
void MidiHost::CallClient(Method method, Params... params) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&MidiHost::CallClient<Method, Params...>,
                                  weak_ptr_factory_.GetWeakPtr(), method,
                                  std::move(params)...));
    return;
  }
  (midi_client_.get()->*method)(std::move(params)...);
}

void MidiHost::EndSession() {
  if (midi_service_)
    midi_service_->EndSession(this);
  midi_client_.reset();
  midi_session_.reset();
}

}  // namespace content
