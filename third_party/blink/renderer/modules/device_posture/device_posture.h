// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_POSTURE_DEVICE_POSTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_POSTURE_DEVICE_POSTURE_H_

#include "third_party/blink/public/mojom/device_posture/device_posture.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class LocalDOMWindow;

class MODULES_EXPORT DevicePosture : public EventTargetWithInlineData,
                                     public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit DevicePosture(LocalDOMWindow*);
  ~DevicePosture() override;

  // Web-exposed interfaces
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)
  String type() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor*) const override;

 private:
  // TODO(baul.eun): Retrieve infomation from browser side.
  // And will process change logic.
  mojom::blink::DevicePostureType posture_ =
      mojom::blink::DevicePostureType::kNoFold;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_POSTURE_DEVICE_POSTURE_H_
