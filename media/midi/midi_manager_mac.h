// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_MAC_H_
#define MEDIA_MIDI_MIDI_MANAGER_MAC_H_

#include <CoreMIDI/MIDIServices.h>
#include <stdint.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_service.mojom.h"

namespace midi {

class MidiService;

class MIDI_EXPORT MidiManagerMac final : public MidiManager {
 public:
  explicit MidiManagerMac(MidiService* service);

  MidiManagerMac(const MidiManagerMac&) = delete;
  MidiManagerMac& operator=(const MidiManagerMac&) = delete;

  ~MidiManagerMac() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) override;

 private:
  // Initializes CoreMIDI on |client_thread_| asynchronously. Called from
  // StartInitialization().
  void InitializeCoreMIDI();

  // Completes CoreMIDI initialization and asks the thread that ran
  // StartInitialization() to call CompleteStartSession() safely.
  void CompleteCoreMIDIInitialization(mojom::Result result);

  // CoreMIDI callback for MIDI notification.
  // Receives MIDI related event notifications from CoreMIDI.
  static void ReceiveMidiNotifyDispatch(const MIDINotification* message,
                                        void* refcon);
  void ReceiveMidiNotify(const MIDINotification* message);

  // CoreMIDI callback for MIDI data.
  // Each callback can contain multiple packets, each of which can contain
  // multiple MIDI messages.
  static void ReadMidiDispatch(const MIDIPacketList* packet_list,
                               void* read_proc_refcon,
                               void* src_conn_refcon);

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(MidiManagerClient* client,
                    uint32_t port_index,
                    const std::vector<uint8_t>& data,
                    base::TimeTicks timestamp);

  // CoreMIDI client reference.
  MIDIClientRef midi_client_ GUARDED_BY(midi_client_lock_) = 0;
  base::Lock midi_client_lock_;

  // Following members can be accessed without any lock on kClientTaskRunner,
  // or on I/O thread before calling BindInstance() or after calling
  // UnbindInstance().

  // CoreMIDI other references.
  MIDIPortRef midi_input_ = 0;
  MIDIPortRef midi_output_ = 0;
  std::vector<uint8_t> midi_buffer_;

  // Keeps track of all sources.
  std::vector<MIDIEndpointRef> sources_;

  // Keeps track of all destinations.
  std::vector<MIDIEndpointRef> destinations_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MANAGER_MAC_H_
