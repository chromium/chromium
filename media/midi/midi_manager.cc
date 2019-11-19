// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

namespace midi {

namespace {

using Sample = base::HistogramBase::Sample;
using midi::mojom::PortState;
using midi::mojom::Result;

// Used to count events for usage histogram. The item order should not be
// changed, and new items should be just appended.
enum class Usage {
  CREATED,
  CREATED_ON_UNSUPPORTED_PLATFORMS,
  SESSION_STARTED,
  SESSION_ENDED,
  INITIALIZED,
  INPUT_PORT_ADDED,
  OUTPUT_PORT_ADDED,
  ERROR_OBSERVED,

  // New items should be inserted here, and |MAX| should point the last item.
  MAX = ERROR_OBSERVED,
};

// Used to count events for transaction usage histogram. The item order should
// not be changed, and new items should be just appended.
enum class SendReceiveUsage {
  NO_USE,
  SENT,
  RECEIVED,
  SENT_AND_RECEIVED,

  // New items should be inserted here, and |MAX| should point the last item.
  MAX = SENT_AND_RECEIVED,
};

void ReportUsage(Usage usage) {
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.Usage", usage,
                            static_cast<Sample>(Usage::MAX) + 1);
}

}  // namespace

MidiManager::MidiManager(MidiService* service) : service_(service) {
  ReportUsage(Usage::CREATED);
}

MidiManager::~MidiManager() {
  base::AutoLock auto_lock(lock_);
  DCHECK(pending_clients_.empty() && clients_.empty());

  if (session_thread_runner_) {
    DCHECK(session_thread_runner_->BelongsToCurrentThread());
    session_thread_runner_ = nullptr;
  }

  if (result_ == Result::INITIALIZATION_ERROR)
    ReportUsage(Usage::ERROR_OBSERVED);

  UMA_HISTOGRAM_ENUMERATION(
      "Media.Midi.SendReceiveUsage",
      data_sent_ ? (data_received_ ? SendReceiveUsage::SENT_AND_RECEIVED
                                   : SendReceiveUsage::SENT)
                 : (data_received_ ? SendReceiveUsage::RECEIVED
                                   : SendReceiveUsage::NO_USE),
      static_cast<Sample>(SendReceiveUsage::MAX) + 1);
}

#if !defined(OS_MACOSX) && !defined(OS_WIN) && \
    !(defined(USE_ALSA) && defined(USE_UDEV)) && !defined(OS_ANDROID)
MidiManager* MidiManager::Create(MidiService* service) {
  ReportUsage(Usage::CREATED_ON_UNSUPPORTED_PLATFORMS);
  return new MidiManager(service);
}
#endif

void MidiManager::StartSession(MidiManagerClient* client) {
  ReportUsage(Usage::SESSION_STARTED);

  bool needs_initialization = false;

  {
    base::AutoLock auto_lock(lock_);

    if (clients_.find(client) != clients_.end() ||
        pending_clients_.find(client) != pending_clients_.end()) {
      // Should not happen. But just in case the renderer is compromised.
      NOTREACHED();
      return;
    }

    if (initialization_state_ == InitializationState::COMPLETED) {
      // Platform dependent initialization was already finished for previously
      // initialized clients.
      if (result_ == Result::OK) {
        for (const auto& info : input_ports_)
          client->AddInputPort(info);
        for (const auto& info : output_ports_)
          client->AddOutputPort(info);
      }

      // Complete synchronously with |result_|;
      clients_.insert(client);
      client->CompleteStartSession(result_);
      return;
    }

    // Do not accept a new request if the pending client list contains too
    // many clients.
    if (pending_clients_.size() >= kMaxPendingClientCount) {
      client->CompleteStartSession(Result::INITIALIZATION_ERROR);
      return;
    }

    if (initialization_state_ == InitializationState::NOT_STARTED) {
      // Set fields protected by |lock_| here and call StartInitialization()
      // later.
      needs_initialization = true;
      session_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
      initialization_state_ = InitializationState::STARTED;
    }

    pending_clients_.insert(client);
  }

  if (needs_initialization) {
    // Lazily initialize the MIDI back-end.
    TRACE_EVENT0("midi", "MidiManager::StartInitialization");
    // CompleteInitialization() will be called asynchronously when platform
    // dependent initialization is finished.
    StartInitialization();
  }
}

bool MidiManager::EndSession(MidiManagerClient* client) {
  ReportUsage(Usage::SESSION_ENDED);

  // At this point, |client| can be in the destruction process, and calling
  // any method of |client| is dangerous. Calls on clients *must* be protected
  // by |lock_| to prevent race conditions.
  base::AutoLock auto_lock(lock_);
  if (clients_.find(client) == clients_.end() &&
      pending_clients_.find(client) == pending_clients_.end()) {
    return false;
  }

  clients_.erase(client);
  pending_clients_.erase(client);
  return true;
}

bool MidiManager::HasOpenSession() {
  base::AutoLock auto_lock(lock_);
  return clients_.size() != 0u;
}

void MidiManager::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32_t port_index,
                                       const std::vector<uint8_t>& data,
                                       base::TimeTicks timestamp) {
  NOTREACHED();
}

void MidiManager::EndAllSessions() {
  base::AutoLock lock(lock_);
  for (auto* client : pending_clients_)
    client->Detach();
  for (auto* client : clients_)
    client->Detach();
  pending_clients_.clear();
  clients_.clear();
}

void MidiManager::StartInitialization() {
  CompleteInitialization(Result::NOT_SUPPORTED);
}

void MidiManager::CompleteInitialization(Result result) {
  DCHECK_EQ(InitializationState::STARTED, initialization_state_);

  TRACE_EVENT0("midi", "MidiManager::CompleteInitialization");
  ReportUsage(Usage::INITIALIZED);

  base::AutoLock auto_lock(lock_);
  if (!session_thread_runner_)
    return;
  DCHECK(session_thread_runner_->BelongsToCurrentThread());

  DCHECK(clients_.empty());
  initialization_state_ = InitializationState::COMPLETED;
  result_ = result;

  for (auto* client : pending_clients_) {
    if (result_ == Result::OK) {
      for (const auto& info : input_ports_)
        client->AddInputPort(info);
      for (const auto& info : output_ports_)
        client->AddOutputPort(info);
    }

    clients_.insert(client);
    client->CompleteStartSession(result_);
  }
  pending_clients_.clear();
}

void MidiManager::AddInputPort(const mojom::PortInfo& info) {
  ReportUsage(Usage::INPUT_PORT_ADDED);
  base::AutoLock auto_lock(lock_);
  input_ports_.push_back(info);
  for (auto* client : clients_)
    client->AddInputPort(info);
}

void MidiManager::AddOutputPort(const mojom::PortInfo& info) {
  ReportUsage(Usage::OUTPUT_PORT_ADDED);
  base::AutoLock auto_lock(lock_);
  output_ports_.push_back(info);
  for (auto* client : clients_)
    client->AddOutputPort(info);
}

void MidiManager::SetInputPortState(uint32_t port_index, PortState state) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, input_ports_.size());
  input_ports_[port_index].state = state;
  for (auto* client : clients_)
    client->SetInputPortState(port_index, state);
}

void MidiManager::SetOutputPortState(uint32_t port_index, PortState state) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, output_ports_.size());
  output_ports_[port_index].state = state;
  for (auto* client : clients_)
    client->SetOutputPortState(port_index, state);
}

mojom::PortState MidiManager::GetOutputPortState(uint32_t port_index) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, output_ports_.size());
  return output_ports_[port_index].state;
}

void MidiManager::AccumulateMidiBytesSent(MidiManagerClient* client, size_t n) {
  base::AutoLock auto_lock(lock_);
  data_sent_ = true;
  if (clients_.find(client) == clients_.end())
    return;

  // Continue to hold lock_ here in case another thread is currently doing
  // EndSession.
  client->AccumulateMidiBytesSent(n);
}

void MidiManager::ReceiveMidiData(uint32_t port_index,
                                  const uint8_t* data,
                                  size_t length,
                                  base::TimeTicks timestamp) {
  base::AutoLock auto_lock(lock_);
  data_received_ = true;

  for (auto* client : clients_)
    client->ReceiveMidiData(port_index, data, length, timestamp);
}

size_t MidiManager::GetClientCountForTesting() {
  base::AutoLock auto_lock(lock_);
  return clients_.size();
}

size_t MidiManager::GetPendingClientCountForTesting() {
  base::AutoLock auto_lock(lock_);
  return pending_clients_.size();
}

}  // namespace midi
