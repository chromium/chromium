// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_IN_TRANSFER_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_IN_TRANSFER_RESULT_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class USBInTransferResult final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBInTransferResult* Create(const String& status,
                                     base::span<const uint8_t> data) {
    DOMDataView* data_view = DOMDataView::Create(
        DOMArrayBuffer::Create(data.data(), data.size()), 0, data.size());
    return MakeGarbageCollected<USBInTransferResult>(status, data_view);
  }

  static USBInTransferResult* Create(const String& status) {
    return MakeGarbageCollected<USBInTransferResult>(status, nullptr);
  }

  static USBInTransferResult* Create(const String& status,
                                     NotShared<DOMDataView> data) {
    return MakeGarbageCollected<USBInTransferResult>(status, data.Get());
  }

  USBInTransferResult(const String& status, DOMDataView* data)
      : status_(status), data_(data) {}

  ~USBInTransferResult() override = default;

  String status() const { return status_; }
  DOMDataView* data() const { return data_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(data_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  const String status_;
  const NotShared<DOMDataView> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBUSB_USB_IN_TRANSFER_RESULT_H_
