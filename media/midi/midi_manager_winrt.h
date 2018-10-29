// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_WINRT_H_
#define MEDIA_MIDI_MIDI_MANAGER_WINRT_H_

#include <memory>

#include "base/strings/string16.h"
#include "base/thread_annotations.h"
#include "media/midi/midi_manager.h"

namespace midi {

class MidiService;

class MIDI_EXPORT MidiManagerWinrt final : public MidiManager {
 public:
  class MidiInPortManager;
  class MidiOutPortManager;

  explicit MidiManagerWinrt(MidiService* service);
  ~MidiManagerWinrt() override;

  // MidiManager overrides:
  void StartInitialization() final;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) final;

 private:
  // Subclasses that access private/protected members of MidiManager.
  template <typename InterfaceType,
            typename RuntimeType,
            typename StaticsInterfaceType,
            base::char16 const* runtime_class_id>
  class MidiPortManager;

  // Callbacks on kComTaskRunner.
  void InitializeOnComRunner();
  void SendOnComRunner(uint32_t port_index, const std::vector<uint8_t>& data);

  // Callback from MidiPortManager::OnEnumerationComplete on kComTaskRunner.
  // Calls CompleteInitialization() when both MidiPortManagers are ready.
  void OnPortManagerReady();

  // Lock to ensure all smart pointers initialized in InitializeOnComRunner()
  // and destroyed in FinalizeOnComRunner() will not be accidentally destructed
  // twice in the destructor.
  base::Lock lazy_init_member_lock_;

  // All operations to Midi{In|Out}PortManager should be done on kComTaskRunner.
  std::unique_ptr<MidiInPortManager> port_manager_in_
      GUARDED_BY(lazy_init_member_lock_);
  std::unique_ptr<MidiOutPortManager> port_manager_out_
      GUARDED_BY(lazy_init_member_lock_);

  // Incremented when a MidiPortManager is ready.
  uint8_t port_manager_ready_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerWinrt);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MANAGER_WINRT_H_
