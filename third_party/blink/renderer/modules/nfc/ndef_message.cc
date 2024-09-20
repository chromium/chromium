// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_message.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_message_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_record_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_arraybuffer_arraybufferview_ndefmessageinit_string.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

// Spec-defined maximum recursion depth for NDEF messages.
// https://w3c.github.io/web-nfc/#creating-ndef-message
constexpr uint8_t kMaxRecursionDepth = 32;

constexpr char kRecursionLimitExceededErrorMessage[] =
    "NDEFMessage recursion limit exceeded.";

}  // namespace

// static
NDEFMessage* NDEFMessage::Create(const ScriptState* script_state,
                                 const NDEFMessageInit* init,
                                 ExceptionState& exception_state,
                                 uint8_t records_depth,
                                 bool is_embedded) {
  // https://w3c.github.io/web-nfc/#creating-ndef-message

  // NDEFMessageInit#records is a required field.
  DCHECK(init->hasRecords());
  if (init->records().empty()) {
    exception_state.ThrowTypeError(
        "NDEFMessageInit#records being empty makes no sense.");
    return nullptr;
  }

  if (++records_depth > kMaxRecursionDepth) {
    exception_state.ThrowTypeError(kRecursionLimitExceededErrorMessage);
    return nullptr;
  }

  NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
  for (const NDEFRecordInit* record_init : init->records()) {
    NDEFRecord* record = NDEFRecord::Create(
        script_state, record_init, exception_state, records_depth, is_embedded);
    if (exception_state.HadException())
      return nullptr;
    DCHECK(record);
    message->records_.push_back(record);
  }
  return message;
}

// static
NDEFMessage* NDEFMessage::Create(const ScriptState* script_state,
                                 const V8NDEFMessageSource* source,
                                 ExceptionState& exception_state) {
  DCHECK(source);

  // https://w3c.github.io/web-nfc/#creating-ndef-message
  switch (source->GetContentType()) {
    case V8NDEFMessageSource::ContentType::kArrayBuffer: {
      const DOMArrayBuffer* buffer = source->GetAsArrayBuffer();
      if (buffer->ByteLength() > std::numeric_limits<wtf_size_t>::max()) {
        exception_state.ThrowRangeError(
            "Buffer size exceeds maximum heap object size.");
        return nullptr;
      }
      Vector<uint8_t> payload_data;
      payload_data.AppendSpan(buffer->ByteSpan());
      NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
      message->records_.push_back(MakeGarbageCollected<NDEFRecord>(
          String() /* id */, "application/octet-stream",
          std::move(payload_data)));
      return message;
    }
    case V8NDEFMessageSource::ContentType::kArrayBufferView: {
      const DOMArrayBufferView* buffer_view =
          source->GetAsArrayBufferView().Get();
      if (buffer_view->byteLength() > std::numeric_limits<wtf_size_t>::max()) {
        exception_state.ThrowRangeError(
            "Buffer size exceeds maximum heap object size.");
        return nullptr;
      }
      Vector<uint8_t> payload_data;
      payload_data.AppendSpan(buffer_view->ByteSpan());
      NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
      message->records_.push_back(MakeGarbageCollected<NDEFRecord>(
          String() /* id */, "application/octet-stream",
          std::move(payload_data)));
      return message;
    }
    case V8NDEFMessageSource::ContentType::kNDEFMessageInit: {
      return Create(script_state, source->GetAsNDEFMessageInit(),
                    exception_state,
                    /*records_depth=*/0U);
    }
    case V8NDEFMessageSource::ContentType::kString: {
      NDEFMessage* message = MakeGarbageCollected<NDEFMessage>();
      message->records_.push_back(MakeGarbageCollected<NDEFRecord>(
          script_state, source->GetAsString()));
      return message;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
NDEFMessage* NDEFMessage::CreateAsPayloadOfSmartPoster(
    const ScriptState* script_state,
    const NDEFMessageInit* init,
    ExceptionState& exception_state,
    uint8_t records_depth) {
  // NDEFMessageInit#records is a required field.
  DCHECK(init->hasRecords());

  if (++records_depth > kMaxRecursionDepth) {
    exception_state.ThrowTypeError(kRecursionLimitExceededErrorMessage);
    return nullptr;
  }

  NDEFMessage* payload_message = MakeGarbageCollected<NDEFMessage>();

  bool has_url_record = false;
  bool has_size_record = false;
  bool has_type_record = false;
  bool has_action_record = false;
  for (const NDEFRecordInit* record_init : init->records()) {
    const String& record_type = record_init->recordType();
    if (record_type == "url") {
      // The single mandatory url record.
      if (has_url_record) {
        exception_state.ThrowTypeError(
            "'smart-poster' NDEFRecord contains more than one url record.");
        return nullptr;
      }
      has_url_record = true;
    } else if (record_type == ":s") {
      // Zero or one size record.
      if (has_size_record) {
        exception_state.ThrowTypeError(
            "'smart-poster' NDEFRecord contains more than one size record.");
        return nullptr;
      }
      has_size_record = true;
    } else if (record_type == ":t") {
      // Zero or one type record.
      if (has_type_record) {
        exception_state.ThrowTypeError(
            "'smart-poster' NDEFRecord contains more than one type record.");
        return nullptr;
      }
      has_type_record = true;
    } else if (record_type == ":act") {
      // Zero or one action record.
      if (has_action_record) {
        exception_state.ThrowTypeError(
            "'smart-poster' NDEFRecord contains more than one action record.");
        return nullptr;
      }
      has_action_record = true;
    } else {
      // No restriction on other record types.
    }
    NDEFRecord* record =
        NDEFRecord::Create(script_state, record_init, exception_state,
                           records_depth, /*is_embedded=*/true);
    if (exception_state.HadException())
      return nullptr;
    DCHECK(record);

    if (record->recordType() == ":s" && record->payloadData().size() != 4) {
      exception_state.ThrowTypeError(
          "Size record of smart-poster must contain a 4-byte 32 bit unsigned "
          "integer.");
      return nullptr;
    }
    if (record->recordType() == ":act" && record->payloadData().size() != 1) {
      exception_state.ThrowTypeError(
          "Action record of smart-poster must contain only a single byte.");
      return nullptr;
    }

    payload_message->records_.push_back(record);
  }

  if (!has_url_record) {
    exception_state.ThrowTypeError(
        "'smart-poster' NDEFRecord is missing the single mandatory url "
        "record.");
    return nullptr;
  }

  return payload_message;
}

NDEFMessage::NDEFMessage() = default;

NDEFMessage::NDEFMessage(const device::mojom::blink::NDEFMessage& message) {
  for (wtf_size_t i = 0; i < message.data.size(); ++i) {
    records_.push_back(MakeGarbageCollected<NDEFRecord>(*message.data[i]));
  }
}

const HeapVector<Member<NDEFRecord>>& NDEFMessage::records() const {
  return records_;
}

void NDEFMessage::Trace(Visitor* visitor) const {
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
