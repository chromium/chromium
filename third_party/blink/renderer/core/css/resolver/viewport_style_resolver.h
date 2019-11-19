/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_VIEWPORT_STYLE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_VIEWPORT_STYLE_RESOLVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

class ComputedStyle;
class Document;
class DocumentStyleSheetCollection;
class MutableCSSPropertyValueSet;
class StyleRuleViewport;

class CORE_EXPORT ViewportStyleResolver final
    : public GarbageCollected<ViewportStyleResolver> {
 public:
  explicit ViewportStyleResolver(Document&);

  void InitialStyleChanged();
  void InitialViewportChanged();
  void SetNeedsCollectRules();
  bool NeedsUpdate() const { return needs_update_; }
  void UpdateViewport(DocumentStyleSheetCollection&);

  void CollectViewportRulesFromAuthorSheet(const CSSStyleSheet&);

  void Trace(blink::Visitor*);

 private:
  void Reset();
  void Resolve();

  enum Origin { kUserAgentOrigin, kAuthorOrigin };
  enum UpdateType { kNoUpdate, kResolve, kCollectRules };

  void CollectViewportRulesFromUASheets();
  void CollectViewportChildRules(const HeapVector<Member<StyleRuleBase>>&,
                                 Origin);
  void CollectViewportRulesFromImports(StyleSheetContents&);
  void CollectViewportRulesFromAuthorSheetContents(StyleSheetContents&);
  void AddViewportRule(StyleRuleViewport&, Origin);

  float ViewportArgumentValue(CSSPropertyID) const;
  Length ViewportLengthValue(CSSPropertyID);
  mojom::ViewportFit ViewportFitValue() const;

  Member<Document> document_;
  Member<MutableCSSPropertyValueSet> property_set_;
  Member<MediaQueryEvaluator> initial_viewport_medium_;
  scoped_refptr<ComputedStyle> initial_style_;
  MediaQueryResultList viewport_dependent_media_query_results_;
  MediaQueryResultList device_dependent_media_query_results_;
  bool has_author_style_ = false;
  bool has_viewport_units_ = false;
  UpdateType needs_update_ = kCollectRules;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_VIEWPORT_STYLE_RESOLVER_H_
