// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error.h"
#include "third_party/blink/renderer/modules/nfc/nfc_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_push_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_watch_options.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace {
const char kJsonMimePostfix[] = "+json";
const char kJsonMimePrefix[] = "application/";
const char kJsonMimeType[] = "application/json";
const char kOpaqueMimeType[] = "application/octet-stream";
const char kPlainTextMimeType[] = "text/plain";
const char kPlainTextMimePrefix[] = "text/";
const char kProtocolHttps[] = "https";
const char kCharSetUTF8[] = ";charset=UTF-8";
}  // anonymous namespace

// Mojo type converters
namespace mojo {

using device::mojom::blink::NFCMessage;
using device::mojom::blink::NFCMessagePtr;
using device::mojom::blink::NFCRecord;
using device::mojom::blink::NFCRecordPtr;
using device::mojom::blink::NFCRecordType;
using device::mojom::blink::NFCRecordTypeFilter;
using device::mojom::blink::NFCPushOptions;
using device::mojom::blink::NFCPushOptionsPtr;
using device::mojom::blink::NFCPushTarget;
using device::mojom::blink::NFCWatchMode;
using device::mojom::blink::NFCWatchOptions;
using device::mojom::blink::NFCWatchOptionsPtr;

NFCPushTarget toNFCPushTarget(const String& target) {
  if (target == "tag")
    return NFCPushTarget::TAG;

  if (target == "peer")
    return NFCPushTarget::PEER;

  return NFCPushTarget::ANY;
}

NFCRecordType toNFCRecordType(const String& recordType) {
  if (recordType == "empty")
    return NFCRecordType::EMPTY;

  if (recordType == "text")
    return NFCRecordType::TEXT;

  if (recordType == "url")
    return NFCRecordType::URL;

  if (recordType == "json")
    return NFCRecordType::JSON;

  if (recordType == "opaque")
    return NFCRecordType::OPAQUE_RECORD;

  NOTREACHED();
  return NFCRecordType::EMPTY;
}

NFCWatchMode toNFCWatchMode(const String& watchMode) {
  if (watchMode == "web-nfc-only")
    return NFCWatchMode::WEBNFC_ONLY;

  if (watchMode == "any")
    return NFCWatchMode::ANY;

  NOTREACHED();
  return NFCWatchMode::WEBNFC_ONLY;
}

// https://w3c.github.io/web-nfc/#creating-web-nfc-message Step 2.1
// If NFCRecord type is not provided, deduce NFCRecord type from JS data type:
// String or Number => 'text' record
// ArrayBuffer => 'opaque' record
// JSON serializable Object => 'json' record
NFCRecordType deduceRecordTypeFromDataType(const blink::NFCRecord& record) {
  if (record.hasData()) {
    v8::Local<v8::Value> value = record.data().V8Value();

    if (value->IsString() ||
        (value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value()))) {
      return NFCRecordType::TEXT;
    }

    if (value->IsObject() && !value->IsArrayBuffer()) {
      return NFCRecordType::JSON;
    }

    if (value->IsArrayBuffer()) {
      return NFCRecordType::OPAQUE_RECORD;
    }
  }

  return NFCRecordType::EMPTY;
}

void setMediaType(NFCRecordPtr& recordPtr,
                  const String& recordMediaType,
                  const String& defaultMediaType) {
  recordPtr->media_type =
      recordMediaType.IsEmpty() ? defaultMediaType : recordMediaType;
}

template <>
struct TypeConverter<Vector<uint8_t>, String> {
  static Vector<uint8_t> Convert(const String& string) {
    CString utf8String = string.Utf8();
    Vector<uint8_t> array;
    array.Append(utf8String.data(), utf8String.length());
    return array;
  }
};

template <>
struct TypeConverter<Vector<uint8_t>, blink::DOMArrayBuffer*> {
  static Vector<uint8_t> Convert(blink::DOMArrayBuffer* buffer) {
    Vector<uint8_t> array;
    array.Append(static_cast<uint8_t*>(buffer->Data()), buffer->ByteLength());
    return array;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, String> {
  static NFCRecordPtr Convert(const String& string) {
    NFCRecordPtr record = NFCRecord::New();
    record->record_type = NFCRecordType::TEXT;
    record->media_type = kPlainTextMimeType;
    record->media_type.append(kCharSetUTF8);
    record->data = mojo::ConvertTo<Vector<uint8_t>>(string);
    return record;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::DOMArrayBuffer*> {
  static NFCRecordPtr Convert(blink::DOMArrayBuffer* buffer) {
    NFCRecordPtr record = NFCRecord::New();
    record->record_type = NFCRecordType::OPAQUE_RECORD;
    record->media_type = kOpaqueMimeType;
    record->data = mojo::ConvertTo<Vector<uint8_t>>(buffer);
    return record;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, String> {
  static NFCMessagePtr Convert(const String& string) {
    NFCMessagePtr message = NFCMessage::New();
    message->data.push_back(NFCRecord::From(string));
    return message;
  }
};

template <>
struct TypeConverter<base::Optional<Vector<uint8_t>>, blink::ScriptValue> {
  static base::Optional<Vector<uint8_t>> Convert(
      const blink::ScriptValue& scriptValue) {
    v8::Local<v8::Value> value = scriptValue.V8Value();

    if (value->IsNumber()) {
      return mojo::ConvertTo<Vector<uint8_t>>(
          String::Number(value.As<v8::Number>()->Value()));
    }

    if (value->IsString()) {
      blink::V8StringResource<> stringResource = value;
      if (stringResource.Prepare()) {
        return mojo::ConvertTo<Vector<uint8_t>>(String(stringResource));
      }
    }

    if (value->IsObject() && !value->IsArray() && !value->IsArrayBuffer()) {
      v8::Local<v8::String> jsonString;
      v8::Isolate* isolate = scriptValue.GetIsolate();
      v8::TryCatch try_catch(isolate);

      // https://w3c.github.io/web-nfc/#mapping-json-to-ndef
      // If serialization throws, reject promise with a "SyntaxError" exception.
      if (!v8::JSON::Stringify(scriptValue.GetContext(), value.As<v8::Object>())
               .ToLocal(&jsonString) ||
          try_catch.HasCaught()) {
        return base::nullopt;
      }

      String string =
          blink::ToBlinkString<String>(jsonString, blink::kDoNotExternalize);
      return mojo::ConvertTo<Vector<uint8_t>>(string);
    }

    if (value->IsArrayBuffer()) {
      return mojo::ConvertTo<Vector<uint8_t>>(
          blink::V8ArrayBuffer::ToImpl(value.As<v8::Object>()));
    }

    return base::nullopt;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::NFCRecord> {
  static NFCRecordPtr Convert(const blink::NFCRecord& record) {
    NFCRecordPtr recordPtr = NFCRecord::New();

    if (record.hasRecordType())
      recordPtr->record_type = toNFCRecordType(record.recordType());
    else
      recordPtr->record_type = deduceRecordTypeFromDataType(record);

    // If record type is "empty", no need to set media type or data.
    // https://w3c.github.io/web-nfc/#creating-web-nfc-message
    if (recordPtr->record_type == NFCRecordType::EMPTY)
      return recordPtr;

    switch (recordPtr->record_type) {
      case NFCRecordType::TEXT:
      case NFCRecordType::URL:
        setMediaType(recordPtr, record.mediaType(), kPlainTextMimeType);
        recordPtr->media_type.append(kCharSetUTF8);
        break;
      case NFCRecordType::JSON:
        setMediaType(recordPtr, record.mediaType(), kJsonMimeType);
        break;
      case NFCRecordType::OPAQUE_RECORD:
        setMediaType(recordPtr, record.mediaType(), kOpaqueMimeType);
        break;
      default:
        NOTREACHED();
        break;
    }

    auto recordData =
        mojo::ConvertTo<base::Optional<Vector<uint8_t>>>(record.data());
    // If JS object cannot be converted to uint8_t array, return null,
    // interrupt NFCMessage conversion algorithm and reject promise with
    // SyntaxError exception.
    if (!recordData)
      return nullptr;

    recordPtr->data = recordData.value();
    return recordPtr;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCMessage> {
  static NFCMessagePtr Convert(const blink::NFCMessage& message) {
    NFCMessagePtr messagePtr = NFCMessage::New();
    messagePtr->url = message.url();
    messagePtr->data.resize(message.records().size());
    for (wtf_size_t i = 0; i < message.records().size(); ++i) {
      NFCRecordPtr record = NFCRecord::From(message.records()[i]);
      if (record.is_null())
        return nullptr;

      messagePtr->data[i] = std::move(record);
    }
    return messagePtr;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::DOMArrayBuffer*> {
  static NFCMessagePtr Convert(blink::DOMArrayBuffer* buffer) {
    NFCMessagePtr message = NFCMessage::New();
    message->data.push_back(NFCRecord::From(buffer));
    return message;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCPushMessage> {
  static NFCMessagePtr Convert(const blink::NFCPushMessage& message) {
    if (message.IsString())
      return NFCMessage::From(message.GetAsString());

    if (message.IsNFCMessage())
      return NFCMessage::From(message.GetAsNFCMessage());

    if (message.IsArrayBuffer())
      return NFCMessage::From(message.GetAsArrayBuffer());

    NOTREACHED();
    return nullptr;
  }
};

template <>
struct TypeConverter<NFCPushOptionsPtr, blink::NFCPushOptions> {
  static NFCPushOptionsPtr Convert(const blink::NFCPushOptions& pushOptions) {
    // https://w3c.github.io/web-nfc/#the-nfcpushoptions-dictionary
    // Default values for NFCPushOptions dictionary are:
    // target = 'any', timeout = Infinity, ignoreRead = true
    NFCPushOptionsPtr pushOptionsPtr = NFCPushOptions::New();

    if (pushOptions.hasTarget())
      pushOptionsPtr->target = toNFCPushTarget(pushOptions.target());
    else
      pushOptionsPtr->target = NFCPushTarget::ANY;

    if (pushOptions.hasTimeout())
      pushOptionsPtr->timeout = pushOptions.timeout();
    else
      pushOptionsPtr->timeout = std::numeric_limits<double>::infinity();

    if (pushOptions.hasIgnoreRead())
      pushOptionsPtr->ignore_read = pushOptions.ignoreRead();
    else
      pushOptionsPtr->ignore_read = true;

    return pushOptionsPtr;
  }
};

template <>
struct TypeConverter<NFCWatchOptionsPtr, blink::NFCWatchOptions> {
  static NFCWatchOptionsPtr Convert(
      const blink::NFCWatchOptions& watchOptions) {
    // https://w3c.github.io/web-nfc/#the-nfcwatchoptions-dictionary
    // Default values for NFCWatchOptions dictionary are:
    // url = "", recordType = null, mediaType = "", mode = "web-nfc-only"
    NFCWatchOptionsPtr watchOptionsPtr = NFCWatchOptions::New();
    watchOptionsPtr->url = watchOptions.url();
    watchOptionsPtr->media_type = watchOptions.mediaType();

    if (watchOptions.hasMode())
      watchOptionsPtr->mode = toNFCWatchMode(watchOptions.mode());
    else
      watchOptionsPtr->mode = NFCWatchMode::WEBNFC_ONLY;

    if (watchOptions.hasRecordType()) {
      watchOptionsPtr->record_filter = NFCRecordTypeFilter::New();
      watchOptionsPtr->record_filter->record_type =
          toNFCRecordType(watchOptions.recordType());
    }

    return watchOptionsPtr;
  }
};

}  // namespace mojo

namespace blink {
namespace {

ScriptPromise RejectWithTypeError(ScriptState* script_state,
                                  const String& message) {
  return ScriptPromise::Reject(
      script_state,
      V8ThrowException::CreateTypeError(script_state->GetIsolate(), message));
}

ScriptPromise RejectWithDOMException(ScriptState* script_state,
                                     DOMExceptionCode exception_code,
                                     const String& message) {
  return ScriptPromise::RejectWithDOMException(
      script_state, DOMException::Create(exception_code, message));
}

ScriptPromise RejectIfInvalidTextRecord(ScriptState* script_state,
                                        const NFCRecord& record) {
  v8::Local<v8::Value> value = record.data().V8Value();
  if (!value->IsString() &&
      !(value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value()))) {
    return RejectWithTypeError(script_state,
                               "The data for 'text' NFCRecords must be of "
                               "String or UnrestrctedDouble type.");
  }

  if (record.hasMediaType() &&
      !record.mediaType().StartsWithIgnoringASCIICase(kPlainTextMimePrefix)) {
    return RejectWithDOMException(script_state, DOMExceptionCode::kSyntaxError,
                                  "Invalid media type for 'text' record.");
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidURLRecord(ScriptState* script_state,
                                       const NFCRecord& record) {
  if (!record.data().V8Value()->IsString()) {
    return RejectWithTypeError(
        script_state, "The data for 'url' NFCRecord must be of String type.");
  }

  blink::V8StringResource<> string_resource = record.data().V8Value();
  if (!string_resource.Prepare() ||
      !KURL(NullURL(), string_resource).IsValid()) {
    return RejectWithDOMException(script_state, DOMExceptionCode::kSyntaxError,
                                  "Cannot parse data for 'url' record.");
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidJSONRecord(ScriptState* script_state,
                                        const NFCRecord& record) {
  v8::Local<v8::Value> value = record.data().V8Value();
  if (!value->IsObject() || value->IsArrayBuffer()) {
    return RejectWithTypeError(
        script_state, "The data for 'json' NFCRecord must be of Object type.");
  }

  // If JSON record has media type, it must be equal to "application/json" or
  // start with "application/" and end with "+json".
  if (record.hasMediaType() &&
      (record.mediaType() != kJsonMimeType &&
       !(record.mediaType().StartsWithIgnoringASCIICase(kJsonMimePrefix) &&
         record.mediaType().EndsWithIgnoringASCIICase(kJsonMimePostfix)))) {
    return RejectWithDOMException(script_state, DOMExceptionCode::kSyntaxError,
                                  "Invalid media type for 'json' record.");
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidOpaqueRecord(ScriptState* script_state,
                                          const NFCRecord& record) {
  if (!record.data().V8Value()->IsArrayBuffer()) {
    return RejectWithTypeError(
        script_state,
        "The data for 'opaque' NFCRecord must be of ArrayBuffer type.");
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidNFCRecord(ScriptState* script_state,
                                       const NFCRecord& record) {
  device::mojom::blink::NFCRecordType type;
  if (record.hasRecordType()) {
    type = mojo::toNFCRecordType(record.recordType());
  } else {
    type = mojo::deduceRecordTypeFromDataType(record);

    // https://w3c.github.io/web-nfc/#creating-web-nfc-message
    // If NFCRecord.recordType is not set and record type cannot be deduced
    // from NFCRecord.data, reject promise with TypeError.
    if (type == device::mojom::blink::NFCRecordType::EMPTY)
      return RejectWithTypeError(script_state, "Unknown NFCRecord type.");
  }

  // Non-empty records must have data.
  if (!record.hasData() &&
      (type != device::mojom::blink::NFCRecordType::EMPTY)) {
    return RejectWithTypeError(script_state,
                               "Nonempty NFCRecord must have data.");
  }

  switch (type) {
    case device::mojom::blink::NFCRecordType::TEXT:
      return RejectIfInvalidTextRecord(script_state, record);
    case device::mojom::blink::NFCRecordType::URL:
      return RejectIfInvalidURLRecord(script_state, record);
    case device::mojom::blink::NFCRecordType::JSON:
      return RejectIfInvalidJSONRecord(script_state, record);
    case device::mojom::blink::NFCRecordType::OPAQUE_RECORD:
      return RejectIfInvalidOpaqueRecord(script_state, record);
    case device::mojom::blink::NFCRecordType::EMPTY:
      return ScriptPromise();
  }

  NOTREACHED();
  return RejectWithTypeError(script_state,
                             "Invalid NFCRecordType was provided.");
}

ScriptPromise RejectIfInvalidNFCRecordArray(
    ScriptState* script_state,
    const HeapVector<NFCRecord>& records) {
  for (const auto& record : records) {
    ScriptPromise isValidRecord =
        RejectIfInvalidNFCRecord(script_state, record);
    if (!isValidRecord.IsEmpty())
      return isValidRecord;
  }

  return ScriptPromise();
}

ScriptPromise RejectIfInvalidNFCPushMessage(
    ScriptState* script_state,
    const NFCPushMessage& push_message) {
  // If NFCPushMessage of invalid type, reject promise with TypeError
  if (!push_message.IsNFCMessage() && !push_message.IsString() &&
      !push_message.IsArrayBuffer()) {
    return RejectWithTypeError(script_state,
                               "Invalid NFCPushMessage type was provided.");
  }

  if (push_message.IsNFCMessage()) {
    // https://w3c.github.io/web-nfc/#the-push-method
    // If NFCMessage.records is empty, reject promise with TypeError
    const NFCMessage& message = push_message.GetAsNFCMessage();
    if (!message.hasRecords() || message.records().IsEmpty()) {
      return RejectWithTypeError(script_state,
                                 "Empty NFCMessage was provided.");
    }

    return RejectIfInvalidNFCRecordArray(script_state, message.records());
  }

  return ScriptPromise();
}

bool SetURL(const String& origin,
            device::mojom::blink::NFCMessagePtr& message) {
  KURL origin_url(origin);

  if (!message->url.IsEmpty() && origin_url.CanSetPathname()) {
    origin_url.SetPath(message->url);
  }

  message->url = origin_url;
  return origin_url.IsValid();
}

String ToNFCRecordType(const device::mojom::blink::NFCRecordType& type) {
  switch (type) {
    case device::mojom::blink::NFCRecordType::TEXT:
      return "text";
    case device::mojom::blink::NFCRecordType::URL:
      return "url";
    case device::mojom::blink::NFCRecordType::JSON:
      return "json";
    case device::mojom::blink::NFCRecordType::OPAQUE_RECORD:
      return "opaque";
    case device::mojom::blink::NFCRecordType::EMPTY:
      return "empty";
  }

  NOTREACHED();
  return String();
}

v8::Local<v8::Value> ToV8(ScriptState* script_state,
                          const device::mojom::blink::NFCRecordPtr& record) {
  switch (record->record_type) {
    case device::mojom::blink::NFCRecordType::TEXT:
    case device::mojom::blink::NFCRecordType::URL:
    case device::mojom::blink::NFCRecordType::JSON: {
      String string_data;
      if (!record->data.IsEmpty()) {
        string_data = String::FromUTF8WithLatin1Fallback(
            static_cast<unsigned char*>(&record->data.front()),
            record->data.size());
      }

      v8::Isolate* isolate = script_state->GetIsolate();
      v8::Local<v8::String> string = V8String(isolate, string_data);

      // Stringified JSON must be converted back to an Object.
      if (record->record_type == device::mojom::blink::NFCRecordType::JSON) {
        v8::Local<v8::Value> json_object;
        v8::TryCatch try_catch(isolate);
        if (!v8::JSON::Parse(script_state->GetContext(), string)
                 .ToLocal(&json_object)) {
          return v8::Null(isolate);
        }

        return json_object;
      }

      return string;
    }

    case device::mojom::blink::NFCRecordType::OPAQUE_RECORD: {
      if (!record->data.IsEmpty()) {
        DOMArrayBuffer* buffer = DOMArrayBuffer::Create(
            static_cast<void*>(&record->data.front()), record->data.size());
        return ToV8(buffer, script_state->GetContext()->Global(),
                    script_state->GetIsolate());
      }

      return v8::Null(script_state->GetIsolate());
    }

    case device::mojom::blink::NFCRecordType::EMPTY:
      return v8::Null(script_state->GetIsolate());
  }

  NOTREACHED();
  return v8::Local<v8::Value>();
}

NFCRecord ToNFCRecord(ScriptState* script_state,
                      const device::mojom::blink::NFCRecordPtr& record) {
  NFCRecord nfc_record;
  nfc_record.setMediaType(record->media_type);
  nfc_record.setRecordType(ToNFCRecordType(record->record_type));
  nfc_record.setData(ScriptValue(script_state, ToV8(script_state, record)));
  return nfc_record;
}

NFCMessage ToNFCMessage(ScriptState* script_state,
                        const device::mojom::blink::NFCMessagePtr& message) {
  NFCMessage nfc_message;
  nfc_message.setURL(message->url);
  blink::HeapVector<NFCRecord> records;
  for (wtf_size_t i = 0; i < message->data.size(); ++i)
    records.push_back(ToNFCRecord(script_state, message->data[i]));
  nfc_message.setRecords(records);
  return nfc_message;
}

size_t GetNFCMessageSize(const device::mojom::blink::NFCMessagePtr& message) {
  size_t message_size = message->url.CharactersSizeInBytes();
  for (wtf_size_t i = 0; i < message->data.size(); ++i) {
    message_size += message->data[i]->media_type.CharactersSizeInBytes();
    message_size += message->data[i]->data.size();
  }
  return message_size;
}

}  // namespace

NFC::NFC(LocalFrame* frame)
    : PageVisibilityObserver(frame->GetPage()),
      ContextLifecycleObserver(frame->GetDocument()),
      client_binding_(this) {
  String error_message;

  // Only connect to NFC if we are in a context that supports it.
  if (!IsSupportedInContext(GetExecutionContext(), error_message))
    return;

  frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&nfc_));
  nfc_.set_connection_error_handler(
      WTF::Bind(&NFC::OnConnectionError, WrapWeakPersistent(this)));
  device::mojom::blink::NFCClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));
  nfc_->SetClient(std::move(client));
}

NFC* NFC::Create(LocalFrame* frame) {
  NFC* nfc = new NFC(frame);
  return nfc;
}

NFC::~NFC() {
  // |m_nfc| may hold persistent handle to |this| object, therefore, there
  // should be no more outstanding requests when NFC object is destructed.
  DCHECK(requests_.IsEmpty());
}

void NFC::Dispose() {
  client_binding_.Close();
}

void NFC::ContextDestroyed(ExecutionContext*) {
  nfc_.reset();
  requests_.clear();
  callbacks_.clear();
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
// https://w3c.github.io/web-nfc/#dom-nfc-push
ScriptPromise NFC::push(ScriptState* script_state,
                        const NFCPushMessage& push_message,
                        const NFCPushOptions& options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  ScriptPromise isValidMessage =
      RejectIfInvalidNFCPushMessage(script_state, push_message);
  if (!isValidMessage.IsEmpty())
    return isValidMessage;

  // https://w3c.github.io/web-nfc/#dom-nfc-push
  // 9. If timeout value is NaN or negative, reject promise with "TypeError"
  // and abort these steps.
  if (options.hasTimeout() &&
      (std::isnan(options.timeout()) || options.timeout() < 0)) {
    return RejectWithTypeError(
        script_state, "Invalid NFCPushOptions.timeout value was provided.");
  }

  device::mojom::blink::NFCMessagePtr message =
      device::mojom::blink::NFCMessage::From(push_message);
  if (!message) {
    return RejectWithDOMException(script_state, DOMExceptionCode::kSyntaxError,
                                  "Cannot convert NFCMessage.");
  }

  if (!SetURL(
          ExecutionContext::From(script_state)->GetSecurityOrigin()->ToString(),
          message)) {
    return RejectWithDOMException(script_state, DOMExceptionCode::kSyntaxError,
                                  "Cannot set WebNFC Id.");
  }

  if (GetNFCMessageSize(message) > device::mojom::blink::NFCMessage::kMaxSize) {
    return RejectWithDOMException(script_state,
                                  DOMExceptionCode::kNotSupportedError,
                                  "NFCMessage exceeds maximum supported size.");
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  auto callback = WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                            WrapPersistent(resolver));
  nfc_->Push(std::move(message),
             device::mojom::blink::NFCPushOptions::From(options),
             std::move(callback));

  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelpush
ScriptPromise NFC::cancelPush(ScriptState* script_state, const String& target) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  auto callback = WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                            WrapPersistent(resolver));
  nfc_->CancelPush(mojo::toNFCPushTarget(target), std::move(callback));

  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#watching-for-content
// https://w3c.github.io/web-nfc/#dom-nfc-watch
ScriptPromise NFC::watch(ScriptState* script_state,
                         V8MessageCallback* callback,
                         const NFCWatchOptions& options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  // https://w3c.github.io/web-nfc/#dom-nfc-watch (Step 9)
  if (options.hasURL() && !options.url().IsEmpty()) {
    KURL pattern_url(options.url());
    if (!pattern_url.IsValid() || pattern_url.Protocol() != kProtocolHttps) {
      return RejectWithDOMException(script_state,
                                    DOMExceptionCode::kSyntaxError,
                                    "Invalid URL pattern was provided.");
    }
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  auto watch_callback =
      WTF::Bind(&NFC::OnWatchRegistered, WrapPersistent(this),
                WrapPersistent(ToV8PersistentCallbackFunction(callback)),
                WrapPersistent(resolver));
  nfc_->Watch(device::mojom::blink::NFCWatchOptions::From(options),
              std::move(watch_callback));
  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
ScriptPromise NFC::cancelWatch(ScriptState* script_state, int32_t id) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  if (id) {
    callbacks_.erase(id);
  } else {
    return RejectWithDOMException(script_state,
                                  DOMExceptionCode::kNotFoundError,
                                  "Provided watch id cannot be found.");
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  nfc_->CancelWatch(id,
                    WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                              WrapPersistent(resolver)));
  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
// If watchId is not provided to nfc.cancelWatch, cancel all watch operations.
ScriptPromise NFC::cancelWatch(ScriptState* script_state) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  callbacks_.clear();
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  nfc_->CancelAllWatches(WTF::Bind(&NFC::OnRequestCompleted,
                                   WrapPersistent(this),
                                   WrapPersistent(resolver)));
  return resolver->Promise();
}

void NFC::PageVisibilityChanged() {
  // If service is not initialized, there cannot be any pending NFC activities
  if (!nfc_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (GetPage()->VisibilityState() == mojom::PageVisibilityState::kVisible)
    nfc_->ResumeNFCOperations();
  else
    nfc_->SuspendNFCOperations();
}

void NFC::OnRequestCompleted(ScriptPromiseResolver* resolver,
                             device::mojom::blink::NFCErrorPtr error) {
  if (!requests_.Contains(resolver))
    return;

  requests_.erase(resolver);
  if (error.is_null())
    resolver->Resolve();
  else
    resolver->Reject(NFCError::Take(resolver, error->error_type));
}

void NFC::OnConnectionError() {
  nfc_.reset();
  callbacks_.clear();

  // If NFCService is not available or disappears when NFC hardware is
  // disabled, reject promise with NotSupportedError exception.
  for (ScriptPromiseResolver* resolver : requests_)
    resolver->Reject(NFCError::Take(
        resolver, device::mojom::blink::NFCErrorType::NOT_SUPPORTED));

  requests_.clear();
}

void NFC::OnWatch(const Vector<uint32_t>& ids,
                  device::mojom::blink::NFCMessagePtr message) {
  if (!GetExecutionContext())
    return;

  for (const auto& id : ids) {
    auto it = callbacks_.find(id);
    if (it != callbacks_.end()) {
      V8MessageCallback* callback = it->value;
      ScriptState* script_state = callback->CallbackRelevantScriptState();
      DCHECK(script_state);
      ScriptState::Scope scope(script_state);
      NFCMessage nfc_message = ToNFCMessage(script_state, message);
      callback->InvokeAndReportException(nullptr, nfc_message);
    }
  }
}

bool NFC::IsSupportedInContext(ExecutionContext* context,
                               String& error_message) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!To<Document>(context)->domWindow()->GetFrame() ||
      !To<Document>(context)->GetFrame()->IsMainFrame()) {
    error_message = "Must be in a top-level browsing context";
    return false;
  }

  return true;
}

ScriptPromise NFC::RejectIfNotSupported(ScriptState* script_state) {
  String error_message;
  if (!IsSupportedInContext(ExecutionContext::From(script_state),
                            error_message)) {
    return RejectWithDOMException(
        script_state, DOMExceptionCode::kSecurityError, error_message);
  }

  if (!nfc_) {
    return RejectWithDOMException(script_state,
                                  DOMExceptionCode::kNotSupportedError,
                                  "WebNFC is not supported.");
  }

  return ScriptPromise();
}

void NFC::OnWatchRegistered(
    V8PersistentCallbackFunction<V8MessageCallback>* callback,
    ScriptPromiseResolver* resolver,
    uint32_t id,
    device::mojom::blink::NFCErrorPtr error) {
  requests_.erase(resolver);

  // Invalid id was returned.
  // https://w3c.github.io/web-nfc/#dom-nfc-watch
  // 8. If the request fails, reject promise with "NotSupportedError"
  // and abort these steps.
  if (!id) {
    resolver->Reject(NFCError::Take(
        resolver, device::mojom::blink::NFCErrorType::NOT_SUPPORTED));
    return;
  }

  if (error.is_null()) {
    callbacks_.insert(id, callback->ToNonV8Persistent());
    resolver->Resolve(id);
  } else {
    resolver->Reject(NFCError::Take(resolver, error->error_type));
  }
}

void NFC::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  visitor->Trace(callbacks_);
  ScriptWrappable::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
