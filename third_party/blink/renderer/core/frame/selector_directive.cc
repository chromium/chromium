// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/selector_directive.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

SelectorDirective::SelectorDirective(Type type) : Directive(type) {}
SelectorDirective::~SelectorDirective() = default;

ScriptPromise<Range> SelectorDirective::getMatchingRange(
    ScriptState* state,
    ExceptionState& exception_state) const {
  if (ExecutionContext::From(state)->IsContextDestroyed())
    return EmptyPromise();

  // TODO(bokan): This method needs to be able to initiate the search since
  // author code can construct a TextDirective; if it then calls this method
  // the returned promise will never resolve.
  // TODO(bokan): If this method can initiate a search, it'd probably be more
  // straightforward to avoid caching and have each call start a new search.
  // That way this is more resilient to changes in the DOM.
  matching_range_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<Range>>(
      state, exception_state.GetContext());

  // Access the promise first to ensure it is created so that the proper state
  // can be changed when it is resolved or rejected.
  auto promise = matching_range_resolver_->Promise();

  if (matching_finished_)
    ResolvePromise();

  return promise;
}

void SelectorDirective::DidFinishMatching(const RangeInFlatTree* range) {
  DCHECK(!selected_range_);
  matching_finished_ = true;

  if (range) {
    selected_range_ = MakeGarbageCollected<RangeInFlatTree>(
        range->StartPosition(), range->EndPosition());

    DCHECK(!selected_range_->IsCollapsed());
    // TODO(bokan): what if selected_range_ spans into a shadow tree?
    DCHECK(selected_range_->StartPosition().GetDocument());
    DCHECK_EQ(selected_range_->StartPosition().GetDocument(),
              selected_range_->EndPosition().GetDocument());
  }

  if (matching_range_resolver_)
    ResolvePromise();
}

void SelectorDirective::ResolvePromise() const {
  DCHECK(matching_range_resolver_);
  DCHECK(matching_finished_);

  if (!selected_range_) {
    matching_range_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNotFoundError,
        "Could not find range matching the given selector");
    return;
  }

  Range* dom_range = MakeGarbageCollected<Range>(
      *selected_range_->StartPosition().GetDocument(),
      ToPositionInDOMTree(selected_range_->StartPosition()),
      ToPositionInDOMTree(selected_range_->EndPosition()));

  matching_range_resolver_->Resolve(dom_range);
  matching_range_resolver_ = nullptr;
}

void SelectorDirective::Trace(Visitor* visitor) const {
  Directive::Trace(visitor);
  visitor->Trace(matching_range_resolver_);
  visitor->Trace(selected_range_);
}

}  // namespace blink
