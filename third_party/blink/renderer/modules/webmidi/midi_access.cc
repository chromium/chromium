/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webmidi/midi_access.h"

#include <memory>
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access_initializer.h"
#include "third_party/blink/renderer/modules/webmidi/midi_connection_event.h"
#include "third_party/blink/renderer/modules/webmidi/midi_input.h"
#include "third_party/blink/renderer/modules/webmidi/midi_input_map.h"
#include "third_party/blink/renderer/modules/webmidi/midi_output.h"
#include "third_party/blink/renderer/modules/webmidi/midi_output_map.h"
#include "third_party/blink/renderer/modules/webmidi/midi_port.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

namespace {

using midi::mojom::PortState;

// Since "open" status is separately managed per MIDIAccess instance, we do not
// expose service level PortState directly.
PortState ToDeviceState(PortState state) {
  if (state == PortState::OPENED)
    return PortState::CONNECTED;
  return state;
}

}  // namespace

MIDIAccess::MIDIAccess(
    MIDIDispatcher* dispatcher,
    bool sysex_enabled,
    const Vector<MIDIAccessInitializer::PortDescriptor>& ports,
    ExecutionContext* execution_context)
    : ActiveScriptWrappable<MIDIAccess>({}),
      ExecutionContextLifecycleObserver(execution_context),
      dispatcher_(dispatcher),
      sysex_enabled_(sysex_enabled),
      has_pending_activity_(false) {
  dispatcher_->SetClient(this);
  for (const auto& port : ports) {
    if (port.type == MIDIPortType::kInput) {
      inputs_.push_back(MakeGarbageCollected<MIDIInput>(
          this, port.id, port.manufacturer, port.name, port.version,
          ToDeviceState(port.state)));
    } else {
      outputs_.push_back(MakeGarbageCollected<MIDIOutput>(
          this, outputs_.size(), port.id, port.manufacturer, port.name,
          port.version, ToDeviceState(port.state)));
    }
  }
  constexpr IdentifiableSurface surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature,
      WebFeature::kRequestMIDIAccess_ObscuredByFootprinting);
  if (IdentifiabilityStudySettings::Get()->ShouldSampleSurface(surface)) {
    IdentifiableTokenBuilder builder;
    for (const auto& port : ports) {
      builder.AddToken(IdentifiabilityBenignStringToken(port.id));
      builder.AddToken(IdentifiabilityBenignStringToken(port.name));
      builder.AddToken(IdentifiabilityBenignStringToken(port.manufacturer));
      builder.AddToken(IdentifiabilityBenignStringToken(port.version));
      builder.AddToken(port.type);
    }
    IdentifiabilityMetricBuilder(execution_context->UkmSourceID())
        .Add(surface, builder.GetToken())
        .Record(execution_context->UkmRecorder());
  }
}

MIDIAccess::~MIDIAccess() = default;

EventListener* MIDIAccess::onstatechange() {
  return GetAttributeEventListener(event_type_names::kStatechange);
}

void MIDIAccess::setOnstatechange(EventListener* listener) {
  has_pending_activity_ = listener;
  SetAttributeEventListener(event_type_names::kStatechange, listener);
}

bool MIDIAccess::HasPendingActivity() const {
  return has_pending_activity_ && GetExecutionContext() &&
         !GetExecutionContext()->IsContextDestroyed();
}

MIDIInputMap* MIDIAccess::inputs() const {
  HeapVector<Member<MIDIInput>> inputs;
  HashSet<String> ids;
  for (MIDIInput* input : inputs_) {
    if (input->GetState() != PortState::DISCONNECTED) {
      inputs.push_back(input);
      ids.insert(input->id());
    }
  }
  if (inputs.size() != ids.size()) {
    // There is id duplication that violates the spec.
    inputs.clear();
  }
  return MakeGarbageCollected<MIDIInputMap>(inputs);
}

MIDIOutputMap* MIDIAccess::outputs() const {
  HeapVector<Member<MIDIOutput>> outputs;
  HashSet<String> ids;
  for (MIDIOutput* output : outputs_) {
    if (output->GetState() != PortState::DISCONNECTED) {
      outputs.push_back(output);
      ids.insert(output->id());
    }
  }
  if (outputs.size() != ids.size()) {
    // There is id duplication that violates the spec.
    outputs.clear();
  }
  return MakeGarbageCollected<MIDIOutputMap>(outputs);
}

void MIDIAccess::DidAddInputPort(const String& id,
                                 const String& manufacturer,
                                 const String& name,
                                 const String& version,
                                 PortState state) {
  DCHECK(IsMainThread());
  auto* port = MakeGarbageCollected<MIDIInput>(this, id, manufacturer, name,
                                               version, ToDeviceState(state));
  inputs_.push_back(port);
  DispatchEvent(*MIDIConnectionEvent::Create(port));
}

void MIDIAccess::DidAddOutputPort(const String& id,
                                  const String& manufacturer,
                                  const String& name,
                                  const String& version,
                                  PortState state) {
  DCHECK(IsMainThread());
  unsigned port_index = outputs_.size();
  auto* port = MakeGarbageCollected<MIDIOutput>(
      this, port_index, id, manufacturer, name, version, ToDeviceState(state));
  outputs_.push_back(port);
  DispatchEvent(*MIDIConnectionEvent::Create(port));
}

void MIDIAccess::DidSetInputPortState(unsigned port_index, PortState state) {
  DCHECK(IsMainThread());
  if (port_index >= inputs_.size())
    return;

  PortState device_state = ToDeviceState(state);
  if (inputs_[port_index]->GetState() != device_state)
    inputs_[port_index]->SetState(device_state);
}

void MIDIAccess::DidSetOutputPortState(unsigned port_index, PortState state) {
  DCHECK(IsMainThread());
  if (port_index >= outputs_.size())
    return;

  PortState device_state = ToDeviceState(state);
  if (outputs_[port_index]->GetState() != device_state)
    outputs_[port_index]->SetState(device_state);
}

void MIDIAccess::DidReceiveMIDIData(unsigned port_index,
                                    const unsigned char* data,
                                    wtf_size_t length,
                                    base::TimeTicks time_stamp) {
  DCHECK(IsMainThread());
  if (port_index >= inputs_.size())
    return;

  inputs_[port_index]->DidReceiveMIDIData(port_index, data, length, time_stamp);
}

void MIDIAccess::SendMIDIData(unsigned port_index,
                              const unsigned char* data,
                              wtf_size_t length,
                              base::TimeTicks time_stamp) {
  DCHECK(!time_stamp.is_null());
  if (!GetExecutionContext() || !data || !length ||
      port_index >= outputs_.size())
    return;

  dispatcher_->SendMIDIData(port_index, data, length, time_stamp);
}

void MIDIAccess::Trace(Visitor* visitor) const {
  visitor->Trace(dispatcher_);
  visitor->Trace(inputs_);
  visitor->Trace(outputs_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
