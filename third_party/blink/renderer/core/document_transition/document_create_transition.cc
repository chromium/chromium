// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_create_transition.h"

#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

// static
DocumentTransition* DocumentCreateTransition::createTransition(
    Document& document,
    const DocumentTransitionInit* init) {
  return MakeGarbageCollected<DocumentTransition>(&document, init);
}

}  // namespace blink
