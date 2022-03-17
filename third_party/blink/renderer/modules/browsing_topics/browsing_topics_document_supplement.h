// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BROWSING_TOPICS_BROWSING_TOPICS_DOCUMENT_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BROWSING_TOPICS_BROWSING_TOPICS_DOCUMENT_SUPPLEMENT_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

// Provides the implementation for the Topics API.
// Explainer: https://github.com/jkarlin/topics
class MODULES_EXPORT BrowsingTopicsDocumentSupplement
    : public GarbageCollected<BrowsingTopicsDocumentSupplement>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  // Supplement functionality.
  static BrowsingTopicsDocumentSupplement* From(Document&);
  static ScriptPromise browsingTopics(ScriptState* script_state,
                                      Document& document,
                                      ExceptionState& exception_state);

  explicit BrowsingTopicsDocumentSupplement(Document&);

  // Implements the document.browsingTopics().
  ScriptPromise GetBrowsingTopics(ScriptState* script_state,
                                  Document& document,
                                  ExceptionState& exception_state);

  // GC functionality.
  void Trace(Visitor* visitor) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BROWSING_TOPICS_BROWSING_TOPICS_DOCUMENT_SUPPLEMENT_H_
