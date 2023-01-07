// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_ACCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_ACCESS_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class LocalDOMWindow;
class QueryOptions;
class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;

class FontAccess final : public GarbageCollected<FontAccess>,
                         public ExecutionContextLifecycleObserver,
                         public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit FontAccess(LocalDOMWindow* window);

  void Trace(blink::Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  // Web-exposed interface:
  static ScriptPromise queryLocalFonts(ScriptState* script_state,
                                       LocalDOMWindow& window,
                                       const QueryOptions* options,
                                       ExceptionState& exception_state);

 private:
  // Returns the supplement, creating one as needed.
  static FontAccess* From(LocalDOMWindow* window);

  ScriptPromise QueryLocalFontsImpl(ScriptState* script_state,
                                    const QueryOptions* options,
                                    ExceptionState& exception_state);

  void DidGetEnumerationResponse(const QueryOptions* options,
                                 ScriptPromiseResolver* resolver,
                                 mojom::blink::FontEnumerationStatus status,
                                 base::ReadOnlySharedMemoryRegion region);

  // Returns whether the resolver has rejected.
  bool RejectPromiseIfNecessary(
      const mojom::blink::FontEnumerationStatus& status,
      ScriptPromiseResolver* resolver);

  void OnDisconnect();

  mojo::Remote<mojom::blink::FontAccessManager> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_ACCESS_H_
