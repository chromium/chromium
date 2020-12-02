// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Document;
class DocumentTransitionInit;
class ScriptState;

class CORE_EXPORT DocumentTransition final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DocumentTransition(Document*, const DocumentTransitionInit*);

  // GC functionality.
  void Trace(Visitor* visitor) const override;

  // JavaScript API implementation.
  ScriptPromise prepare(ScriptState*);
  void start();

 private:
  Member<Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_H_
