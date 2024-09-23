// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MIDI_HOST_H_
#define CONTENT_BROWSER_MEDIA_MIDI_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/tuple.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace midi {
class MidiService;
class MidiMessageQueue;
}  // namespace midi

namespace content {

class CONTENT_EXPORT MidiHost : public midi::MidiManagerClient,
                                public midi::mojom::MidiSessionProvider,
                                public midi::mojom::MidiSession {
 public:
  MidiHost(const MidiHost&) = delete;
  MidiHost& operator=(const MidiHost&) = delete;

  ~MidiHost() override;

  // Creates an instance of MidiHost and binds |receiver| to the instance using
  // a self owned receiver. Should be called on the IO thread.
  static void BindReceiver(
      int render_process_id,
      midi::MidiService* midi_service,
      mojo::PendingReceiver<midi::mojom::MidiSessionProvider> receiver);

  // MidiManagerClient implementation. These methods can be called on any thread
  // by platform specific implementations of MidiManager, so use locks
  // appropriately.
  void CompleteStartSession(midi::mojom::Result result) override;
  void AddInputPort(const midi::mojom::PortInfo& info) override;
  void AddOutputPort(const midi::mojom::PortInfo& info) override;
  void SetInputPortState(uint32_t port, midi::mojom::PortState state) override;
  void SetOutputPortState(uint32_t port, midi::mojom::PortState state) override;
  void ReceiveMidiData(uint32_t port,
                       const uint8_t* data,
                       size_t length,
                       base::TimeTicks timestamp) override;
  void AccumulateMidiBytesSent(size_t n) override;
  void Detach() override;

  // midi::mojom::MidiSessionProvider implementation.
  void StartSession(
      mojo::PendingReceiver<midi::mojom::MidiSession> session_receiver,
      mojo::PendingRemote<midi::mojom::MidiSessionClient> client) override;

  // midi::mojom::MidiSession implementation.
  void SendData(uint32_t port,
                const std::vector<uint8_t>& data,
                base::TimeTicks timestamp) override;

 protected:
  MidiHost(int renderer_process_id, midi::MidiService* midi_service);

  void SetHasMidiPermissionForTesting(bool value) {
    has_midi_permission_ = value;
  }

 private:
  // Use this to call methods on |midi_client_|. It makes sure that midi_client_
  // is only accessed on the IO thread.
  template <typename Method, typename... Params>
  void CallClient(Method method, Params... params);

  void EndSession();

  const int renderer_process_id_;

  // Represents if the renderer has a permission to send/receive MIDI messages.
  bool has_midi_permission_;

  // Represents if the renderer has a permission to send/receive MIDI SysEX
  // messages.
  bool has_midi_sysex_permission_;

  // |midi_service_| manages a MidiManager instance that talks to
  // platform-specific MIDI APIs.  It can be nullptr after detached.
  raw_ptr<midi::MidiService> midi_service_;

  // Buffers where data sent from each MIDI input port is stored.
  std::vector<std::unique_ptr<midi::MidiMessageQueue>> received_messages_queues_
      GUARDED_BY(messages_queues_lock_);

  // Protects access to |received_messages_queues_|;
  base::Lock messages_queues_lock_;

  // The number of bytes sent to the platform-specific MIDI sending
  // system, but not yet completed.
  size_t sent_bytes_in_flight_ GUARDED_BY(in_flight_lock_);

  // The number of bytes successfully sent since the last time
  // we've acknowledged back to the renderer.
  size_t bytes_sent_since_last_acknowledgement_;

  // Protects access to |sent_bytes_in_flight_|.
  base::Lock in_flight_lock_;

  // How many output port exists.
  uint32_t output_port_count_ GUARDED_BY(output_port_count_lock_);

  // Protects access to |output_port_count_|.
  base::Lock output_port_count_lock_;

  // Stores a session request sent from the renderer until CompleteStartSession
  // is called.
  mojo::PendingReceiver<midi::mojom::MidiSession> pending_session_receiver_;

  // Bound on the IO thread if a session is successfully started by MidiService.
  mojo::Receiver<midi::mojom::MidiSession> midi_session_{this};

  // Bound on the IO thread and should only be called there. Use CallClient to
  // call midi::mojom::MidiSessionClient methods.
  mojo::Remote<midi::mojom::MidiSessionClient> midi_client_;

  // WeakPtr factory for CallClient callbacks.
  base::WeakPtrFactory<MidiHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MIDI_HOST_H_
