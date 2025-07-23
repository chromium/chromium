// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_DOM_PATCH_STATUS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_DOM_PATCH_STATUS_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class HTMLTemplateElement;
class ScriptState;
class DOMPatchStatus : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DOMPatchStatus* Start(HTMLTemplateElement& source,
                               ContainerNode& target);
  DOMPatchStatus(HTMLTemplateElement& source, ContainerNode& target);
  ScriptPromise<IDLUndefined> finished(ScriptState*);
  HTMLTemplateElement* source() { return source_; }
  void Trace(Visitor*) const override;
  void OnComplete();
  Node& GetTarget() { return *target_; }
  Document& GetDocument();
  void DispatchPatchEvent();
  void Append(const String&);
  void Finish();

 private:
  Member<HTMLTemplateElement> source_;
  Member<ContainerNode> target_;
  Member<ScriptPromiseProperty<IDLUndefined, IDLAny>> finished_;
  Member<DocumentParser> parser_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_DOM_PATCH_STATUS_H_
