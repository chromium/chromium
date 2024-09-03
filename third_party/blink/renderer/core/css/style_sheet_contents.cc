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

#include "third_party/blink/renderer/core/css/style_sheet_contents.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
const Document* StyleSheetContents::SingleOwnerDocument(
    const StyleSheetContents* style_sheet_contents) {
  // TODO(https://crbug.com/242125): We may want to handle stylesheets that have
  // multiple owners when this is used for UseCounter.
  if (style_sheet_contents && style_sheet_contents->HasSingleOwnerNode()) {
    return style_sheet_contents->SingleOwnerDocument();
  }
  return nullptr;
}

// Rough size estimate for the memory cache.
unsigned StyleSheetContents::EstimatedSizeInBytes() const {
  // Note that this does not take into account size of the strings hanging from
  // various objects. The assumption is that nearly all of of them are atomic
  // and would exist anyway.
  unsigned size = sizeof(*this);

  // FIXME: This ignores the children of media rules.
  // Most rules are StyleRules.
  size += RuleCount() * StyleRule::AverageSizeInBytes();

  for (unsigned i = 0; i < import_rules_.size(); ++i) {
    if (StyleSheetContents* sheet = import_rules_[i]->GetStyleSheet()) {
      size += sheet->EstimatedSizeInBytes();
    }
  }
  return size;
}

StyleSheetContents::StyleSheetContents(const CSSParserContext* context,
                                       const String& original_url,
                                       StyleRuleImport* owner_rule)
    : owner_rule_(owner_rule),
      original_url_(original_url),
      default_namespace_(g_star_atom),
      has_syntactically_valid_css_header_(true),
      did_load_error_occur_(false),
      is_mutable_(false),
      has_font_face_rule_(false),
      has_media_queries_(false),
      has_single_owner_document_(true),
      is_used_from_text_cache_(false),
      parser_context_(context) {}

StyleSheetContents::StyleSheetContents(const StyleSheetContents& o)
    : owner_rule_(nullptr),
      original_url_(o.original_url_),
      pre_import_layer_statement_rules_(
          o.pre_import_layer_statement_rules_.size()),
      import_rules_(o.import_rules_.size()),
      namespace_rules_(o.namespace_rules_.size()),
      child_rules_(o.child_rules_.size()),
      namespaces_(o.namespaces_),
      default_namespace_(o.default_namespace_),
      has_syntactically_valid_css_header_(
          o.has_syntactically_valid_css_header_),
      did_load_error_occur_(false),
      is_mutable_(false),
      has_font_face_rule_(o.has_font_face_rule_),
      has_media_queries_(o.has_media_queries_),
      has_single_owner_document_(true),
      is_used_from_text_cache_(false),
      parser_context_(o.parser_context_) {
  for (unsigned i = 0; i < pre_import_layer_statement_rules_.size(); ++i) {
    pre_import_layer_statement_rules_[i] = To<StyleRuleLayerStatement>(
        o.pre_import_layer_statement_rules_[i]->Copy());
  }

  // FIXME: Copy import rules.
  DCHECK(o.import_rules_.empty());

  for (unsigned i = 0; i < namespace_rules_.size(); ++i) {
    namespace_rules_[i] =
        static_cast<StyleRuleNamespace*>(o.namespace_rules_[i]->Copy());
  }

  // Copying child rules is a strict point for deferred property parsing, so
  // there is no need to copy lazy parsing state here.
  for (unsigned i = 0; i < child_rules_.size(); ++i) {
    child_rules_[i] = o.child_rules_[i]->Copy();
  }
}

StyleSheetContents::~StyleSheetContents() = default;

void StyleSheetContents::SetHasSyntacticallyValidCSSHeader(bool is_valid_css) {
  has_syntactically_valid_css_header_ = is_valid_css;
}

bool StyleSheetContents::IsCacheableForResource() const {
  // This would require dealing with multiple clients for load callbacks.
  if (!LoadCompleted()) {
    return false;
  }
  // FIXME: Support copying import rules.
  if (!import_rules_.empty()) {
    return false;
  }
  // FIXME: Support cached stylesheets in import rules.
  if (owner_rule_) {
    return false;
  }
  if (did_load_error_occur_) {
    return false;
  }
  // It is not the original sheet anymore.
  if (is_mutable_) {
    return false;
  }
  // If the header is valid we are not going to need to check the
  // SecurityOrigin.
  // FIXME: Valid mime type avoids the check too.
  if (!has_syntactically_valid_css_header_) {
    return false;
  }
  return true;
}

bool StyleSheetContents::IsCacheableForStyleElement() const {
  // FIXME: Support copying import rules.
  if (!ImportRules().empty()) {
    return false;
  }
  // Until import rules are supported in cached sheets it's not possible for
  // loading to fail.
  DCHECK(!DidLoadErrorOccur());
  // It is not the original sheet anymore.
  if (IsMutable()) {
    return false;
  }
  if (!HasSyntacticallyValidCSSHeader()) {
    return false;
  }
  return true;
}

void StyleSheetContents::ParserAppendRule(StyleRuleBase* rule) {
  if (auto* layer_statement_rule = DynamicTo<StyleRuleLayerStatement>(rule)) {
    if (import_rules_.empty() && namespace_rules_.empty() &&
        child_rules_.empty()) {
      pre_import_layer_statement_rules_.push_back(layer_statement_rule);
      return;
    }
    // Falls through, insert it into child_rules_ as a regular rule
  }

  if (auto* import_rule = DynamicTo<StyleRuleImport>(rule)) {
    // Parser enforces that @import rules come before anything else other than
    // empty layer statements
    DCHECK(child_rules_.empty());
    if (import_rule->MediaQueries()) {
      SetHasMediaQueries();
    }
    import_rules_.push_back(import_rule);
    import_rules_.back()->SetParentStyleSheet(this);
    import_rules_.back()->RequestStyleSheet();
    return;
  }

  if (auto* namespace_rule = DynamicTo<StyleRuleNamespace>(rule)) {
    // Parser enforces that @namespace rules come before all rules other than
    // import/charset rules and empty layer statements
    DCHECK(child_rules_.empty());
    ParserAddNamespace(namespace_rule->Prefix(), namespace_rule->Uri());
    namespace_rules_.push_back(namespace_rule);
    return;
  }

  child_rules_.push_back(rule);
}

void StyleSheetContents::SetHasMediaQueries() {
  has_media_queries_ = true;
  if (ParentStyleSheet()) {
    ParentStyleSheet()->SetHasMediaQueries();
  }
}

StyleRuleBase* StyleSheetContents::RuleAt(unsigned index) const {
  SECURITY_DCHECK(index < RuleCount());

  if (index < pre_import_layer_statement_rules_.size()) {
    return pre_import_layer_statement_rules_[index].Get();
  }

  index -= pre_import_layer_statement_rules_.size();

  if (index < import_rules_.size()) {
    return import_rules_[index].Get();
  }

  index -= import_rules_.size();

  if (index < namespace_rules_.size()) {
    return namespace_rules_[index].Get();
  }

  index -= namespace_rules_.size();

  return child_rules_[index].Get();
}

unsigned StyleSheetContents::RuleCount() const {
  return pre_import_layer_statement_rules_.size() + import_rules_.size() +
         namespace_rules_.size() + child_rules_.size();
}

void StyleSheetContents::ClearRules() {
  pre_import_layer_statement_rules_.clear();
  for (unsigned i = 0; i < import_rules_.size(); ++i) {
    DCHECK_EQ(import_rules_.at(i)->ParentStyleSheet(), this);
    import_rules_[i]->ClearParentStyleSheet();
  }

  if (rule_set_diff_) {
    rule_set_diff_->MarkUnrepresentable();
  }

  import_rules_.clear();
  namespace_rules_.clear();
  child_rules_.clear();
}

static wtf_size_t ReplaceRuleIfExistsInternal(
    const StyleRuleBase* old_rule,
    StyleRuleBase* new_rule,
    HeapVector<Member<StyleRuleBase>>& child_rules) {
  for (wtf_size_t i = 0; i < child_rules.size(); ++i) {
    StyleRuleBase* rule = child_rules[i].Get();
    if (rule == old_rule) {
      child_rules[i] = new_rule;
      return i;
    }
    if (auto* style_rule_group = DynamicTo<StyleRuleGroup>(rule)) {
      if (ReplaceRuleIfExistsInternal(old_rule, new_rule,
                                      style_rule_group->ChildRules()) !=
          std::numeric_limits<wtf_size_t>::max()) {
        return 0;  // Dummy non-failure value.
      }
    } else if (auto* style_rule = DynamicTo<StyleRule>(rule);
               style_rule && style_rule->ChildRules()) {
      if (ReplaceRuleIfExistsInternal(old_rule, new_rule,
                                      *style_rule->ChildRules()) !=
          std::numeric_limits<wtf_size_t>::max()) {
        return 0;  // Dummy non-failure value.
      }
    }
  }

  // Not found.
  return std::numeric_limits<wtf_size_t>::max();
}

wtf_size_t StyleSheetContents::ReplaceRuleIfExists(StyleRuleBase* old_rule,
                                                   StyleRuleBase* new_rule,
                                                   wtf_size_t position_hint) {
  if (rule_set_diff_) {
    rule_set_diff_->AddDiff(old_rule);
    rule_set_diff_->AddDiff(new_rule);
  }

  if (position_hint < child_rules_.size() &&
      child_rules_[position_hint] == old_rule) {
    child_rules_[position_hint] = new_rule;
    return position_hint;
  }

  return ReplaceRuleIfExistsInternal(old_rule, new_rule, child_rules_);
}

bool StyleSheetContents::WrapperInsertRule(StyleRuleBase* rule,
                                           unsigned index) {
  DCHECK(is_mutable_);
  SECURITY_DCHECK(index <= RuleCount());

  if (rule_set_diff_) {
    rule_set_diff_->AddDiff(rule);
  }

  // If the sheet starts with empty layer statements without any import or
  // namespace rules, we should be able to insert any rule before and between
  // the empty layer statements. To support this case, we move any existing
  // empty layer statement to child_rules_ first.
  if (pre_import_layer_statement_rules_.size() && !import_rules_.size() &&
      !namespace_rules_.size()) {
    child_rules_.PrependVector(pre_import_layer_statement_rules_);
    pre_import_layer_statement_rules_.clear();
  }

  if (index < pre_import_layer_statement_rules_.size() ||
      (index == pre_import_layer_statement_rules_.size() &&
       rule->IsLayerStatementRule())) {
    // Empty layer statements before import rules should be a continuous block.
    auto* layer_statement_rule = DynamicTo<StyleRuleLayerStatement>(rule);
    if (!layer_statement_rule) {
      return false;
    }

    pre_import_layer_statement_rules_.insert(index, layer_statement_rule);
    return true;
  }

  index -= pre_import_layer_statement_rules_.size();

  if (index < import_rules_.size() ||
      (index == import_rules_.size() && rule->IsImportRule())) {
    // Inserting non-import rule before @import is not allowed.
    auto* import_rule = DynamicTo<StyleRuleImport>(rule);
    if (!import_rule) {
      return false;
    }

    if (import_rule->MediaQueries()) {
      SetHasMediaQueries();
    }

    import_rules_.insert(index, import_rule);
    import_rules_[index]->SetParentStyleSheet(this);
    import_rules_[index]->RequestStyleSheet();
    // FIXME: Stylesheet doesn't actually change meaningfully before the
    // imported sheets are loaded.
    return true;
  }
  // Inserting @import rule after a non-import rule is not allowed.
  if (rule->IsImportRule()) {
    return false;
  }

  index -= import_rules_.size();

  if (index < namespace_rules_.size() ||
      (index == namespace_rules_.size() && rule->IsNamespaceRule())) {
    // Inserting non-namespace rules other than import rule before @namespace is
    // not allowed.
    auto* namespace_rule = DynamicTo<StyleRuleNamespace>(rule);
    if (!namespace_rule) {
      return false;
    }
    // Inserting @namespace rule when rules other than import/namespace/charset
    // are present is not allowed.
    if (!child_rules_.empty()) {
      return false;
    }

    namespace_rules_.insert(index, namespace_rule);
    // For now to be compatible with IE and Firefox if namespace rule with same
    // prefix is added irrespective of adding the rule at any index, last added
    // rule's value is considered.
    // TODO (ramya.v@samsung.com): As per spec last valid rule should be
    // considered, which means if namespace rule is added in the middle of
    // existing namespace rules, rule which comes later in rule list with same
    // prefix needs to be considered.
    ParserAddNamespace(namespace_rule->Prefix(), namespace_rule->Uri());
    return true;
  }

  if (rule->IsNamespaceRule()) {
    return false;
  }

  index -= namespace_rules_.size();

  child_rules_.insert(index, rule);
  return true;
}

bool StyleSheetContents::WrapperDeleteRule(unsigned index) {
  DCHECK(is_mutable_);
  SECURITY_DCHECK(index < RuleCount());

  if (index < pre_import_layer_statement_rules_.size()) {
    if (rule_set_diff_) {
      rule_set_diff_->AddDiff(pre_import_layer_statement_rules_[index]);
    }
    pre_import_layer_statement_rules_.EraseAt(index);
    return true;
  }
  index -= pre_import_layer_statement_rules_.size();

  if (index < import_rules_.size()) {
    if (rule_set_diff_) {
      rule_set_diff_->AddDiff(import_rules_[index]);
    }
    import_rules_[index]->ClearParentStyleSheet();
    import_rules_.EraseAt(index);
    return true;
  }
  index -= import_rules_.size();

  if (index < namespace_rules_.size()) {
    if (rule_set_diff_) {
      rule_set_diff_->AddDiff(namespace_rules_[index]);
    }
    if (!child_rules_.empty()) {
      return false;
    }
    namespace_rules_.EraseAt(index);
    return true;
  }
  index -= namespace_rules_.size();

  if (rule_set_diff_) {
    rule_set_diff_->AddDiff(child_rules_[index]);
  }
  if (child_rules_[index]->IsFontFaceRule()) {
    NotifyRemoveFontFaceRule(To<StyleRuleFontFace>(child_rules_[index].Get()));
  }
  child_rules_.EraseAt(index);
  return true;
}

void StyleSheetContents::ParserAddNamespace(const AtomicString& prefix,
                                            const AtomicString& uri) {
  DCHECK(!uri.IsNull());
  if (prefix.IsNull()) {
    default_namespace_ = uri;
    return;
  }
  namespaces_.Set(prefix, uri);
}

const AtomicString& StyleSheetContents::NamespaceURIFromPrefix(
    const AtomicString& prefix) const {
  auto it = namespaces_.find(prefix);
  return it != namespaces_.end() ? it->value : WTF::g_null_atom;
}

void StyleSheetContents::ParseAuthorStyleSheet(
    const CSSStyleSheetResource* cached_style_sheet) {
  TRACE_EVENT1("blink,devtools.timeline", "ParseAuthorStyleSheet", "data",
               [&](perfetto::TracedValue context) {
                 inspector_parse_author_style_sheet_event::Data(
                     std::move(context), cached_style_sheet);
               });

  const ResourceResponse& response = cached_style_sheet->GetResponse();
  CSSStyleSheetResource::MIMETypeCheck mime_type_check =
      (IsQuirksModeBehavior(parser_context_->Mode()) &&
       response.IsCorsSameOrigin())
          ? CSSStyleSheetResource::MIMETypeCheck::kLax
          : CSSStyleSheetResource::MIMETypeCheck::kStrict;
  String sheet_text =
      cached_style_sheet->SheetText(parser_context_, mime_type_check);

  source_map_url_ = response.HttpHeaderField(http_names::kSourceMap);
  if (source_map_url_.empty()) {
    // Try to get deprecated header.
    source_map_url_ = response.HttpHeaderField(http_names::kXSourceMap);
  }

  const auto* context =
      MakeGarbageCollected<CSSParserContext>(ParserContext(), this);
  CSSParser::ParseSheet(context, this, sheet_text,
                        CSSDeferPropertyParsing::kYes);
}

ParseSheetResult StyleSheetContents::ParseString(
    const String& sheet_text,
    bool allow_import_rules,
    CSSDeferPropertyParsing defer_property_parsing) {
  const auto* context =
      MakeGarbageCollected<CSSParserContext>(ParserContext(), this);
  return CSSParser::ParseSheet(context, this, sheet_text,
                               defer_property_parsing, allow_import_rules);
}

bool StyleSheetContents::IsLoading() const {
  for (unsigned i = 0; i < import_rules_.size(); ++i) {
    if (import_rules_[i]->IsLoading()) {
      return true;
    }
  }
  return false;
}

bool StyleSheetContents::LoadCompleted() const {
  StyleSheetContents* parent_sheet = ParentStyleSheet();
  if (parent_sheet) {
    return parent_sheet->LoadCompleted();
  }

  StyleSheetContents* root = RootStyleSheet();
  return root->loading_clients_.empty();
}

void StyleSheetContents::CheckLoaded() {
  if (IsLoading()) {
    return;
  }

  StyleSheetContents* parent_sheet = ParentStyleSheet();
  if (parent_sheet) {
    parent_sheet->CheckLoaded();
    return;
  }

  DCHECK_EQ(this, RootStyleSheet());
  if (loading_clients_.empty()) {
    return;
  }

  // Avoid |CSSSStyleSheet| and |OwnerNode| being deleted by scripts that run
  // via ScriptableDocumentParser::ExecuteScriptsWaitingForResources(). Also
  // protect the |CSSStyleSheet| from being deleted during iteration via the
  // |SheetLoaded| method.
  //
  // When a sheet is loaded it is moved from the set of loading clients
  // to the set of completed clients. We therefore need the copy in order to
  // not modify the set while iterating it.
  HeapVector<Member<CSSStyleSheet>> loading_clients(loading_clients_);

  for (unsigned i = 0; i < loading_clients.size(); ++i) {
    if (loading_clients[i]->LoadCompleted()) {
      continue;
    }
    DCHECK(!loading_clients[i]->IsConstructed());

    // sheetLoaded might be invoked after its owner node is removed from
    // document.
    if (Node* owner_node = loading_clients[i]->ownerNode()) {
      if (loading_clients[i]->SheetLoaded()) {
        owner_node->NotifyLoadedSheetAndAllCriticalSubresources(
            did_load_error_occur_ ? Node::kErrorOccurredLoadingSubresource
                                  : Node::kNoErrorLoadingSubresource);
      }
    }
  }
}

void StyleSheetContents::NotifyLoadedSheet(const CSSStyleSheetResource* sheet) {
  DCHECK(sheet);
  did_load_error_occur_ |= sheet->ErrorOccurred();
  // updateLayoutIgnorePendingStyleSheets can cause us to create the RuleSet on
  // this sheet before its imports have loaded. So clear the RuleSet when the
  // imports load since the import's subrules are flattened into its parent
  // sheet's RuleSet.
  ClearRuleSet();
}

void StyleSheetContents::SetToPendingState() {
  StyleSheetContents* root = RootStyleSheet();
  for (const auto& client : root->loading_clients_) {
    client->SetToPendingState();
  }
  // Copy the completed clients to a vector for iteration.
  // SetToPendingState() will move the style sheet from the completed state
  // to the loading state which modifies the set of completed clients. We
  // therefore need the copy in order to not modify the set of completed clients
  // while iterating it.
  HeapVector<Member<CSSStyleSheet>> completed_clients(completed_clients_);
  for (unsigned i = 0; i < completed_clients.size(); ++i) {
    completed_clients[i]->SetToPendingState();
  }
}

StyleSheetContents* StyleSheetContents::RootStyleSheet() const {
  const StyleSheetContents* root = this;
  while (root->ParentStyleSheet()) {
    root = root->ParentStyleSheet();
  }
  return const_cast<StyleSheetContents*>(root);
}

bool StyleSheetContents::HasSingleOwnerNode() const {
  return RootStyleSheet()->HasOneClient();
}

Node* StyleSheetContents::SingleOwnerNode() const {
  StyleSheetContents* root = RootStyleSheet();
  if (!root->HasOneClient()) {
    return nullptr;
  }
  if (root->loading_clients_.size()) {
    return (*root->loading_clients_.begin())->ownerNode();
  }
  return (*root->completed_clients_.begin())->ownerNode();
}

Document* StyleSheetContents::SingleOwnerDocument() const {
  StyleSheetContents* root = RootStyleSheet();
  return root->ClientSingleOwnerDocument();
}

Document* StyleSheetContents::AnyOwnerDocument() const {
  return RootStyleSheet()->ClientAnyOwnerDocument();
}

static bool ChildRulesHaveFailedOrCanceledSubresources(
    const HeapVector<Member<StyleRuleBase>>& rules) {
  for (unsigned i = 0; i < rules.size(); ++i) {
    const StyleRuleBase* rule = rules[i].Get();
    switch (rule->GetType()) {
      case StyleRuleBase::kStyle:
        if (To<StyleRule>(rule)->PropertiesHaveFailedOrCanceledSubresources()) {
          return true;
        }
        break;
      case StyleRuleBase::kFontFace:
        if (To<StyleRuleFontFace>(rule)
                ->Properties()
                .HasFailedOrCanceledSubresources()) {
          return true;
        }
        break;
      case StyleRuleBase::kContainer:
      case StyleRuleBase::kMedia:
      case StyleRuleBase::kLayerBlock:
      case StyleRuleBase::kScope:
      case StyleRuleBase::kStartingStyle:
        if (ChildRulesHaveFailedOrCanceledSubresources(
                To<StyleRuleGroup>(rule)->ChildRules())) {
          return true;
        }
        break;
      case StyleRuleBase::kCharset:
      case StyleRuleBase::kImport:
      case StyleRuleBase::kNamespace:
      case StyleRuleBase::kMixin:
        NOTREACHED_IN_MIGRATION();
        break;
      case StyleRuleBase::kNestedDeclarations:
      case StyleRuleBase::kPage:
      case StyleRuleBase::kPageMargin:
      case StyleRuleBase::kProperty:
      case StyleRuleBase::kKeyframes:
      case StyleRuleBase::kKeyframe:
      case StyleRuleBase::kLayerStatement:
      case StyleRuleBase::kSupports:
      case StyleRuleBase::kFontPaletteValues:
      case StyleRuleBase::kFontFeatureValues:
      case StyleRuleBase::kFontFeature:
      case StyleRuleBase::kViewTransition:
      case StyleRuleBase::kFunction:
      case StyleRuleBase::kPositionTry:
        break;
      case StyleRuleBase::kApplyMixin:
        // TODO(sesse): Should we go down into the rules here?
        // Do we need to do a new name lookup then?
        break;
      case StyleRuleBase::kCounterStyle:
        if (To<StyleRuleCounterStyle>(rule)
                ->HasFailedOrCanceledSubresources()) {
          return true;
        }
        break;
    }
  }
  return false;
}

bool StyleSheetContents::HasFailedOrCanceledSubresources() const {
  DCHECK(IsCacheableForResource());
  return ChildRulesHaveFailedOrCanceledSubresources(child_rules_);
}

Document* StyleSheetContents::ClientAnyOwnerDocument() const {
  if (ClientSize() <= 0) {
    return nullptr;
  }
  if (loading_clients_.size()) {
    return (*loading_clients_.begin())->OwnerDocument();
  }
  return (*completed_clients_.begin())->OwnerDocument();
}

Document* StyleSheetContents::ClientSingleOwnerDocument() const {
  return has_single_owner_document_ ? ClientAnyOwnerDocument() : nullptr;
}

StyleSheetContents* StyleSheetContents::ParentStyleSheet() const {
  return owner_rule_ ? owner_rule_->ParentStyleSheet() : nullptr;
}

void StyleSheetContents::RegisterClient(CSSStyleSheet* sheet) {
  DCHECK(!loading_clients_.Contains(sheet));
  DCHECK(!completed_clients_.Contains(sheet));
  // InspectorCSSAgent::BuildObjectForRule creates CSSStyleSheet without any
  // owner node.
  if (!sheet->OwnerDocument()) {
    return;
  }

  if (Document* document = ClientSingleOwnerDocument()) {
    if (sheet->OwnerDocument() != document) {
      has_single_owner_document_ = false;
    }
  }

  if (sheet->IsConstructed()) {
    // Constructed stylesheets don't need loading. Note that @import is ignored
    // in both CSSStyleSheet.replaceSync and CSSStyleSheet.replace.
    //
    // https://drafts.csswg.org/cssom/#dom-cssstylesheet-replacesync
    // https://drafts.csswg.org/cssom/#dom-cssstylesheet-replace
    completed_clients_.insert(sheet);
  } else {
    loading_clients_.insert(sheet);
  }
}

void StyleSheetContents::UnregisterClient(CSSStyleSheet* sheet) {
  loading_clients_.erase(sheet);
  completed_clients_.erase(sheet);

  if (!sheet->OwnerDocument() || !loading_clients_.empty() ||
      !completed_clients_.empty()) {
    return;
  }

  has_single_owner_document_ = true;
}

void StyleSheetContents::ClientLoadCompleted(CSSStyleSheet* sheet) {
  DCHECK(loading_clients_.Contains(sheet) || !sheet->OwnerDocument());
  loading_clients_.erase(sheet);
  // In owner_node_->SheetLoaded, the CSSStyleSheet might be detached.
  // (i.e. ClearOwnerNode was invoked.)
  // In this case, we don't need to add the stylesheet to completed clients.
  if (!sheet->OwnerDocument()) {
    return;
  }
  completed_clients_.insert(sheet);
}

void StyleSheetContents::ClientLoadStarted(CSSStyleSheet* sheet) {
  DCHECK(completed_clients_.Contains(sheet));
  completed_clients_.erase(sheet);
  loading_clients_.insert(sheet);
}

void StyleSheetContents::SetReferencedFromResource(
    CSSStyleSheetResource* resource) {
  DCHECK(resource);
  DCHECK(!IsReferencedFromResource());
  DCHECK(IsCacheableForResource());
  referenced_from_resource_ = resource;
}

void StyleSheetContents::ClearReferencedFromResource() {
  DCHECK(IsReferencedFromResource());
  DCHECK(IsCacheableForResource());
  referenced_from_resource_ = nullptr;
}

RuleSet& StyleSheetContents::EnsureRuleSet(const MediaQueryEvaluator& medium) {
  if (rule_set_ && rule_set_->DidMediaQueryResultsChange(medium)) {
    rule_set_ = nullptr;
  }
  if (rule_set_diff_) {
    rule_set_diff_->NewRuleSetCleared();
  }
  if (!rule_set_) {
    rule_set_ = MakeGarbageCollected<RuleSet>();
    rule_set_->AddRulesFromSheet(this, medium);
    if (rule_set_diff_) {
      rule_set_diff_->NewRuleSetCreated(rule_set_);
    }
  }
  return *rule_set_.Get();
}

static void SetNeedsActiveStyleUpdateForClients(
    HeapHashSet<WeakMember<CSSStyleSheet>>& clients) {
  for (const auto& sheet : clients) {
    Document* document = sheet->OwnerDocument();
    Node* node = sheet->ownerNode();
    if (!document || !node || !node->isConnected()) {
      continue;
    }
    document->GetStyleEngine().SetNeedsActiveStyleUpdate(node->GetTreeScope());
  }
}

void StyleSheetContents::StartMutation() {
  is_mutable_ = true;
  if (rule_set_) {
    rule_set_diff_ = MakeGarbageCollected<RuleSetDiff>(rule_set_);
  }
}

void StyleSheetContents::ClearRuleSet() {
  if (StyleSheetContents* parent_sheet = ParentStyleSheet()) {
    parent_sheet->ClearRuleSet();
  }

  if (!rule_set_) {
    return;
  }

  rule_set_.Clear();
  if (rule_set_diff_) {
    rule_set_diff_->NewRuleSetCleared();
  }
  SetNeedsActiveStyleUpdateForClients(loading_clients_);
  SetNeedsActiveStyleUpdateForClients(completed_clients_);
}

static void RemoveFontFaceRules(HeapHashSet<WeakMember<CSSStyleSheet>>& clients,
                                const StyleRuleFontFace* font_face_rule) {
  for (const auto& sheet : clients) {
    if (Node* owner_node = sheet->ownerNode()) {
      owner_node->GetDocument().GetStyleEngine().RemoveFontFaceRules(
          HeapVector<Member<const StyleRuleFontFace>>(1, font_face_rule));
    }
  }
}

void StyleSheetContents::NotifyRemoveFontFaceRule(
    const StyleRuleFontFace* font_face_rule) {
  StyleSheetContents* root = RootStyleSheet();
  RemoveFontFaceRules(root->loading_clients_, font_face_rule);
  RemoveFontFaceRules(root->completed_clients_, font_face_rule);
}

void StyleSheetContents::Trace(Visitor* visitor) const {
  visitor->Trace(owner_rule_);
  visitor->Trace(pre_import_layer_statement_rules_);
  visitor->Trace(import_rules_);
  visitor->Trace(namespace_rules_);
  visitor->Trace(child_rules_);
  visitor->Trace(loading_clients_);
  visitor->Trace(completed_clients_);
  visitor->Trace(rule_set_);
  visitor->Trace(referenced_from_resource_);
  visitor->Trace(parser_context_);
  visitor->Trace(rule_set_diff_);
}

}  // namespace blink
