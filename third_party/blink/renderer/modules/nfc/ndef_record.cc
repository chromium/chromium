// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/ndef_record.h"

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_array_buffer_or_array_buffer_view_or_ndef_message_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_record_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

using NDEFRecordDataSource =
    StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

namespace {

WTF::Vector<uint8_t> GetUTF8DataFromString(const String& string) {
  StringUTF8Adaptor utf8_string(string);
  WTF::Vector<uint8_t> data;
  data.Append(utf8_string.data(), utf8_string.size());
  return data;
}

bool IsBufferSource(const NDEFRecordDataSource& data) {
  return data.IsArrayBuffer() || data.IsArrayBufferView();
}

bool GetBytesOfBufferSource(const NDEFRecordDataSource& buffer_source,
                            WTF::Vector<uint8_t>* target,
                            ExceptionState& exception_state) {
  DCHECK(IsBufferSource(buffer_source));
  uint8_t* data;
  size_t data_length;
  if (buffer_source.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = buffer_source.GetAsArrayBuffer();
    data = reinterpret_cast<uint8_t*>(array_buffer->Data());
    data_length = array_buffer->ByteLength();
  } else if (buffer_source.IsArrayBufferView()) {
    const DOMArrayBufferView* array_buffer_view =
        buffer_source.GetAsArrayBufferView().View();
    data = reinterpret_cast<uint8_t*>(array_buffer_view->BaseAddress());
    data_length = array_buffer_view->byteLength();
  } else {
    NOTREACHED();
    return false;
  }
  wtf_size_t checked_length;
  if (!base::CheckedNumeric<wtf_size_t>(data_length)
           .AssignIfValid(&checked_length)) {
    exception_state.ThrowRangeError(
        "The provided buffer source exceeds the maximum supported length");
    return false;
  }
  target->Append(data, checked_length);
  return true;
}

// https://w3c.github.io/web-nfc/#dfn-validate-external-type
// Validates |input| as an external type.
bool IsValidExternalType(const String& input) {
  static const String kOtherCharsForCustomType(":!()+,-=@;$_*'.");

  // Ensure |input| is an ASCII string.
  if (!input.ContainsOnlyASCIIOrEmpty())
    return false;

  // As all characters in |input| is ASCII, limiting its length within 255 just
  // limits the length of its utf-8 encoded bytes we finally write into the
  // record payload.
  if (input.IsEmpty() || input.length() > 255)
    return false;

  // Finds the first occurrence of ':'.
  wtf_size_t colon_index = input.find(':');
  if (colon_index == kNotFound)
    return false;

  // Validates the domain (the part before ':').
  String domain = input.Left(colon_index);
  if (domain.IsEmpty())
    return false;
  // TODO(https://crbug.com/520391): Validate |domain|.

  // Validates the type (the part after ':').
  String type = input.Substring(colon_index + 1);
  if (type.IsEmpty())
    return false;
  for (wtf_size_t i = 0; i < type.length(); i++) {
    if (!IsASCIIAlphanumeric(type[i]) &&
        !kOtherCharsForCustomType.Contains(type[i])) {
      return false;
    }
  }

  return true;
}

// https://w3c.github.io/web-nfc/#dfn-validate-local-type
// Validates |input| as an local type.
bool IsValidLocalType(const String& input) {
  // Ensure |input| is an ASCII string.
  if (!input.ContainsOnlyASCIIOrEmpty())
    return false;

  // The prefix ':' will be omitted when we actually write the record type into
  // the nfc tag. We're taking it into consideration for validating the length
  // here.
  if (input.length() < 2 || input.length() > 256)
    return false;
  if (input[0] != ':')
    return false;
  if (!IsASCIILower(input[1]) && !IsASCIIDigit(input[1]))
    return false;

  // TODO(https://crbug.com/520391): Validate |input| is not equal to the record
  // type of any NDEF record defined in its containing NDEF message.

  return true;
}

String getDocumentLanguage(const ExecutionContext* execution_context) {
  String document_language;
  if (execution_context) {
    Element* document_element =
        To<LocalDOMWindow>(execution_context)->document()->documentElement();
    if (document_element) {
      document_language = document_element->getAttribute(html_names::kLangAttr);
    }
    if (document_language.IsEmpty()) {
      document_language = "en";
    }
  }
  return document_language;
}

static NDEFRecord* CreateTextRecord(const ExecutionContext* execution_context,
                                    const String& id,
                                    const NDEFRecordInit& record,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-string-to-ndef
  if (!record.hasData() ||
      !(record.data().IsString() || IsBufferSource(record.data()))) {
    exception_state.ThrowTypeError(
        "The data for 'text' NDEFRecords must be a String or a BufferSource.");
    return nullptr;
  }

  // Set language to lang if it exists, or the document element's lang
  // attribute, or 'en'.
  String language;
  if (record.hasLang()) {
    language = record.lang();
  } else {
    language = getDocumentLanguage(execution_context);
  }

  // Bits 0 to 5 define the length of the language tag
  // https://w3c.github.io/web-nfc/#text-record
  if (language.length() > 63) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Lang length cannot be stored in 6 bit.");
    return nullptr;
  }

  auto& data = record.data();
  // TODO(crbug.com/1070871): Use encodingOr("utf-8").
  String encoding_label = record.hasEncoding() ? record.encoding() : "utf-8";
  WTF::Vector<uint8_t> bytes;
  if (data.IsString()) {
    if (encoding_label != "utf-8") {
      exception_state.ThrowTypeError(
          "A DOMString data source is always encoded as \"utf-8\" so other "
          "encodings are not allowed.");
      return nullptr;
    }
    StringUTF8Adaptor utf8_string(data.GetAsString());
    bytes.Append(utf8_string.data(), utf8_string.size());
  } else {
    DCHECK(IsBufferSource(data));
    if (encoding_label != "utf-8" && encoding_label != "utf-16" &&
        encoding_label != "utf-16be" && encoding_label != "utf-16le") {
      exception_state.ThrowTypeError(
          "Encoding must be either \"utf-8\", \"utf-16\", \"utf-16be\", or "
          "\"utf-16le\".");
      return nullptr;
    }
    if (!GetBytesOfBufferSource(data, &bytes, exception_state)) {
      return nullptr;
    }
  }

  return MakeGarbageCollected<NDEFRecord>(id, encoding_label, language,
                                          std::move(bytes));
}

// Create a 'url' record or an 'absolute-url' record.
static NDEFRecord* CreateUrlRecord(const String& id,
                                   const NDEFRecordInit& record,
                                   ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-url-to-ndef
  if (!record.hasData() || !record.data().IsString()) {
    exception_state.ThrowTypeError(
        "The data for url NDEFRecord must be a String.");
    return nullptr;
  }

  // No need to check mediaType according to the spec.
  String url = record.data().GetAsString();
  if (!KURL(NullURL(), url).IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Cannot parse data for url record.");
    return nullptr;
  }

  return MakeGarbageCollected<NDEFRecord>(
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized,
      record.recordType(), id, GetUTF8DataFromString(url));
}

static NDEFRecord* CreateMimeRecord(const String& id,
                                    const NDEFRecordInit& record,
                                    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#mapping-binary-data-to-ndef
  if (!record.hasData() || !IsBufferSource(record.data())) {
    exception_state.ThrowTypeError(
        "The data for 'mime' NDEFRecord must be a BufferSource.");
    return nullptr;
  }

  // ExtractMIMETypeFromMediaType() ignores parameters of the MIME type.
  String mime_type;
  if (record.hasMediaType() && !record.mediaType().IsEmpty()) {
    mime_type = ExtractMIMETypeFromMediaType(AtomicString(record.mediaType()));
  } else {
    mime_type = "application/octet-stream";
  }

  WTF::Vector<uint8_t> bytes;
  if (!GetBytesOfBufferSource(record.data(), &bytes, exception_state)) {
    return nullptr;
  }

  return MakeGarbageCollected<NDEFRecord>(id, mime_type, bytes);
}

static NDEFRecord* CreateUnknownRecord(const String& id,
                                       const NDEFRecordInit& record,
                                       ExceptionState& exception_state) {
  if (!record.hasData() || !IsBufferSource(record.data())) {
    exception_state.ThrowTypeError(
        "The data for 'unknown' NDEFRecord must be a BufferSource.");
    return nullptr;
  }

  WTF::Vector<uint8_t> bytes;
  if (!GetBytesOfBufferSource(record.data(), &bytes, exception_state)) {
    return nullptr;
  }

  return MakeGarbageCollected<NDEFRecord>(
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized, "unknown",
      id, bytes);
}

static NDEFRecord* CreateSmartPosterRecord(
    const ExecutionContext* execution_context,
    const String& id,
    const NDEFRecordInit& record,
    ExceptionState& exception_state) {
  // https://w3c.github.io/web-nfc/#dfn-map-smart-poster-to-ndef
  if (!record.hasData() || !record.data().IsNDEFMessageInit()) {
    exception_state.ThrowTypeError(
        "The data for 'smart-poster' NDEFRecord must be an NDEFMessageInit.");
    return nullptr;
  }

  NDEFMessage* payload_message = NDEFMessage::CreateAsPayloadOfSmartPoster(
      execution_context, record.data().GetAsNDEFMessageInit(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  DCHECK(payload_message);

  return MakeGarbageCollected<NDEFRecord>(
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized,
      "smart-poster", id, payload_message);
}

static NDEFRecord* CreateExternalRecord(
    const ExecutionContext* execution_context,
    const String& id,
    const NDEFRecordInit& record,
    ExceptionState& exception_state) {
  const String& record_type = record.recordType();

  // https://w3c.github.io/web-nfc/#dfn-map-external-data-to-ndef
  if (record.hasData() && IsBufferSource(record.data())) {
    WTF::Vector<uint8_t> bytes;
    if (!GetBytesOfBufferSource(record.data(), &bytes, exception_state)) {
      return nullptr;
    }
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::blink::NDEFRecordTypeCategory::kExternal, record_type,
        id, bytes);
  } else if (record.hasData() && record.data().IsNDEFMessageInit()) {
    NDEFMessage* payload_message = NDEFMessage::Create(
        execution_context, record.data().GetAsNDEFMessageInit(),
        exception_state, /*is_embedded=*/true);
    if (exception_state.HadException())
      return nullptr;
    DCHECK(payload_message);
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::blink::NDEFRecordTypeCategory::kExternal, record_type,
        id, payload_message);
  }

  exception_state.ThrowTypeError(
      "The data for external type NDEFRecord must be a BufferSource or an "
      "NDEFMessageInit.");
  return nullptr;
}

static NDEFRecord* CreateLocalRecord(const ExecutionContext* execution_context,
                                     const String& id,
                                     const NDEFRecordInit& record,
                                     ExceptionState& exception_state) {
  const String& record_type = record.recordType();

  // https://w3c.github.io/web-nfc/#dfn-map-local-type-to-ndef
  if (record.hasData() && IsBufferSource(record.data())) {
    WTF::Vector<uint8_t> bytes;
    if (!GetBytesOfBufferSource(record.data(), &bytes, exception_state)) {
      return nullptr;
    }
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::blink::NDEFRecordTypeCategory::kLocal, record_type, id,
        bytes);
  } else if (record.hasData() && record.data().IsNDEFMessageInit()) {
    NDEFMessage* payload_message = NDEFMessage::Create(
        execution_context, record.data().GetAsNDEFMessageInit(),
        exception_state, /*is_embedded=*/true);
    if (exception_state.HadException())
      return nullptr;
    DCHECK(payload_message);
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::blink::NDEFRecordTypeCategory::kLocal, record_type, id,
        payload_message);
  }

  exception_state.ThrowTypeError(
      "The data for local type NDEFRecord must be a BufferSource or an "
      "NDEFMessageInit.");
  return nullptr;
}

}  // namespace

// static
NDEFRecord* NDEFRecord::Create(const ExecutionContext* execution_context,
                               const NDEFRecordInit* record,
                               ExceptionState& exception_state,
                               bool is_embedded) {
  // https://w3c.github.io/web-nfc/#creating-ndef-record
  const String& record_type = record->recordType();

  // https://w3c.github.io/web-nfc/#dom-ndefrecordinit-mediatype
  if (record->hasMediaType() && record_type != "mime") {
    exception_state.ThrowTypeError(
        "NDEFRecordInit#mediaType is only applicable for 'mime' records.");
    return nullptr;
  }

  // https://w3c.github.io/web-nfc/#dfn-map-empty-record-to-ndef
  if (record->hasId() && record_type == "empty") {
    exception_state.ThrowTypeError(
        "NDEFRecordInit#id is not applicable for 'empty' records.");
    return nullptr;
  }

  // TODO(crbug.com/1070871): Use IdOr(String()).
  String id;
  if (record->hasId())
    id = record->id();

  if (record_type == "empty") {
    // https://w3c.github.io/web-nfc/#mapping-empty-record-to-ndef
    return MakeGarbageCollected<NDEFRecord>(
        device::mojom::blink::NDEFRecordTypeCategory::kStandardized,
        record_type, /*id=*/String(), WTF::Vector<uint8_t>());
  } else if (record_type == "text") {
    return CreateTextRecord(execution_context, id, *record, exception_state);
  } else if (record_type == "url" || record_type == "absolute-url") {
    return CreateUrlRecord(id, *record, exception_state);
  } else if (record_type == "mime") {
    return CreateMimeRecord(id, *record, exception_state);
  } else if (record_type == "unknown") {
    return CreateUnknownRecord(id, *record, exception_state);
  } else if (record_type == "smart-poster") {
    return CreateSmartPosterRecord(execution_context, id, *record,
                                   exception_state);
  } else if (IsValidExternalType(record_type)) {
    return CreateExternalRecord(execution_context, id, *record,
                                exception_state);
  } else if (IsValidLocalType(record_type)) {
    if (!is_embedded) {
      exception_state.ThrowTypeError(
          "Local type records are only supposed to be embedded in the payload "
          "of another record (smart-poster, external, or local).");
      return nullptr;
    }
    return CreateLocalRecord(execution_context, id, *record, exception_state);
  }

  exception_state.ThrowTypeError("Invalid NDEFRecord type.");
  return nullptr;
}

NDEFRecord::NDEFRecord(device::mojom::blink::NDEFRecordTypeCategory category,
                       const String& record_type,
                       const String& id,
                       WTF::Vector<uint8_t> data)
    : category_(category),
      record_type_(record_type),
      id_(id),
      payload_data_(std::move(data)) {
  DCHECK_EQ(
      category_ == device::mojom::blink::NDEFRecordTypeCategory::kExternal,
      IsValidExternalType(record_type_));
  DCHECK_EQ(category_ == device::mojom::blink::NDEFRecordTypeCategory::kLocal,
            IsValidLocalType(record_type_));
}

NDEFRecord::NDEFRecord(device::mojom::blink::NDEFRecordTypeCategory category,
                       const String& record_type,
                       const String& id,
                       NDEFMessage* payload_message)
    : category_(category),
      record_type_(record_type),
      id_(id),
      payload_message_(payload_message) {
  DCHECK(record_type_ == "smart-poster" ||
         category_ == device::mojom::blink::NDEFRecordTypeCategory::kExternal ||
         category_ == device::mojom::blink::NDEFRecordTypeCategory::kLocal);
  DCHECK_EQ(
      category_ == device::mojom::blink::NDEFRecordTypeCategory::kExternal,
      IsValidExternalType(record_type_));
  DCHECK_EQ(category_ == device::mojom::blink::NDEFRecordTypeCategory::kLocal,
            IsValidLocalType(record_type_));
}

NDEFRecord::NDEFRecord(const String& id,
                       const String& encoding,
                       const String& lang,
                       WTF::Vector<uint8_t> data)
    : category_(device::mojom::blink::NDEFRecordTypeCategory::kStandardized),
      record_type_("text"),
      id_(id),
      encoding_(encoding),
      lang_(lang),
      payload_data_(std::move(data)) {}

NDEFRecord::NDEFRecord(const ExecutionContext* execution_context,
                       const String& text)
    : category_(device::mojom::blink::NDEFRecordTypeCategory::kStandardized),
      record_type_("text"),
      encoding_("utf-8"),
      lang_(getDocumentLanguage(execution_context)),
      payload_data_(GetUTF8DataFromString(text)) {}

NDEFRecord::NDEFRecord(const String& id,
                       const String& media_type,
                       WTF::Vector<uint8_t> data)
    : category_(device::mojom::blink::NDEFRecordTypeCategory::kStandardized),
      record_type_("mime"),
      id_(id),
      media_type_(media_type),
      payload_data_(std::move(data)) {}

// Even if |record| is for a local type record, here we do not validate if it's
// in the context of a parent record but just expose to JS as is.
NDEFRecord::NDEFRecord(const device::mojom::blink::NDEFRecord& record)
    : category_(record.category),
      record_type_(record.record_type),
      id_(record.id),
      media_type_(record.media_type),
      encoding_(record.encoding),
      lang_(record.lang),
      payload_data_(record.data),
      payload_message_(
          record.payload_message
              ? MakeGarbageCollected<NDEFMessage>(*record.payload_message)
              : nullptr) {
  DCHECK_NE(record_type_ == "mime", media_type_.IsNull());
  DCHECK_EQ(
      category_ == device::mojom::blink::NDEFRecordTypeCategory::kExternal,
      IsValidExternalType(record_type_));
  DCHECK_EQ(category_ == device::mojom::blink::NDEFRecordTypeCategory::kLocal,
            IsValidLocalType(record_type_));
}

const String& NDEFRecord::mediaType() const {
  DCHECK_NE(record_type_ == "mime", media_type_.IsNull());
  return media_type_;
}

DOMDataView* NDEFRecord::data() const {
  // Step 4 in https://w3c.github.io/web-nfc/#dfn-parse-an-ndef-record
  if (record_type_ == "empty") {
    DCHECK(payload_data_.IsEmpty());
    return nullptr;
  }
  DOMArrayBuffer* dom_buffer =
      DOMArrayBuffer::Create(payload_data_.data(), payload_data_.size());
  return DOMDataView::Create(dom_buffer, 0, payload_data_.size());
}

// https://w3c.github.io/web-nfc/#dfn-convert-ndefrecord-data-bytes
base::Optional<HeapVector<Member<NDEFRecord>>> NDEFRecord::toRecords(
    ExceptionState& exception_state) const {
  if (record_type_ != "smart-poster" &&
      category_ != device::mojom::blink::NDEFRecordTypeCategory::kExternal &&
      category_ != device::mojom::blink::NDEFRecordTypeCategory::kLocal) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Only {smart-poster, external, local} type records could have a ndef "
        "message as payload.");
    return base::nullopt;
  }

  if (!payload_message_)
    return base::nullopt;

  return payload_message_->records();
}

void NDEFRecord::Trace(Visitor* visitor) const {
  visitor->Trace(payload_message_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
