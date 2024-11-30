/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2012 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_style_sheet.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_style_sheet_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_medialist_string.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class StyleSheetCSSRuleList final : public CSSRuleList {
 public:
  StyleSheetCSSRuleList(CSSStyleSheet* sheet) : style_sheet_(sheet) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(style_sheet_);
    CSSRuleList::Trace(visitor);
  }

 private:
  unsigned length() const override { return style_sheet_->length(); }
  CSSRule* Item(unsigned index, bool trigger_use_counters) const override {
    return style_sheet_->item(index, trigger_use_counters);
  }

  CSSStyleSheet* GetStyleSheet() const override { return style_sheet_.Get(); }

  Member<CSSStyleSheet> style_sheet_;
};

#if DCHECK_IS_ON()
static bool IsAcceptableCSSStyleSheetParent(const Node& parent_node) {
  // Only these nodes can be parents of StyleSheets, and they need to call
  // clearOwnerNode() when moved out of document. Note that destructor of
  // the nodes don't call clearOwnerNode() with Oilpan.
  return parent_node.IsDocumentNode() || IsA<HTMLLinkElement>(parent_node) ||
         IsA<HTMLStyleElement>(parent_node) ||
         IsA<SVGStyleElement>(parent_node) ||
         parent_node.getNodeType() == Node::kProcessingInstructionNode;
}
#endif

// static
const Document* CSSStyleSheet::SingleOwnerDocument(
    const CSSStyleSheet* style_sheet) {
  if (style_sheet) {
    return StyleSheetContents::SingleOwnerDocument(style_sheet->Contents());
  }
  return nullptr;
}

CSSStyleSheet* CSSStyleSheet::Create(Document& document,
                                     const CSSStyleSheetInit* options,
                                     ExceptionState& exception_state) {
  return CSSStyleSheet::Create(document, document.BaseURL(), options,
                               exception_state);
}

CSSStyleSheet* CSSStyleSheet::Create(Document& document,
                                     const KURL& base_url,
                                     const CSSStyleSheetInit* options,
                                     ExceptionState& exception_state) {
  auto* parser_context =
      MakeGarbageCollected<CSSParserContext>(document, base_url);
  if (AdTracker::IsAdScriptExecutingInDocument(&document)) {
    parser_context->SetIsAdRelated();
  }

  auto* contents = MakeGarbageCollected<StyleSheetContents>(parser_context);
  return MakeGarbageCollected<CSSStyleSheet>(contents, document, options);
}

CSSStyleSheet* CSSStyleSheet::CreateInline(StyleSheetContents* sheet,
                                           Node& owner_node,
                                           const TextPosition& start_position) {
  DCHECK(sheet);
  return MakeGarbageCollected<CSSStyleSheet>(sheet, owner_node, true,
                                             start_position);
}

CSSStyleSheet* CSSStyleSheet::CreateInline(Node& owner_node,
                                           const KURL& base_url,
                                           const TextPosition& start_position,
                                           const WTF::TextEncoding& encoding) {
  Document& owner_node_document = owner_node.GetDocument();
  auto* parser_context = MakeGarbageCollected<CSSParserContext>(
      owner_node_document, owner_node_document.BaseURL(),
      true /* origin_clean */,
      Referrer(
          // Fetch requests from an inline CSS use the referrer of the owner
          // document. `Referrer::ClientReferrerString()` for a fetch request
          // just means "use the default referrer", which will be computed from
          // the client (in this case, the owner document's ExecutionContext)
          // when fetching.
          Referrer::ClientReferrerString(),
          network::mojom::ReferrerPolicy::kDefault),
      encoding);
  if (AdTracker::IsAdScriptExecutingInDocument(&owner_node.GetDocument())) {
    parser_context->SetIsAdRelated();
  }
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(parser_context,
                                                         base_url.GetString());
  return MakeGarbageCollected<CSSStyleSheet>(sheet, owner_node, true,
                                             start_position);
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents,
                             CSSImportRule* owner_rule)
    : contents_(contents),
      owner_rule_(owner_rule),
      start_position_(TextPosition::MinimumPosition()) {
  contents_->RegisterClient(this);
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents,
                             Document& document,
                             const CSSStyleSheetInit* options)
    : CSSStyleSheet(contents, nullptr) {
  // Following steps at spec draft
  // https://wicg.github.io/construct-stylesheets/#dom-cssstylesheet-cssstylesheet
  SetConstructorDocument(document);
  ClearOwnerNode();
  ClearOwnerRule();
  Contents()->RegisterClient(this);
  switch (options->media()->GetContentType()) {
    case V8UnionMediaListOrString::ContentType::kMediaList:
      media_queries_ = options->media()->GetAsMediaList()->Queries();
      break;
    case V8UnionMediaListOrString::ContentType::kString:
      media_queries_ = MediaQuerySet::Create(options->media()->GetAsString(),
                                             document.GetExecutionContext());
      break;
  }
  if (options->alternate()) {
    SetAlternateFromConstructor(true);
  }
  if (options->disabled()) {
    setDisabled(true);
  }
}

CSSStyleSheet::CSSStyleSheet(StyleSheetContents* contents,
                             Node& owner_node,
                             bool is_inline_stylesheet,
                             const TextPosition& start_position)
    : contents_(contents),
      owner_node_(&owner_node),
      owner_parent_or_shadow_host_element_(
          owner_node.ParentOrShadowHostElement()),
      start_position_(start_position),
      is_inline_stylesheet_(is_inline_stylesheet) {
#if DCHECK_IS_ON()
  DCHECK(IsAcceptableCSSStyleSheetParent(owner_node));
#endif
  contents_->RegisterClient(this);
}

CSSStyleSheet::~CSSStyleSheet() = default;

void CSSStyleSheet::WillMutateRules() {
  // If we are the only client it is safe to mutate.
  if (!contents_->IsUsedFromTextCache() &&
      !contents_->IsReferencedFromResource()) {
    contents_->StartMutation();
    contents_->ClearRuleSet();
    return;
  }
  // Only cacheable stylesheets should have multiple clients.
  DCHECK(contents_->IsCacheableForStyleElement() ||
         contents_->IsCacheableForResource());

  // Copy-on-write. Note that this eagerly parses any rules that were
  // lazily parsed.
  contents_->UnregisterClient(this);
  contents_ = contents_->Copy();
  contents_->RegisterClient(this);

  contents_->StartMutation();

  // Any existing CSSOM wrappers need to be connected to the copied child rules.
  ReattachChildRuleCSSOMWrappers();
}

void CSSStyleSheet::DidMutate(Mutation mutation) {
  if (mutation == Mutation::kRules) {
    DCHECK(contents_->IsMutable());
    DCHECK_LE(contents_->ClientSize(), 1u);
  }
  Document* document = OwnerDocument();
  if (!document || !document->IsActive()) {
    return;
  }
  if (!custom_element_tag_names_.empty()) {
    document->GetStyleEngine().ScheduleCustomElementInvalidations(
        custom_element_tag_names_);
  }
  bool invalidate_matched_properties_cache = false;
  if (ownerNode() && ownerNode()->isConnected()) {
    document->GetStyleEngine().SetNeedsActiveStyleUpdate(
        ownerNode()->GetTreeScope());
    invalidate_matched_properties_cache = true;
  } else if (!adopted_tree_scopes_.empty()) {
    for (auto tree_scope : adopted_tree_scopes_.Keys()) {
      // It is currently required that adopted sheets can not be moved between
      // documents.
      DCHECK(tree_scope->GetDocument() == document);
      if (!tree_scope->RootNode().isConnected()) {
        continue;
      }
      document->GetStyleEngine().SetNeedsActiveStyleUpdate(*tree_scope);
      invalidate_matched_properties_cache = true;
    }
  }
  if (mutation == Mutation::kRules) {
    if (invalidate_matched_properties_cache) {
      document->GetStyleResolver().InvalidateMatchedPropertiesCache();
    }
    probe::DidMutateStyleSheet(document, this);
  }
}

void CSSStyleSheet::EnableRuleAccessForInspector() {
  enable_rule_access_for_inspector_ = true;
}
void CSSStyleSheet::DisableRuleAccessForInspector() {
  enable_rule_access_for_inspector_ = false;
}

CSSStyleSheet::InspectorMutationScope::InspectorMutationScope(
    CSSStyleSheet* sheet)
    : style_sheet_(sheet) {
  style_sheet_->EnableRuleAccessForInspector();
}

CSSStyleSheet::InspectorMutationScope::~InspectorMutationScope() {
  style_sheet_->DisableRuleAccessForInspector();
}

void CSSStyleSheet::ReattachChildRuleCSSOMWrappers() {
  for (unsigned i = 0; i < child_rule_cssom_wrappers_.size(); ++i) {
    if (!child_rule_cssom_wrappers_[i]) {
      continue;
    }
    child_rule_cssom_wrappers_[i]->Reattach(contents_->RuleAt(i));
  }
}

void CSSStyleSheet::setDisabled(bool disabled) {
  if (disabled == is_disabled_) {
    return;
  }
  is_disabled_ = disabled;

  DidMutate(Mutation::kSheet);
}

bool CSSStyleSheet::MatchesMediaQueries(const MediaQueryEvaluator& evaluator) {
  media_query_result_flags_.Clear();

  if (!media_queries_) {
    return true;
  }
  return evaluator.Eval(*media_queries_, &media_query_result_flags_);
}

void CSSStyleSheet::AddedAdoptedToTreeScope(TreeScope& tree_scope) {
  auto add_result = adopted_tree_scopes_.insert(&tree_scope, 1u);
  if (!add_result.is_new_entry) {
    add_result.stored_value->value++;
  }
}

void CSSStyleSheet::RemovedAdoptedFromTreeScope(TreeScope& tree_scope) {
  auto it = adopted_tree_scopes_.find(&tree_scope);
  if (it != adopted_tree_scopes_.end()) {
    CHECK_GT(it->value, 0u);
    if (--it->value == 0) {
      adopted_tree_scopes_.erase(&tree_scope);
    }
  }
}

bool CSSStyleSheet::IsAdoptedByTreeScope(TreeScope& tree_scope) {
  return adopted_tree_scopes_.Contains(&tree_scope);
}

bool CSSStyleSheet::HasViewportDependentMediaQueries() const {
  return media_query_result_flags_.is_viewport_dependent;
}

bool CSSStyleSheet::HasDynamicViewportDependentMediaQueries() const {
  return media_query_result_flags_.unit_flags &
         MediaQueryExpValue::UnitFlags::kDynamicViewport;
}

unsigned CSSStyleSheet::length() const {
  return contents_->RuleCount();
}

CSSRule* CSSStyleSheet::item(unsigned index, bool trigger_use_counters) {
  unsigned rule_count = length();
  if (index >= rule_count) {
    return nullptr;
  }

  if (child_rule_cssom_wrappers_.empty()) {
    child_rule_cssom_wrappers_.Grow(rule_count);
  }
  DCHECK_EQ(child_rule_cssom_wrappers_.size(), rule_count);

  Member<CSSRule>& css_rule = child_rule_cssom_wrappers_[index];
  if (!css_rule) {
    css_rule = contents_->RuleAt(index)->CreateCSSOMWrapper(
        index, this, trigger_use_counters);
  }
  return css_rule.Get();
}

void CSSStyleSheet::ClearOwnerNode() {
  DidMutate(Mutation::kSheet);
  if (owner_node_) {
    contents_->UnregisterClient(this);
  }
  owner_node_ = nullptr;
}

bool CSSStyleSheet::CanAccessRules() const {
  return enable_rule_access_for_inspector_ || contents_->IsOriginClean();
}

CSSRuleList* CSSStyleSheet::rules(ExceptionState& exception_state) {
  return cssRules(exception_state);
}

unsigned CSSStyleSheet::insertRule(const String& rule_string,
                                   unsigned index,
                                   ExceptionState& exception_state) {
  if (!CanAccessRules()) {
    exception_state.ThrowSecurityError(
        "Cannot access StyleSheet to insertRule");
    return 0;
  }

  DCHECK(child_rule_cssom_wrappers_.empty() ||
         child_rule_cssom_wrappers_.size() == contents_->RuleCount());

  if (index > length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) +
            ") is larger than the maximum index (" + String::Number(length()) +
            ").");
    return 0;
  }

  const auto* context =
      MakeGarbageCollected<CSSParserContext>(contents_->ParserContext(), this);

  StyleRuleBase* rule =
      CSSParser::ParseRule(context, contents_.Get(), CSSNestingType::kNone,
                           /*parent_rule_for_nesting=*/nullptr, rule_string);

  if (!rule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Failed to parse the rule '" + rule_string + "'.");
    return 0;
  }
  RuleMutationScope mutation_scope(this);
  if (rule->IsImportRule() && IsConstructed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Can't insert @import rules into a constructed stylesheet.");
    return 0;
  }
  bool success = contents_->WrapperInsertRule(rule, index);
  if (!success) {
    if (rule->IsNamespaceRule()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Failed to insert the rule");
    } else {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kHierarchyRequestError,
          "Failed to insert the rule.");
    }
    return 0;
  }
  if (!child_rule_cssom_wrappers_.empty()) {
    child_rule_cssom_wrappers_.insert(index, Member<CSSRule>(nullptr));
  }

  return index;
}

void CSSStyleSheet::deleteRule(unsigned index,
                               ExceptionState& exception_state) {
  if (!CanAccessRules()) {
    exception_state.ThrowSecurityError(
        "Cannot access StyleSheet to deleteRule");
    return;
  }

  DCHECK(child_rule_cssom_wrappers_.empty() ||
         child_rule_cssom_wrappers_.size() == contents_->RuleCount());

  if (index >= length()) {
    if (length()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          "The index provided (" + String::Number(index) +
              ") is larger than the maximum index (" +
              String::Number(length() - 1) + ").");
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                        "Style sheet is empty (length 0).");
    }
    return;
  }

  RuleMutationScope mutation_scope(this);

  bool success = contents_->WrapperDeleteRule(index);
  if (!success) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to delete rule");
    return;
  }

  if (!child_rule_cssom_wrappers_.empty()) {
    if (child_rule_cssom_wrappers_[index]) {
      child_rule_cssom_wrappers_[index]->SetParentStyleSheet(nullptr);
    }
    child_rule_cssom_wrappers_.EraseAt(index);
  }
}

int CSSStyleSheet::addRule(const String& selector,
                           const String& style,
                           int index,
                           ExceptionState& exception_state) {
  StringBuilder text;
  text.Append(selector);
  text.Append(" { ");
  text.Append(style);
  if (!style.empty()) {
    text.Append(' ');
  }
  text.Append('}');
  insertRule(text.ReleaseString(), index, exception_state);

  // As per Microsoft documentation, always return -1.
  return -1;
}

int CSSStyleSheet::addRule(const String& selector,
                           const String& style,
                           ExceptionState& exception_state) {
  return addRule(selector, style, length(), exception_state);
}

ScriptPromise<CSSStyleSheet> CSSStyleSheet::replace(
    ScriptState* script_state,
    const String& text,
    ExceptionState& exception_state) {
  if (!IsConstructed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Can't call replace on non-constructed CSSStyleSheets.");
    return EmptyPromise();
  }
  SetText(text, CSSImportRules::kIgnoreWithWarning);
  probe::DidReplaceStyleSheetText(OwnerDocument(), this, text);
  // We currently parse synchronously, and since @import support was removed,
  // nothing else happens asynchronously. This API is left as-is, so that future
  // async parsing can still be supported here.
  return ToResolvedPromise<CSSStyleSheet>(script_state, this);
}

void CSSStyleSheet::replaceSync(const String& text,
                                ExceptionState& exception_state) {
  if (!IsConstructed()) {
    return exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Can't call replaceSync on non-constructed CSSStyleSheets.");
  }
  SetText(text, CSSImportRules::kIgnoreWithWarning);
  probe::DidReplaceStyleSheetText(OwnerDocument(), this, text);
}

CSSRuleList* CSSStyleSheet::cssRules(ExceptionState& exception_state) {
  if (!CanAccessRules()) {
    exception_state.ThrowSecurityError("Cannot access rules");
    return nullptr;
  }
  if (!rule_list_cssom_wrapper_) {
    rule_list_cssom_wrapper_ =
        MakeGarbageCollected<StyleSheetCSSRuleList>(this);
  }
  return rule_list_cssom_wrapper_.Get();
}

String CSSStyleSheet::href() const {
  return contents_->OriginalURL();
}

KURL CSSStyleSheet::BaseURL() const {
  return contents_->BaseURL();
}

bool CSSStyleSheet::IsLoading() const {
  return contents_->IsLoading();
}

MediaList* CSSStyleSheet::media() {
  if (!media_queries_) {
    media_queries_ = MediaQuerySet::Create();
  }
  if (!media_cssom_wrapper_) {
    media_cssom_wrapper_ = MakeGarbageCollected<MediaList>(this);
  }
  return media_cssom_wrapper_.Get();
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const {
  return owner_rule_ ? owner_rule_->parentStyleSheet() : nullptr;
}

Document* CSSStyleSheet::OwnerDocument() const {
  if (CSSStyleSheet* parent = parentStyleSheet()) {
    return parent->OwnerDocument();
  }
  if (IsConstructed()) {
    DCHECK(!ownerNode());
    return ConstructorDocument();
  }
  return ownerNode() ? &ownerNode()->GetDocument() : nullptr;
}

bool CSSStyleSheet::SheetLoaded() {
  DCHECK(owner_node_);
  SetLoadCompleted(owner_node_->SheetLoaded());
  return load_completed_;
}

void CSSStyleSheet::SetToPendingState() {
  SetLoadCompleted(false);
  owner_node_->SetToPendingState();
}

void CSSStyleSheet::SetLoadCompleted(bool completed) {
  if (completed == load_completed_) {
    return;
  }

  load_completed_ = completed;

  if (completed) {
    contents_->ClientLoadCompleted(this);
  } else {
    contents_->ClientLoadStarted(this);
  }
}

void CSSStyleSheet::SetText(const String& text, CSSImportRules import_rules) {
  child_rule_cssom_wrappers_.clear();

  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  contents_->ClearRules();
  bool allow_imports = import_rules == CSSImportRules::kAllow;
  if (contents_->ParseString(text, allow_imports) ==
          ParseSheetResult::kHasUnallowedImportRule &&
      import_rules == CSSImportRules::kIgnoreWithWarning) {
    OwnerDocument()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "@import rules are not allowed here. See "
        "https://github.com/WICG/construct-stylesheets/issues/"
        "119#issuecomment-588352418."));
  }
}

void CSSStyleSheet::SetAlternateFromConstructor(
    bool alternate_from_constructor) {
  alternate_from_constructor_ = alternate_from_constructor;
}

bool CSSStyleSheet::IsAlternate() const {
  if (owner_node_) {
    auto* owner_element = DynamicTo<Element>(owner_node_.Get());
    return owner_element &&
           owner_element->FastGetAttribute(html_names::kRelAttr)
               .Contains("alternate");
  }
  return alternate_from_constructor_;
}

bool CSSStyleSheet::CanBeActivated(
    const String& current_preferrable_name) const {
  if (disabled()) {
    return false;
  }

  if (owner_node_ && owner_node_->IsInShadowTree()) {
    if (IsA<HTMLStyleElement>(owner_node_.Get()) ||
        IsA<SVGStyleElement>(owner_node_.Get())) {
      return true;
    }
  }

  auto* html_link_element = DynamicTo<HTMLLinkElement>(owner_node_.Get());
  if (!owner_node_ ||
      owner_node_->getNodeType() == Node::kProcessingInstructionNode ||
      !html_link_element || !html_link_element->IsEnabledViaScript()) {
    if (!title_.empty() && title_ != current_preferrable_name) {
      return false;
    }
  }

  if (IsAlternate() && title_.empty()) {
    return false;
  }

  return true;
}

void CSSStyleSheet::Trace(Visitor* visitor) const {
  visitor->Trace(contents_);
  visitor->Trace(media_queries_);
  visitor->Trace(owner_node_);
  visitor->Trace(owner_parent_or_shadow_host_element_);
  visitor->Trace(owner_rule_);
  visitor->Trace(media_cssom_wrapper_);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(rule_list_cssom_wrapper_);
  visitor->Trace(adopted_tree_scopes_);
  visitor->Trace(constructor_document_);
  StyleSheet::Trace(visitor);
}

}  // namespace blink
