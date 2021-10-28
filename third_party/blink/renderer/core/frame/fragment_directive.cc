// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/fragment_directive.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_range_selection.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/dom_selection.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/text_directive.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

FragmentDirective::FragmentDirective(Document& owner_document)
    : owner_document_(&owner_document) {}
FragmentDirective::~FragmentDirective() = default;

void FragmentDirective::ClearDirectives() {
  directives_.clear();
}

void FragmentDirective::AddDirective(Directive* directive) {
  directives_.push_back(directive);
}

void FragmentDirective::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(directives_);
  visitor->Trace(owner_document_);
}

const HeapVector<Member<Directive>>& FragmentDirective::items() const {
  return directives_;
}

namespace {
void RejectWithCode(ScriptPromiseResolver* resolver,
                    DOMExceptionCode code,
                    const String& message) {
  ScriptState::Scope scope(resolver->GetScriptState());
  ExceptionState exception_state(resolver->GetScriptState()->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "FragmentDirective",
                                 "createSelectorDirective");
  exception_state.ThrowDOMException(code, message);
  resolver->Reject(exception_state);
}
}  // namespace

ScriptPromise FragmentDirective::createSelectorDirective(
    ScriptState* state,
    const V8UnionRangeOrSelection* arg) {
  if (ExecutionContext::From(state)->IsContextDestroyed())
    return ScriptPromise();

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(state);

  // Access the promise first to ensure it is created so that the proper state
  // can be changed when it is resolved or rejected.
  ScriptPromise promise = resolver->Promise();

  Range* range = nullptr;

  if (arg->GetContentType() ==
      V8UnionRangeOrSelection::ContentType::kSelection) {
    DOMSelection* selection = arg->GetAsSelection();
    if (selection->rangeCount() == 0) {
      RejectWithCode(resolver, DOMExceptionCode::kNotSupportedError,
                     "Selection must contain a range");
      return promise;
    }

    range = selection->getRangeAt(0, ASSERT_NO_EXCEPTION);
  } else {
    DCHECK_EQ(arg->GetContentType(),
              V8UnionRangeOrSelection::ContentType::kRange);
    range = arg->GetAsRange();
  }

  if (!range || range->collapsed()) {
    RejectWithCode(resolver, DOMExceptionCode::kNotSupportedError,
                   "RangeOrSelector must be non-null and non-collapsed");
    return promise;
  }

  if (range->OwnerDocument() != owner_document_) {
    RejectWithCode(resolver, DOMExceptionCode::kWrongDocumentError,
                   "RangeOrSelector must be from this document");
    return promise;
  }

  LocalFrame* frame = range->OwnerDocument().GetFrame();
  if (!frame) {
    RejectWithCode(resolver, DOMExceptionCode::kInvalidStateError,
                   "Document must be attached to frame");
    return promise;
  }

  EphemeralRangeInFlatTree ephemeral_range(range);
  RangeInFlatTree* range_in_flat_tree = MakeGarbageCollected<RangeInFlatTree>(
      ephemeral_range.StartPosition(), ephemeral_range.EndPosition());

  auto* generator = MakeGarbageCollected<TextFragmentSelectorGenerator>(frame);
  generator->Generate(
      *range_in_flat_tree,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver,
             TextFragmentSelectorGenerator* generator,
             const RangeInFlatTree* range,
             const TextFragmentSelector& selector) {
            if (selector.Type() ==
                TextFragmentSelector::SelectorType::kInvalid) {
              RejectWithCode(resolver, DOMExceptionCode::kOperationError,
                             "Failed to generate selector for the given range");
              return;
            }
            TextDirective* dom_text_directive =
                MakeGarbageCollected<TextDirective>(selector);
            dom_text_directive->DidFinishMatching(range);
            resolver->Resolve(dom_text_directive);
          },
          WrapPersistent(resolver), WrapPersistent(generator),
          WrapPersistent(range_in_flat_tree)));

  return promise;
}

}  // namespace blink
