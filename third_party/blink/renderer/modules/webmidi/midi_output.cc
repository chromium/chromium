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

#include "third_party/blink/renderer/modules/webmidi/midi_output.h"

#include <array>

#include "media/midi/midi_service.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

using midi::mojom::PortState;

namespace blink {

namespace {

DOMUint8Array* ConvertUnsignedDataToUint8Array(
    Vector<unsigned> unsigned_data,
    ExceptionState& exception_state) {
  DOMUint8Array* array = DOMUint8Array::Create(unsigned_data.size());
  auto array_data = array->ByteSpan();
  for (wtf_size_t i = 0; i < unsigned_data.size(); ++i) {
    if (unsigned_data[i] > 0xff) {
      exception_state.ThrowTypeError("The value at index " + String::Number(i) +
                                     " (" + String::Number(unsigned_data[i]) +
                                     ") is greater than 0xFF.");
      return nullptr;
    }
    array_data[i] = unsigned_data[i];
  }
  return array;
}

base::TimeTicks GetTimeOrigin(ExecutionContext* context) {
  DCHECK(context);
  Performance* performance = nullptr;
  if (LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context)) {
    performance = DOMWindowPerformance::performance(*window);
  } else {
    DCHECK(context->IsWorkerGlobalScope());
    performance = WorkerGlobalScopePerformance::performance(
        *static_cast<WorkerGlobalScope*>(context));
  }

  DCHECK(performance);
  return performance->GetTimeOriginInternal();
}

class MessageValidator {
  STACK_ALLOCATED();

 public:
  static bool Validate(DOMUint8Array* array,
                       ExceptionState& exception_state,
                       bool sysex_enabled) {
    MessageValidator validator(array);
    return validator.Process(exception_state, sysex_enabled);
  }

 private:
  explicit MessageValidator(DOMUint8Array* array) : data_(array->ByteSpan()) {}

  bool Process(ExceptionState& exception_state, bool sysex_enabled) {
    // data_ is put into a WTF::Vector eventually, which only has wtf_size_t
    // space.
    if (!base::CheckedNumeric<wtf_size_t>(data_.size()).IsValid()) {
      exception_state.ThrowRangeError(
          "Data exceeds the maximum supported length");
      return false;
    }
    while (!IsEndOfData() && AcceptRealTimeMessages()) {
      if (!IsStatusByte()) {
        exception_state.ThrowTypeError("Running status is not allowed " +
                                       GetPositionString());
        return false;
      }
      if (IsEndOfSysex()) {
        exception_state.ThrowTypeError(
            "Unexpected end of system exclusive message " +
            GetPositionString());
        return false;
      }
      if (IsReservedStatusByte()) {
        exception_state.ThrowTypeError("Reserved status is not allowed " +
                                       GetPositionString());
        return false;
      }
      if (IsSysex()) {
        if (!sysex_enabled) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kInvalidAccessError,
              "System exclusive message is not allowed " + GetPositionString());
          return false;
        }
        if (!AcceptCurrentSysex()) {
          if (IsEndOfData())
            exception_state.ThrowTypeError(
                "System exclusive message is not ended by end of system "
                "exclusive message.");
          else
            exception_state.ThrowTypeError(
                "System exclusive message contains a status byte " +
                GetPositionString());
          return false;
        }
      } else {
        if (!AcceptCurrentMessage()) {
          if (IsEndOfData())
            exception_state.ThrowTypeError("Message is incomplete.");
          else
            exception_state.ThrowTypeError("Unexpected status byte " +
                                           GetPositionString());
          return false;
        }
      }
    }
    return true;
  }

 private:
  bool IsEndOfData() { return offset_ >= data_.size(); }
  bool IsSysex() { return data_[offset_] == 0xf0; }
  bool IsSystemMessage() { return data_[offset_] >= 0xf0; }
  bool IsEndOfSysex() { return data_[offset_] == 0xf7; }
  bool IsRealTimeMessage() { return data_[offset_] >= 0xf8; }
  bool IsStatusByte() { return data_[offset_] & 0x80; }
  bool IsReservedStatusByte() {
    return data_[offset_] == 0xf4 || data_[offset_] == 0xf5 ||
           data_[offset_] == 0xf9 || data_[offset_] == 0xfd;
  }

  bool AcceptRealTimeMessages() {
    for (; !IsEndOfData(); offset_++) {
      if (IsRealTimeMessage() && !IsReservedStatusByte())
        continue;
      return true;
    }
    return false;
  }

  bool AcceptCurrentSysex() {
    DCHECK(IsSysex());
    for (offset_++; !IsEndOfData(); offset_++) {
      if (IsReservedStatusByte())
        return false;
      if (IsRealTimeMessage())
        continue;
      if (IsEndOfSysex()) {
        offset_++;
        return true;
      }
      if (IsStatusByte())
        return false;
    }
    return false;
  }

  bool AcceptCurrentMessage() {
    DCHECK(IsStatusByte());
    DCHECK(!IsSysex());
    DCHECK(!IsReservedStatusByte());
    DCHECK(!IsRealTimeMessage());
    DCHECK(!IsEndOfSysex());
    static const std::array<int, 7> kChannelMessageLength = {
        3, 3, 3, 3, 2, 2, 3};  // for 0x8*, 0x9*, ..., 0xe*
    static const std::array<int, 7> kSystemMessageLength = {
        2, 3, 2, 0, 0, 1, 0};  // for 0xf1, 0xf2, ..., 0xf7
    size_t length = IsSystemMessage()
                        ? kSystemMessageLength[data_[offset_] - 0xf1]
                        : kChannelMessageLength[(data_[offset_] >> 4) - 8];
    offset_++;
    DCHECK_GT(length, 0UL);
    if (length == 1)
      return true;
    for (size_t count = 1; !IsEndOfData(); offset_++) {
      if (IsReservedStatusByte())
        return false;
      if (IsRealTimeMessage())
        continue;
      if (IsStatusByte())
        return false;
      if (++count == length) {
        offset_++;
        return true;
      }
    }
    return false;
  }

  String GetPositionString() {
    return "at index " + String::Number(offset_) + " (" +
           String::Number(static_cast<uint16_t>(data_[offset_])) + ").";
  }

  base::span<const uint8_t> data_;
  size_t offset_ = 0;
};

}  // namespace

MIDIOutput::MIDIOutput(MIDIAccess* access,
                       unsigned port_index,
                       const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       PortState state)
    : MIDIPort(access,
               id,
               manufacturer,
               name,
               MIDIPortType::kOutput,
               version,
               state),
      port_index_(port_index) {}

MIDIOutput::~MIDIOutput() = default;

void MIDIOutput::send(NotShared<DOMUint8Array> array,
                      double timestamp_in_milliseconds,
                      ExceptionState& exception_state) {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return;

  base::TimeTicks timestamp;
  if (timestamp_in_milliseconds == 0.0) {
    timestamp = base::TimeTicks::Now();
  } else {
    timestamp =
        GetTimeOrigin(context) + base::Milliseconds(timestamp_in_milliseconds);
  }
  SendInternal(array.Get(), timestamp, exception_state);
}

void MIDIOutput::send(Vector<unsigned> unsigned_data,
                      double timestamp_in_milliseconds,
                      ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;

  DOMUint8Array* array = ConvertUnsignedDataToUint8Array(
      std::move(unsigned_data), exception_state);
  if (!array) {
    DCHECK(exception_state.HadException());
    return;
  }

  send(NotShared<DOMUint8Array>(array), timestamp_in_milliseconds,
       exception_state);
}

void MIDIOutput::send(NotShared<DOMUint8Array> data,
                      ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;

  DCHECK(data);
  SendInternal(data.Get(), base::TimeTicks::Now(), exception_state);
}

void MIDIOutput::send(Vector<unsigned> unsigned_data,
                      ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return;

  DOMUint8Array* array = ConvertUnsignedDataToUint8Array(
      std::move(unsigned_data), exception_state);
  if (!array) {
    DCHECK(exception_state.HadException());
    return;
  }

  SendInternal(array, base::TimeTicks::Now(), exception_state);
}

void MIDIOutput::DidOpen(bool opened) {
  if (!opened)
    pending_data_.clear();

  HeapVector<std::pair<Member<DOMUint8Array>, base::TimeTicks>> queued_data;
  queued_data.swap(pending_data_);
  for (auto& data : queued_data) {
    midiAccess()->SendMIDIData(
        port_index_, data.first->Data(),
        base::checked_cast<wtf_size_t>(data.first->length()), data.second);
  }
  queued_data.clear();
  DCHECK(pending_data_.empty());
}

void MIDIOutput::Trace(Visitor* visitor) const {
  MIDIPort::Trace(visitor);
  visitor->Trace(pending_data_);
}

void MIDIOutput::SendInternal(DOMUint8Array* array,
                              base::TimeTicks timestamp,
                              ExceptionState& exception_state) {
  DCHECK(GetExecutionContext());
  DCHECK(array);
  DCHECK(!timestamp.is_null());
  UseCounter::Count(GetExecutionContext(), WebFeature::kMIDIOutputSend);

  // Implicit open. It does nothing if the port is already opened.
  // This should be performed even if |array| is invalid.
  open();

  if (!MessageValidator::Validate(array, exception_state,
                                  midiAccess()->sysexEnabled()))
    return;

  if (IsOpening()) {
    pending_data_.emplace_back(array, timestamp);
  } else {
    midiAccess()->SendMIDIData(port_index_, array->Data(),
                               base::checked_cast<wtf_size_t>(array->length()),
                               timestamp);
  }
}

}  // namespace blink
