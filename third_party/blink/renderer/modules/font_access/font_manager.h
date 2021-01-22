// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;
class QueryOptions;

class FontManager final : public ScriptWrappable,
                          public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FontManager(ExecutionContext* context);

  // Disallow copy and assign.
  FontManager(const FontManager&) = delete;
  FontManager operator=(const FontManager&) = delete;

  // FontManager IDL interface implementation.
  ScriptPromise query(ScriptState*, const QueryOptions* options);

  void Trace(blink::Visitor*) const override;

 private:
  void DidShowFontChooser(ScriptPromiseResolver* resolver,
                          mojom::blink::FontEnumerationStatus status,
                          Vector<mojom::blink::FontMetadataPtr> fonts);
  void DidGetEnumerationResponse(ScriptPromiseResolver* resolver,
                                 const Vector<String>& selection,
                                 mojom::blink::FontEnumerationStatus,
                                 base::ReadOnlySharedMemoryRegion);
  // Returns whether the resolver has rejected.
  bool RejectPromiseIfNecessary(
      const mojom::blink::FontEnumerationStatus& status,
      ScriptPromiseResolver* resolver);
  void ContextDestroyed() override;
  void OnDisconnect();

  mojo::Remote<mojom::blink::FontAccessManager> remote_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_
