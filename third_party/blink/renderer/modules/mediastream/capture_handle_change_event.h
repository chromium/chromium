// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CAPTURE_HANDLE_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CAPTURE_HANDLE_CHANGE_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_change_event_init.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CaptureHandle;

class MODULES_EXPORT CaptureHandleChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~CaptureHandleChangeEvent() override;

  static CaptureHandleChangeEvent* Create(const AtomicString& type,
                                          const CaptureHandleChangeEventInit*);

  CaptureHandleChangeEvent(const AtomicString& type, CaptureHandle*);
  CaptureHandleChangeEvent(const AtomicString& type,
                           const CaptureHandleChangeEventInit*);

  CaptureHandle* captureHandle() const;

  // Implement Event
  const AtomicString& InterfaceName() const override;
  void Trace(Visitor*) const override;

 private:
  Member<CaptureHandle> capture_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CAPTURE_HANDLE_CHANGE_EVENT_H_
