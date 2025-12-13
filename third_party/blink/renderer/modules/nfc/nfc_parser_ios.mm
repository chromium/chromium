// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_parser_ios.h"

#include <CoreNFC/CoreNFC.h>

#include "base/apple/foundation_util.h"
#include "services/device/public/cpp/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

device::mojom::blink::NDEFRawMessagePtr ConvertNFCDefRawMessage(
    NFCNDEFMessage* message) {
  if (!message) {
    return nullptr;
  }
  Vector<device::mojom::blink::NDEFRawRecordPtr> records;
  for (NFCNDEFPayload* record : [message records]) {
    device::mojom::blink::NDEFRawRecordPtr mojo_record =
        device::mojom::blink::NDEFRawRecord::New();
    mojo_record->type_name_format =
        device::MapCoreNFCFormat([record typeNameFormat]);
    auto identifier_span = base::apple::NSDataToSpan([record identifier]);
    mojo_record->identifier.assign(identifier_span);
    auto payload_span = base::apple::NSDataToSpan([record payload]);
    mojo_record->identifier.assign(payload_span);
    auto type_span = base::apple::NSDataToSpan([record type]);
    mojo_record->identifier.assign(type_span);
    records.push_back(std::move(mojo_record));
  }
  return device::mojom::blink::NDEFRawMessage::New(std::move(records));
}

device::mojom::blink::NDEFRawMessagePtr ConvertNFCDefMessage(
    const Vector<uint8_t>& payload) {
  return ConvertNFCDefRawMessage([NFCNDEFMessage
      ndefMessageWithData:[NSData dataWithBytes:payload.data()
                                         length:payload.size()]]);
}

device::mojom::blink::NDEFRecordPtr CreateEmptyRecord() {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category =
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized;
  result->record_type = "empty";
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateUnknownRecord(
    const Vector<uint8_t>& payload) {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category =
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized;
  result->record_type = "unknown";
  result->data = payload;
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateExternalRecord(
    const String& type,
    const Vector<uint8_t>& payload) {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category = device::mojom::blink::NDEFRecordTypeCategory::kExternal;
  result->record_type = type;
  result->data = payload;
  result->payload_message =
      blink::ParseRawNDEFMessage(ConvertNFCDefMessage(payload));
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateMediaRecord(
    const String& type,
    const Vector<uint8_t>& payload) {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category =
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized;
  result->record_type = "mime";
  result->media_type = type;
  result->data = payload;
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateUrlRecord(
    bool absolute,
    const Vector<uint8_t>& payload) {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category =
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized;
  result->record_type = absolute ? "absolute-url" : "url";
  result->data = payload;
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateTextRecord(
    const Vector<uint8_t>& payload) {
  if (payload.empty()) {
    return nullptr;
  }
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category =
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized;
  result->record_type = "text";
  result->encoding = (payload[0] & (1 << 7)) == 0 ? "utf-8" : "utf-16";
  unsigned lang_length = payload[0] & 0x3f;
  wtf_size_t text_body_start = lang_length + 1;
  if (text_body_start > payload.size()) {
    return nullptr;
  }
  base::span<const uint8_t> data_span(payload);
  result->lang = String(data_span.subspan(1u, lang_length));
  result->data.assign(data_span.subspan(text_body_start));
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateSmartPosterRecord(
    const Vector<uint8_t>& payload) {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category =
      device::mojom::blink::NDEFRecordTypeCategory::kStandardized;
  result->record_type = "smart-poster";
  result->data = payload;
  result->payload_message =
      blink::ParseRawNDEFMessage(ConvertNFCDefMessage(payload));
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateLocalRecord(
    const String& type,
    const Vector<uint8_t>& payload) {
  auto result = device::mojom::blink::NDEFRecord::New();
  result->category = device::mojom::blink::NDEFRecordTypeCategory::kLocal;
  result->record_type = StrCat({":", type});
  result->data = payload;
  result->payload_message =
      blink::ParseRawNDEFMessage(ConvertNFCDefMessage(payload));
  return result;
}

device::mojom::blink::NDEFRecordPtr CreateWellKnownRecord(
    const String& type,
    const Vector<uint8_t>& payload) {
  if (type == "U") {
    return CreateUrlRecord(/*absolute=*/false, payload);
  } else if (type == "T") {
    return CreateTextRecord(payload);
  } else if (type == "Sp") {
    return CreateSmartPosterRecord(payload);
  }
  return CreateLocalRecord(type, payload);
}

}  // namespace

device::mojom::blink::NDEFMessagePtr ParseRawNDEFMessage(
    device::mojom::blink::NDEFRawMessagePtr message) {
  if (!message) {
    return nullptr;
  }
  Vector<device::mojom::blink::NDEFRecordPtr> records;
  for (const device::mojom::blink::NDEFRawRecordPtr& record : message->data) {
    device::mojom::blink::NDEFRecordPtr mojo_record;
    switch (record->type_name_format) {
      case device::mojom::blink::NSRawTypeNameFormat::kEmpty:
        mojo_record = CreateEmptyRecord();
        break;
      case device::mojom::blink::NSRawTypeNameFormat::kAbsoluteURI:
        mojo_record = CreateUrlRecord(/*absolute=*/true, record->payload);
        break;
      case device::mojom::blink::NSRawTypeNameFormat::kMedia:
        mojo_record =
            CreateMediaRecord(String::FromUTF8(record->type), record->payload);
        break;
      case device::mojom::blink::NSRawTypeNameFormat::kExternal:
        mojo_record = CreateExternalRecord(String::FromUTF8(record->type),
                                           record->payload);
        break;
      case device::mojom::blink::NSRawTypeNameFormat::kWellKnown:
        mojo_record = CreateWellKnownRecord(String::FromUTF8(record->type),
                                            record->payload);
        break;
      case device::mojom::blink::NSRawTypeNameFormat::kUnknown:
        mojo_record = CreateUnknownRecord(record->payload);
        break;
      case device::mojom::blink::NSRawTypeNameFormat::kUnchanged:
        // Android doesn't support these chunked types so allow these for now.
        return nullptr;
    }
    if (mojo_record) {
      mojo_record->id = String::FromUTF8(record->identifier);
      records.push_back(std::move(mojo_record));
    }
  }

  return device::mojom::blink::NDEFMessage::New(std::move(records));
}

}  // namespace blink
