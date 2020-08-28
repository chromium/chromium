// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

void SynchronousMutationObserver::ObserverListWillBeCleared() {
  document_ = nullptr;
}

void SynchronousMutationObserver::SetDocument(Document* document) {
  if (document == document_)
    return;

  if (document_)
    document_->SynchronousMutationObserverList().RemoveObserver(this);

  document_ = document;

  if (document_)
    document_->SynchronousMutationObserverList().AddObserver(this);
}

void SynchronousMutationObserver::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

}  // namespace blink
