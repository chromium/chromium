// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_H_
#define MEDIA_MIDI_MIDI_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_service.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace midi {

class MidiService;

// A MidiManagerClient registers with the MidiManager to receive MIDI data.
// See MidiManager::RequestAccess() and MidiManager::ReleaseAccess()
// for details.
// TODO(toyoshim): Consider to have a MidiServiceClient interface.
class MIDI_EXPORT MidiManagerClient {
 public:
  virtual ~MidiManagerClient() {}

  // AddInputPort() and AddOutputPort() are called before CompleteStartSession()
  // is called to notify existing MIDI ports, and also called after that to
  // notify new MIDI ports are added.
  virtual void AddInputPort(const mojom::PortInfo& info) = 0;
  virtual void AddOutputPort(const mojom::PortInfo& info) = 0;

  // SetInputPortState() and SetOutputPortState() are called to notify a known
  // device gets disconnected, or connected again.
  virtual void SetInputPortState(uint32_t port_index,
                                 mojom::PortState state) = 0;
  virtual void SetOutputPortState(uint32_t port_index,
                                  mojom::PortState state) = 0;

  // CompleteStartSession() is called when platform dependent preparation is
  // finished.
  virtual void CompleteStartSession(mojom::Result result) = 0;

  // ReceiveMidiData() is called when MIDI data has been received from the
  // MIDI system.
  // |port_index| represents the specific input port from input_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time the data was received, in seconds.
  virtual void ReceiveMidiData(uint32_t port_index,
                               const uint8_t* data,
                               size_t length,
                               base::TimeTicks timestamp) = 0;

  // AccumulateMidiBytesSent() is called to acknowledge when bytes have
  // successfully been sent to the hardware.
  // This happens as a result of the client having previously called
  // MidiManager::DispatchSendMidiData().
  virtual void AccumulateMidiBytesSent(size_t n) = 0;

  // Detach() is called when MidiManager is going to shutdown immediately.
  // Client should not touch MidiManager instance after Detach() is called.
  virtual void Detach() = 0;
};

// Manages access to all MIDI hardware. MidiManager runs on the I/O thread.
//
// Note: We will eventually remove utility functions that are shared among
// platform dependent MidiManager inheritances such as MidiManagerClient
// management. MidiService should provide such shareable utility functions as
// it does TaskService.
class MIDI_EXPORT MidiManager {
 public:
  static const size_t kMaxPendingClientCount = 128;

  explicit MidiManager(MidiService* service);
  virtual ~MidiManager();

  static MidiManager* Create(MidiService* service);

  // A client calls StartSession() to receive and send MIDI data.
  // If the session is ready to start, the MIDI system is lazily initialized
  // and the client is registered to receive MIDI data.
  // CompleteStartSession() is called with mojom::Result::OK if the session is
  // started. Otherwise CompleteStartSession() is called with a proper
  // mojom::Result code.
  void StartSession(MidiManagerClient* client);

  // A client calls EndSession() to stop receiving MIDI data.
  // Returns false if |client| did not start a session.
  bool EndSession(MidiManagerClient* client);

  // Returns true if there is at least one client that keep a session open.
  bool HasOpenSession();

  // DispatchSendMidiData() is called when MIDI data should be sent to the MIDI
  // system.
  // This method is supposed to return immediately and should not block.
  // |port_index| represents the specific output port from output_ports().
  // |data| represents a series of bytes encoding one or more MIDI messages.
  // |length| is the number of bytes in |data|.
  // |timestamp| is the time to send the data. A value of 0 means send "now" or
  // as soon as possible. The default implementation is for unsupported
  // platforms.
  virtual void DispatchSendMidiData(MidiManagerClient* client,
                                    uint32_t port_index,
                                    const std::vector<uint8_t>& data,
                                    base::TimeTicks timestamp);

  // This method ends all sessions by detaching and removing all registered
  // clients. This method can be called from any thread.
  void EndAllSessions();

 protected:
  friend class MidiManagerUsb;

  // Initializes the platform dependent MIDI system. MidiManager class has a
  // default implementation that synchronously calls CompleteInitialization()
  // with mojom::Result::NOT_SUPPORTED. A derived class for a specific platform
  // should override this method correctly.
  // Platform dependent initialization can be processed synchronously or
  // asynchronously. When the initialization is completed,
  // CompleteInitialization() should be called with |result|.
  // |result| should be mojom::Result::OK on success, otherwise a proper
  // mojom::Result.
  virtual void StartInitialization();

  // Called from a platform dependent implementation of StartInitialization().
  // The method distributes |result| to MIDIManagerClient objects in
  // |pending_clients_|.
  void CompleteInitialization(mojom::Result result);

  // The following five methods can be called on any thread to notify clients of
  // status changes on ports, or to obtain port status.
  void AddInputPort(const mojom::PortInfo& info);
  void AddOutputPort(const mojom::PortInfo& info);
  void SetInputPortState(uint32_t port_index, mojom::PortState state);
  void SetOutputPortState(uint32_t port_index, mojom::PortState state);
  mojom::PortState GetOutputPortState(uint32_t port_index);

  // Invoke AccumulateMidiBytesSent() for |client| safely. If the session was
  // already closed, do nothing. Can be called on any thread.
  void AccumulateMidiBytesSent(MidiManagerClient* client, size_t n);

  // Dispatches to all clients. Can be called on any thread.
  void ReceiveMidiData(uint32_t port_index,
                       const uint8_t* data,
                       size_t length,
                       base::TimeTicks time);

  // Only for testing.
  size_t GetClientCountForTesting();
  size_t GetPendingClientCountForTesting();

  MidiService* service() { return service_; }

 private:
  enum class InitializationState {
    NOT_STARTED,
    STARTED,
    COMPLETED,
  };

  // Note: Members that are not protected by any lock should be accessed only on
  // the I/O thread.

  // Tracks platform dependent initialization state.
  InitializationState initialization_state_ = InitializationState::NOT_STARTED;

  // Keeps the platform dependent initialization result if initialization is
  // completed. Otherwise keeps mojom::Result::NOT_INITIALIZED.
  mojom::Result result_ = mojom::Result::NOT_INITIALIZED;

  // Keeps track of all clients who are waiting for CompleteStartSession().
  std::set<MidiManagerClient*> pending_clients_ GUARDED_BY(lock_);

  // Keeps track of all clients who wish to receive MIDI data.
  std::set<MidiManagerClient*> clients_ GUARDED_BY(lock_);

  // Keeps a SingleThreadTaskRunner of the thread that calls StartSession in
  // order to invoke CompleteStartSession() on the thread. This is touched only
  // on the IO thread usually, but to be guarded by |lock_| for thread checks.
  scoped_refptr<base::SingleThreadTaskRunner> session_thread_runner_
      GUARDED_BY(lock_);

  // Keeps all PortInfo.
  std::vector<mojom::PortInfo> input_ports_ GUARDED_BY(lock_);
  std::vector<mojom::PortInfo> output_ports_ GUARDED_BY(lock_);

  // Tracks if actual data transmission happens.
  bool data_sent_ GUARDED_BY(lock_) = false;
  bool data_received_ GUARDED_BY(lock_) = false;

  // Protects members above.
  base::Lock lock_;

  // MidiService outlives MidiManager.
  MidiService* const service_;

  DISALLOW_COPY_AND_ASSIGN(MidiManager);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MANAGER_H_
