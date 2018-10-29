// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_ACCESS_INITIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_ACCESS_INITIALIZER_H_

#include <memory>
#include "media/midi/midi_service.mojom-blink.h"
#include "third_party/blink/public/platform/modules/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webmidi/midi_accessor.h"
#include "third_party/blink/renderer/modules/webmidi/midi_accessor_client.h"
#include "third_party/blink/renderer/modules/webmidi/midi_options.h"
#include "third_party/blink/renderer/modules/webmidi/midi_port.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT MIDIAccessInitializer : public ScriptPromiseResolver,
                                             public MIDIAccessorClient {
 public:
  struct PortDescriptor {
    DISALLOW_NEW();
    String id;
    String manufacturer;
    String name;
    MIDIPort::TypeCode type;
    String version;
    midi::mojom::PortState state;

    PortDescriptor(const String& id,
                   const String& manufacturer,
                   const String& name,
                   MIDIPort::TypeCode type,
                   const String& version,
                   midi::mojom::PortState state)
        : id(id),
          manufacturer(manufacturer),
          name(name),
          type(type),
          version(version),
          state(state) {}
  };

  static ScriptPromise Start(ScriptState* script_state,
                             const MIDIOptions& options) {
    MIDIAccessInitializer* resolver =
        new MIDIAccessInitializer(script_state, options);
    resolver->KeepAliveWhilePending();
    resolver->PauseIfNeeded();
    return resolver->Start();
  }

  ~MIDIAccessInitializer() override = default;

  // Eager finalization to allow dispose() operation access
  // other (non eager) heap objects.
  EAGERLY_FINALIZE();

  // MIDIAccessorClient
  void DidAddInputPort(const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       midi::mojom::PortState) override;
  void DidAddOutputPort(const String& id,
                        const String& manufacturer,
                        const String& name,
                        const String& version,
                        midi::mojom::PortState) override;
  void DidSetInputPortState(unsigned port_index,
                            midi::mojom::PortState) override;
  void DidSetOutputPortState(unsigned port_index,
                             midi::mojom::PortState) override;
  void DidStartSession(midi::mojom::Result) override;
  void DidReceiveMIDIData(unsigned port_index,
                          const unsigned char* data,
                          size_t length,
                          TimeTicks time_stamp) override {}

 private:
  MIDIAccessInitializer(ScriptState*, const MIDIOptions&);

  ExecutionContext* GetExecutionContext() const;
  ScriptPromise Start();

  void ContextDestroyed(ExecutionContext*) override;

  void OnPermissionsUpdated(mojom::blink::PermissionStatus);
  void OnPermissionUpdated(mojom::blink::PermissionStatus);

  std::unique_ptr<MIDIAccessor> accessor_;
  Vector<PortDescriptor> port_descriptors_;
  MIDIOptions options_;

  mojom::blink::PermissionServicePtr permission_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_ACCESS_INITIALIZER_H_
