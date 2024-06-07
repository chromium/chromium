// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_disallow_transition_scope.h"

#if DCHECK_IS_ON()

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"

namespace blink {

WebDisallowTransitionScope::WebDisallowTransitionScope(
    WebDocument* web_document)
    : document_lifecycle_(Lifecycle(web_document)) {
  document_lifecycle_->IncrementNoTransitionCount();
}

WebDisallowTransitionScope::~WebDisallowTransitionScope() {
  document_lifecycle_->DecrementNoTransitionCount();
}

DocumentLifecycle& WebDisallowTransitionScope::Lifecycle(
    WebDocument* web_document) const {
  Document* document = *web_document;
  return document->Lifecycle();
}

}  // namespace blink

#endif  // DCHECK_IS_ON()
