// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_CREATE_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_CREATE_TRANSITION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class DocumentTransition;
class DocumentTransitionInit;

class CORE_EXPORT DocumentCreateTransition {
  STATIC_ONLY(DocumentCreateTransition);

 public:
  static DocumentTransition* createTransition(Document&,
                                              const DocumentTransitionInit*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_CREATE_TRANSITION_H_
