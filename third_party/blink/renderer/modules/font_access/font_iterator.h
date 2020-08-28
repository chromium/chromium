// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_ITERATOR_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

using mojom::blink::FontEnumerationStatus;

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class FontMetadata;
class FontIteratorEntry;

class FontIterator final : public ScriptWrappable,
                           public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PermissionStatus = mojom::blink::PermissionStatus;
  explicit FontIterator(ExecutionContext* context);

  ScriptPromise next(ScriptState*);

  void Trace(Visitor*) const override;

 private:
  FontIteratorEntry* GetNextEntry();
  void DidGetPermissionResponse(PermissionStatus);
  void DidGetEnumerationResponse(FontEnumerationStatus,
                                 base::ReadOnlySharedMemoryRegion);
  void ContextDestroyed() override;
  void OnDisconnect();

  HeapDeque<Member<FontMetadata>> entries_;
  Member<ScriptPromiseResolver> pending_resolver_;
  mojo::Remote<mojom::blink::FontAccessManager> remote_manager_;

  PermissionStatus permission_status_ = PermissionStatus::ASK;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_ITERATOR_H_
