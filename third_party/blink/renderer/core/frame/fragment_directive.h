// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAGMENT_DIRECTIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAGMENT_DIRECTIVE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/directive.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Document;
class ScriptState;
class V8UnionRangeOrSelection;

// This class implements the `window.fragmentDirective` web API and serves as a
// home for features based on the fragment directive portion of a URL (the part
// of the URL fragment that comes after ':~:'. See:
// https://github.com/WICG/scroll-to-text-fragment/
class FragmentDirective : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FragmentDirective(Document& owner_document);
  ~FragmentDirective() override;

  // TODO(bokan): It'd be better to parse and create all the DOM Directive
  // objects in one place so that items is globally ordered but we need to
  // extract that logic from TextFragmentSelector and Document.
  void ClearDirectives();
  void AddDirective(Directive*);
  void Trace(Visitor*) const override;

  // Web-exposed FragmentDirective interface.
  const HeapVector<Member<Directive>>& items() const;
  ScriptPromise createSelectorDirective(ScriptState*,
                                        const V8UnionRangeOrSelection*);

 private:
  HeapVector<Member<Directive>> directives_;
  Member<Document> owner_document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAGMENT_DIRECTIVE_H_
