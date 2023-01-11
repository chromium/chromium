// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_WIN_H_
#define MEDIA_MIDI_MIDI_MANAGER_WIN_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/system/system_monitor.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace base {
class SingleThreadTaskRunner;
class TimeDelta;
}  // namespace base

namespace midi {

// New backend for legacy Windows that support dynamic instantiation.
class MidiManagerWin final
    : public MidiManager,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  class PortManager;

  MIDI_EXPORT static void OverflowInstanceIdForTesting();

  explicit MidiManagerWin(MidiService* service);

  MidiManagerWin(const MidiManagerWin&) = delete;
  MidiManagerWin& operator=(const MidiManagerWin&) = delete;

  ~MidiManagerWin() override;

  // Returns PortManager that implements interfaces to help implementation.
  // This hides Windows specific structures, i.e. HMIDIIN in the header.
  PortManager* port_manager() { return port_manager_.get(); }

  // MidiManager overrides:
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) override;

  // base::SystemMonitor::DevicesChangedObserver overrides:
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;

 private:
  class InPort;
  class OutPort;

  // Handles MIDI inport event posted from a thread system provides.
  void ReceiveMidiData(uint32_t index,
                       const std::vector<uint8_t>& data,
                       base::TimeTicks time);

  // Posts a task to TaskRunner, and ensures that the instance keeps alive while
  // the task is running.
  void PostTask(base::OnceClosure);
  void PostDelayedTask(base::OnceClosure, base::TimeDelta delay);

  // Posts a reply task to the I/O thread that hosts MidiManager instance, runs
  // it safely, and ensures that the instance keeps alive while the task is
  // running.
  void PostReplyTask(base::OnceClosure);

  // Initializes instance asynchronously on TaskRunner.
  void InitializeOnTaskRunner();

  // Updates device lists on TaskRunner.
  // Returns true if device lists were changed.
  void UpdateDeviceListOnTaskRunner();

  // Reflect active port list to a device list.
  template <typename T>
  void ReflectActiveDeviceList(MidiManagerWin* manager,
                               std::vector<std::unique_ptr<T>>* known_ports,
                               std::vector<std::unique_ptr<T>>* active_ports);

  // Sends MIDI data on TaskRunner.
  void SendOnTaskRunner(MidiManagerClient* client,
                        uint32_t port_index,
                        const std::vector<uint8_t>& data);

  // Holds an unique instance ID.
  const int64_t instance_id_;

  // Keeps a TaskRunner for the I/O thread.
  scoped_refptr<base::SingleThreadTaskRunner> thread_runner_;

  // Manages platform dependent implementation for port managegent. Should be
  // accessed with the task lock.
  std::unique_ptr<PortManager> port_manager_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MANAGER_WIN_H_
