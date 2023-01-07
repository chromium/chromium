// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_write_options.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using device::mojom::blink::NDEFMessage;
using device::mojom::blink::NDEFMessagePtr;
using device::mojom::blink::NDEFRecord;
using device::mojom::blink::NDEFRecordPtr;
using device::mojom::blink::NDEFWriteOptions;
using device::mojom::blink::NDEFWriteOptionsPtr;

// Mojo type converters
namespace mojo {

NDEFRecordPtr TypeConverter<NDEFRecordPtr, blink::NDEFRecord*>::Convert(
    const blink::NDEFRecord* record) {
  return NDEFRecord::New(
      record->category(), record->recordType(), record->mediaType(),
      record->id(), record->encoding(), record->lang(), record->payloadData(),
      TypeConverter<NDEFMessagePtr, blink::NDEFMessage*>::Convert(
          record->payload_message()));
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, blink::NDEFMessage*>::Convert(
    const blink::NDEFMessage* message) {
  // |message| may come from blink::NDEFRecord::payload_message() which is
  // possible to be null for some "smart-poster" and external type records.
  if (!message)
    return nullptr;
  NDEFMessagePtr message_ptr = NDEFMessage::New();
  message_ptr->data.resize(message->records().size());
  for (wtf_size_t i = 0; i < message->records().size(); ++i) {
    NDEFRecordPtr record = NDEFRecord::From(message->records()[i].Get());
    DCHECK(record);
    message_ptr->data[i] = std::move(record);
  }
  return message_ptr;
}

NDEFWriteOptionsPtr
TypeConverter<NDEFWriteOptionsPtr, const blink::NDEFWriteOptions*>::Convert(
    const blink::NDEFWriteOptions* write_options) {
  // https://w3c.github.io/web-nfc/#the-ndefwriteoptions-dictionary
  // Default values for NDEFWriteOptions dictionary are:
  // overwrite = true
  NDEFWriteOptionsPtr write_options_ptr = NDEFWriteOptions::New();
  write_options_ptr->overwrite = write_options->overwrite();

  return write_options_ptr;
}

}  // namespace mojo
