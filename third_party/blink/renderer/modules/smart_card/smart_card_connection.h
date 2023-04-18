// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONNECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONNECTION_H_

#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class V8SmartCardDisposition;

class SmartCardConnection final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SmartCardConnection(
      mojo::PendingRemote<device::mojom::blink::SmartCardConnection>,
      ExecutionContext*);

  // SmartCardConnection idl
  ScriptPromise disconnect(const V8SmartCardDisposition& disposition);
  ScriptPromise status();

  // ScriptWrappable overrides
  void Trace(Visitor*) const override;

 private:
  HeapMojoRemote<device::mojom::blink::SmartCardConnection> connection_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONNECTION_H_
