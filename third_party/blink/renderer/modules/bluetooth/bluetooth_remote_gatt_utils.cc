// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_utils.h"

namespace blink {

// static
DOMDataView* BluetoothRemoteGATTUtils::ConvertSpanToDataView(
    base::span<const uint8_t> span) {
  static_assert(sizeof(*span.data()) == 1, "uint8_t should be a single byte");
  DOMArrayBuffer* dom_buffer = DOMArrayBuffer::Create(span);
  return DOMDataView::Create(dom_buffer, 0, span.size());
}

}  // namespace blink
