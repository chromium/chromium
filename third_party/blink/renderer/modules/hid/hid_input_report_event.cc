// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_input_report_event.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"

namespace blink {

HIDInputReportEvent::HIDInputReportEvent(const AtomicString& type,
                                         HIDDevice* device,
                                         uint8_t report_id,
                                         const Vector<uint8_t>& data)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      device_(device),
      report_id_(report_id) {
  DOMArrayBuffer* dom_buffer = DOMArrayBuffer::Create(data.data(), data.size());
  data_ = DOMDataView::Create(dom_buffer, 0, data.size());
}

HIDInputReportEvent::~HIDInputReportEvent() = default;

const AtomicString& HIDInputReportEvent::InterfaceName() const {
  return event_interface_names::kHIDInputReportEvent;
}

void HIDInputReportEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(data_);
  Event::Trace(visitor);
}

}  // namespace blink
