// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMDataView;
class ExceptionState;
class ExecutionContext;
class NDEFMessage;
class NDEFRecordInit;

class MODULES_EXPORT NDEFRecord final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NDEFRecord* Create(const ExecutionContext*,
                            const NDEFRecordInit*,
                            ExceptionState&);

  // Construct a "text" record from a string.
  explicit NDEFRecord(const ExecutionContext*, const String&);

  // Construct a "mime" record from the raw payload bytes.
  explicit NDEFRecord(WTF::Vector<uint8_t> /* payload_data */,
                      const String& /* media_type */);

  NDEFRecord(const String& /* record_type */, WTF::Vector<uint8_t> /* data */);
  NDEFRecord(const String& /* record_type */,
             const String& /* encoding */,
             const String& /* lang */,
             WTF::Vector<uint8_t> /* data */);
  explicit NDEFRecord(const device::mojom::blink::NDEFRecord&);

  const String& recordType() const;
  const String& mediaType() const;
  const String& id() const;
  const String& encoding() const;
  const String& lang() const;
  DOMDataView* data() const;
  base::Optional<HeapVector<Member<NDEFRecord>>> toRecords(
      ExceptionState& exception_state) const;

  const WTF::Vector<uint8_t>& payloadData() const;
  const NDEFMessage* payload_message() const;

  void Trace(blink::Visitor*) override;

 private:
  String record_type_;
  String media_type_;
  String id_;
  String encoding_;
  String lang_;
  // Holds the NDEFRecord.[[PayloadData]] bytes defined at
  // https://w3c.github.io/web-nfc/#the-ndefrecord-interface.
  WTF::Vector<uint8_t> payload_data_;
  // |payload_data_| parsed as an NDEFMessage. This field will be set for some
  // "smart-poster" and external type records.
  Member<NDEFMessage> payload_message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_RECORD_H_
