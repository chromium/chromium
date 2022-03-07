// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_REPORTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_REPORTING_H_

#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class AttributionSourceParams;
class LocalDOMWindow;

class CORE_EXPORT AttributionReporting final
    : public ScriptWrappable,
      public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static AttributionReporting& attributionReporting(LocalDOMWindow&);

  explicit AttributionReporting(LocalDOMWindow&);

  void Trace(Visitor*) const override;

  ScriptPromise registerAttributionSource(ScriptState* script_state,
                                          const AttributionSourceParams* params,
                                          ExceptionState& exception_state);

 private:
  void RegisterImpression(blink::Impression impression);

  HeapMojoAssociatedRemote<mojom::blink::ConversionHost> conversion_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_REPORTING_H_
