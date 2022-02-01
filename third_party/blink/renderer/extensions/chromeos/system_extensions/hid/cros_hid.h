// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_HID_CROS_HID_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_HID_CROS_HID_H_

#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/hid/cros_hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class HIDDeviceRequestOptions;

class CrosHID : public ScriptWrappable, public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CrosHID(ExecutionContext* execution_context);

  void Trace(Visitor*) const override;

  ScriptPromise requestDevice(ScriptState* script_state,
                              const HIDDeviceRequestOptions*);

 private:
  // Returns the remote for communication with the browser's HID
  // implementation. May return null in error cases.
  mojom::blink::CrosHID* GetCrosHIDOrNull();

  HeapMojoRemote<mojom::blink::CrosHID> cros_hid_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_HID_CROS_HID_H_
