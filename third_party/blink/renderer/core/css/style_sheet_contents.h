/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHEET_CONTENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHEET_CONTENTS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSStyleSheet;
class CSSStyleSheetResource;
class Document;
class Node;
class SecurityOrigin;
class StyleRuleBase;
class StyleRuleFontFace;
class StyleRuleImport;
class StyleRuleNamespace;
enum class ParseSheetResult;

class CORE_EXPORT StyleSheetContents
    : public GarbageCollectedFinalized<StyleSheetContents> {
 public:
  static StyleSheetContents* Create(const CSSParserContext* context) {
    return new StyleSheetContents(nullptr, String(), context);
  }
  static StyleSheetContents* Create(const String& original_url,
                                    const CSSParserContext* context) {
    return new StyleSheetContents(nullptr, original_url, context);
  }
  static StyleSheetContents* Create(StyleRuleImport* owner_rule,
                                    const String& original_url,
                                    const CSSParserContext* context) {
    return new StyleSheetContents(owner_rule, original_url, context);
  }

  static const Document* SingleOwnerDocument(const StyleSheetContents*);

  ~StyleSheetContents();

  const CSSParserContext* ParserContext() const { return parser_context_; }

  const AtomicString& DefaultNamespace() const { return default_namespace_; }
  const AtomicString& NamespaceURIFromPrefix(const AtomicString& prefix) const;

  void ParseAuthorStyleSheet(const CSSStyleSheetResource*,
                             const SecurityOrigin*);
  ParseSheetResult ParseString(const String&, bool allow_import_rules = true);
  ParseSheetResult ParseStringAtPosition(const String&,
                                         const TextPosition&,
                                         bool allow_import_rules = true);

  bool IsCacheableForResource() const;
  bool IsCacheableForStyleElement() const;

  bool IsLoading() const;

  void CheckLoaded();
  void StartLoadingDynamicSheet();

  StyleSheetContents* RootStyleSheet() const;
  bool HasSingleOwnerNode() const;
  Node* SingleOwnerNode() const;
  Document* SingleOwnerDocument() const;
  bool HasSingleOwnerDocument() const { return has_single_owner_document_; }

  // Gets the first owner document in the list of registered clients, or nullptr
  // if there are none.
  Document* AnyOwnerDocument() const;

  const WTF::TextEncoding& Charset() const {
    return parser_context_->Charset();
  }

  bool LoadCompleted() const;
  bool HasFailedOrCanceledSubresources() const;

  void SetHasSyntacticallyValidCSSHeader(bool is_valid_css);
  bool HasSyntacticallyValidCSSHeader() const {
    return has_syntactically_valid_css_header_;
  }

  void SetHasFontFaceRule() { has_font_face_rule_ = true; }
  bool HasFontFaceRule() const { return has_font_face_rule_; }
  void FindFontFaceRules(
      HeapVector<Member<const StyleRuleFontFace>>& font_face_rules);

  void SetHasViewportRule() { has_viewport_rule_ = true; }
  bool HasViewportRule() const { return has_viewport_rule_; }

  void ParserAddNamespace(const AtomicString& prefix, const AtomicString& uri);
  void ParserAppendRule(StyleRuleBase*);

  void ClearRules();

  // Rules other than @import.
  const HeapVector<Member<StyleRuleBase>>& ChildRules() const {
    return child_rules_;
  }
  const HeapVector<Member<StyleRuleImport>>& ImportRules() const {
    return import_rules_;
  }
  const HeapVector<Member<StyleRuleNamespace>>& NamespaceRules() const {
    return namespace_rules_;
  }

  void NotifyLoadedSheet(const CSSStyleSheetResource*);

  StyleSheetContents* ParentStyleSheet() const;
  StyleRuleImport* OwnerRule() const { return owner_rule_; }
  void ClearOwnerRule() { owner_rule_ = nullptr; }

  // The URL that started the redirect chain that led to this
  // style sheet. This property probably isn't useful for much except the
  // JavaScript binding (which needs to use this value for security).
  String OriginalURL() const { return original_url_; }
  // The final request URL after redirects. WARNING: Be careful when
  // using this for security checks. It can be different from the actual
  // response URL if a service worker is involved. See
  // IsOpaqueResponseFromServiceWorker().
  const KURL& BaseURL() const { return parser_context_->BaseURL(); }

  // True if a service worker intercepted the request for this style sheet and
  // returned an opaque response. This context should NOT have access to the
  // contents, regardless of BaseURL().
  //
  // For example:
  // 1. Page at a.com requests a.com/style.css.
  // 2. Service worker responds with b.com/style.css (without CORS).
  // 3. The BaseURL() is "a.com/style.css" but this context is should not have
  // access to contents.
  //
  // You might ask why we don't change BaseURL() to be the actual response URL.
  // In fact, the spec says we should! See crbug.com/553535. But we would still
  // need this "is opaque" bit, since in step 2 above the service worker might
  // have used CORS to get a non-opaque response from b.com.
  bool IsOpaqueResponseFromServiceWorker() const {
    return parser_context_->IsOpaqueResponseFromServiceWorker();
  }

  unsigned RuleCount() const;
  StyleRuleBase* RuleAt(unsigned index) const;

  unsigned EstimatedSizeInBytes() const;

  bool WrapperInsertRule(StyleRuleBase*, unsigned index);
  bool WrapperDeleteRule(unsigned index);

  StyleSheetContents* Copy() const { return new StyleSheetContents(*this); }

  void RegisterClient(CSSStyleSheet*);
  void UnregisterClient(CSSStyleSheet*);
  size_t ClientSize() const {
    return loading_clients_.size() + completed_clients_.size();
  }
  bool HasOneClient() { return ClientSize() == 1; }
  void ClientLoadCompleted(CSSStyleSheet*);
  void ClientLoadStarted(CSSStyleSheet*);

  bool IsMutable() const { return is_mutable_; }
  void SetMutable() { is_mutable_ = true; }

  bool IsUsedFromTextCache() const { return is_used_from_text_cache_; }
  void SetIsUsedFromTextCache() { is_used_from_text_cache_ = true; }

  bool IsReferencedFromResource() const { return referenced_from_resource_; }
  void SetReferencedFromResource(CSSStyleSheetResource*);
  void ClearReferencedFromResource();

  void SetHasMediaQueries();
  bool HasMediaQueries() const { return has_media_queries_; }

  bool DidLoadErrorOccur() const { return did_load_error_occur_; }

  RuleSet& GetRuleSet() {
    DCHECK(rule_set_);
    return *rule_set_.Get();
  }

  bool HasRuleSet() { return rule_set_.Get(); }
  RuleSet& EnsureRuleSet(const MediaQueryEvaluator&, AddRuleFlags);
  void ClearRuleSet();

  String SourceMapURL() const { return source_map_url_; }

  void Trace(blink::Visitor*);

 private:
  StyleSheetContents(StyleRuleImport* owner_rule,
                     const String& original_url,
                     const CSSParserContext*);
  StyleSheetContents(const StyleSheetContents&);
  StyleSheetContents() = delete;
  StyleSheetContents& operator=(const StyleSheetContents&) = delete;
  void NotifyRemoveFontFaceRule(const StyleRuleFontFace*);

  Document* ClientSingleOwnerDocument() const;
  Document* ClientAnyOwnerDocument() const;

  Member<StyleRuleImport> owner_rule_;

  String original_url_;

  HeapVector<Member<StyleRuleImport>> import_rules_;
  HeapVector<Member<StyleRuleNamespace>> namespace_rules_;
  HeapVector<Member<StyleRuleBase>> child_rules_;
  using PrefixNamespaceURIMap = HashMap<AtomicString, AtomicString>;
  PrefixNamespaceURIMap namespaces_;
  AtomicString default_namespace_;
  WeakMember<CSSStyleSheetResource> referenced_from_resource_;

  bool has_syntactically_valid_css_header_ : 1;
  bool did_load_error_occur_ : 1;
  bool is_mutable_ : 1;
  bool has_font_face_rule_ : 1;
  bool has_viewport_rule_ : 1;
  bool has_media_queries_ : 1;
  bool has_single_owner_document_ : 1;
  bool is_used_from_text_cache_ : 1;

  Member<const CSSParserContext> parser_context_;

  HeapHashSet<WeakMember<CSSStyleSheet>> loading_clients_;
  HeapHashSet<WeakMember<CSSStyleSheet>> completed_clients_;

  Member<RuleSet> rule_set_;
  String source_map_url_;
};

}  // namespace blink

#endif
