// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
class HTMLTemplateElement;
class ScriptState;

class Patch : public ScriptWrappable, public ThreadableLoaderClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Patch* Create(ContainerNode& target,
                       HTMLTemplateElement* source = nullptr,
                       const KURL& source_url = KURL(),
                       Node* previous_child = nullptr,
                       Node* next_child = nullptr);
  Patch(HTMLTemplateElement* source,
        ContainerNode& target,
        const KURL& source_url,
        Node* previous_child,
        Node* next_child);
  ScriptPromise<IDLUndefined> finished(ScriptState*);
  HTMLTemplateElement* source() { return source_; }
  void Trace(Visitor*) const override;
  void OnComplete();
  Node& Target() { return *target_; }
  Document& GetDocument();
  void DispatchPatchEvent();
  void Append(const String&);
  void Start();
  void Finish();
  void Terminate(ScriptValue);
  void AppendBytes(base::span<uint8_t>);
  void Commit();

 private:
  void Fetch();

  // ThreadableLoaderClient implementation
  void DidReceiveResponse(uint64_t, const ResourceResponse& response) override;
  void DidReceiveData(base::span<const char> bytes) override;
  void DidFinishLoading(uint64_t /*identifier*/) override;
  void DidFail(uint64_t /*identifier*/, const ResourceError& error) override;
  void OnFetchError(DOMExceptionCode code, const AtomicString& message);

  enum class State { kPending, kActive, kTerminated, kFinished };
  State state_ = State::kPending;
  Member<HTMLTemplateElement> source_;
  Member<ContainerNode> target_;
  Member<Node> previous_child_;
  Member<Node> next_child_;
  Member<ScriptPromiseProperty<IDLUndefined, IDLAny>> finished_;
  Member<DocumentParser> parser_;
  // In some cases we buffer into a fragment instead of streaming directly to
  // the DOM.
  Member<DocumentFragment> buffer_fragment_;
  KURL source_url_;
  Member<ThreadableLoader> loader_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_H_
