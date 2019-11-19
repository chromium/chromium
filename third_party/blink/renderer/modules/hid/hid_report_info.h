// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_INFO_H_

#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class HIDReportItem;

class MODULES_EXPORT HIDReportInfo : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HIDReportInfo(
      const device::mojom::blink::HidReportDescription& report);
  ~HIDReportInfo() override;

  uint8_t reportId() const;
  const HeapVector<Member<HIDReportItem>>& items() const;

  void Trace(blink::Visitor* visitor) override;

 private:
  uint8_t report_id_;
  HeapVector<Member<HIDReportItem>> items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_INFO_H_
