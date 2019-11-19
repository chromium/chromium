// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"

#include <limits>
#include <utility>

#include "services/device/public/mojom/nfc.mojom-blink.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_push_options.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/modules/nfc/ndef_scan_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using device::mojom::blink::NDEFMessage;
using device::mojom::blink::NDEFMessagePtr;
using device::mojom::blink::NDEFPushOptions;
using device::mojom::blink::NDEFPushOptionsPtr;
using device::mojom::blink::NDEFRecord;
using device::mojom::blink::NDEFRecordPtr;
using device::mojom::blink::NDEFScanOptions;
using device::mojom::blink::NDEFScanOptionsPtr;

// Mojo type converters
namespace mojo {

NDEFRecordPtr TypeConverter<NDEFRecordPtr, blink::NDEFRecord*>::Convert(
    const blink::NDEFRecord* record) {
  return NDEFRecord::New(
      record->recordType(), record->mediaType(), record->id(),
      record->encoding(), record->lang(), record->payloadData(),
      TypeConverter<NDEFMessagePtr, blink::NDEFMessage*>::Convert(
          record->payload_message()));
}

NDEFMessagePtr TypeConverter<NDEFMessagePtr, blink::NDEFMessage*>::Convert(
    const blink::NDEFMessage* message) {
  // |message| may come from blink::NDEFRecord::payload_message() which is
  // possible to be null for some "smart-poster" and external type records.
  if (!message)
    return nullptr;
  NDEFMessagePtr messagePtr = NDEFMessage::New();
  messagePtr->url = message->url();
  messagePtr->data.resize(message->records().size());
  for (wtf_size_t i = 0; i < message->records().size(); ++i) {
    NDEFRecordPtr record = NDEFRecord::From(message->records()[i].Get());
    DCHECK(record);
    messagePtr->data[i] = std::move(record);
  }
  return messagePtr;
}

NDEFPushOptionsPtr
TypeConverter<NDEFPushOptionsPtr, const blink::NDEFPushOptions*>::Convert(
    const blink::NDEFPushOptions* pushOptions) {
  // https://w3c.github.io/web-nfc/#the-ndefpushoptions-dictionary
  // Default values for NDEFPushOptions dictionary are:
  // target = 'any', ignoreRead = true
  NDEFPushOptionsPtr pushOptionsPtr = NDEFPushOptions::New();
  pushOptionsPtr->target = blink::StringToNDEFPushTarget(pushOptions->target());
  pushOptionsPtr->ignore_read = pushOptions->ignoreRead();

  return pushOptionsPtr;
}

NDEFScanOptionsPtr
TypeConverter<NDEFScanOptionsPtr, const blink::NDEFScanOptions*>::Convert(
    const blink::NDEFScanOptions* scanOptions) {
  // https://w3c.github.io/web-nfc/#dom-ndefscanoptions
  // Default values for NDEFScanOptions dictionary are:
  // url = "", recordType = null, mediaType = ""
  NDEFScanOptionsPtr scanOptionsPtr = NDEFScanOptions::New();
  scanOptionsPtr->url = scanOptions->url();
  scanOptionsPtr->media_type = scanOptions->mediaType();

  if (scanOptions->hasRecordType()) {
    scanOptionsPtr->record_type = scanOptions->recordType();
  }

  return scanOptionsPtr;
}

}  // namespace mojo
