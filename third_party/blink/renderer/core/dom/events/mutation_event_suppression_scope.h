// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_MUTATION_EVENT_SUPPRESSION_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_MUTATION_EVENT_SUPPRESSION_SCOPE_H_

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class MutationEventSuppressionScope {
  STACK_ALLOCATED();

 public:
  MutationEventSuppressionScope(Document& document) : document_(document) {
    // Document::SetSuppressMutationEvents enforces this with a CHECK(),
    // but we'll DCHECK() here as well for documentation.
    DCHECK(!document.ShouldSuppressMutationEvents());

    document.SetSuppressMutationEvents(true);
  }
  ~MutationEventSuppressionScope() {
    // Document::SetSuppressMutationEvents enforces this with a CHECK(),
    // but we'll DCHECK() here as well for documentation.
    DCHECK(document_.ShouldSuppressMutationEvents());

    document_.SetSuppressMutationEvents(false);
  }

 private:
  Document& document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_MUTATION_EVENT_SUPPRESSION_SCOPE_H_
