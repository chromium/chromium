// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_DISPATCHER_H_

#include "media/midi/midi_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MIDIDispatcher : public midi::mojom::blink::MidiSessionClient {
 public:
  class Client {
   public:
    virtual void DidAddInputPort(const String& id,
                                 const String& manufacturer,
                                 const String& name,
                                 const String& version,
                                 midi::mojom::PortState) = 0;
    virtual void DidAddOutputPort(const String& id,
                                  const String& manufacturer,
                                  const String& name,
                                  const String& version,
                                  midi::mojom::PortState) = 0;
    virtual void DidSetInputPortState(unsigned port_index,
                                      midi::mojom::PortState) = 0;
    virtual void DidSetOutputPortState(unsigned port_index,
                                       midi::mojom::PortState) = 0;

    virtual void DidStartSession(midi::mojom::Result) = 0;
    virtual void DidReceiveMIDIData(unsigned port_index,
                                    const unsigned char* data,
                                    wtf_size_t length,
                                    base::TimeTicks time_stamp) = 0;
  };

  explicit MIDIDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ExecutionContext* execution_context);
  ~MIDIDispatcher() override;

  void SetClient(Client* client) { client_ = client; }

  void SendMIDIData(uint32_t port,
                    const uint8_t* data,
                    wtf_size_t length,
                    base::TimeTicks timestamp);

  // midi::mojom::blink::MidiSessionClient implementation.
  // All of the following methods are run on the main thread.
  void AddInputPort(midi::mojom::blink::PortInfoPtr info) override;
  void AddOutputPort(midi::mojom::blink::PortInfoPtr info) override;
  void SetInputPortState(uint32_t port,
                         midi::mojom::blink::PortState state) override;
  void SetOutputPortState(uint32_t port,
                          midi::mojom::blink::PortState state) override;
  void SessionStarted(midi::mojom::blink::Result result) override;
  void AcknowledgeSentData(uint32_t bytes) override;
  void DataReceived(uint32_t port,
                    const Vector<uint8_t>& data,
                    base::TimeTicks timestamp) override;

 private:
  Client* client_ = nullptr;

  bool initialized_ = false;

  // Holds MidiPortInfoList for input ports and output ports.
  Vector<midi::mojom::blink::PortInfo> inputs_;
  Vector<midi::mojom::blink::PortInfo> outputs_;

  // TODO(toyoshim): Consider to have a per-process limit.
  size_t unacknowledged_bytes_sent_ = 0u;

  // TODO(1049056): Now that MidiSession is passed an ExecutionContext use it to
  // convert these to HeapMojo.
  mojo::Remote<midi::mojom::blink::MidiSession> midi_session_;

  mojo::Receiver<midi::mojom::blink::MidiSessionClient> receiver_{this};
  mojo::Remote<midi::mojom::blink::MidiSessionProvider> midi_session_provider_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_DISPATCHER_H_
