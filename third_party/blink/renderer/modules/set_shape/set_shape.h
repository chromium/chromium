// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SET_SHAPE_SET_SHAPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SET_SHAPE_SET_SHAPE_H_

#include "third_party/blink/public/mojom/set_shape/set_shape.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class DOMRectReadOnly;
class ExceptionState;
class LocalDOMWindow;
class ScriptState;

class MODULES_EXPORT SetShape : public GarbageCollected<SetShape>,
                                public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static SetShape& From(LocalDOMWindow&);

  explicit SetShape(LocalDOMWindow&);

  SetShape(const SetShape&) = delete;
  SetShape& operator=(const SetShape&) = delete;

  // Web-exposed interface:
  static ScriptPromise<IDLUndefined> setShape(
      ScriptState*,
      LocalDOMWindow&,
      const HeapVector<Member<DOMRectReadOnly>>& rects,
      ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  HeapMojoRemote<mojom::blink::SetShapeService>& GetRemote();

  HeapMojoRemote<mojom::blink::SetShapeService> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SET_SHAPE_SET_SHAPE_H_
