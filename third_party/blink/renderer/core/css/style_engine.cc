/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "third_party/blink/renderer/core/css/style_engine.h"

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/hash/hash.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/document_style_sheet_collection.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/selector_filter_parent_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"
#include "third_party/blink/renderer/core/css/resolver/viewport_style_resolver.h"
#include "third_party/blink/renderer/core/css/shadow_tree_style_sheet_collection.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"
#include "third_party/blink/renderer/core/css/style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/layout_inline_list_item.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/core/preferences/preference_overrides.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/style/style_initial_data.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

CSSFontSelector* CreateCSSFontSelectorFor(Document& document) {
  DCHECK(document.GetFrame());
  if (document.GetFrame()->PagePopupOwner()) [[unlikely]] {
    return PagePopupController::CreateCSSFontSelector(document);
  }
  return MakeGarbageCollected<CSSFontSelector>(document);
}

enum RuleSetFlags {
  kFontFaceRules = 1 << 0,
  kKeyframesRules = 1 << 1,
  kPropertyRules = 1 << 2,
  kCounterStyleRules = 1 << 3,
  kLayerRules = 1 << 4,
  kFontPaletteValuesRules = 1 << 5,
  kPositionTryRules = 1 << 6,
  kFontFeatureValuesRules = 1 << 7,
  kViewTransitionRules = 1 << 8,
  kFunctionRules = 1 << 9,
};

const unsigned kRuleSetFlagsAll = ~0u;

unsigned GetRuleSetFlags(const HeapHashSet<Member<RuleSet>> rule_sets) {
  unsigned flags = 0;
  for (auto& rule_set : rule_sets) {
    if (!rule_set->KeyframesRules().empty()) {
      flags |= kKeyframesRules;
    }
    if (!rule_set->FontFaceRules().empty()) {
      flags |= kFontFaceRules;
    }
    if (!rule_set->FontPaletteValuesRules().empty()) {
      flags |= kFontPaletteValuesRules;
    }
    if (!rule_set->FontFeatureValuesRules().empty()) {
      flags |= kFontFeatureValuesRules;
    }
    if (!rule_set->PropertyRules().empty()) {
      flags |= kPropertyRules;
    }
    if (!rule_set->CounterStyleRules().empty()) {
      flags |= kCounterStyleRules;
    }
    if (rule_set->HasCascadeLayers()) {
      flags |= kLayerRules;
    }
    if (!rule_set->PositionTryRules().empty()) {
      flags |= kPositionTryRules;
    }
    if (!rule_set->ViewTransitionRules().empty()) {
      flags |= kViewTransitionRules;
    }
    if (!rule_set->FunctionRules().empty()) {
      flags |= kFunctionRules;
    }
  }
  return flags;
}

const Vector<AtomicString> ConvertFontFamilyToVector(const CSSValue* value) {
  const CSSValueList* family_list = DynamicTo<CSSValueList>(value);
  if (!family_list) {
    return Vector<AtomicString>();
  }
  wtf_size_t length = family_list->length();
  if (!length) {
    return Vector<AtomicString>();
  }
  Vector<AtomicString> families(length);
  for (wtf_size_t i = 0; i < length; i++) {
    const CSSFontFamilyValue* family_value =
        DynamicTo<CSSFontFamilyValue>(family_list->Item(i));
    if (!family_value) {
      return Vector<AtomicString>();
    }
    families[i] = family_value->Value();
  }
  return families;
}

}  // namespace

StyleEngine::StyleEngine(Document& document)
    : document_(&document),
      style_containment_scope_tree_(
          MakeGarbageCollected<StyleContainmentScopeTree>()),
      document_style_sheet_collection_(
          MakeGarbageCollected<DocumentStyleSheetCollection>(document)),
      preferred_color_scheme_(mojom::blink::PreferredColorScheme::kLight),
      owner_preferred_color_scheme_(mojom::blink::PreferredColorScheme::kLight),
      owner_color_scheme_(mojom::blink::ColorScheme::kLight) {
  if (document.GetFrame()) {
    resolver_ = MakeGarbageCollected<StyleResolver>(document);
    global_rule_set_ = MakeGarbageCollected<CSSGlobalRuleSet>();
    font_selector_ = CreateCSSFontSelectorFor(document);
    font_selector_->RegisterForInvalidationCallbacks(this);
    if (const FrameOwner* owner = document.GetFrame()->Owner()) {
      owner_color_scheme_ = owner->GetColorScheme();
      owner_preferred_color_scheme_ = owner->GetPreferredColorScheme();
    }

    // Viewport styles are only processed in the main frame of a page with an
    // active viewport. That is, a pages that their own independently zoomable
    // viewport: the outermost main frame.
    DCHECK(document.GetPage());
    VisualViewport& viewport = document.GetPage()->GetVisualViewport();
    if (document.IsInMainFrame() && viewport.IsActiveViewport()) {
      viewport_resolver_ =
          MakeGarbageCollected<ViewportStyleResolver>(document);
    }
  }

  UpdateColorScheme();

  // Mostly for the benefit of unit tests.
  UpdateViewportSize();
}

StyleEngine::~StyleEngine() = default;

TreeScopeStyleSheetCollection& StyleEngine::EnsureStyleSheetCollectionFor(
    TreeScope& tree_scope) {
  if (tree_scope == document_) {
    return GetDocumentStyleSheetCollection();
  }

  StyleSheetCollectionMap::AddResult result =
      style_sheet_collection_map_.insert(&tree_scope, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<ShadowTreeStyleSheetCollection>(
            To<ShadowRoot>(tree_scope));
  }
  return *result.stored_value->value.Get();
}

TreeScopeStyleSheetCollection* StyleEngine::StyleSheetCollectionFor(
    TreeScope& tree_scope) {
  if (tree_scope == document_) {
    return &GetDocumentStyleSheetCollection();
  }

  StyleSheetCollectionMap::iterator it =
      style_sheet_collection_map_.find(&tree_scope);
  if (it == style_sheet_collection_map_.end()) {
    return nullptr;
  }
  return it->value.Get();
}

const HeapVector<Member<StyleSheet>>& StyleEngine::StyleSheetsForStyleSheetList(
    TreeScope& tree_scope) {
  DCHECK(document_);
  TreeScopeStyleSheetCollection& collection =
      EnsureStyleSheetCollectionFor(tree_scope);
  if (document_->IsActive()) {
    collection.UpdateStyleSheetList();
  }
  return collection.StyleSheetsForStyleSheetList();
}

void StyleEngine::InjectSheet(const StyleSheetKey& key,
                              StyleSheetContents* sheet,
                              WebCssOrigin origin) {
  HeapVector<std::pair<StyleSheetKey, Member<CSSStyleSheet>>>&
      injected_style_sheets =
          origin == WebCssOrigin::kUser ? injected_user_style_sheets_
                                        : injected_author_style_sheets_;
  injected_style_sheets.push_back(std::make_pair(
      key, MakeGarbageCollected<CSSStyleSheet>(sheet, *document_)));
  if (origin == WebCssOrigin::kUser) {
    MarkUserStyleDirty();
  } else {
    MarkDocumentDirty();
  }
}

void StyleEngine::RemoveInjectedSheet(const StyleSheetKey& key,
                                      WebCssOrigin origin) {
  HeapVector<std::pair<StyleSheetKey, Member<CSSStyleSheet>>>&
      injected_style_sheets =
          origin == WebCssOrigin::kUser ? injected_user_style_sheets_
                                        : injected_author_style_sheets_;
  // Remove the last sheet that matches.
  const auto& it = base::ranges::find(
      base::Reversed(injected_style_sheets), key,
      &std::pair<StyleSheetKey, Member<CSSStyleSheet>>::first);
  if (it != injected_style_sheets.rend()) {
    injected_style_sheets.erase(std::next(it).base());
    if (origin == WebCssOrigin::kUser) {
      MarkUserStyleDirty();
    } else {
      MarkDocumentDirty();
    }
  }
}

CSSStyleSheet& StyleEngine::EnsureInspectorStyleSheet() {
  if (inspector_style_sheet_) {
    return *inspector_style_sheet_;
  }

  auto* contents = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(*document_));
  inspector_style_sheet_ =
      MakeGarbageCollected<CSSStyleSheet>(contents, *document_);
  MarkDocumentDirty();
  // TODO(futhark@chromium.org): Making the active stylesheets up-to-date here
  // is required by some inspector tests, at least. I theory this should not be
  // necessary. Need to investigate to figure out if/why.
  UpdateActiveStyle();
  return *inspector_style_sheet_;
}

void StyleEngine::AddPendingBlockingSheet(Node& style_sheet_candidate_node,
                                          PendingSheetType type) {
  DCHECK(type == PendingSheetType::kBlocking ||
         type == PendingSheetType::kDynamicRenderBlocking);

  auto* manager = GetDocument().GetRenderBlockingResourceManager();
  bool is_render_blocking =
      manager && manager->AddPendingStylesheet(style_sheet_candidate_node);

  if (type != PendingSheetType::kBlocking) {
    return;
  }

  pending_script_blocking_stylesheets_++;

  if (!is_render_blocking) {
    pending_parser_blocking_stylesheets_++;
    if (GetDocument().body()) {
      GetDocument().CountUse(
          WebFeature::kPendingStylesheetAddedAfterBodyStarted);
    }
    GetDocument().DidAddPendingParserBlockingStylesheet();
  }
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::RemovePendingBlockingSheet(Node& style_sheet_candidate_node,
                                             PendingSheetType type) {
  DCHECK(type == PendingSheetType::kBlocking ||
         type == PendingSheetType::kDynamicRenderBlocking);

  if (style_sheet_candidate_node.isConnected()) {
    SetNeedsActiveStyleUpdate(style_sheet_candidate_node.GetTreeScope());
  }

  auto* manager = GetDocument().GetRenderBlockingResourceManager();
  bool is_render_blocking =
      manager && manager->RemovePendingStylesheet(style_sheet_candidate_node);

  if (type != PendingSheetType::kBlocking) {
    return;
  }

  if (!is_render_blocking) {
    DCHECK_GT(pending_parser_blocking_stylesheets_, 0);
    pending_parser_blocking_stylesheets_--;
    if (!pending_parser_blocking_stylesheets_) {
      GetDocument().DidLoadAllPendingParserBlockingStylesheets();
    }
  }

  // Make sure we knew this sheet was pending, and that our count isn't out of
  // sync.
  DCHECK_GT(pending_script_blocking_stylesheets_, 0);

  pending_script_blocking_stylesheets_--;
  if (pending_script_blocking_stylesheets_) {
    return;
  }

  GetDocument().DidRemoveAllPendingStylesheets();
}

void StyleEngine::SetNeedsActiveStyleUpdate(TreeScope& tree_scope) {
  DCHECK(tree_scope.RootNode().isConnected());
  if (GetDocument().IsActive()) {
    MarkTreeScopeDirty(tree_scope);
  }
}

void StyleEngine::AddStyleSheetCandidateNode(Node& node) {
  if (!node.isConnected() || GetDocument().IsDetached()) {
    return;
  }

  DCHECK(!IsXSLStyleSheet(node));
  TreeScope& tree_scope = node.GetTreeScope();
  EnsureStyleSheetCollectionFor(tree_scope).AddStyleSheetCandidateNode(node);

  SetNeedsActiveStyleUpdate(tree_scope);
  if (tree_scope != document_) {
    active_tree_scopes_.insert(&tree_scope);
  }
}

void StyleEngine::RemoveStyleSheetCandidateNode(
    Node& node,
    ContainerNode& insertion_point) {
  DCHECK(!IsXSLStyleSheet(node));
  DCHECK(insertion_point.isConnected());

  ShadowRoot* shadow_root = node.ContainingShadowRoot();
  if (!shadow_root) {
    shadow_root = insertion_point.ContainingShadowRoot();
  }

  static_assert(std::is_base_of<TreeScope, ShadowRoot>::value,
                "The ShadowRoot must be subclass of TreeScope.");
  TreeScope& tree_scope =
      shadow_root ? static_cast<TreeScope&>(*shadow_root) : GetDocument();
  TreeScopeStyleSheetCollection* collection =
      StyleSheetCollectionFor(tree_scope);
  // After detaching document, collection could be null. In the case,
  // we should not update anything. Instead, just return.
  if (!collection) {
    return;
  }
  collection->RemoveStyleSheetCandidateNode(node);

  SetNeedsActiveStyleUpdate(tree_scope);
}

void StyleEngine::ModifiedStyleSheetCandidateNode(Node& node) {
  if (node.isConnected()) {
    SetNeedsActiveStyleUpdate(node.GetTreeScope());
  }
}

void StyleEngine::AdoptedStyleSheetAdded(TreeScope& tree_scope,
                                         CSSStyleSheet* sheet) {
  if (GetDocument().IsDetached()) {
    return;
  }
  sheet->AddedAdoptedToTreeScope(tree_scope);
  if (!tree_scope.RootNode().isConnected()) {
    return;
  }
  EnsureStyleSheetCollectionFor(tree_scope);
  if (tree_scope != document_) {
    active_tree_scopes_.insert(&tree_scope);
  }
  SetNeedsActiveStyleUpdate(tree_scope);
}

void StyleEngine::AdoptedStyleSheetRemoved(TreeScope& tree_scope,
                                           CSSStyleSheet* sheet) {
  if (GetDocument().IsDetached()) {
    return;
  }
  sheet->RemovedAdoptedFromTreeScope(tree_scope);
  if (!tree_scope.RootNode().isConnected()) {
    return;
  }
  if (!StyleSheetCollectionFor(tree_scope)) {
    return;
  }
  SetNeedsActiveStyleUpdate(tree_scope);
}

void StyleEngine::MediaQueryAffectingValueChanged(TreeScope& tree_scope,
                                                  MediaValueChange change) {
  auto* collection = StyleSheetCollectionFor(tree_scope);
  DCHECK(collection);
  if (AffectedByMediaValueChange(collection->ActiveStyleSheets(), change)) {
    SetNeedsActiveStyleUpdate(tree_scope);
  }
}

void StyleEngine::WatchedSelectorsChanged() {
  DCHECK(global_rule_set_);
  global_rule_set_->InitWatchedSelectorsRuleSet(GetDocument());
  // TODO(futhark@chromium.org): Should be able to use RuleSetInvalidation here.
  MarkAllElementsForStyleRecalc(StyleChangeReasonForTracing::Create(
      style_change_reason::kDeclarativeContent));
}

void StyleEngine::DocumentRulesSelectorsChanged() {
  DCHECK(global_rule_set_);
  Member<RuleSet> old_rule_set =
      global_rule_set_->DocumentRulesSelectorsRuleSet();
  global_rule_set_->UpdateDocumentRulesSelectorsRuleSet(GetDocument());
  Member<RuleSet> new_rule_set =
      global_rule_set_->DocumentRulesSelectorsRuleSet();
  DCHECK_NE(old_rule_set, new_rule_set);

  HeapHashSet<Member<RuleSet>> changed_rule_sets;
  if (old_rule_set) {
    changed_rule_sets.insert(old_rule_set);
  }
  if (new_rule_set) {
    changed_rule_sets.insert(new_rule_set);
  }

  const unsigned changed_rule_flags = GetRuleSetFlags(changed_rule_sets);
  InvalidateForRuleSetChanges(GetDocument(), changed_rule_sets,
                              changed_rule_flags, kInvalidateAllScopes);

  // The global rule set must be updated immediately, so that any DOM mutations
  // that happen after this (but before the next style update) can use the
  // updated invalidation sets.
  UpdateActiveStyle();
}

bool StyleEngine::ShouldUpdateDocumentStyleSheetCollection() const {
  return document_scope_dirty_;
}

bool StyleEngine::ShouldUpdateShadowTreeStyleSheetCollection() const {
  return !dirty_tree_scopes_.empty();
}

void StyleEngine::MediaQueryAffectingValueChanged(
    UnorderedTreeScopeSet& tree_scopes,
    MediaValueChange change) {
  for (TreeScope* tree_scope : tree_scopes) {
    DCHECK(tree_scope != document_);
    MediaQueryAffectingValueChanged(*tree_scope, change);
  }
}

void StyleEngine::AddTextTrack(TextTrack* text_track) {
  text_tracks_.insert(text_track);
}

void StyleEngine::RemoveTextTrack(TextTrack* text_track) {
  text_tracks_.erase(text_track);
}

Element* StyleEngine::EnsureVTTOriginatingElement() {
  if (!vtt_originating_element_) {
    vtt_originating_element_ = MakeGarbageCollected<Element>(
        QualifiedName(g_null_atom, g_empty_atom, g_empty_atom), document_);
  }
  return vtt_originating_element_.Get();
}

void StyleEngine::MediaQueryAffectingValueChanged(
    HeapHashSet<Member<TextTrack>>& text_tracks,
    MediaValueChange change) {
  if (text_tracks.empty()) {
    return;
  }

  for (auto text_track : text_tracks) {
    bool style_needs_recalc = false;
    auto style_sheets = text_track->GetCSSStyleSheets();
    for (const auto& sheet : style_sheets) {
      StyleSheetContents* contents = sheet->Contents();
      if (contents->HasMediaQueries()) {
        style_needs_recalc = true;
        contents->ClearRuleSet();
      }
    }

    if (style_needs_recalc && text_track->Owner()) {
      // Use kSubtreeTreeStyleChange instead of RuleSet style invalidation
      // because it won't be expensive for tracks and we won't have dynamic
      // changes.
      text_track->Owner()->SetNeedsStyleRecalc(
          kSubtreeStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kShadow));
    }
  }
}

void StyleEngine::MediaQueryAffectingValueChanged(MediaValueChange change) {
  if (AffectedByMediaValueChange(active_user_style_sheets_, change)) {
    MarkUserStyleDirty();
  }
  MediaQueryAffectingValueChanged(GetDocument(), change);
  MediaQueryAffectingValueChanged(active_tree_scopes_, change);
  MediaQueryAffectingValueChanged(text_tracks_, change);
  if (resolver_) {
    resolver_->UpdateMediaType();
  }
}

void StyleEngine::UpdateActiveStyleSheetsInShadow(
    TreeScope* tree_scope,
    UnorderedTreeScopeSet& tree_scopes_removed) {
  DCHECK_NE(tree_scope, document_);
  auto* collection =
      To<ShadowTreeStyleSheetCollection>(StyleSheetCollectionFor(*tree_scope));
  DCHECK(collection);
  collection->UpdateActiveStyleSheets(*this);
  if (!collection->HasStyleSheetCandidateNodes() &&
      !tree_scope->HasAdoptedStyleSheets()) {
    tree_scopes_removed.insert(tree_scope);
    // When removing TreeScope from ActiveTreeScopes,
    // its resolver should be destroyed by invoking resetAuthorStyle.
    DCHECK(!tree_scope->GetScopedStyleResolver());
  }
}

void StyleEngine::UpdateActiveUserStyleSheets() {
  DCHECK(user_style_dirty_);

  ActiveStyleSheetVector new_active_sheets;
  for (auto& sheet : injected_user_style_sheets_) {
    if (RuleSet* rule_set = RuleSetForSheet(*sheet.second)) {
      new_active_sheets.push_back(std::make_pair(sheet.second, rule_set));
    }
  }

  ApplyUserRuleSetChanges(active_user_style_sheets_, new_active_sheets);
  new_active_sheets.swap(active_user_style_sheets_);
}

void StyleEngine::UpdateActiveStyleSheets() {
  if (!NeedsActiveStyleSheetUpdate()) {
    return;
  }

  DCHECK(!GetDocument().InStyleRecalc());
  DCHECK(GetDocument().IsActive());

  TRACE_EVENT0("blink,blink_style", "StyleEngine::updateActiveStyleSheets");

  if (user_style_dirty_) {
    UpdateActiveUserStyleSheets();
  }

  if (ShouldUpdateDocumentStyleSheetCollection()) {
    GetDocumentStyleSheetCollection().UpdateActiveStyleSheets(*this);
  }

  if (ShouldUpdateShadowTreeStyleSheetCollection()) {
    UnorderedTreeScopeSet tree_scopes_removed;
    for (TreeScope* tree_scope : dirty_tree_scopes_) {
      UpdateActiveStyleSheetsInShadow(tree_scope, tree_scopes_removed);
    }
    for (TreeScope* tree_scope : tree_scopes_removed) {
      active_tree_scopes_.erase(tree_scope);
    }
  }

  probe::ActiveStyleSheetsUpdated(document_);

  dirty_tree_scopes_.clear();
  document_scope_dirty_ = false;
  tree_scopes_removed_ = false;
  user_style_dirty_ = false;
}

void StyleEngine::UpdateCounterStyles() {
  if (!counter_styles_need_update_) {
    return;
  }
  CounterStyleMap::MarkAllDirtyCounterStyles(GetDocument(),
                                             active_tree_scopes_);
  CounterStyleMap::ResolveAllReferences(GetDocument(), active_tree_scopes_);
  counter_styles_need_update_ = false;
}

void StyleEngine::MarkPositionTryStylesDirty(
    const HeapHashSet<Member<RuleSet>>& changed_rule_sets) {
  for (RuleSet* rule_set : changed_rule_sets) {
    CHECK(rule_set);
    for (StyleRulePositionTry* try_rule : rule_set->PositionTryRules()) {
      if (try_rule) {
        dirty_position_try_names_.insert(try_rule->Name());
      }
    }
  }
  // TODO(crbug.com/1381623): Currently invalidating all elements in the
  // document with position-options, regardless of where the @position-try rules
  // are added. In order to make invalidation more targeted we would need to add
  // per tree-scope dirtiness, but also adding at-rules in one tree-scope may
  // affect multiple other tree scopes through :host, ::slotted, ::part,
  // exportparts, and inheritance. Doing that is going to be a lot more
  // complicated.
  position_try_styles_dirty_ = true;
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::InvalidatePositionTryStyles() {
  if (!position_try_styles_dirty_) {
    return;
  }
  position_try_styles_dirty_ = false;
  const bool mark_style_dirty = true;
  GetDocument().GetLayoutView()->InvalidateSubtreePositionTry(mark_style_dirty);
}

void StyleEngine::UpdateViewport() {
  if (viewport_resolver_) {
    viewport_resolver_->UpdateViewport();
  }
}

bool StyleEngine::NeedsActiveStyleUpdate() const {
  return (viewport_resolver_ && viewport_resolver_->NeedsUpdate()) ||
         NeedsActiveStyleSheetUpdate() ||
         (global_rule_set_ && global_rule_set_->IsDirty());
}

void StyleEngine::UpdateActiveStyle() {
  DCHECK(GetDocument().IsActive());
  DCHECK(IsMainThread());
  TRACE_EVENT0("blink", "Document::updateActiveStyle");
  InvalidationSetToSelectorMap::StartOrStopTrackingIfNeeded(*this);
  UpdateViewport();
  UpdateActiveStyleSheets();
  UpdateGlobalRuleSet();
}

const ActiveStyleSheetVector StyleEngine::ActiveStyleSheetsForInspector() {
  if (GetDocument().IsActive()) {
    UpdateActiveStyle();
  }

  if (active_tree_scopes_.empty()) {
    return GetDocumentStyleSheetCollection().ActiveStyleSheets();
  }

  ActiveStyleSheetVector active_style_sheets;

  active_style_sheets.AppendVector(
      GetDocumentStyleSheetCollection().ActiveStyleSheets());
  for (TreeScope* tree_scope : active_tree_scopes_) {
    if (TreeScopeStyleSheetCollection* collection =
            style_sheet_collection_map_.at(tree_scope)) {
      active_style_sheets.AppendVector(collection->ActiveStyleSheets());
    }
  }

  // FIXME: Inspector needs a vector which has all active stylesheets.
  // However, creating such a large vector might cause performance regression.
  // Need to implement some smarter solution.
  return active_style_sheets;
}

void StyleEngine::UpdateCounters() {
  if (!CountersChanged() || !GetDocument().documentElement()) {
    return;
  }
  counters_changed_ = false;
  CountersAttachmentContext context;
  context.SetAttachmentRootIsDocumentElement();
  UpdateCounters(*GetDocument().documentElement(), context);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

// Recursively look for potential LayoutCounters to update,
// since in case of ::marker they can be deep child of original
// pseudo element's layout object.
void StyleEngine::UpdateLayoutCounters(const LayoutObject& layout_object,
                                       CountersAttachmentContext& context) {
  // Check out the parameter list ^^^
  for (LayoutObject* child = layout_object.NextInPreOrder(&layout_object);
       child; child = child->NextInPreOrder(&layout_object)) {
    if (auto* layout_counter = DynamicTo<LayoutCounter>(child)) {
      Vector<int> counter_values =
          context.GetCounterValues(layout_object, layout_counter->Identifier(),
                                   layout_counter->Separator().IsNull());
      layout_counter->UpdateCounter(std::move(counter_values));
    }
  }
}

void StyleEngine::UpdateCounters(const Element& element,
                                 CountersAttachmentContext& context) {
  LayoutObject* layout_object = element.GetLayoutObject();
  // Manually update list item ordinals here.
  if (layout_object) {
    context.EnterObject(*layout_object);
    if (auto* ng_list_item = DynamicTo<LayoutListItem>(layout_object)) {
      if (!ng_list_item->Ordinal().ExplicitValue().has_value()) {
        ng_list_item->Ordinal().MarkDirty();
        ng_list_item->OrdinalValueChanged();
      }
    } else if (auto* inline_list_item =
                   DynamicTo<LayoutInlineListItem>(layout_object)) {
      if (!inline_list_item->Ordinal().ExplicitValue().has_value()) {
        inline_list_item->Ordinal().MarkDirty();
        inline_list_item->OrdinalValueChanged();
      }
    }
    if (element.GetComputedStyle() &&
        !element.GetComputedStyle()->ContentBehavesAsNormal()) {
      UpdateLayoutCounters(*layout_object, context);
    }
  }
  for (Node* child = LayoutTreeBuilderTraversal::FirstChild(element); child;
       child = LayoutTreeBuilderTraversal::NextSibling(*child)) {
    if (Element* child_element = DynamicTo<Element>(child)) {
      UpdateCounters(*child_element, context);
    }
  }
  if (layout_object) {
    context.LeaveObject(*layout_object);
  }
}

void StyleEngine::ShadowRootInsertedToDocument(ShadowRoot& shadow_root) {
  DCHECK(shadow_root.isConnected());
  if (GetDocument().IsDetached() || !shadow_root.HasAdoptedStyleSheets()) {
    return;
  }
  EnsureStyleSheetCollectionFor(shadow_root);
  SetNeedsActiveStyleUpdate(shadow_root);
  active_tree_scopes_.insert(&shadow_root);
}

void StyleEngine::ShadowRootRemovedFromDocument(ShadowRoot* shadow_root) {
  style_sheet_collection_map_.erase(shadow_root);
  active_tree_scopes_.erase(shadow_root);
  dirty_tree_scopes_.erase(shadow_root);
  tree_scopes_removed_ = true;
  ResetAuthorStyle(*shadow_root);
}

void StyleEngine::ResetAuthorStyle(TreeScope& tree_scope) {
  ScopedStyleResolver* scoped_resolver = tree_scope.GetScopedStyleResolver();
  if (!scoped_resolver) {
    return;
  }

  if (global_rule_set_) {
    global_rule_set_->MarkDirty();
  }
  if (tree_scope.RootNode().IsDocumentNode()) {
    scoped_resolver->ResetStyle();
    return;
  }

  tree_scope.ClearScopedStyleResolver();
}

StyleContainmentScopeTree& StyleEngine::EnsureStyleContainmentScopeTree() {
  if (!style_containment_scope_tree_) {
    style_containment_scope_tree_ =
        MakeGarbageCollected<StyleContainmentScopeTree>();
  }
  return *style_containment_scope_tree_;
}

void StyleEngine::SetRuleUsageTracker(StyleRuleUsageTracker* tracker) {
  tracker_ = tracker;

  if (resolver_) {
    resolver_->SetRuleUsageTracker(tracker_);
  }
}

Font StyleEngine::ComputeFont(Element& element,
                              const ComputedStyle& font_style,
                              const CSSPropertyValueSet& font_properties) {
  UpdateActiveStyle();
  return GetStyleResolver().ComputeFont(element, font_style, font_properties);
}

RuleSet* StyleEngine::RuleSetForSheet(CSSStyleSheet& sheet) {
  if (!sheet.MatchesMediaQueries(EnsureMediaQueryEvaluator())) {
    return nullptr;
  }
  return &sheet.Contents()->EnsureRuleSet(*media_query_evaluator_);
}

RuleSet* StyleEngine::RuleSetScope::RuleSetForSheet(StyleEngine& engine,
                                                    CSSStyleSheet* css_sheet) {
  RuleSet* rule_set = engine.RuleSetForSheet(*css_sheet);
  if (rule_set && rule_set->HasCascadeLayers() &&
      !css_sheet->Contents()->HasSingleOwnerNode() &&
      !layer_rule_sets_.insert(rule_set).is_new_entry) {
    // The condition above is met for a stylesheet with cascade layers which
    // shares StyleSheetContents with another stylesheet in this TreeScope.
    // WillMutateRules() creates a unique StyleSheetContents for this sheet to
    // avoid incorrectly identifying two separate anonymous layers as the same
    // layer.
    css_sheet->WillMutateRules();
    rule_set = engine.RuleSetForSheet(*css_sheet);
  }
  return rule_set;
}

void StyleEngine::ClearResolvers() {
  DCHECK(!GetDocument().InStyleRecalc());

  GetDocument().ClearScopedStyleResolver();
  for (TreeScope* tree_scope : active_tree_scopes_) {
    tree_scope->ClearScopedStyleResolver();
  }

  if (resolver_) {
    TRACE_EVENT1("blink", "StyleEngine::clearResolver", "frame",
                 GetFrameIdForTracing(GetDocument().GetFrame()));
    resolver_->Dispose();
    resolver_.Clear();
  }
}

void StyleEngine::DidDetach() {
  ClearResolvers();
  if (global_rule_set_) {
    global_rule_set_->Dispose();
  }
  global_rule_set_ = nullptr;
  dirty_tree_scopes_.clear();
  active_tree_scopes_.clear();
  viewport_resolver_ = nullptr;
  media_query_evaluator_ = nullptr;
  style_invalidation_root_.Clear();
  style_recalc_root_.Clear();
  layout_tree_rebuild_root_.Clear();
  if (font_selector_) {
    font_selector_->GetFontFaceCache()->ClearAll();
  }
  font_selector_ = nullptr;
  if (environment_variables_) {
    environment_variables_->DetachFromParent();
  }
  environment_variables_ = nullptr;
  style_containment_scope_tree_ = nullptr;
}

bool StyleEngine::ClearFontFaceCacheAndAddUserFonts(
    const ActiveStyleSheetVector& user_sheets) {
  bool fonts_changed = false;

  if (font_selector_ &&
      font_selector_->GetFontFaceCache()->ClearCSSConnected()) {
    fonts_changed = true;
    if (resolver_) {
      resolver_->InvalidateMatchedPropertiesCache();
    }
  }

  // Rebuild the font cache with @font-face rules from user style sheets.
  for (unsigned i = 0; i < user_sheets.size(); ++i) {
    DCHECK(user_sheets[i].second);
    if (AddUserFontFaceRules(*user_sheets[i].second)) {
      fonts_changed = true;
    }
  }

  return fonts_changed;
}

void StyleEngine::UpdateGenericFontFamilySettings() {
  // FIXME: we should not update generic font family settings when
  // document is inactive.
  DCHECK(GetDocument().IsActive());

  if (!font_selector_) {
    return;
  }

  font_selector_->UpdateGenericFontFamilySettings(*document_);
  if (resolver_) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
  FontCache::Get().InvalidateShapeCache();
}

void StyleEngine::RemoveFontFaceRules(
    const HeapVector<Member<const StyleRuleFontFace>>& font_face_rules) {
  if (!font_selector_) {
    return;
  }

  FontFaceCache* cache = font_selector_->GetFontFaceCache();
  for (const auto& rule : font_face_rules) {
    cache->Remove(rule);
  }
  if (resolver_) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
}

void StyleEngine::MarkTreeScopeDirty(TreeScope& scope) {
  if (scope == document_) {
    MarkDocumentDirty();
    return;
  }

  TreeScopeStyleSheetCollection* collection = StyleSheetCollectionFor(scope);
  DCHECK(collection);
  collection->MarkSheetListDirty();
  dirty_tree_scopes_.insert(&scope);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::MarkDocumentDirty() {
  document_scope_dirty_ = true;
  document_style_sheet_collection_->MarkSheetListDirty();
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::MarkUserStyleDirty() {
  user_style_dirty_ = true;
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::MarkViewportStyleDirty() {
  viewport_style_dirty_ = true;
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

CSSStyleSheet* StyleEngine::CreateSheet(
    Element& element,
    const String& text,
    TextPosition start_position,
    PendingSheetType type,
    RenderBlockingBehavior render_blocking_behavior) {
  DCHECK(element.GetDocument() == GetDocument());
  CSSStyleSheet* style_sheet = nullptr;

  if (type != PendingSheetType::kNonBlocking) {
    AddPendingBlockingSheet(element, type);
  }

  // The style sheet text can be long; hundreds of kilobytes. In order not to
  // insert such a huge string into the AtomicString table, we take its hash
  // instead and use that. (This is not a cryptographic hash, so a page could
  // cause collisions if it wanted to, but only within its own renderer.)
  // Note that in many cases, we won't actually be able to free the
  // memory used by the string, since it may e.g. be already stuck in
  // the DOM (as text contents of the <style> tag), but it may eventually
  // be parked (compressed, or stored to disk) if there's memory pressure,
  // or otherwise dropped, so this keeps us from being the only thing
  // that keeps it alive.
  AtomicString key;
  if (text.length() >= 1024) {
    size_t digest = FastHash(text.RawByteSpan());
    key = AtomicString(base::byte_span_from_ref(digest));
  } else {
    key = AtomicString(text);
  }

  auto result = text_to_sheet_cache_.insert(key, nullptr);
  StyleSheetContents* contents = result.stored_value->value;
  if (result.is_new_entry || !contents ||
      !contents->IsCacheableForStyleElement()) {
    result.stored_value->value = nullptr;
    style_sheet =
        ParseSheet(element, text, start_position, render_blocking_behavior);
    if (style_sheet->Contents()->IsCacheableForStyleElement()) {
      result.stored_value->value = style_sheet->Contents();
    }
  } else {
    DCHECK(contents);
    DCHECK(contents->IsCacheableForStyleElement());
    DCHECK(contents->HasSingleOwnerDocument());
    contents->SetIsUsedFromTextCache();
    style_sheet =
        CSSStyleSheet::CreateInline(contents, element, start_position);
  }

  DCHECK(style_sheet);
  if (!element.IsInShadowTree()) {
    String title = element.title();
    if (!title.empty()) {
      style_sheet->SetTitle(title);
      SetPreferredStylesheetSetNameIfNotSet(title);
    }
  }
  return style_sheet;
}

CSSStyleSheet* StyleEngine::ParseSheet(
    Element& element,
    const String& text,
    TextPosition start_position,
    RenderBlockingBehavior render_blocking_behavior) {
  CSSStyleSheet* style_sheet = nullptr;
  style_sheet = CSSStyleSheet::CreateInline(element, NullURL(), start_position,
                                            GetDocument().Encoding());
  style_sheet->Contents()->SetRenderBlocking(render_blocking_behavior);
  style_sheet->Contents()->ParseString(text);
  return style_sheet;
}

void StyleEngine::CollectUserStyleFeaturesTo(RuleFeatureSet& features) const {
  for (unsigned i = 0; i < active_user_style_sheets_.size(); ++i) {
    CSSStyleSheet* sheet = active_user_style_sheets_[i].first;
    features.MutableMediaQueryResultFlags().Add(
        sheet->GetMediaQueryResultFlags());
    DCHECK(sheet->Contents()->HasRuleSet());
    features.Merge(sheet->Contents()->GetRuleSet().Features());
  }
}

void StyleEngine::CollectScopedStyleFeaturesTo(RuleFeatureSet& features) const {
  HeapHashSet<Member<const StyleSheetContents>>
      visited_shared_style_sheet_contents;
  if (GetDocument().GetScopedStyleResolver()) {
    GetDocument().GetScopedStyleResolver()->CollectFeaturesTo(
        features, visited_shared_style_sheet_contents);
  }
  for (TreeScope* tree_scope : active_tree_scopes_) {
    if (ScopedStyleResolver* resolver = tree_scope->GetScopedStyleResolver()) {
      resolver->CollectFeaturesTo(features,
                                  visited_shared_style_sheet_contents);
    }
  }
}

void StyleEngine::MarkViewportUnitDirty(ViewportUnitFlag flag) {
  if (viewport_unit_dirty_flags_ & static_cast<unsigned>(flag)) {
    return;
  }

  viewport_unit_dirty_flags_ |= static_cast<unsigned>(flag);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

namespace {

void SetNeedsStyleRecalcForViewportUnits(TreeScope& tree_scope,
                                         unsigned dirty_flags) {
  for (Element* element = ElementTraversal::FirstWithin(tree_scope.RootNode());
       element; element = ElementTraversal::NextIncludingPseudo(*element)) {
    if (ShadowRoot* root = element->GetShadowRoot()) {
      SetNeedsStyleRecalcForViewportUnits(*root, dirty_flags);
    }
    const ComputedStyle* style = element->GetComputedStyle();
    if (style && ((style->ViewportUnitFlags() & dirty_flags) ||
                  style->HighlightPseudoElementStylesDependOnViewportUnits())) {
      element->SetNeedsStyleRecalc(kLocalStyleChange,
                                   StyleChangeReasonForTracing::Create(
                                       style_change_reason::kViewportUnits));
    }
  }
}

}  // namespace

void StyleEngine::InvalidateViewportUnitStylesIfNeeded() {
  if (!viewport_unit_dirty_flags_) {
    return;
  }
  unsigned dirty_flags = 0;
  std::swap(viewport_unit_dirty_flags_, dirty_flags);

  // If there are registered custom properties which depend on the invalidated
  // viewport units, it can potentially affect every element.
  if (initial_data_ && (initial_data_->GetViewportUnitFlags() & dirty_flags)) {
    InvalidateInitialData();
    MarkAllElementsForStyleRecalc(StyleChangeReasonForTracing::Create(
        style_change_reason::kViewportUnits));
    return;
  }

  SetNeedsStyleRecalcForViewportUnits(GetDocument(), dirty_flags);
}

void StyleEngine::InvalidateStyleAndLayoutForFontUpdates() {
  if (!fonts_need_update_) {
    return;
  }

  TRACE_EVENT0("blink", "StyleEngine::InvalidateStyleAndLayoutForFontUpdates");

  fonts_need_update_ = false;

  if (Element* root = GetDocument().documentElement()) {
    TRACE_EVENT0("blink", "Node::MarkSubtreeNeedsStyleRecalcForFontUpdates");
    root->MarkSubtreeNeedsStyleRecalcForFontUpdates();
  }

  // TODO(xiaochengh): Move layout invalidation after style update.
  if (LayoutView* layout_view = GetDocument().GetLayoutView()) {
    TRACE_EVENT0("blink", "LayoutObject::InvalidateSubtreeForFontUpdates");
    layout_view->InvalidateSubtreeLayoutForFontUpdates();
  }
}

void StyleEngine::MarkFontsNeedUpdate() {
  fonts_need_update_ = true;
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::MarkCounterStylesNeedUpdate() {
  counter_styles_need_update_ = true;
  if (LayoutView* layout_view = GetDocument().GetLayoutView()) {
    layout_view->SetNeedsMarkerOrCounterUpdate();
  }
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::FontsNeedUpdate(FontSelector*, FontInvalidationReason) {
  if (!GetDocument().IsActive()) {
    return;
  }

  if (resolver_) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
  MarkViewportStyleDirty();
  MarkFontsNeedUpdate();

  probe::FontsUpdated(document_->GetExecutionContext(), nullptr, String(),
                      nullptr);
}

void StyleEngine::PlatformColorsChanged() {
  UpdateForcedBackgroundColor();
  UpdateColorSchemeBackground(/* color_scheme_changed */ true);
  if (resolver_) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
  MarkAllElementsForStyleRecalc(StyleChangeReasonForTracing::Create(
      style_change_reason::kPlatformColorChange));

  // Invalidate paint so that SVG images can update the preferred color scheme
  // of their document.
  if (auto* view = GetDocument().GetLayoutView()) {
    view->InvalidatePaintForViewAndDescendants();
  }
}

bool StyleEngine::ShouldSkipInvalidationFor(const Element& element) const {
  DCHECK(element.GetDocument() == &GetDocument())
      << "Only schedule invalidations using the StyleEngine of the Document "
         "which owns the element.";
  if (!element.InActiveDocument()) {
    return true;
  }
  if (!global_rule_set_) {
    // TODO(crbug.com/1175902): This is a speculative fix for a crash.
    NOTREACHED_IN_MIGRATION()
        << "global_rule_set_ should only be null for inactive documents.";
    return true;
  }
  if (GetDocument().InStyleRecalc()) {
#if DCHECK_IS_ON()
    // TODO(futhark): The InStyleRecalc() if-guard above should have been a
    // DCHECK(!InStyleRecalc()), but there are a couple of cases where we try to
    // invalidate style from style recalc:
    //
    // 1. We may animate the class attribute of an SVG element and change it
    //    during style recalc when applying the animation effect.
    // 2. We may call SetInlineStyle on elements in a UA shadow tree as part of
    //    style recalc. For instance from HTMLImageFallbackHelper.
    //
    // If there are more cases, we need to adjust the DCHECKs below, but ideally
    // The origin of these invalidations should be fixed.
    if (!element.IsSVGElement()) {
      DCHECK(element.ContainingShadowRoot());
      DCHECK(element.ContainingShadowRoot()->IsUserAgent());
    }
#endif  // DCHECK_IS_ON()
    return true;
  }
  return false;
}

bool StyleEngine::IsSubtreeAndSiblingsStyleDirty(const Element& element) const {
  if (GetDocument().GetStyleChangeType() == kSubtreeStyleChange) {
    return true;
  }
  Element* root = GetDocument().documentElement();
  if (!root || root->GetStyleChangeType() == kSubtreeStyleChange) {
    return true;
  }
  if (!element.parentNode()) {
    return true;
  }
  return element.parentNode()->GetStyleChangeType() == kSubtreeStyleChange;
}

namespace {

bool PossiblyAffectingHasState(Element& element) {
  return element.AncestorsOrAncestorSiblingsAffectedByHas() ||
         element.GetSiblingsAffectedByHasFlags() ||
         element.AffectedByLogicalCombinationsInHas();
}

bool InsertionOrRemovalPossiblyAffectHasStateOfAncestorsOrAncestorSiblings(
    Element* parent) {
  // Only if the parent of the inserted element or subtree has the
  // AncestorsOrAncestorSiblingsAffectedByHas or
  // SiblingsAffectedByHasForSiblingDescendantRelationship flag set, the
  // inserted element or subtree possibly affect the :has() state on its (or the
  // subtree root's) ancestors.
  return parent && (parent->AncestorsOrAncestorSiblingsAffectedByHas() ||
                    parent->HasSiblingsAffectedByHasFlags(
                        SiblingsAffectedByHasFlags::
                            kFlagForSiblingDescendantRelationship));
}

bool InsertionOrRemovalPossiblyAffectHasStateOfPreviousSiblings(
    Element* previous_sibling) {
  // Only if the previous sibling of the inserted element or subtree has the
  // SiblingsAffectedByHas flag set, the inserted element or subtree possibly
  // affect the :has() state on its (or the subtree root's) previous siblings.
  return previous_sibling && previous_sibling->GetSiblingsAffectedByHasFlags();
}

inline Element* SelfOrPreviousSibling(Node* node) {
  if (!node) {
    return nullptr;
  }
  if (Element* element = DynamicTo<Element>(node)) {
    return element;
  }
  return ElementTraversal::PreviousSibling(*node);
}

}  // namespace

void PossiblyScheduleNthPseudoInvalidations(Node& node) {
  if (!node.IsElementNode()) {
    return;
  }
  ContainerNode* parent = node.parentNode();
  if (parent == nullptr) {
    return;
  }

  if ((parent->ChildrenAffectedByForwardPositionalRules() &&
       node.nextSibling()) ||
      (parent->ChildrenAffectedByBackwardPositionalRules() &&
       node.previousSibling())) {
    node.GetDocument().GetStyleEngine().ScheduleNthPseudoInvalidations(*parent);
  }
}

void StyleEngine::InvalidateElementAffectedByHas(
    Element& element,
    bool for_element_affected_by_pseudo_in_has) {
  if (for_element_affected_by_pseudo_in_has &&
      !element.AffectedByPseudoInHas()) {
    return;
  }

  if (element.AffectedBySubjectHas()) {
    // TODO(blee@igalia.com) Need filtering for irrelevant elements.
    // e.g. When we have '.a:has(.b) {}', '.c:has(.d) {}', mutation of class
    // value 'd' can invalidate ancestor with class value 'a' because we
    // don't have any filtering for this case.
    element.SetNeedsStyleRecalc(
        StyleChangeType::kLocalStyleChange,
        StyleChangeReasonForTracing::Create(
            blink::style_change_reason::kAffectedByHas));

    if (GetRuleFeatureSet().GetRuleInvalidationData().UsesHasInsideNth()) {
      PossiblyScheduleNthPseudoInvalidations(element);
    }
  }

  if (element.AffectedByNonSubjectHas()) {
    InvalidationLists invalidation_lists;
    GetRuleFeatureSet()
        .GetRuleInvalidationData()
        .CollectInvalidationSetsForPseudoClass(invalidation_lists, element,
                                               CSSSelector::kPseudoHas);
    pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                           element);
  }
}

// Context class to provide :has() invalidation traversal information.
//
// This class provides this information to the :has() invalidation traversal:
// - first element of the traversal.
// - flag to indicate whether the traversal moves to the parent of the first
//   element.
// - flag to indicate whether the :has() invalidation invalidates the elements
//   with AffectedByPseudoInHas flag set.
class StyleEngine::PseudoHasInvalidationTraversalContext {
  STACK_ALLOCATED();

 public:
  Element* FirstElement() const { return first_element_; }

  // Returns true if the traversal starts at the shadow host for an
  // insertion/removal at a shadow root. In that case we only need to
  // invalidate for that host.
  bool IsFirstElementShadowHost() const {
    return is_first_element_shadow_host_;
  }

  bool TraverseToParentOfFirstElement() const {
    return traverse_to_parent_of_first_element_;
  }

  bool ForElementAffectedByPseudoInHas() const {
    return for_element_affected_by_pseudo_in_has_;
  }

  PseudoHasInvalidationTraversalContext& SetForElementAffectedByPseudoInHas() {
    for_element_affected_by_pseudo_in_has_ = true;
    return *this;
  }

  // Create :has() invalidation traversal context for attribute change or
  // pseudo state change without structural DOM changes.
  static PseudoHasInvalidationTraversalContext ForAttributeOrPseudoStateChange(
      Element& changed_element) {
    bool traverse_ancestors =
        changed_element.AncestorsOrAncestorSiblingsAffectedByHas();

    Element* first_element = nullptr;
    bool is_first_element_shadow_host = false;
    if (traverse_ancestors) {
      first_element = changed_element.parentElement();
      if (!first_element) {
        first_element = changed_element.ParentOrShadowHostElement();
        is_first_element_shadow_host = first_element;
      }
    }

    Element* previous_sibling =
        changed_element.GetSiblingsAffectedByHasFlags()
            ? ElementTraversal::PreviousSibling(changed_element)
            : nullptr;
    if (previous_sibling) {
      first_element = previous_sibling;
      is_first_element_shadow_host = false;
    }

    return PseudoHasInvalidationTraversalContext(
        first_element, is_first_element_shadow_host, traverse_ancestors);
  }

  // Create :has() invalidation traversal context for element or subtree
  // insertion.
  static PseudoHasInvalidationTraversalContext ForInsertion(
      Element* parent_or_shadow_host,
      bool insert_shadow_root_child,
      Element* previous_sibling) {
    Element* first_element = parent_or_shadow_host;
    bool is_first_element_shadow_host = false;
    bool traverse_ancestors = false;

    if (first_element) {
      traverse_ancestors =
          first_element->AncestorsOrAncestorSiblingsAffectedByHas();
      is_first_element_shadow_host = insert_shadow_root_child;
    }

    if (previous_sibling) {
      first_element = previous_sibling;
      is_first_element_shadow_host = false;
    }

    return PseudoHasInvalidationTraversalContext(
        first_element, is_first_element_shadow_host, traverse_ancestors);
  }

  // Create :has() invalidation traversal context for element or subtree
  // removal. In case of subtree removal, the subtree root element will be
  // passed through the 'removed_element'.
  static PseudoHasInvalidationTraversalContext ForRemoval(
      Element* parent_or_shadow_host,
      bool remove_shadow_root_child,
      Element* previous_sibling,
      Element& removed_element) {
    Element* first_element = nullptr;
    bool is_first_element_shadow_host = false;

    bool traverse_ancestors =
        removed_element.AncestorsOrAncestorSiblingsAffectedByHas();
    if (traverse_ancestors) {
      first_element = parent_or_shadow_host;
      if (first_element) {
        is_first_element_shadow_host = remove_shadow_root_child;
      }
    }

    if (!removed_element.GetSiblingsAffectedByHasFlags()) {
      previous_sibling = nullptr;
    }

    if (previous_sibling) {
      first_element = previous_sibling;
      is_first_element_shadow_host = false;
    }

    return PseudoHasInvalidationTraversalContext(
        first_element, is_first_element_shadow_host, traverse_ancestors);
  }

  // Create :has() invalidation traversal context for removing all children of
  // a parent.
  static PseudoHasInvalidationTraversalContext ForAllChildrenRemoved(
      Element& parent) {
    return PseudoHasInvalidationTraversalContext(
        &parent, /* is_first_element_shadow_host */ false,
        parent.AncestorsOrAncestorSiblingsAffectedByHas());
  }

 private:
  PseudoHasInvalidationTraversalContext(
      Element* first_element,
      bool is_first_element_shadow_host,
      bool traverse_to_parent_of_first_element)
      : first_element_(first_element),
        is_first_element_shadow_host_(is_first_element_shadow_host),
        traverse_to_parent_of_first_element_(
            traverse_to_parent_of_first_element) {}

  // The first element of the :has() invalidation traversal.
  Element* first_element_;

  bool is_first_element_shadow_host_;

  // This flag indicates whether the :has() invalidation traversal moves to the
  // parent of the first element or not.
  bool traverse_to_parent_of_first_element_;

  // This flag indicates that the :has() invalidation invalidates a element
  // only when the element has the AffectedByPseudoInHas flag set. If this flag
  // is true, the :has() invalidation skips the elements that doesn't have the
  // AffectedByPseudoInHas flag set even if the elements have the
  // AffectedBy[Subject|NonSubject]Has flag set.
  //
  // FYI. The AffectedByPseudoInHas flag indicates that the element can be
  // affected by any pseudo state change. (e.g. :hover state change by moving
  // mouse pointer) If an element doesn't have the flag set, it means the
  // element is not affected by any pseudo state change.
  bool for_element_affected_by_pseudo_in_has_{false};
};

void StyleEngine::InvalidateAncestorsOrSiblingsAffectedByHas(
    const PseudoHasInvalidationTraversalContext& traversal_context) {
  bool traverse_to_parent = traversal_context.TraverseToParentOfFirstElement();
  bool traverse_to_previous_sibling = false;
  Element* element = traversal_context.FirstElement();
  bool for_element_affected_by_pseudo_in_has =
      traversal_context.ForElementAffectedByPseudoInHas();
  Element* shadow_host = nullptr;
  if (traversal_context.IsFirstElementShadowHost()) {
    shadow_host = element;
    element = nullptr;
  }

  while (element) {
    traverse_to_parent |= element->AncestorsOrAncestorSiblingsAffectedByHas();
    traverse_to_previous_sibling = element->GetSiblingsAffectedByHasFlags();

    InvalidateElementAffectedByHas(*element,
                                   for_element_affected_by_pseudo_in_has);

    if (traverse_to_previous_sibling) {
      if (Element* previous = ElementTraversal::PreviousSibling(*element)) {
        element = previous;
        continue;
      }
    }

    if (!traverse_to_parent) {
      return;
    }

    if (Element* parent = element->parentElement()) {
      element = parent;
    } else {
      shadow_host = element->ParentOrShadowHostElement();
      element = nullptr;
    }
    traverse_to_parent = false;
  }

  if (shadow_host) {
    InvalidateElementAffectedByHas(*shadow_host,
                                   for_element_affected_by_pseudo_in_has);
  }
}

void StyleEngine::InvalidateChangedElementAffectedByLogicalCombinationsInHas(
    Element& changed_element,
    bool for_element_affected_by_pseudo_in_has) {
  if (!changed_element.AffectedByLogicalCombinationsInHas()) {
    return;
  }
  InvalidateElementAffectedByHas(changed_element,
                                 for_element_affected_by_pseudo_in_has);
}

void StyleEngine::ClassChangedForElement(
    const SpaceSplitString& changed_classes,
    Element& element) {
  if (ShouldSkipInvalidationFor(element)) {
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  if (rule_invalidation_data.NeedsHasInvalidationForClassChange() &&
      PossiblyAffectingHasState(element)) {
    for (const AtomicString& changed_class : changed_classes) {
      if (rule_invalidation_data.NeedsHasInvalidationForClass(changed_class)) {
        InvalidateChangedElementAffectedByLogicalCombinationsInHas(
            element, /* for_element_affected_by_pseudo_in_has */ false);
        InvalidateAncestorsOrSiblingsAffectedByHas(
            PseudoHasInvalidationTraversalContext::
                ForAttributeOrPseudoStateChange(element));
        break;
      }
    }
  }

  if (IsSubtreeAndSiblingsStyleDirty(element)) {
    return;
  }

  InvalidationLists invalidation_lists;
  for (const AtomicString& changed_class : changed_classes) {
    rule_invalidation_data.CollectInvalidationSetsForClass(
        invalidation_lists, element, changed_class);
  }
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         element);
}

void StyleEngine::ClassChangedForElement(const SpaceSplitString& old_classes,
                                         const SpaceSplitString& new_classes,
                                         Element& element) {
  if (ShouldSkipInvalidationFor(element)) {
    return;
  }

  if (!old_classes.size()) {
    ClassChangedForElement(new_classes, element);
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  bool needs_schedule_invalidation = !IsSubtreeAndSiblingsStyleDirty(element);
  bool possibly_affecting_has_state =
      rule_invalidation_data.NeedsHasInvalidationForClassChange() &&
      PossiblyAffectingHasState(element);
  if (!needs_schedule_invalidation && !possibly_affecting_has_state) {
    return;
  }

  // Class vectors tend to be very short. This is faster than using a hash
  // table.
  WTF::Vector<bool> remaining_class_bits(old_classes.size());

  InvalidationLists invalidation_lists;
  bool affecting_has_state = false;

  for (const AtomicString& new_class : new_classes) {
    bool found = false;
    for (unsigned i = 0; i < old_classes.size(); ++i) {
      if (new_class == old_classes[i]) {
        // Mark each class that is still in the newClasses so we can skip doing
        // an n^2 search below when looking for removals. We can't break from
        // this loop early since a class can appear more than once.
        remaining_class_bits[i] = true;
        found = true;
      }
    }
    // Class was added.
    if (!found) {
      if (needs_schedule_invalidation) [[likely]] {
        rule_invalidation_data.CollectInvalidationSetsForClass(
            invalidation_lists, element, new_class);
      }
      if (possibly_affecting_has_state) [[unlikely]] {
        if (rule_invalidation_data.NeedsHasInvalidationForClass(new_class)) {
          affecting_has_state = true;
          possibly_affecting_has_state = false;  // Clear to skip check
        }
      }
    }
  }

  for (unsigned i = 0; i < old_classes.size(); ++i) {
    if (remaining_class_bits[i]) {
      continue;
    }
    // Class was removed.
    if (needs_schedule_invalidation) [[likely]] {
      rule_invalidation_data.CollectInvalidationSetsForClass(
          invalidation_lists, element, old_classes[i]);
    }
    if (possibly_affecting_has_state) [[unlikely]] {
      if (rule_invalidation_data.NeedsHasInvalidationForClass(old_classes[i])) {
        affecting_has_state = true;
        possibly_affecting_has_state = false;  // Clear to skip check
      }
    }
  }
  if (needs_schedule_invalidation) {
    pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                           element);
  }

  if (affecting_has_state) {
    InvalidateChangedElementAffectedByLogicalCombinationsInHas(
        element, /* for_element_affected_by_pseudo_in_has */ false);
    InvalidateAncestorsOrSiblingsAffectedByHas(
        PseudoHasInvalidationTraversalContext::ForAttributeOrPseudoStateChange(
            element));
  }
}

namespace {

bool HasAttributeDependentGeneratedContent(const Element& element) {
  DCHECK(!RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled());
  if (PseudoElement* before = element.GetPseudoElement(kPseudoIdBefore)) {
    const ComputedStyle* style = before->GetComputedStyle();
    if (style && style->HasAttrFunction()) {
      return true;
    }
  }
  if (PseudoElement* after = element.GetPseudoElement(kPseudoIdAfter)) {
    const ComputedStyle* style = after->GetComputedStyle();
    if (style && style->HasAttrFunction()) {
      return true;
    }
  }
  if (PseudoElement* scroll_marker =
          element.GetPseudoElement(kPseudoIdScrollMarker)) {
    const ComputedStyle* style = scroll_marker->GetComputedStyle();
    if (style && style->HasAttrFunction()) {
      return true;
    }
  }
  return false;
}

bool HasAttributeDependentStyle(const Element& element) {
  DCHECK(RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled());
  const ComputedStyle* style = element.GetComputedStyle();
  if (style && style->HasAttrFunction()) {
    return true;
  }
  return element.PseudoElementStylesDependOnAttr();
}

}  // namespace

void StyleEngine::AttributeChangedForElement(
    const QualifiedName& attribute_name,
    Element& element) {
  if (ShouldSkipInvalidationFor(element)) {
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  if (rule_invalidation_data.NeedsHasInvalidationForAttributeChange() &&
      PossiblyAffectingHasState(element)) {
    if (rule_invalidation_data.NeedsHasInvalidationForAttribute(
            attribute_name)) {
      InvalidateChangedElementAffectedByLogicalCombinationsInHas(
          element, /* for_element_affected_by_pseudo_in_has */ false);
      InvalidateAncestorsOrSiblingsAffectedByHas(
          PseudoHasInvalidationTraversalContext::
              ForAttributeOrPseudoStateChange(element));
    }
  }

  if (IsSubtreeAndSiblingsStyleDirty(element)) {
    return;
  }

  InvalidationLists invalidation_lists;
  rule_invalidation_data.CollectInvalidationSetsForAttribute(
      invalidation_lists, element, attribute_name);
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         element);

  if (!element.NeedsStyleRecalc()) {
    bool attr_dependent =
        RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()
            ? HasAttributeDependentStyle(element)
            : HasAttributeDependentGeneratedContent(element);
    if (attr_dependent) {
      element.SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::FromAttribute(attribute_name));
    }
  }
}

void StyleEngine::IdChangedForElement(const AtomicString& old_id,
                                      const AtomicString& new_id,
                                      Element& element) {
  if (ShouldSkipInvalidationFor(element)) {
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  if (rule_invalidation_data.NeedsHasInvalidationForIdChange() &&
      PossiblyAffectingHasState(element)) {
    if ((!old_id.empty() &&
         rule_invalidation_data.NeedsHasInvalidationForId(old_id)) ||
        (!new_id.empty() &&
         rule_invalidation_data.NeedsHasInvalidationForId(new_id))) {
      InvalidateChangedElementAffectedByLogicalCombinationsInHas(
          element, /* for_element_affected_by_pseudo_in_has */ false);
      InvalidateAncestorsOrSiblingsAffectedByHas(
          PseudoHasInvalidationTraversalContext::
              ForAttributeOrPseudoStateChange(element));
    }
  }

  if (IsSubtreeAndSiblingsStyleDirty(element)) {
    return;
  }

  InvalidationLists invalidation_lists;
  if (!old_id.empty()) {
    rule_invalidation_data.CollectInvalidationSetsForId(invalidation_lists,
                                                        element, old_id);
  }
  if (!new_id.empty()) {
    rule_invalidation_data.CollectInvalidationSetsForId(invalidation_lists,
                                                        element, new_id);
  }
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         element);
}

void StyleEngine::PseudoStateChangedForElement(
    CSSSelector::PseudoType pseudo_type,
    Element& element,
    bool invalidate_descendants_or_siblings,
    bool invalidate_ancestors_or_siblings) {
  DCHECK(invalidate_descendants_or_siblings ||
         invalidate_ancestors_or_siblings);

  if (ShouldSkipInvalidationFor(element)) {
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  if (invalidate_ancestors_or_siblings &&
      rule_invalidation_data.NeedsHasInvalidationForPseudoStateChange() &&
      PossiblyAffectingHasState(element)) {
    if (rule_invalidation_data.NeedsHasInvalidationForPseudoClass(
            pseudo_type)) {
      InvalidateChangedElementAffectedByLogicalCombinationsInHas(
          element, /* for_element_affected_by_pseudo_in_has */ true);
      InvalidateAncestorsOrSiblingsAffectedByHas(
          PseudoHasInvalidationTraversalContext::
              ForAttributeOrPseudoStateChange(element)
                  .SetForElementAffectedByPseudoInHas());
    }
  }

  if (!invalidate_descendants_or_siblings ||
      IsSubtreeAndSiblingsStyleDirty(element)) {
    return;
  }

  InvalidationLists invalidation_lists;
  rule_invalidation_data.CollectInvalidationSetsForPseudoClass(
      invalidation_lists, element, pseudo_type);
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         element);
}

void StyleEngine::PartChangedForElement(Element& element) {
  if (ShouldSkipInvalidationFor(element)) {
    return;
  }
  if (IsSubtreeAndSiblingsStyleDirty(element)) {
    return;
  }
  if (element.GetTreeScope() == document_) {
    return;
  }
  if (!GetRuleFeatureSet().GetRuleInvalidationData().InvalidatesParts()) {
    return;
  }
  element.SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::FromAttribute(html_names::kPartAttr));
}

void StyleEngine::ExportpartsChangedForElement(Element& element) {
  if (ShouldSkipInvalidationFor(element)) {
    return;
  }
  if (IsSubtreeAndSiblingsStyleDirty(element)) {
    return;
  }
  if (!element.GetShadowRoot()) {
    return;
  }

  InvalidationLists invalidation_lists;
  GetRuleFeatureSet().GetRuleInvalidationData().CollectPartInvalidationSet(
      invalidation_lists);
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         element);
}

void StyleEngine::ScheduleSiblingInvalidationsForElement(
    Element& element,
    ContainerNode& scheduling_parent,
    unsigned min_direct_adjacent) {
  DCHECK(min_direct_adjacent);

  InvalidationLists invalidation_lists;

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  if (element.HasID()) {
    rule_invalidation_data.CollectSiblingInvalidationSetForId(
        invalidation_lists, element, element.IdForStyleResolution(),
        min_direct_adjacent);
  }

  if (element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    for (const AtomicString& class_name : class_names) {
      rule_invalidation_data.CollectSiblingInvalidationSetForClass(
          invalidation_lists, element, class_name, min_direct_adjacent);
    }
  }

  for (const Attribute& attribute : element.Attributes()) {
    rule_invalidation_data.CollectSiblingInvalidationSetForAttribute(
        invalidation_lists, element, attribute.GetName(), min_direct_adjacent);
  }

  rule_invalidation_data.CollectUniversalSiblingInvalidationSet(
      invalidation_lists, min_direct_adjacent);

  pending_invalidations_.ScheduleSiblingInvalidationsAsDescendants(
      invalidation_lists, scheduling_parent);
}

void StyleEngine::ScheduleInvalidationsForInsertedSibling(
    Element* before_element,
    Element& inserted_element) {
  unsigned affected_siblings =
      inserted_element.parentNode()->ChildrenAffectedByIndirectAdjacentRules()
          ? SiblingInvalidationSet::kDirectAdjacentMax
          : MaxDirectAdjacentSelectors();

  ContainerNode* scheduling_parent =
      inserted_element.ParentElementOrShadowRoot();
  if (!scheduling_parent) {
    return;
  }

  ScheduleSiblingInvalidationsForElement(inserted_element, *scheduling_parent,
                                         1);

  for (unsigned i = 1; before_element && i <= affected_siblings;
       i++, before_element =
                ElementTraversal::PreviousSibling(*before_element)) {
    ScheduleSiblingInvalidationsForElement(*before_element, *scheduling_parent,
                                           i);
  }
}

void StyleEngine::ScheduleInvalidationsForRemovedSibling(
    Element* before_element,
    Element& removed_element,
    Element& after_element) {
  unsigned affected_siblings =
      after_element.parentNode()->ChildrenAffectedByIndirectAdjacentRules()
          ? SiblingInvalidationSet::kDirectAdjacentMax
          : MaxDirectAdjacentSelectors();

  ContainerNode* scheduling_parent = after_element.ParentElementOrShadowRoot();
  if (!scheduling_parent) {
    return;
  }

  ScheduleSiblingInvalidationsForElement(removed_element, *scheduling_parent,
                                         1);

  for (unsigned i = 1; before_element && i <= affected_siblings;
       i++, before_element =
                ElementTraversal::PreviousSibling(*before_element)) {
    ScheduleSiblingInvalidationsForElement(*before_element, *scheduling_parent,
                                           i);
  }
}

void StyleEngine::ScheduleNthPseudoInvalidations(ContainerNode& nth_parent) {
  InvalidationLists invalidation_lists;
  GetRuleFeatureSet().GetRuleInvalidationData().CollectNthInvalidationSet(
      invalidation_lists);
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         nth_parent);
}

// Inserting/changing some types of rules cause invalidation even if they don't
// match, because the very act of evaluating them has side effects for the
// ComputedStyle. For instance, evaluating a rule with :hover will set the
// AffectedByHover() flag on ComputedStyle even if it matches (for
// invalidation). So we need to test for that here, and invalidate the element
// so that such rules are properly evaluated.
//
// We don't need to care specifically about @starting-style, but all other flags
// should probably be covered here.
static bool FlagsCauseInvalidation(const MatchResult& result) {
  return result.HasFlag(MatchFlag::kAffectedByDrag) ||
         result.HasFlag(MatchFlag::kAffectedByFocusWithin) ||
         result.HasFlag(MatchFlag::kAffectedByHover) ||
         result.HasFlag(MatchFlag::kAffectedByActive);
}

static bool AnyRuleCausesInvalidation(const MatchRequest& match_request,
                                      ElementRuleCollector& collector,
                                      bool is_shadow_host) {
  if (collector.CheckIfAnyRuleMatches(match_request) ||
      FlagsCauseInvalidation(collector.MatchedResult())) {
    return true;
  }
  if (is_shadow_host) {
    if (collector.CheckIfAnyShadowHostRuleMatches(match_request) ||
        FlagsCauseInvalidation(collector.MatchedResult())) {
      return true;
    }
  }
  return false;
}

namespace {

bool CanRejectRuleSet(ElementRuleCollector& collector,
                      const RuleSet& rule_set) {
  const StyleScope* scope = rule_set.SingleScope();
  return scope && collector.CanRejectScope(*scope);
}

}  // namespace

// See if a given element needs to be recalculated after RuleSet changes
// (see ApplyRuleSetInvalidation()).
void StyleEngine::ApplyRuleSetInvalidationForElement(
    const TreeScope& tree_scope,
    Element& element,
    SelectorFilter& selector_filter,
    StyleScopeFrame& style_scope_frame,
    const HeapHashSet<Member<RuleSet>>& rule_sets,
    unsigned changed_rule_flags,
    bool is_shadow_host) {
  if ((changed_rule_flags & kFunctionRules) && element.GetComputedStyle() &&
      element.GetComputedStyle()->AffectedByCSSFunction()) {
    // If @function rules have changed, and the style is (was) using a function,
    // we invalidate it unconditionally. We currently do not attempt
    // finer-grained invalidation, since it would also require tracking which
    // functions call other functions on some level.
    element.SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    style_change_reason::kFunctionRuleChange));
    return;
  }
  ElementResolveContext element_resolve_context(element);
  MatchResult match_result;
  EInsideLink inside_link =
      EInsideLink::kNotInsideLink;  // Only used for MatchedProperties, so does
                                    // not matter for us.
  StyleRecalcContext style_recalc_context =
      StyleRecalcContext::FromAncestors(element);
  style_recalc_context.style_scope_frame = &style_scope_frame;
  ElementRuleCollector collector(element_resolve_context, style_recalc_context,
                                 selector_filter, match_result, inside_link);

  MatchRequest match_request{&tree_scope.RootNode()};
  bool matched_any = false;
  for (const Member<RuleSet>& rule_set : rule_sets) {
    if (CanRejectRuleSet(collector, *rule_set)) {
      continue;
    }
    match_request.AddRuleset(rule_set.Get());
    if (match_request.IsFull()) {
      if (AnyRuleCausesInvalidation(match_request, collector, is_shadow_host)) {
        matched_any = true;
        break;
      }
      match_request.ClearAfterMatching();
    }
  }
  if (!match_request.IsEmpty() && !matched_any) {
    matched_any =
        AnyRuleCausesInvalidation(match_request, collector, is_shadow_host);
  }
  if (matched_any) {
    element.SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    style_change_reason::kStyleRuleChange));
  }
}

void StyleEngine::ScheduleCustomElementInvalidations(
    HashSet<AtomicString> tag_names) {
  scoped_refptr<DescendantInvalidationSet> invalidation_set =
      DescendantInvalidationSet::Create();
  for (auto& tag_name : tag_names) {
    invalidation_set->AddTagName(tag_name);
  }
  invalidation_set->SetTreeBoundaryCrossing();
  InvalidationLists invalidation_lists;
  invalidation_lists.descendants.push_back(invalidation_set);
  pending_invalidations_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                         *document_);
}

void StyleEngine::ScheduleInvalidationsForHasPseudoAffectedByInsertionOrRemoval(
    ContainerNode* parent,
    Node* node_before_change,
    Element& changed_element,
    bool removal) {
  Element* parent_or_shadow_host = nullptr;
  bool insert_or_remove_shadow_root_child = false;
  if (Element* element = DynamicTo<Element>(parent)) {
    parent_or_shadow_host = element;
  } else if (ShadowRoot* shadow_root = DynamicTo<ShadowRoot>(parent)) {
    parent_or_shadow_host = &shadow_root->host();
    insert_or_remove_shadow_root_child = true;
  }

  if (!parent_or_shadow_host) {
    return;
  }

  if (ShouldSkipInvalidationFor(*parent_or_shadow_host)) {
    return;
  }

  if (!GetRuleFeatureSet()
           .GetRuleInvalidationData()
           .NeedsHasInvalidationForInsertionOrRemoval()) {
    return;
  }

  Element* previous_sibling = SelfOrPreviousSibling(node_before_change);

  if (removal) {
    ScheduleInvalidationsForHasPseudoAffectedByRemoval(
        parent_or_shadow_host, previous_sibling, changed_element,
        insert_or_remove_shadow_root_child);
  } else {
    ScheduleInvalidationsForHasPseudoAffectedByInsertion(
        parent_or_shadow_host, previous_sibling, changed_element,
        insert_or_remove_shadow_root_child);
  }
}

void StyleEngine::ScheduleInvalidationsForHasPseudoAffectedByInsertion(
    Element* parent_or_shadow_host,
    Element* previous_sibling,
    Element& inserted_element,
    bool insert_shadow_root_child) {
  bool possibly_affecting_has_state = false;
  bool descendants_possibly_affecting_has_state = false;

  if (InsertionOrRemovalPossiblyAffectHasStateOfPreviousSiblings(
          previous_sibling)) {
    inserted_element.SetSiblingsAffectedByHasFlags(
        previous_sibling->GetSiblingsAffectedByHasFlags());
    possibly_affecting_has_state = true;
    descendants_possibly_affecting_has_state =
        inserted_element.HasSiblingsAffectedByHasFlags(
            SiblingsAffectedByHasFlags::kFlagForSiblingDescendantRelationship);
  }
  if (InsertionOrRemovalPossiblyAffectHasStateOfAncestorsOrAncestorSiblings(
          parent_or_shadow_host)) {
    inserted_element.SetAncestorsOrAncestorSiblingsAffectedByHas();
    possibly_affecting_has_state = true;
    descendants_possibly_affecting_has_state = true;
  }

  if (!possibly_affecting_has_state) {
    return;  // Inserted subtree will not affect :has() state
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  // Always schedule :has() invalidation if the inserted element may affect
  // a match result of a compound after direct adjacent combinator by changing
  // sibling order. (e.g. When we have a style rule '.a:has(+ .b) {}', we always
  // need :has() invalidation if any element is inserted before '.b')
  bool needs_has_invalidation_for_inserted_subtree =
      parent_or_shadow_host->ChildrenAffectedByDirectAdjacentRules();

  if (!needs_has_invalidation_for_inserted_subtree &&
      rule_invalidation_data.NeedsHasInvalidationForInsertedOrRemovedElement(
          inserted_element)) {
    needs_has_invalidation_for_inserted_subtree = true;
  }

  if (descendants_possibly_affecting_has_state) {
    // Do not stop subtree traversal early so that all the descendants have the
    // AncestorsOrAncestorSiblingsAffectedByHas flag set.
    for (Element& element : ElementTraversal::DescendantsOf(inserted_element)) {
      element.SetAncestorsOrAncestorSiblingsAffectedByHas();
      if (!needs_has_invalidation_for_inserted_subtree &&
          rule_invalidation_data
              .NeedsHasInvalidationForInsertedOrRemovedElement(element)) {
        needs_has_invalidation_for_inserted_subtree = true;
      }
    }
  }

  if (needs_has_invalidation_for_inserted_subtree) {
    InvalidateAncestorsOrSiblingsAffectedByHas(
        PseudoHasInvalidationTraversalContext::ForInsertion(
            parent_or_shadow_host, insert_shadow_root_child, previous_sibling));
    return;
  }

  if (rule_invalidation_data.NeedsHasInvalidationForPseudoStateChange()) {
    InvalidateAncestorsOrSiblingsAffectedByHas(
        PseudoHasInvalidationTraversalContext::ForInsertion(
            parent_or_shadow_host, insert_shadow_root_child, previous_sibling)
            .SetForElementAffectedByPseudoInHas());
  }
}

void StyleEngine::ScheduleInvalidationsForHasPseudoAffectedByRemoval(
    Element* parent_or_shadow_host,
    Element* previous_sibling,
    Element& removed_element,
    bool remove_shadow_root_child) {
  if (!InsertionOrRemovalPossiblyAffectHasStateOfAncestorsOrAncestorSiblings(
          parent_or_shadow_host) &&
      !InsertionOrRemovalPossiblyAffectHasStateOfPreviousSiblings(
          previous_sibling)) {
    // Removed element will not affect :has() state
    return;
  }

  // Always schedule :has() invalidation if the removed element may affect
  // a match result of a compound after direct adjacent combinator by changing
  // sibling order. (e.g. When we have a style rule '.a:has(+ .b) {}', we always
  // need :has() invalidation if the preceding element of '.b' is removed)
  if (parent_or_shadow_host->ChildrenAffectedByDirectAdjacentRules()) {
    InvalidateAncestorsOrSiblingsAffectedByHas(
        PseudoHasInvalidationTraversalContext::ForRemoval(
            parent_or_shadow_host, remove_shadow_root_child, previous_sibling,
            removed_element));
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();

  for (Element& element :
       ElementTraversal::InclusiveDescendantsOf(removed_element)) {
    if (rule_invalidation_data.NeedsHasInvalidationForInsertedOrRemovedElement(
            element)) {
      InvalidateAncestorsOrSiblingsAffectedByHas(
          PseudoHasInvalidationTraversalContext::ForRemoval(
              parent_or_shadow_host, remove_shadow_root_child, previous_sibling,
              removed_element));
      return;
    }
  }

  if (rule_invalidation_data.NeedsHasInvalidationForPseudoStateChange()) {
    InvalidateAncestorsOrSiblingsAffectedByHas(
        PseudoHasInvalidationTraversalContext::ForRemoval(
            parent_or_shadow_host, remove_shadow_root_child, previous_sibling,
            removed_element)
            .SetForElementAffectedByPseudoInHas());
  }
}

void StyleEngine::ScheduleInvalidationsForHasPseudoWhenAllChildrenRemoved(
    Element& parent) {
  if (ShouldSkipInvalidationFor(parent)) {
    return;
  }

  const RuleInvalidationData& rule_invalidation_data =
      GetRuleFeatureSet().GetRuleInvalidationData();
  if (!rule_invalidation_data.NeedsHasInvalidationForInsertionOrRemoval()) {
    return;
  }

  if (!InsertionOrRemovalPossiblyAffectHasStateOfAncestorsOrAncestorSiblings(
          &parent)) {
    // Removed children will not affect :has() state
    return;
  }

  // Always invalidate elements possibly affected by the removed children.
  InvalidateAncestorsOrSiblingsAffectedByHas(
      PseudoHasInvalidationTraversalContext::ForAllChildrenRemoved(parent));
}

void StyleEngine::InvalidateStyle() {
  StyleInvalidator style_invalidator(
      pending_invalidations_.GetPendingInvalidationMap());
  style_invalidator.Invalidate(GetDocument(),
                               style_invalidation_root_.RootElement());
  style_invalidation_root_.Clear();
}

void StyleEngine::InvalidateSlottedElements(
    HTMLSlotElement& slot,
    const StyleChangeReasonForTracing& reason) {
  for (auto& node : slot.FlattenedAssignedNodes()) {
    if (node->IsElementNode()) {
      node->SetNeedsStyleRecalc(kLocalStyleChange, reason);
    }
  }
}

bool StyleEngine::HasViewportDependentPropertyRegistrations() {
  UpdateActiveStyle();
  const PropertyRegistry* registry = GetDocument().GetPropertyRegistry();
  return registry && registry->GetViewportUnitFlags();
}

// Given a list of RuleSets that have changed (both old and new), see what
// elements in the given TreeScope that could be affected by them and need
// style recalculation.
//
// This generally works by our regular selector matching; if any selector
// in any of the given RuleSets match, it means we need to mark the element
// for style recalc. This could either be because the element is affected
// by a rule where it wasn't before, or because the element used to be
// affected by some rule and isn't anymore, or even that the rule itself
// changed. (It could also be a false positive, e.g. because someone added
// a single new rule to a style sheet, causing a new RuleSet to be created
// that also contains all the old rules, and the element matches one of them.)
//
// There are some twists to this; e.g., for a rule like a:hover, we will need
// to invalidate all <a> elements whether they are currently matching :hover
// or not (see FlagsCauseInvalidation()).
//
// In general, we check all elements in this TreeScope and nothing else.
// There are some exceptions (in both directions); in particular, if an element
// is already marked for subtree recalc, we don't need to go below it. Also,
// if invalidation_scope says so, or if we have rules pertaining to UA shadows,
// we may need to descend into child TreeScopes.
void StyleEngine::ApplyRuleSetInvalidationForTreeScope(
    TreeScope& tree_scope,
    ContainerNode& node,
    SelectorFilter& selector_filter,
    StyleScopeFrame& style_scope_frame,
    const HeapHashSet<Member<RuleSet>>& rule_sets,
    unsigned changed_rule_flags,
    InvalidationScope invalidation_scope) {
  TRACE_EVENT0("blink,blink_style",
               "StyleEngine::scheduleInvalidationsForRuleSets");

  bool invalidate_slotted = false;
  bool invalidate_part = false;
  if (auto* shadow_root = DynamicTo<ShadowRoot>(&node)) {
    Element& host = shadow_root->host();
    // The SelectorFilter stack is set up for invalidating the tree
    // under the host, which includes the host. When invalidating the
    // host itself, we need to take it out so that the stack is consistent.
    selector_filter.PopParent(host);
    ApplyRuleSetInvalidationForElement(tree_scope, host, selector_filter,
                                       style_scope_frame, rule_sets,
                                       changed_rule_flags,
                                       /*is_shadow_host=*/true);
    selector_filter.PushParent(host);
    if (host.GetStyleChangeType() == kSubtreeStyleChange) {
      return;
    }
    for (auto rule_set : rule_sets) {
      if (rule_set->HasSlottedRules()) {
        invalidate_slotted = true;
        break;
      }
      if (rule_set->HasPartPseudoRules()) {
        invalidate_part = true;
        break;
      }
    }
  }

  // If there are any rules that cover UA pseudos, we need to descend into
  // UA shadows so that we can invalidate them. This is pretty crude
  // (it descends into all shadows), but such rules are fairly rare anyway.
  //
  // We do a similar thing for :part(), descending into all shadows.
  if (invalidation_scope != kInvalidateAllScopes) {
    for (auto rule_set : rule_sets) {
      if (rule_set->HasUAShadowPseudoElementRules() ||
          rule_set->HasPartPseudoRules()) {
        invalidation_scope = kInvalidateAllScopes;
        break;
      }
    }
  }

  // Note that there is no need to meddle with the SelectorFilter
  // or StyleScopeFrame here: the caller should already have set up
  // the required state for `node` in both cases.
  for (Element& child : ElementTraversal::ChildrenOf(node)) {
    ApplyRuleSetInvalidationForSubtree(
        tree_scope, child, selector_filter,
        /* parent_style_scope_frame */ style_scope_frame, rule_sets,
        changed_rule_flags, invalidation_scope, invalidate_slotted,
        invalidate_part);
  }
}

void StyleEngine::ApplyRuleSetInvalidationForSubtree(
    TreeScope& tree_scope,
    Element& element,
    SelectorFilter& selector_filter,
    StyleScopeFrame& parent_style_scope_frame,
    const HeapHashSet<Member<RuleSet>>& rule_sets,
    unsigned changed_rule_flags,
    InvalidationScope invalidation_scope,
    bool invalidate_slotted,
    bool invalidate_part) {
  StyleScopeFrame style_scope_frame(element, &parent_style_scope_frame);

  if (invalidate_part && element.hasAttribute(html_names::kPartAttr)) {
    // It's too complicated to try to handle ::part() precisely.
    // If we have any ::part() rules, and the element has a [part]
    // attribute, just invalidate it.
    element.SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    style_change_reason::kStyleRuleChange));
  } else {
    ApplyRuleSetInvalidationForElement(tree_scope, element, selector_filter,
                                       style_scope_frame, rule_sets,
                                       changed_rule_flags,
                                       /*is_shadow_host=*/false);
  }

  auto* html_slot_element = DynamicTo<HTMLSlotElement>(element);
  if (html_slot_element && invalidate_slotted) {
    InvalidateSlottedElements(*html_slot_element,
                              StyleChangeReasonForTracing::Create(
                                  style_change_reason::kStyleRuleChange));
  }

  if (invalidation_scope == kInvalidateAllScopes) {
    if (ShadowRoot* shadow_root = element.GetShadowRoot()) {
      selector_filter.PushParent(element);
      ApplyRuleSetInvalidationForTreeScope(tree_scope, shadow_root->RootNode(),
                                           selector_filter, style_scope_frame,
                                           rule_sets, kInvalidateAllScopes);
      selector_filter.PopParent(element);
    }
  }

  // Skip traversal of the subtree if we're going to update the entire subtree
  // anyway.
  const bool traverse_children =
      (element.GetStyleChangeType() < kSubtreeStyleChange &&
       element.GetComputedStyle());

  if (traverse_children) {
    selector_filter.PushParent(element);

    for (Element& child : ElementTraversal::ChildrenOf(element)) {
      ApplyRuleSetInvalidationForSubtree(
          tree_scope, child, selector_filter,
          /* parent_style_scope_frame */ style_scope_frame, rule_sets,
          changed_rule_flags, invalidation_scope, invalidate_slotted,
          invalidate_part);
    }

    selector_filter.PopParent(element);
  }
}

void StyleEngine::SetStatsEnabled(bool enabled) {
  if (!enabled) {
    style_resolver_stats_ = nullptr;
    return;
  }
  if (!style_resolver_stats_) {
    style_resolver_stats_ = std::make_unique<StyleResolverStats>();
  } else {
    style_resolver_stats_->Reset();
  }
}

void StyleEngine::SetPreferredStylesheetSetNameIfNotSet(const String& name) {
  DCHECK(!name.empty());
  if (!preferred_stylesheet_set_name_.empty()) {
    return;
  }
  preferred_stylesheet_set_name_ = name;
  MarkDocumentDirty();
}

void StyleEngine::SetHttpDefaultStyle(const String& content) {
  if (!content.empty()) {
    SetPreferredStylesheetSetNameIfNotSet(content);
  }
}

void StyleEngine::CollectFeaturesTo(RuleFeatureSet& features) {
  CollectUserStyleFeaturesTo(features);
  CollectScopedStyleFeaturesTo(features);
}

void StyleEngine::EnsureUAStyleForFullscreen(const Element& element) {
  DCHECK(global_rule_set_);
  if (global_rule_set_->HasFullscreenUAStyle()) {
    return;
  }
  CSSDefaultStyleSheets::Instance().EnsureDefaultStyleSheetForFullscreen(
      element);
  global_rule_set_->MarkDirty();
  UpdateActiveStyle();
}

void StyleEngine::EnsureUAStyleForElement(const Element& element) {
  DCHECK(global_rule_set_);
  if (CSSDefaultStyleSheets::Instance().EnsureDefaultStyleSheetsForElement(
          element)) {
    global_rule_set_->MarkDirty();
    UpdateActiveStyle();
  }
}

void StyleEngine::EnsureUAStyleForPseudoElement(PseudoId pseudo_id) {
  DCHECK(global_rule_set_);

  if (CSSDefaultStyleSheets::Instance()
          .EnsureDefaultStyleSheetsForPseudoElement(pseudo_id)) {
    global_rule_set_->MarkDirty();
    UpdateActiveStyle();
  }
}

void StyleEngine::EnsureUAStyleForForcedColors() {
  DCHECK(global_rule_set_);
  if (CSSDefaultStyleSheets::Instance()
          .EnsureDefaultStyleSheetForForcedColors()) {
    global_rule_set_->MarkDirty();
    if (GetDocument().IsActive()) {
      UpdateActiveStyle();
    }
  }
}

RuleSet* StyleEngine::DefaultViewTransitionStyle() const {
  auto* transition = ViewTransitionUtils::GetTransition(GetDocument());
  if (!transition) {
    return nullptr;
  }

  auto* css_style_sheet = transition->UAStyleSheet();
  return &css_style_sheet->Contents()->EnsureRuleSet(
      CSSDefaultStyleSheets::ScreenEval());
}

void StyleEngine::UpdateViewTransitionOptIn() {
  bool cross_document_enabled = false;

  // TODO(https://crbug.com/1463966): This will likely need to change to a
  // CSSValueList if we want to support multiple tokens as a trigger.
  Vector<String> types;
  if (view_transition_rule_) {
    types = view_transition_rule_->GetTypes();
    if (const CSSValue* value = view_transition_rule_->GetNavigation()) {
      cross_document_enabled =
          To<CSSIdentifierValue>(value)->GetValueID() == CSSValueID::kAuto;
    }
  }

  ViewTransitionSupplement::From(GetDocument())
      ->OnViewTransitionsStyleUpdated(cross_document_enabled, types);
}

bool StyleEngine::HasRulesForId(const AtomicString& id) const {
  DCHECK(global_rule_set_);
  return global_rule_set_->GetRuleFeatureSet()
      .GetRuleInvalidationData()
      .HasSelectorForId(id);
}

void StyleEngine::InitialStyleChanged() {
  MarkViewportStyleDirty();
  // We need to update the viewport style immediately because media queries
  // evaluated in MediaQueryAffectingValueChanged() below may rely on the
  // initial font size relative lengths which may have changed.
  UpdateViewportStyle();
  MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kSettings));
}

void StyleEngine::ViewportStyleSettingChanged() {
  if (viewport_resolver_) {
    viewport_resolver_->SetNeedsUpdate();
  }

  // When we remove an import link and re-insert it into the document, the
  // import Document and CSSStyleSheet pointers are persisted. That means the
  // comparison of active stylesheets is not able to figure out that the order
  // of the stylesheets have changed after insertion.
  //
  // This is also the case when we import the same document twice where the
  // last inserted document is inserted before the first one in dom order where
  // the last would take precedence.
  //
  // Fall back to re-add all sheets to the scoped resolver and recalculate style
  // for the whole document when we remove or insert an import document.
  if (ScopedStyleResolver* resolver = GetDocument().GetScopedStyleResolver()) {
    MarkDocumentDirty();
    resolver->SetNeedsAppendAllSheets();
    MarkAllElementsForStyleRecalc(StyleChangeReasonForTracing::Create(
        style_change_reason::kActiveStylesheetsUpdate));
  }
}

void StyleEngine::InvalidateForRuleSetChanges(
    TreeScope& tree_scope,
    const HeapHashSet<Member<RuleSet>>& changed_rule_sets,
    unsigned changed_rule_flags,
    InvalidationScope invalidation_scope) {
  if (tree_scope.GetDocument().HasPendingForcedStyleRecalc()) {
    return;
  }
  if (!tree_scope.GetDocument().documentElement()) {
    return;
  }
  if (changed_rule_sets.empty()) {
    return;
  }

  Element& invalidation_root =
      ScopedStyleResolver::InvalidationRootForTreeScope(tree_scope);
  if (invalidation_root.GetStyleChangeType() == kSubtreeStyleChange) {
    return;
  }

  SelectorFilter selector_filter;
  selector_filter.PushAllParentsOf(tree_scope);

  // Note that unlike the SelectorFilter, there is no need to explicitly
  // handle the ancestor chain. It's OK to have a "root" StyleScopeFrame
  // (i.e. a StyleScopeFrame without a parent frame) in the middle of the
  // tree.
  //
  // Note also in the below call to ApplyRuleSetInvalidationForTreeScope,
  // when `tree_scope` is a ShadowRoot, we have special behavior inside
  // which invalidates "up" to the shadow *host*. This is why we use the
  // host (if applicable) as the StyleScopeFrame element here.
  StyleScopeFrame style_scope_frame(
      IsA<ShadowRoot>(tree_scope)
          ? To<ShadowRoot>(tree_scope).host()
          : *tree_scope.GetDocument().documentElement());

  NthIndexCache nth_index_cache(tree_scope.GetDocument());
  ApplyRuleSetInvalidationForTreeScope(
      tree_scope, tree_scope.RootNode(), selector_filter, style_scope_frame,
      changed_rule_sets, changed_rule_flags, invalidation_scope);
}

void StyleEngine::InvalidateInitialData() {
  initial_data_ = nullptr;
}

// A miniature CascadeMap for cascading @property at-rules according to their
// origin, cascade layer order and position.
class StyleEngine::AtRuleCascadeMap {
  STACK_ALLOCATED();

 public:
  explicit AtRuleCascadeMap(Document& document) : document_(document) {}

  // No need to use the full CascadePriority class, since we are not handling UA
  // style, shadow DOM or importance, and rules are inserted in source ordering.
  struct Priority {
    DISALLOW_NEW();
    bool is_user_style;
    uint16_t layer_order;

    bool operator<(const Priority& other) const {
      if (is_user_style != other.is_user_style) {
        return is_user_style;
      }
      return layer_order < other.layer_order;
    }
  };

  Priority GetPriority(bool is_user_style, const CascadeLayer* layer) {
    return Priority{is_user_style, GetLayerOrder(is_user_style, layer)};
  }

  // Returns true if this is the first rule with the name, or if this has a
  // higher priority than all the previously added rules with the same name.
  bool AddAndCascade(const AtomicString& name, Priority priority) {
    auto add_result = map_.insert(name, priority);
    if (add_result.is_new_entry) {
      return true;
    }
    if (priority < add_result.stored_value->value) {
      return false;
    }
    add_result.stored_value->value = priority;
    return true;
  }

 private:
  uint16_t GetLayerOrder(bool is_user_style, const CascadeLayer* layer) {
    if (!layer) {
      return CascadeLayerMap::kImplicitOuterLayerOrder;
    }
    const CascadeLayerMap* layer_map = nullptr;
    if (is_user_style) {
      layer_map = document_.GetStyleEngine().GetUserCascadeLayerMap();
    } else if (document_.GetScopedStyleResolver()) {
      layer_map = document_.GetScopedStyleResolver()->GetCascadeLayerMap();
    }
    if (!layer_map) {
      return CascadeLayerMap::kImplicitOuterLayerOrder;
    }
    return layer_map->GetLayerOrder(*layer);
  }

  Document& document_;
  HashMap<AtomicString, Priority> map_;
};

void StyleEngine::ApplyUserRuleSetChanges(
    const ActiveStyleSheetVector& old_style_sheets,
    const ActiveStyleSheetVector& new_style_sheets) {
  DCHECK(global_rule_set_);
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  ActiveSheetsChange change = CompareActiveStyleSheets(
      old_style_sheets, new_style_sheets, /*diffs=*/{}, changed_rule_sets);

  if (change == kNoActiveSheetsChanged) {
    return;
  }

  // With rules added or removed, we need to re-aggregate rule meta data.
  global_rule_set_->MarkDirty();

  unsigned changed_rule_flags = GetRuleSetFlags(changed_rule_sets);

  // Cascade layer map must be built before adding other at-rules, because other
  // at-rules rely on layer order to resolve name conflicts.
  if (changed_rule_flags & kLayerRules) {
    // Rebuild cascade layer map in all cases, because a newly inserted
    // sub-layer can precede an original layer in the final ordering.
    user_cascade_layer_map_ =
        MakeGarbageCollected<CascadeLayerMap>(new_style_sheets);

    if (resolver_) {
      resolver_->InvalidateMatchedPropertiesCache();
    }

    // When we have layer changes other than appended, existing layer ordering
    // may be changed, which requires rebuilding all at-rule registries and
    // full document style recalc.
    if (change == kActiveSheetsChanged) {
      changed_rule_flags = kRuleSetFlagsAll;
    }
  }

  if (changed_rule_flags & kFontFaceRules) {
    if (ScopedStyleResolver* scoped_resolver =
            GetDocument().GetScopedStyleResolver()) {
      // User style and document scope author style shares the font cache. If
      // @font-face rules are added/removed from user stylesheets, we need to
      // reconstruct the font cache because @font-face rules from author style
      // need to be added to the cache after user rules.
      scoped_resolver->SetNeedsAppendAllSheets();
      MarkDocumentDirty();
    } else {
      bool has_rebuilt_font_face_cache =
          ClearFontFaceCacheAndAddUserFonts(new_style_sheets);
      if (has_rebuilt_font_face_cache) {
        GetFontSelector()->FontFaceInvalidated(
            FontInvalidationReason::kGeneralInvalidation);
      }
    }
  }

  if (changed_rule_flags & kKeyframesRules) {
    if (change == kActiveSheetsChanged) {
      ClearKeyframeRules();
    }

    for (const auto& sheet : new_style_sheets) {
      DCHECK(sheet.second);
      AddUserKeyframeRules(*sheet.second);
    }
    ScopedStyleResolver::KeyframesRulesAdded(GetDocument());
  }

  if (changed_rule_flags & kCounterStyleRules) {
    if (change == kActiveSheetsChanged && user_counter_style_map_) {
      user_counter_style_map_->Dispose();
    }

    for (const auto& sheet : new_style_sheets) {
      DCHECK(sheet.second);
      if (!sheet.second->CounterStyleRules().empty()) {
        EnsureUserCounterStyleMap().AddCounterStyles(*sheet.second);
      }
    }

    MarkCounterStylesNeedUpdate();
  }

  if (changed_rule_flags &
      (kPropertyRules | kFontPaletteValuesRules | kFontFeatureValuesRules)) {
    if (changed_rule_flags & kPropertyRules) {
      ClearPropertyRules();
      AtRuleCascadeMap cascade_map(GetDocument());
      AddPropertyRulesFromSheets(cascade_map, new_style_sheets,
                                 true /* is_user_style */);
    }

    if (changed_rule_flags & kFontPaletteValuesRules) {
      font_palette_values_rule_map_.clear();
      AddFontPaletteValuesRulesFromSheets(new_style_sheets);
      MarkFontsNeedUpdate();
    }

    // TODO(https://crbug.com/1402199): kFontFeatureValuesRules changes not
    // handled in user sheets.

    // We just cleared all the rules, which includes any author rules. They
    // must be forcibly re-added.
    if (ScopedStyleResolver* scoped_resolver =
            GetDocument().GetScopedStyleResolver()) {
      scoped_resolver->SetNeedsAppendAllSheets();
      MarkDocumentDirty();
    }
  }

  if (changed_rule_flags & kPositionTryRules) {
    // TODO(crbug.com/1383907): @position-try rules are not yet collected from
    // user stylesheets.
    MarkPositionTryStylesDirty(changed_rule_sets);
  }

  InvalidateForRuleSetChanges(GetDocument(), changed_rule_sets,
                              changed_rule_flags, kInvalidateAllScopes);
}

void StyleEngine::ApplyRuleSetChanges(
    TreeScope& tree_scope,
    const ActiveStyleSheetVector& old_style_sheets,
    const ActiveStyleSheetVector& new_style_sheets,
    const HeapVector<Member<RuleSetDiff>>& diffs) {
  DCHECK(global_rule_set_);
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  ActiveSheetsChange change = CompareActiveStyleSheets(
      old_style_sheets, new_style_sheets, diffs, changed_rule_sets);

  unsigned changed_rule_flags = GetRuleSetFlags(changed_rule_sets);

  bool rebuild_font_face_cache = change == kActiveSheetsChanged &&
                                 (changed_rule_flags & kFontFaceRules) &&
                                 tree_scope.RootNode().IsDocumentNode();
  bool rebuild_at_property_registry = false;
  bool rebuild_at_font_palette_values_map = false;
  ScopedStyleResolver* scoped_resolver = tree_scope.GetScopedStyleResolver();
  if (scoped_resolver && scoped_resolver->NeedsAppendAllSheets()) {
    rebuild_font_face_cache = true;
    rebuild_at_property_registry = true;
    rebuild_at_font_palette_values_map = true;
    change = kActiveSheetsChanged;
  }

  if (change == kNoActiveSheetsChanged) {
    return;
  }

  // With rules added or removed, we need to re-aggregate rule meta data.
  global_rule_set_->MarkDirty();

  if (changed_rule_flags & kKeyframesRules) {
    ScopedStyleResolver::KeyframesRulesAdded(tree_scope);
  }

  if (changed_rule_flags & kCounterStyleRules) {
    MarkCounterStylesNeedUpdate();
  }

  unsigned append_start_index = 0;
  bool rebuild_cascade_layer_map = changed_rule_flags & kLayerRules;
  if (scoped_resolver) {
    // - If all sheets were removed, we remove the ScopedStyleResolver
    // - If new sheets were appended to existing ones, start appending after the
    //   common prefix, and rebuild CascadeLayerMap only if layers are changed.
    // - For other diffs, reset author style and re-add all sheets for the
    //   TreeScope. If new sheets need a CascadeLayerMap, rebuild it.
    if (new_style_sheets.empty()) {
      rebuild_cascade_layer_map = false;
      ResetAuthorStyle(tree_scope);
    } else if (change == kActiveSheetsAppended) {
      append_start_index = old_style_sheets.size();
    } else {
      rebuild_cascade_layer_map = (changed_rule_flags & kLayerRules) ||
                                  scoped_resolver->HasCascadeLayerMap();
      scoped_resolver->ResetStyle();
    }
  }

  if (rebuild_cascade_layer_map) {
    tree_scope.EnsureScopedStyleResolver().RebuildCascadeLayerMap(
        new_style_sheets);
  }

  if (changed_rule_flags & kLayerRules) {
    if (resolver_) {
      resolver_->InvalidateMatchedPropertiesCache();
    }

    // When we have layer changes other than appended, existing layer ordering
    // may be changed, which requires rebuilding all at-rule registries and
    // full document style recalc.
    if (change == kActiveSheetsChanged) {
      changed_rule_flags = kRuleSetFlagsAll;
      if (tree_scope.RootNode().IsDocumentNode()) {
        rebuild_font_face_cache = true;
      }
    }
  }

  if ((changed_rule_flags & kPropertyRules) || rebuild_at_property_registry) {
    // @property rules are (for now) ignored in shadow trees, per spec.
    // https://drafts.css-houdini.org/css-properties-values-api-1/#at-property-rule
    if (tree_scope.RootNode().IsDocumentNode()) {
      ClearPropertyRules();
      AtRuleCascadeMap cascade_map(GetDocument());
      AddPropertyRulesFromSheets(cascade_map, active_user_style_sheets_,
                                 true /* is_user_style */);
      AddPropertyRulesFromSheets(cascade_map, new_style_sheets,
                                 false /* is_user_style */);
    }
  }

  if ((changed_rule_flags & kFontPaletteValuesRules) ||
      rebuild_at_font_palette_values_map) {
    // TODO(crbug.com/1296114): Support @font-palette-values in shadow trees and
    // support scoping correctly.
    if (tree_scope.RootNode().IsDocumentNode()) {
      font_palette_values_rule_map_.clear();
      AddFontPaletteValuesRulesFromSheets(active_user_style_sheets_);
      AddFontPaletteValuesRulesFromSheets(new_style_sheets);
    }
  }

  // The kFontFeatureValuesRules case is handled in
  // tree_scope.EnsureScopedStyleResolver().AppendActiveStyleSheets below.

  if (tree_scope.RootNode().IsDocumentNode()) {
    bool has_rebuilt_font_face_cache = false;
    if (rebuild_font_face_cache) {
      has_rebuilt_font_face_cache =
          ClearFontFaceCacheAndAddUserFonts(active_user_style_sheets_);
    }
    if ((changed_rule_flags & kFontFaceRules) ||
        (changed_rule_flags & kFontPaletteValuesRules) ||
        (changed_rule_flags & kFontFeatureValuesRules) ||
        has_rebuilt_font_face_cache) {
      GetFontSelector()->FontFaceInvalidated(
          FontInvalidationReason::kGeneralInvalidation);
    }
  }

  if (changed_rule_flags & kPositionTryRules) {
    MarkPositionTryStylesDirty(changed_rule_sets);
  }

  if (changed_rule_flags & kViewTransitionRules) {
    // Since a shadow-tree isn't an independent navigable, @view-transition
    // doesn't apply within one.
    if (tree_scope.RootNode().IsDocumentNode()) {
      AddViewTransitionRules(new_style_sheets);
    }
  }

  if (changed_rule_flags & kFunctionRules) {
    // Changes in function can affect function-using declarations
    // in arbitrary ways.
    if (resolver_) {
      resolver_->InvalidateMatchedPropertiesCache();
    }
  }

  if (!new_style_sheets.empty()) {
    tree_scope.EnsureScopedStyleResolver().AppendActiveStyleSheets(
        append_start_index, new_style_sheets);
  }

  InvalidateForRuleSetChanges(tree_scope, changed_rule_sets, changed_rule_flags,
                              kInvalidateCurrentScope);
}

void StyleEngine::LoadVisionDeficiencyFilter() {
  VisionDeficiency old_vision_deficiency = vision_deficiency_;
  vision_deficiency_ = GetDocument().GetPage()->GetVisionDeficiency();
  if (vision_deficiency_ == old_vision_deficiency) {
    return;
  }

  if (vision_deficiency_ == VisionDeficiency::kNoVisionDeficiency) {
    vision_deficiency_filter_ = nullptr;
  } else {
    AtomicString url = CreateVisionDeficiencyFilterUrl(vision_deficiency_);
    cssvalue::CSSURIValue* css_uri_value =
        MakeGarbageCollected<cssvalue::CSSURIValue>(CSSUrlData(url));
    SVGResource* svg_resource = css_uri_value->EnsureResourceReference();
    // Note: The fact that we're using data: URLs here is an
    // implementation detail. Emulating vision deficiencies should still
    // work even if the Document's Content-Security-Policy disallows
    // data: URLs.
    svg_resource->LoadWithoutCSP(GetDocument());
    vision_deficiency_filter_ =
        MakeGarbageCollected<ReferenceFilterOperation>(url, svg_resource);
  }
}

void StyleEngine::VisionDeficiencyChanged() {
  MarkViewportStyleDirty();
}

void StyleEngine::ApplyVisionDeficiencyStyle(
    ComputedStyleBuilder& layout_view_style_builder) {
  LoadVisionDeficiencyFilter();
  if (vision_deficiency_filter_) {
    FilterOperations ops;
    ops.Operations().push_back(vision_deficiency_filter_);
    layout_view_style_builder.SetFilter(ops);
  }
}

const MediaQueryEvaluator& StyleEngine::EnsureMediaQueryEvaluator() {
  if (!media_query_evaluator_) {
    if (GetDocument().GetFrame()) {
      media_query_evaluator_ =
          MakeGarbageCollected<MediaQueryEvaluator>(GetDocument().GetFrame());
    } else {
      media_query_evaluator_ = MakeGarbageCollected<MediaQueryEvaluator>("all");
    }
  }
  return *media_query_evaluator_;
}

bool StyleEngine::StyleMaybeAffectedByLayout(const Node& node) {
  // Note that the StyleAffectedByLayout flag is set based on which
  // ComputedStyles we've resolved previously. Since style resolution may never
  // reach elements in display:none, we defensively treat any null-or-ensured
  // ComputedStyle as affected by layout.
  return StyleAffectedByLayout() ||
         ComputedStyle::IsNullOrEnsured(node.GetComputedStyle());
}

bool StyleEngine::UpdateRootFontRelativeUnits(
    const ComputedStyle* old_root_style,
    const ComputedStyle* new_root_style) {
  if (!new_root_style || !UsesRootFontRelativeUnits()) {
    return false;
  }
  bool rem_changed = !old_root_style || old_root_style->SpecifiedFontSize() !=
                                            new_root_style->SpecifiedFontSize();
  bool root_font_glyphs_changed =
      !old_root_style ||
      (UsesGlyphRelativeUnits() &&
       old_root_style->GetFont() != new_root_style->GetFont());
  bool root_line_height_changed =
      !old_root_style ||
      (UsesLineHeightUnits() &&
       old_root_style->LineHeight() != new_root_style->LineHeight());
  bool root_font_changed =
      rem_changed || root_font_glyphs_changed || root_line_height_changed;
  if (root_font_changed) {
    // Resolved root font relative units are stored in the matched properties
    // cache so we need to make sure to invalidate the cache if the
    // documentElement font size changes.
    GetStyleResolver().InvalidateMatchedPropertiesCache();
    return true;
  }
  return false;
}

void StyleEngine::PropertyRegistryChanged() {
  // TODO(timloh): Invalidate only elements with this custom property set
  MarkAllElementsForStyleRecalc(StyleChangeReasonForTracing::Create(
      style_change_reason::kPropertyRegistration));
  if (resolver_) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
  InvalidateInitialData();
}

void StyleEngine::EnvironmentVariableChanged() {
  MarkAllElementsForStyleRecalc(StyleChangeReasonForTracing::Create(
      style_change_reason::kPropertyRegistration));
  if (resolver_) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
}

void StyleEngine::NodeWillBeRemoved(Node& node) {
  if (auto* element = DynamicTo<Element>(node)) {
    if (const ComputedStyle* style = node.GetComputedStyle();
        style && style->GetCounterDirectives()) {
      MarkCountersDirty();
    }
    if (element->GetComputedStyle() &&
        element->ComputedStyleRef().ContainsStyle()) {
      MarkCountersDirty();
    }
    if (element->PseudoElementStylesAffectCounters()) {
      MarkCountersDirty();
    }
    if (StyleContainmentScopeTree* tree = GetStyleContainmentScopeTree()) {
      if (element->GetComputedStyle() &&
          element->ComputedStyleRef().ContainsStyle()) {
        tree->RemoveScopeForElement(*element);
      }
    }
    pending_invalidations_.RescheduleSiblingInvalidationsAsDescendants(
        *element);
  }
}

void StyleEngine::ChildrenRemoved(ContainerNode& parent) {
  if (!parent.isConnected()) {
    return;
  }
  DCHECK(!layout_tree_rebuild_root_.GetRootNode());
  if (InDOMRemoval()) {
    // This is necessary for nested removals. There are elements which
    // removes parts of its UA shadow DOM as part of being removed which means
    // we do a removal from within another removal where isConnected() is not
    // completely up to date which would confuse this code. Also, the removal
    // doesn't have to be in the same subtree as the outer removal. For instance
    // for the ListAttributeTargetChanged mentioned below.
    //
    // Instead we fall back to use the document root as the traversal root for
    // all traversal roots.
    //
    // TODO(crbug.com/882869): MediaControlLoadingPanelElement
    // TODO(crbug.com/888448): TextFieldInputType::ListAttributeTargetChanged
    if (style_invalidation_root_.GetRootNode()) {
      UpdateStyleInvalidationRoot(nullptr, nullptr);
    }
    if (style_recalc_root_.GetRootNode()) {
      UpdateStyleRecalcRoot(nullptr, nullptr);
    }
    return;
  }
  style_invalidation_root_.SubtreeModified(parent);
  style_recalc_root_.SubtreeModified(parent);
}

void StyleEngine::CollectMatchingUserRules(
    ElementRuleCollector& collector) const {
  MatchRequest match_request;
  for (const ActiveStyleSheet& style_sheet : active_user_style_sheets_) {
    match_request.AddRuleset(style_sheet.second);
    if (match_request.IsFull()) {
      collector.CollectMatchingRules(match_request, /*part_names*/ nullptr);
      match_request.ClearAfterMatching();
    }
  }
  if (!match_request.IsEmpty()) {
    collector.CollectMatchingRules(match_request, /*part_names*/ nullptr);
  }
}

void StyleEngine::ClearKeyframeRules() {
  keyframes_rule_map_.clear();
}

void StyleEngine::ClearPropertyRules() {
  PropertyRegistration::RemoveDeclaredProperties(GetDocument());
}

void StyleEngine::AddPropertyRulesFromSheets(
    AtRuleCascadeMap& cascade_map,
    const ActiveStyleSheetVector& sheets,
    bool is_user_style) {
  for (const ActiveStyleSheet& active_sheet : sheets) {
    if (RuleSet* rule_set = active_sheet.second) {
      AddPropertyRules(cascade_map, *rule_set, is_user_style);
    }
  }
}

void StyleEngine::AddFontPaletteValuesRulesFromSheets(
    const ActiveStyleSheetVector& sheets) {
  for (const ActiveStyleSheet& active_sheet : sheets) {
    if (RuleSet* rule_set = active_sheet.second) {
      AddFontPaletteValuesRules(*rule_set);
    }
  }
}

bool StyleEngine::AddUserFontFaceRules(const RuleSet& rule_set) {
  if (!font_selector_) {
    return false;
  }

  const HeapVector<Member<StyleRuleFontFace>> font_face_rules =
      rule_set.FontFaceRules();
  for (auto& font_face_rule : font_face_rules) {
    if (FontFace* font_face = FontFace::Create(document_, font_face_rule,
                                               true /* is_user_style */)) {
      font_selector_->GetFontFaceCache()->Add(font_face_rule, font_face);
    }
  }
  if (resolver_ && font_face_rules.size()) {
    resolver_->InvalidateMatchedPropertiesCache();
  }
  return font_face_rules.size();
}

void StyleEngine::AddUserKeyframeRules(const RuleSet& rule_set) {
  const HeapVector<Member<StyleRuleKeyframes>> keyframes_rules =
      rule_set.KeyframesRules();
  for (unsigned i = 0; i < keyframes_rules.size(); ++i) {
    AddUserKeyframeStyle(keyframes_rules[i]);
  }
}

void StyleEngine::AddUserKeyframeStyle(StyleRuleKeyframes* rule) {
  AtomicString animation_name(rule->GetName());

  KeyframesRuleMap::iterator it = keyframes_rule_map_.find(animation_name);
  if (it == keyframes_rule_map_.end() ||
      UserKeyframeStyleShouldOverride(rule, it->value)) {
    keyframes_rule_map_.Set(animation_name, rule);
  }
}

bool StyleEngine::UserKeyframeStyleShouldOverride(
    const StyleRuleKeyframes* new_rule,
    const StyleRuleKeyframes* existing_rule) const {
  if (new_rule->IsVendorPrefixed() != existing_rule->IsVendorPrefixed()) {
    return existing_rule->IsVendorPrefixed();
  }
  return !user_cascade_layer_map_ || user_cascade_layer_map_->CompareLayerOrder(
                                         existing_rule->GetCascadeLayer(),
                                         new_rule->GetCascadeLayer()) <= 0;
}

void StyleEngine::AddViewTransitionRules(const ActiveStyleSheetVector& sheets) {
  if (!RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled()) {
    return;
  }
  view_transition_rule_.Clear();

  for (const ActiveStyleSheet& active_sheet : sheets) {
    RuleSet* rule_set = active_sheet.second;
    if (!rule_set || rule_set->ViewTransitionRules().empty()) {
      continue;
    }

    const CascadeLayerMap* layer_map =
        document_->GetScopedStyleResolver()
            ? document_->GetScopedStyleResolver()->GetCascadeLayerMap()
            : nullptr;
    for (auto& rule : rule_set->ViewTransitionRules()) {
      if (!view_transition_rule_ || !layer_map ||
          layer_map->CompareLayerOrder(view_transition_rule_->GetCascadeLayer(),
                                       rule->GetCascadeLayer()) <= 0) {
        view_transition_rule_ = rule;
      }
    }
  }

  UpdateViewTransitionOptIn();
}

void StyleEngine::AddFontPaletteValuesRules(const RuleSet& rule_set) {
  const HeapVector<Member<StyleRuleFontPaletteValues>>
      font_palette_values_rules = rule_set.FontPaletteValuesRules();
  for (auto& rule : font_palette_values_rules) {
    // TODO(https://crbug.com/1170794): Handle cascade layer reordering here.
    for (auto& family : ConvertFontFamilyToVector(rule->GetFontFamily())) {
      font_palette_values_rule_map_.Set(
          std::make_pair(rule->GetName(), String(family).FoldCase()), rule);
    }
  }
}

void StyleEngine::AddPropertyRules(AtRuleCascadeMap& cascade_map,
                                   const RuleSet& rule_set,
                                   bool is_user_style) {
  const HeapVector<Member<StyleRuleProperty>> property_rules =
      rule_set.PropertyRules();
  for (unsigned i = 0; i < property_rules.size(); ++i) {
    StyleRuleProperty* rule = property_rules[i];
    AtomicString name(rule->GetName());

    PropertyRegistration* registration =
        PropertyRegistration::MaybeCreateForDeclaredProperty(GetDocument(),
                                                             name, *rule);
    if (!registration) {
      continue;
    }

    auto priority =
        cascade_map.GetPriority(is_user_style, rule->GetCascadeLayer());
    if (!cascade_map.AddAndCascade(name, priority)) {
      continue;
    }

    GetDocument().EnsurePropertyRegistry().DeclareProperty(name, *registration);
    PropertyRegistryChanged();
  }
}

StyleRuleKeyframes* StyleEngine::KeyframeStylesForAnimation(
    const AtomicString& animation_name) {
  if (keyframes_rule_map_.empty()) {
    return nullptr;
  }

  KeyframesRuleMap::iterator it = keyframes_rule_map_.find(animation_name);
  if (it == keyframes_rule_map_.end()) {
    return nullptr;
  }

  return it->value.Get();
}

StyleRuleFontPaletteValues* StyleEngine::FontPaletteValuesForNameAndFamily(
    AtomicString palette_name,
    AtomicString family_name) {
  if (font_palette_values_rule_map_.empty() || palette_name.empty()) {
    return nullptr;
  }

  auto it = font_palette_values_rule_map_.find(
      std::make_pair(palette_name, String(family_name).FoldCase()));
  if (it == font_palette_values_rule_map_.end()) {
    return nullptr;
  }

  return it->value.Get();
}

DocumentStyleEnvironmentVariables& StyleEngine::EnsureEnvironmentVariables() {
  if (!environment_variables_) {
    environment_variables_ =
        MakeGarbageCollected<DocumentStyleEnvironmentVariables>(
            StyleEnvironmentVariables::GetRootInstance(), *document_);
  }
  return *environment_variables_.Get();
}

StyleInitialData* StyleEngine::MaybeCreateAndGetInitialData() {
  if (!initial_data_) {
    if (const PropertyRegistry* registry = document_->GetPropertyRegistry()) {
      if (!registry->IsEmpty()) {
        initial_data_ =
            MakeGarbageCollected<StyleInitialData>(GetDocument(), *registry);
      }
    }
  }
  return initial_data_.Get();
}

bool StyleEngine::RecalcHighlightStylesForContainer(Element& container) {
  const ComputedStyle& style = container.ComputedStyleRef();
  // If we depend on container queries we need to update styles, and also
  // the styles for dependents. Hence we return this value, which is used
  // in RecalcStyleForContainer to set the flag for child recalc.
  bool depends_on_container_queries =
      style.HighlightData().DependsOnSizeContainerQueries() ||
      style.HighlightsDependOnSizeContainerQueries();
  if (!style.HasAnyHighlightPseudoElementStyles() ||
      !style.HasNonUaHighlightPseudoStyles() || !depends_on_container_queries) {
    return false;
  }

  // We are recalculating styles for a size container whose highlight pseudo
  // styles depend on size container queries. Make sure we update those styles
  // based on the changed container size.
  StyleRecalcContext recalc_context;
  recalc_context.container = &container;
  if (const ComputedStyle* new_style = container.RecalcHighlightStyles(
          recalc_context, nullptr /* old_style */, style,
          container.ParentComputedStyle());
      new_style != &style) {
    container.SetComputedStyle(new_style);
    container.GetLayoutObject()->SetStyle(new_style,
                                          LayoutObject::ApplyStyleChanges::kNo);
  }

  return depends_on_container_queries;
}

#if DCHECK_IS_ON()
namespace {
bool ContainerStyleChangesAllowed(Element& container,
                                  const ComputedStyle* old_element_style,
                                  const ComputedStyle* old_layout_style) {
  // Generally, the size container element style is not allowed to change during
  // layout, but for highlight pseudo elements depending on queries against
  // their originating element, we need to update the style during layout since
  // the highlight styles hangs off the originating element's ComputedStyle.
  const ComputedStyle* new_element_style = container.GetComputedStyle();
  const ComputedStyle* new_layout_style =
      container.GetLayoutObject() ? container.GetLayoutObject()->Style()
                                  : nullptr;

  if (!new_element_style || !old_element_style) {
    // The container should always have a ComputedStyle.
    return false;
  }
  if (new_element_style != old_element_style) {
    Vector<ComputedStyleBase::DebugDiff> diff =
        old_element_style->DebugDiffFields(*new_element_style);
    // Allow highlight styles to change, but only highlight styles.
    if (diff.size() > 1 ||
        (diff.size() == 1 &&
         diff[0].field != ComputedStyleBase::DebugField::highlight_data_)) {
      return false;
    }
  }
  if (new_layout_style == old_layout_style) {
    return true;
  }
  if (!new_layout_style || !old_element_style) {
    // Container may not have a LayoutObject when called from
    // UpdateStyleForNonEligibleContainer(), but then make sure the style is
    // null for both cases.
    return new_layout_style == old_element_style;
  }
  Vector<ComputedStyleBase::DebugDiff> diff =
      old_layout_style->DebugDiffFields(*new_layout_style);
  // Allow highlight styles to change, but only highlight styles.
  return diff.size() == 0 ||
         (diff.size() == 1 &&
          diff[0].field == ComputedStyleBase::DebugField::highlight_data_);
}
}  // namespace
#endif  // DCHECK_IS_ON()

void StyleEngine::RecalcStyleForContainer(Element& container,
                                          StyleRecalcChange change) {
  // The container node must not need recalc at this point.
  DCHECK(!StyleRecalcChange().ShouldRecalcStyleFor(container));

#if DCHECK_IS_ON()
  const ComputedStyle* old_element_style = container.GetComputedStyle();
  const ComputedStyle* old_layout_style =
      container.GetLayoutObject() ? container.GetLayoutObject()->Style()
                                  : nullptr;
#endif  // DCHECK_IS_ON()

  // If the container itself depends on an outer container, then its
  // DependsOnSizeContainerQueries flag will be set, and we would recalc its
  // style (due to ForceRecalcContainer/ForceRecalcDescendantSizeContainers).
  // This is not necessary, hence we suppress recalc for this element.
  change = change.SuppressRecalc();

  // The StyleRecalcRoot invariants requires the root to be dirty/child-dirty
  container.SetChildNeedsStyleRecalc();
  style_recalc_root_.Update(nullptr, &container);

  if (RecalcHighlightStylesForContainer(container)) {
    change = change.ForceRecalcDescendantSizeContainers();
  }

  // TODO(crbug.com/1145970): Consider use a caching mechanism for FromAncestors
  // as we typically will call it for all containers on the first style/layout
  // pass.
  RecalcStyle(change, StyleRecalcContext::FromAncestors(container));

#if DCHECK_IS_ON()
  DCHECK(ContainerStyleChangesAllowed(container, old_element_style,
                                      old_layout_style));
#endif  // DCHECK_IS_ON()
}

void StyleEngine::UpdateStyleForNonEligibleContainer(Element& container) {
  DCHECK(InRebuildLayoutTree());
  // This method is called from AttachLayoutTree() when we skipped style recalc
  // for descendants of a size query container but figured that the LayoutObject
  // we created is not going to be reached for layout in block_node.cc where
  // we would otherwise resume style recalc.
  //
  // This may be due to legacy layout fallback, inline box, table box, etc.
  // Also, if we could not predict that the LayoutObject would not be created,
  // like if the parent LayoutObject returns false for IsChildAllowed.
  ContainerQueryData* cq_data = container.GetContainerQueryData();
  if (!cq_data) {
    return;
  }

  StyleRecalcChange change;
  ContainerQueryEvaluator& evaluator =
      container.EnsureContainerQueryEvaluator();
  ContainerQueryEvaluator::Change query_change =
      evaluator.SizeContainerChanged(PhysicalSize(), kPhysicalAxesNone);
  switch (query_change) {
    case ContainerQueryEvaluator::Change::kNone:
      DCHECK(cq_data->SkippedStyleRecalc());
      break;
    case ContainerQueryEvaluator::Change::kNearestContainer:
      if (RuntimeEnabledFeatures::CSSFlatTreeContainerEnabled() ||
          !IsShadowHost(container)) {
        change = change.ForceRecalcSizeContainer();
        break;
      }
      // Since the nearest container is found in shadow-including ancestors
      // and not in flat tree ancestors, and style recalc traversal happens in
      // flat tree order, we need to invalidate inside flat tree descendant
      // containers if such containers are inside shadow trees.
      //
      // See also StyleRecalcChange::FlagsForChildren where we turn
      // kRecalcContainer into kRecalcDescendantContainers when traversing
      // past a shadow host.
      [[fallthrough]];
    case ContainerQueryEvaluator::Change::kDescendantContainers:
      change = change.ForceRecalcDescendantSizeContainers();
      break;
  }
  if (query_change != ContainerQueryEvaluator::Change::kNone) {
    container.ComputedStyleRef().ClearCachedPseudoElementStyles();
  }

  AllowMarkForReattachFromRebuildLayoutTreeScope allow_reattach(*this);
  base::AutoReset<bool> cq_recalc(&in_container_query_style_recalc_, true);
  RecalcStyleForContainer(container, change);
}

void StyleEngine::UpdateStyleAndLayoutTreeForContainer(
    Element& container,
    const LogicalSize& logical_size,
    LogicalAxes contained_axes) {
  DCHECK(!style_recalc_root_.GetRootNode());
  DCHECK(!container.NeedsStyleRecalc());
  DCHECK(!in_container_query_style_recalc_);

  base::AutoReset<bool> cq_recalc(&in_container_query_style_recalc_, true);

  DCHECK(container.GetLayoutObject()) << "Containers must have a LayoutObject";
  const ComputedStyle& style = container.GetLayoutObject()->StyleRef();
  DCHECK(style.IsContainerForSizeContainerQueries());
  WritingMode writing_mode = style.GetWritingMode();
  PhysicalSize physical_size = AdjustForAbsoluteZoom::AdjustPhysicalSize(
      ToPhysicalSize(logical_size, writing_mode), style);
  PhysicalAxes physical_axes = ToPhysicalAxes(contained_axes, writing_mode);

  StyleRecalcChange change;

  ContainerQueryEvaluator::Change query_change =
      container.EnsureContainerQueryEvaluator().SizeContainerChanged(
          physical_size, physical_axes);

  ContainerQueryData* cq_data = container.GetContainerQueryData();
  CHECK(cq_data);

  switch (query_change) {
    case ContainerQueryEvaluator::Change::kNone:
      if (!cq_data->SkippedStyleRecalc()) {
        return;
      }
      break;
    case ContainerQueryEvaluator::Change::kNearestContainer:
      if (RuntimeEnabledFeatures::CSSFlatTreeContainerEnabled() ||
          !IsShadowHost(container)) {
        change = change.ForceRecalcSizeContainer();
        break;
      }
      // Since the nearest container is found in shadow-including ancestors and
      // not in flat tree ancestors, and style recalc traversal happens in flat
      // tree order, we need to invalidate inside flat tree descendant
      // containers if such containers are inside shadow trees.
      //
      // See also StyleRecalcChange::FlagsForChildren where we turn
      // kRecalcContainer into kRecalcDescendantContainers when traversing past
      // a shadow host.
      [[fallthrough]];
    case ContainerQueryEvaluator::Change::kDescendantContainers:
      change = change.ForceRecalcDescendantSizeContainers();
      break;
  }

  if (query_change != ContainerQueryEvaluator::Change::kNone) {
    style.ClearCachedPseudoElementStyles();
    // When the container query changes, the ::first-line matching the container
    // itself is not detected as changed. Firstly, because the style for the
    // container is computed before the layout causing the ::first-line styles
    // to change. Also, we mark the ComputedStyle with HasPseudoElementStyle()
    // for kPseudoIdFirstLine, even when the container query for the
    // ::first-line rules doesn't match, which means a diff for that flag would
    // not detect a change. Instead, if a container has ::first-line rules which
    // depends on size container queries, fall back to re-attaching its box tree
    // when any of the size queries change the evaluation result.
    if (style.HasPseudoElementStyle(kPseudoIdFirstLine) &&
        style.FirstLineDependsOnSizeContainerQueries()) {
      change = change.ForceMarkReattachLayoutTree().ForceReattachLayoutTree();
    }
  }

  NthIndexCache nth_index_cache(GetDocument());

  UpdateViewportSize();
  RecalcStyleForContainer(container, change);

  if (container.NeedsReattachLayoutTree()) {
    ReattachContainerSubtree(container);
  } else if (NeedsLayoutTreeRebuild()) {
    if (layout_tree_rebuild_root_.GetRootNode()->IsDocumentNode()) {
      // Avoid traversing from outside the container root. We know none of the
      // elements outside the subtree should be marked dirty in this pass, but
      // we may have fallen back to the document root.
      layout_tree_rebuild_root_.Clear();
      layout_tree_rebuild_root_.Update(nullptr, &container);
    } else {
      DCHECK(FlatTreeTraversal::ContainsIncludingPseudoElement(
          container, *layout_tree_rebuild_root_.GetRootNode()));
    }
    RebuildLayoutTree(&container);
  }

  // Update quotes only if there are any scopes marked dirty.
  if (StyleContainmentScopeTree* tree = GetStyleContainmentScopeTree()) {
    tree->UpdateQuotes();
  }
  UpdateCounters();
  if (container == GetDocument().documentElement()) {
    // If the container is the root element, there may be body styles which have
    // changed as a result of the new container query evaluation, and if
    // properties propagated from body changed, we need to update the viewport
    // styles.
    GetStyleResolver().PropagateStyleToViewport();
  }
  GetDocument().GetLayoutView()->UpdateCountersAfterStyleChange(
      container.GetLayoutObject());
  GetDocument().InvalidatePendingSVGResources();
}

void StyleEngine::UpdateStyleForOutOfFlow(Element& element,
                                          const CSSPropertyValueSet* try_set,
                                          const TryTacticList& tactic_list,
                                          AnchorEvaluator* anchor_evaluator) {
  const CSSPropertyValueSet* try_tactics_set =
      try_value_flips_.FlipSet(tactic_list);

  base::AutoReset<bool> pt_recalc(&in_position_try_style_recalc_, true);

  UpdateViewportSize();

  StyleRecalcContext style_recalc_context =
      StyleRecalcContext::FromAncestors(element);
  style_recalc_context.is_interleaved_oof = true;
  style_recalc_context.anchor_evaluator = anchor_evaluator;
  style_recalc_context.try_set = try_set;
  style_recalc_context.try_tactics_set = try_tactics_set;

  StyleRecalcChange change = StyleRecalcChange().ForceRecalcChildren();

  if (auto* pseudo_element = DynamicTo<PseudoElement>(element)) {
    RecalcPositionTryStyleForPseudoElement(*pseudo_element, change,
                                           style_recalc_context);
  } else {
    element.SetChildNeedsStyleRecalc();
    style_recalc_root_.Update(nullptr, &element);
    RecalcStyle(change, style_recalc_context);
  }
}

StyleRulePositionTry* StyleEngine::GetPositionTryRule(
    const ScopedCSSName& scoped_name) {
  const TreeScope* tree_scope = scoped_name.GetTreeScope();
  if (!tree_scope) {
    tree_scope = &GetDocument();
  }
  return GetStyleResolver().ResolvePositionTryRule(tree_scope,
                                                   scoped_name.GetName());
}

void StyleEngine::RecalcStyle(StyleRecalcChange change,
                              const StyleRecalcContext& style_recalc_context) {
  DCHECK(GetDocument().documentElement());
  ScriptForbiddenScope forbid_script;
  SkipStyleRecalcScope skip_scope(*this);
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &GetDocument(), /*within_selector_checking=*/false);
  Element& root_element = style_recalc_root_.RootElement();
  Element* parent = FlatTreeTraversal::ParentElement(root_element);

  SelectorFilterRootScope filter_scope(parent);
  root_element.RecalcStyle(change, style_recalc_context);

  for (ContainerNode* ancestor = root_element.GetStyleRecalcParent(); ancestor;
       ancestor = ancestor->GetStyleRecalcParent()) {
    if (auto* ancestor_element = DynamicTo<Element>(ancestor)) {
      ancestor_element->RecalcStyleForTraversalRootAncestor();
    }
    ancestor->ClearChildNeedsStyleRecalc();
  }
  style_recalc_root_.Clear();
  if (!parent || IsA<HTMLBodyElement>(root_element)) {
    PropagateWritingModeAndDirectionToHTMLRoot();
  }
}

void StyleEngine::RecalcPositionTryStyleForPseudoElement(
    PseudoElement& pseudo_element,
    const StyleRecalcChange style_recalc_change,
    const StyleRecalcContext& style_recalc_context) {
  ScriptForbiddenScope forbid_script;
  SkipStyleRecalcScope skip_scope(*this);
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &GetDocument(), /*within-selector_checking=*/false);
  SelectorFilterRootScope filter_scope(
      FlatTreeTraversal::ParentElement(*pseudo_element.OriginatingElement()));
  pseudo_element.RecalcStyle(style_recalc_change, style_recalc_context);
}

void StyleEngine::RecalcTransitionPseudoStyle() {
  // TODO(khushalsagar) : This forces a style recalc and layout tree rebuild
  // for the pseudo element tree each time we do a style recalc phase. See if
  // we can optimize this to only when the pseudo element tree is dirtied.
  SelectorFilterRootScope filter_scope(nullptr);
  document_->documentElement()->RecalcTransitionPseudoTreeStyle(
      view_transition_names_);
}

void StyleEngine::RecalcStyle() {
  RecalcStyle(
      {}, StyleRecalcContext::FromAncestors(style_recalc_root_.RootElement()));
  RecalcTransitionPseudoStyle();
}

void StyleEngine::ClearEnsuredDescendantStyles(Element& root) {
  Node* current = &root;
  while (current) {
    if (auto* element = DynamicTo<Element>(current)) {
      if (const auto* style = element->GetComputedStyle()) {
        DCHECK(style->IsEnsuredOutsideFlatTree());
        element->SetComputedStyle(nullptr);
        element->ClearNeedsStyleRecalc();
        element->ClearChildNeedsStyleRecalc();
        current = FlatTreeTraversal::Next(*current, &root);
        continue;
      }
    }
    current = FlatTreeTraversal::NextSkippingChildren(*current, &root);
  }
}

void StyleEngine::RebuildLayoutTreeForTraversalRootAncestors(
    Element* parent,
    Element* container_parent) {
  bool is_container_ancestor = false;

  for (auto* ancestor = parent; ancestor;
       ancestor = ancestor->GetReattachParent()) {
    if (ancestor == container_parent) {
      is_container_ancestor = true;
    }
    if (is_container_ancestor) {
      ancestor->RebuildLayoutTreeForSizeContainerAncestor();
    } else {
      ancestor->RebuildLayoutTreeForTraversalRootAncestor();
    }
    ancestor->ClearChildNeedsStyleRecalc();
    ancestor->ClearChildNeedsReattachLayoutTree();
  }
}

void StyleEngine::RebuildLayoutTree(Element* size_container) {
  bool propagate_to_root = false;
  {
    DCHECK(GetDocument().documentElement());
    DCHECK(!InRebuildLayoutTree());
    base::AutoReset<bool> rebuild_scope(&in_layout_tree_rebuild_, true);

    // We need a root scope here in case we recalc style for ::first-letter
    // elements as part of UpdateFirstLetterPseudoElement.
    SelectorFilterRootScope filter_scope(nullptr);

    Element& root_element = layout_tree_rebuild_root_.RootElement();
    {
      WhitespaceAttacher whitespace_attacher;
      root_element.RebuildLayoutTree(whitespace_attacher);
    }

    Element* container_parent =
        size_container ? size_container->GetReattachParent() : nullptr;
    RebuildLayoutTreeForTraversalRootAncestors(root_element.GetReattachParent(),
                                               container_parent);
    if (size_container == nullptr) {
      document_->documentElement()->RebuildTransitionPseudoLayoutTree(
          view_transition_names_);
    }
    layout_tree_rebuild_root_.Clear();
    propagate_to_root = IsA<HTMLHtmlElement>(root_element) ||
                        IsA<HTMLBodyElement>(root_element);
  }
  if (propagate_to_root) {
    PropagateWritingModeAndDirectionToHTMLRoot();
    if (NeedsLayoutTreeRebuild()) {
      RebuildLayoutTree(size_container);
    }
  }
}

void StyleEngine::ReattachContainerSubtree(Element& container) {
  // Generally, the container itself should not be marked for re-attachment. In
  // the case where we have a fieldset as a container, the fieldset itself is
  // marked for re-attachment in HTMLFieldSetElement::DidRecalcStyle to make
  // sure the rendered legend is appropriately placed in the layout tree. We
  // cannot re-attach the fieldset itself in this case since we are in the
  // process of laying it out. Instead we re-attach all children, which should
  // be sufficient.

  DCHECK(container.NeedsReattachLayoutTree());
  DCHECK(CountersChanged() || DynamicTo<HTMLFieldSetElement>(container));

  base::AutoReset<bool> rebuild_scope(&in_layout_tree_rebuild_, true);
  container.ReattachLayoutTreeChildren(base::PassKey<StyleEngine>());
  RebuildLayoutTreeForTraversalRootAncestors(&container,
                                             container.GetReattachParent());
  layout_tree_rebuild_root_.Clear();
}

void StyleEngine::UpdateStyleAndLayoutTree() {
  // All of layout tree dirtiness and rebuilding needs to happen on a stable
  // flat tree. We have an invariant that all of that happens in this method
  // as a result of style recalc and the following layout tree rebuild.
  //
  // NeedsReattachLayoutTree() marks dirty up the flat tree ancestors. Re-
  // slotting on a dirty tree could break ancestor chains and fail to update the
  // tree properly.
  DCHECK(!NeedsLayoutTreeRebuild());

  UpdateViewportStyle();

  if (GetDocument().documentElement()) {
    UpdateViewportSize();
    NthIndexCache nth_index_cache(GetDocument());
    if (NeedsStyleRecalc()) {
      TRACE_EVENT0("blink,blink_style", "Document::recalcStyle");
      SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Style.RecalcTime");
      Element* viewport_defining = GetDocument().ViewportDefiningElement();
      RecalcStyle();
      if (viewport_defining != GetDocument().ViewportDefiningElement()) {
        ViewportDefiningElementDidChange();
      }
    }
    if (NeedsLayoutTreeRebuild()) {
      TRACE_EVENT0("blink,blink_style", "Document::rebuildLayoutTree");
      SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES("Style.RebuildLayoutTreeTime");
      RebuildLayoutTree();
    }
    // Update quotes only if there are any scopes marked dirty.
    if (StyleContainmentScopeTree* tree = GetStyleContainmentScopeTree()) {
      tree->UpdateQuotes();
    }
    UpdateCounters();
  } else {
    style_recalc_root_.Clear();
  }
  UpdateColorSchemeBackground();
  GetStyleResolver().PropagateStyleToViewport();
}

void StyleEngine::ViewportDefiningElementDidChange() {
  // Guarded by if-test in UpdateStyleAndLayoutTree().
  DCHECK(GetDocument().documentElement());

  // No need to update a layout object which will be destroyed.
  if (GetDocument().documentElement()->NeedsReattachLayoutTree()) {
    return;
  }
  HTMLBodyElement* body = GetDocument().FirstBodyElement();
  if (!body || body->NeedsReattachLayoutTree()) {
    return;
  }

  LayoutObject* layout_object = body->GetLayoutObject();
  if (layout_object && layout_object->IsLayoutBlock()) {
    // When the overflow style for documentElement changes to or from visible,
    // it changes whether the body element's box should have scrollable overflow
    // on its own box or propagated to the viewport. If the body style did not
    // need a recalc, this will not be updated as its done as part of setting
    // ComputedStyle on the LayoutObject. Force a SetStyle for body when the
    // ViewportDefiningElement changes in order to trigger an update of
    // IsScrollContainer() and the PaintLayer in StyleDidChange().
    //
    // This update is also necessary if the first body element changes because
    // another body element is inserted or removed.
    layout_object->SetStyle(
        ComputedStyleBuilder(*layout_object->Style()).TakeStyle());
  }
}

void StyleEngine::FirstBodyElementChanged(HTMLBodyElement* body) {
  // If a body element changed status as being the first body element or not,
  // it might have changed its needs for scrollbars even if the style didn't
  // change. Marking it for recalc here will make sure a new ComputedStyle is
  // set on the layout object for the next style recalc, and the scrollbars will
  // be updated in LayoutObject::SetStyle(). SetStyle cannot be called here
  // directly because SetStyle() relies on style information to be up-to-date,
  // otherwise scrollbar style update might crash.
  //
  // If the body parameter is null, it means the last body is removed. Removing
  // an element does not cause a style recalc on its own, which means we need
  // to force an update of the documentElement to remove used writing-mode and
  // direction which was previously propagated from the removed body element.
  Element* dirty_element = body ? body : GetDocument().documentElement();
  DCHECK(dirty_element);
  if (body) {
    LayoutObject* layout_object = body->GetLayoutObject();
    if (!layout_object || !layout_object->IsLayoutBlock()) {
      return;
    }
  }
  dirty_element->SetNeedsStyleRecalc(
      kLocalStyleChange, StyleChangeReasonForTracing::Create(
                             style_change_reason::kViewportDefiningElement));
}

void StyleEngine::UpdateStyleInvalidationRoot(ContainerNode* ancestor,
                                              Node* dirty_node) {
  if (GetDocument().IsActive()) {
    if (InDOMRemoval()) {
      ancestor = nullptr;
      dirty_node = document_;
    }
    style_invalidation_root_.Update(ancestor, dirty_node);
  }
}

void StyleEngine::UpdateStyleRecalcRoot(ContainerNode* ancestor,
                                        Node* dirty_node) {
  if (!GetDocument().IsActive()) {
    return;
  }
  // We have at least one instance where we mark style dirty from style recalc
  // (from LayoutTextControl::StyleDidChange()). That means we are in the
  // process of traversing down the tree from the recalc root. Any updates to
  // the style recalc root will be cleared after the style recalc traversal
  // finishes and updating it may just trigger sanity DCHECKs in
  // StyleTraversalRoot. Just return here instead.
  if (GetDocument().InStyleRecalc()) {
    DCHECK(allow_mark_style_dirty_from_recalc_);
    return;
  }
  DCHECK(!InRebuildLayoutTree());
  if (InDOMRemoval()) {
    ancestor = nullptr;
    dirty_node = document_;
  }
#if DCHECK_IS_ON()
  DCHECK(!dirty_node || DisplayLockUtilities::AssertStyleAllowed(*dirty_node));
#endif
  style_recalc_root_.Update(ancestor, dirty_node);
}

void StyleEngine::UpdateLayoutTreeRebuildRoot(ContainerNode* ancestor,
                                              Node* dirty_node) {
  DCHECK(!InDOMRemoval());
  if (!GetDocument().IsActive()) {
    return;
  }
  if (InRebuildLayoutTree()) {
    DCHECK(allow_mark_for_reattach_from_rebuild_layout_tree_);
    return;
  }
#if DCHECK_IS_ON()
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(dirty_node);
  DCHECK(DisplayLockUtilities::AssertStyleAllowed(*dirty_node));
#endif
  layout_tree_rebuild_root_.Update(ancestor, dirty_node);
}

namespace {

Node* AnalysisParent(const Node& node) {
  return IsA<ShadowRoot>(node) ? node.ParentOrShadowHostElement()
                               : LayoutTreeBuilderTraversal::Parent(node);
}

bool IsRootOrSibling(const Node* root, const Node& node) {
  if (!root) {
    return false;
  }
  if (root == &node) {
    return true;
  }
  if (Node* root_parent = AnalysisParent(*root)) {
    return root_parent == AnalysisParent(node);
  }
  return false;
}

}  // namespace

StyleEngine::AncestorAnalysis StyleEngine::AnalyzeInclusiveAncestor(
    const Node& node) {
  if (IsRootOrSibling(style_recalc_root_.GetRootNode(), node)) {
    return AncestorAnalysis::kStyleRoot;
  }
  if (IsRootOrSibling(style_invalidation_root_.GetRootNode(), node)) {
    return AncestorAnalysis::kStyleRoot;
  }
  if (ComputedStyle::IsInterleavingRoot(node.GetComputedStyle())) {
    return AncestorAnalysis::kInterleavingRoot;
  }
  return AncestorAnalysis::kNone;
}

StyleEngine::AncestorAnalysis StyleEngine::AnalyzeExclusiveAncestor(
    const Node& node) {
  if (DisplayLockUtilities::IsPotentialStyleRecalcRoot(node)) {
    return AncestorAnalysis::kStyleRoot;
  }
  return AnalyzeInclusiveAncestor(node);
}

StyleEngine::AncestorAnalysis StyleEngine::AnalyzeAncestors(const Node& node) {
  AncestorAnalysis analysis = AnalyzeInclusiveAncestor(node);

  for (const Node* ancestor = LayoutTreeBuilderTraversal::Parent(node);
       ancestor; ancestor = LayoutTreeBuilderTraversal::Parent(*ancestor)) {
    // Already at maximum severity, no need to proceed.
    if (analysis == AncestorAnalysis::kStyleRoot) {
      return analysis;
    }

    // LayoutTreeBuilderTraversal::Parent skips ShadowRoots, so we check it
    // explicitly here.
    if (ShadowRoot* root = ancestor->GetShadowRoot()) {
      analysis = std::max(analysis, AnalyzeExclusiveAncestor(*root));
    }

    analysis = std::max(analysis, AnalyzeExclusiveAncestor(*ancestor));
  }

  return analysis;
}

bool StyleEngine::MarkReattachAllowed() const {
  return !InRebuildLayoutTree() ||
         allow_mark_for_reattach_from_rebuild_layout_tree_;
}

bool StyleEngine::MarkStyleDirtyAllowed() const {
  if (GetDocument().InStyleRecalc() || InContainerQueryStyleRecalc()) {
    return allow_mark_style_dirty_from_recalc_;
  }
  return !InRebuildLayoutTree();
}

bool StyleEngine::SupportsDarkColorScheme() {
  return (page_color_schemes_ &
          static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark)) &&
         (!(page_color_schemes_ &
            static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight)) ||
          preferred_color_scheme_ == mojom::blink::PreferredColorScheme::kDark);
}

void StyleEngine::UpdateColorScheme() {
  const Settings* settings = GetDocument().GetSettings();
  if (!settings) {
    return;
  }

  ForcedColors old_forced_colors = forced_colors_;
  forced_colors_ = settings->GetInForcedColors() ? ForcedColors::kActive
                                                 : ForcedColors::kNone;

  mojom::blink::PreferredColorScheme old_preferred_color_scheme =
      preferred_color_scheme_;
  if (GetDocument().IsInMainFrame()) {
    preferred_color_scheme_ = settings->GetPreferredColorScheme();
  } else {
    preferred_color_scheme_ = owner_preferred_color_scheme_;
  }
  bool old_force_dark_mode_enabled = force_dark_mode_enabled_;
  force_dark_mode_enabled_ = settings->GetForceDarkModeEnabled();
  bool media_feature_override_color_scheme = false;

  // TODO(1479201): Should DevTools emulation use the WebPreferences API
  // overrides?
  if (const MediaFeatureOverrides* overrides =
          GetDocument().GetPage()->GetMediaFeatureOverrides()) {
    if (std::optional<ForcedColors> forced_color_override =
            overrides->GetForcedColors()) {
      forced_colors_ = forced_color_override.value();
    }
    if (std::optional<mojom::blink::PreferredColorScheme>
            preferred_color_scheme_override =
                overrides->GetPreferredColorScheme()) {
      preferred_color_scheme_ = preferred_color_scheme_override.value();
      media_feature_override_color_scheme = true;
    }
  }

  const PreferenceOverrides* preference_overrides =
      GetDocument().GetPage()->GetPreferenceOverrides();
  if (preference_overrides && !media_feature_override_color_scheme) {
    std::optional<mojom::blink::PreferredColorScheme>
        preferred_color_scheme_override =
            preference_overrides->GetPreferredColorScheme();
    if (preferred_color_scheme_override.has_value()) {
      preferred_color_scheme_ = preferred_color_scheme_override.value();
    }
  }

  if (GetDocument().Printing()) {
    preferred_color_scheme_ = mojom::blink::PreferredColorScheme::kLight;
    force_dark_mode_enabled_ = false;
  }

  if (forced_colors_ != old_forced_colors ||
      preferred_color_scheme_ != old_preferred_color_scheme ||
      force_dark_mode_enabled_ != old_force_dark_mode_enabled) {
    PlatformColorsChanged();
  }

  UpdateColorSchemeMetrics();
}

void StyleEngine::UpdateColorSchemeMetrics() {
  const Settings* settings = GetDocument().GetSettings();
  if (settings->GetForceDarkModeEnabled()) {
    UseCounter::Count(GetDocument(), WebFeature::kForcedDarkMode);
  }

  // True if the preferred color scheme will match dark.
  if (preferred_color_scheme_ == mojom::blink::PreferredColorScheme::kDark) {
    UseCounter::Count(GetDocument(), WebFeature::kPreferredColorSchemeDark);
  }

  // This is equal to kPreferredColorSchemeDark in most cases, but can differ
  // with forced dark mode. With the system in dark mode and forced dark mode
  // enabled, the preferred color scheme can be light while the setting is dark.
  if (settings->GetPreferredColorScheme() ==
      mojom::blink::PreferredColorScheme::kDark) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kPreferredColorSchemeDarkSetting);
  }

  // Record kColorSchemeDarkSupportedOnRoot if the meta color-scheme contains
  // dark (though dark may not be used). This metric is also recorded in
  // longhands_custom.cc (see: ColorScheme::ApplyValue) if the root style
  // color-scheme contains dark.
  if (page_color_schemes_ &
      static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kColorSchemeDarkSupportedOnRoot);
  }
}

void StyleEngine::ColorSchemeChanged() {
  UpdateColorScheme();
}

void StyleEngine::SetPageColorSchemes(const CSSValue* color_scheme) {
  if (!GetDocument().IsActive()) {
    return;
  }

  if (auto* value_list = DynamicTo<CSSValueList>(color_scheme)) {
    page_color_schemes_ = StyleBuilderConverter::ExtractColorSchemes(
        GetDocument(), *value_list, nullptr /* color_schemes */);
  } else {
    page_color_schemes_ =
        static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal);
  }
  DCHECK(GetDocument().documentElement());
  // kSubtreeStyleChange is necessary since the page color schemes may affect
  // used values of any element in the document with a specified color-scheme of
  // 'normal'. A more targeted invalidation would need to traverse the whole
  // document tree for specified values.
  GetDocument().documentElement()->SetNeedsStyleRecalc(
      kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                               style_change_reason::kPlatformColorChange));
  UpdateColorScheme();
  UpdateColorSchemeBackground();
}

void StyleEngine::UpdateColorSchemeBackground(bool color_scheme_changed) {
  LocalFrameView* view = GetDocument().View();
  if (!view) {
    return;
  }

  LocalFrameView::UseColorAdjustBackground use_color_adjust_background =
      LocalFrameView::UseColorAdjustBackground::kNo;

  if (forced_colors_ != ForcedColors::kNone) {
    if (GetDocument().IsInMainFrame()) {
      use_color_adjust_background =
          LocalFrameView::UseColorAdjustBackground::kIfBaseNotTransparent;
    }
  } else {
    // Find out if we should use a canvas color that is different from the
    // view's base background color in order to match the root element color-
    // scheme. See spec:
    // https://drafts.csswg.org/css-color-adjust/#color-scheme-effect
    mojom::blink::ColorScheme root_color_scheme =
        mojom::blink::ColorScheme::kLight;
    if (auto* root_element = GetDocument().documentElement()) {
      if (const ComputedStyle* style = root_element->GetComputedStyle()) {
        root_color_scheme = style->UsedColorScheme();
      } else if (SupportsDarkColorScheme()) {
        root_color_scheme = mojom::blink::ColorScheme::kDark;
      }
    }
    color_scheme_background_ =
        root_color_scheme == mojom::blink::ColorScheme::kLight
            ? Color::kWhite
            : Color(0x12, 0x12, 0x12);
    if (GetDocument().IsInMainFrame()) {
      if (root_color_scheme == mojom::blink::ColorScheme::kDark) {
        use_color_adjust_background =
            LocalFrameView::UseColorAdjustBackground::kIfBaseNotTransparent;
      }
    } else if (root_color_scheme != owner_color_scheme_ &&
               // https://html.spec.whatwg.org/C#is-initial-about:blank
               !view->GetFrame().Loader().IsOnInitialEmptyDocument()) {
      // Iframes should paint a solid background if the embedding iframe has a
      // used color-scheme different from the used color-scheme of the embedded
      // root element. Normally, iframes as transparent by default.
      use_color_adjust_background =
          LocalFrameView::UseColorAdjustBackground::kYes;
    }
  }

  view->SetUseColorAdjustBackground(use_color_adjust_background,
                                    color_scheme_changed);
}

void StyleEngine::SetOwnerColorScheme(
    mojom::blink::ColorScheme color_scheme,
    mojom::blink::PreferredColorScheme preferred_color_scheme) {
  DCHECK(!GetDocument().IsInMainFrame());
  if (owner_preferred_color_scheme_ != preferred_color_scheme) {
    owner_preferred_color_scheme_ = preferred_color_scheme;
    GetDocument().ColorSchemeChanged();
  }
  if (owner_color_scheme_ != color_scheme) {
    owner_color_scheme_ = color_scheme;
    UpdateColorSchemeBackground(true);
  }
}

mojom::blink::PreferredColorScheme StyleEngine::ResolveColorSchemeForEmbedding(
    const ComputedStyle* embedder_style) const {
  // ...if 'color-scheme' is 'normal' and there's no 'color-scheme' meta tag,
  // the propagated scheme is the preferred color-scheme of the embedder
  // document.
  if (!embedder_style || embedder_style->ColorSchemeFlagsIsNormal()) {
    return GetPreferredColorScheme();
  }
  return embedder_style && embedder_style->UsedColorScheme() ==
                               mojom::blink::ColorScheme::kDark
             ? mojom::blink::PreferredColorScheme::kDark
             : mojom::blink::PreferredColorScheme::kLight;
}

void StyleEngine::UpdateForcedBackgroundColor() {
  CHECK(GetDocument().GetPage());
  mojom::blink::ColorScheme color_scheme = mojom::blink::ColorScheme::kLight;
  forced_background_color_ = LayoutTheme::GetTheme().SystemColor(
      CSSValueID::kCanvas, color_scheme,
      GetDocument().GetPage()->GetColorProviderForPainting(
          color_scheme, forced_colors_ != ForcedColors::kNone),
      GetDocument().IsInWebAppScope());
}

Color StyleEngine::ColorAdjustBackgroundColor() const {
  if (forced_colors_ != ForcedColors::kNone) {
    return ForcedBackgroundColor();
  }
  return color_scheme_background_;
}

void StyleEngine::MarkAllElementsForStyleRecalc(
    const StyleChangeReasonForTracing& reason) {
  if (Element* root = GetDocument().documentElement()) {
    root->SetNeedsStyleRecalc(kSubtreeStyleChange, reason);
  }
}

void StyleEngine::UpdateViewportStyle() {
  if (!viewport_style_dirty_) {
    return;
  }

  viewport_style_dirty_ = false;

  if (!resolver_) {
    return;
  }

  const ComputedStyle* viewport_style = resolver_->StyleForViewport();
  if (ComputedStyle::ComputeDifference(
          viewport_style, GetDocument().GetLayoutView()->Style()) !=
      ComputedStyle::Difference::kEqual) {
    GetDocument().GetLayoutView()->SetStyle(viewport_style);
  }
}

bool StyleEngine::NeedsFullStyleUpdate() const {
  return NeedsActiveStyleUpdate() || IsViewportStyleDirty() ||
         viewport_unit_dirty_flags_;
}

void StyleEngine::PropagateWritingModeAndDirectionToHTMLRoot() {
  if (HTMLHtmlElement* root_element =
          DynamicTo<HTMLHtmlElement>(GetDocument().documentElement())) {
    root_element->PropagateWritingModeAndDirectionFromBody();
  }
}

CounterStyleMap& StyleEngine::EnsureUserCounterStyleMap() {
  if (!user_counter_style_map_) {
    user_counter_style_map_ =
        CounterStyleMap::CreateUserCounterStyleMap(GetDocument());
  }
  return *user_counter_style_map_;
}

const CounterStyle& StyleEngine::FindCounterStyleAcrossScopes(
    const AtomicString& name,
    const TreeScope* scope) const {
  CounterStyleMap* target_map = nullptr;
  while (scope) {
    if (CounterStyleMap* map =
            CounterStyleMap::GetAuthorCounterStyleMap(*scope)) {
      target_map = map;
      break;
    }
    scope = scope->ParentTreeScope();
  }
  if (!target_map && user_counter_style_map_) {
    target_map = user_counter_style_map_;
  }
  if (!target_map) {
    target_map = CounterStyleMap::GetUACounterStyleMap();
  }
  if (CounterStyle* result = target_map->FindCounterStyleAcrossScopes(name)) {
    return *result;
  }
  return CounterStyle::GetDecimal();
}

void StyleEngine::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(injected_user_style_sheets_);
  visitor->Trace(injected_author_style_sheets_);
  visitor->Trace(active_user_style_sheets_);
  visitor->Trace(keyframes_rule_map_);
  visitor->Trace(font_palette_values_rule_map_);
  visitor->Trace(user_counter_style_map_);
  visitor->Trace(user_cascade_layer_map_);
  visitor->Trace(environment_variables_);
  visitor->Trace(initial_data_);
  visitor->Trace(inspector_style_sheet_);
  visitor->Trace(document_style_sheet_collection_);
  visitor->Trace(style_sheet_collection_map_);
  visitor->Trace(dirty_tree_scopes_);
  visitor->Trace(active_tree_scopes_);
  visitor->Trace(resolver_);
  visitor->Trace(vision_deficiency_filter_);
  visitor->Trace(viewport_resolver_);
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(global_rule_set_);
  visitor->Trace(pending_invalidations_);
  visitor->Trace(style_invalidation_root_);
  visitor->Trace(style_recalc_root_);
  visitor->Trace(layout_tree_rebuild_root_);
  visitor->Trace(font_selector_);
  visitor->Trace(text_to_sheet_cache_);
  visitor->Trace(tracker_);
  visitor->Trace(text_tracks_);
  visitor->Trace(vtt_originating_element_);
  visitor->Trace(parent_for_detached_subtree_);
  visitor->Trace(view_transition_rule_);
  visitor->Trace(style_image_cache_);
  visitor->Trace(fill_or_clip_path_uri_value_cache_);
  visitor->Trace(style_containment_scope_tree_);
  visitor->Trace(try_value_flips_);
  visitor->Trace(last_successful_option_dirty_set_);
  FontSelectorClient::Trace(visitor);
}

namespace {

inline bool MayHaveFlatTreeChildren(const Element& element) {
  return element.firstChild() || IsShadowHost(element) ||
         element.IsActiveSlot();
}

}  // namespace

void StyleEngine::MarkForLayoutTreeChangesAfterDetach() {
  if (!parent_for_detached_subtree_) {
    return;
  }
  auto* layout_object = parent_for_detached_subtree_.Get();
  if (auto* layout_object_element =
          DynamicTo<Element>(layout_object->GetNode())) {
    DCHECK_EQ(layout_object, layout_object_element->GetLayoutObject());

    // Mark the parent of a detached subtree for doing a whitespace or list item
    // update. These flags will be cause the element to be marked for layout
    // tree rebuild traversal during style recalc to make sure we revisit
    // whitespace text nodes and list items.

    bool mark_ancestors = false;

    // If there are no children left, no whitespace children may need
    // reattachment.
    if (MayHaveFlatTreeChildren(*layout_object_element)) {
      if (!layout_object->WhitespaceChildrenMayChange()) {
        layout_object->SetWhitespaceChildrenMayChange(true);
        mark_ancestors = true;
      }
    }
    if (!layout_object->WasNotifiedOfSubtreeChange()) {
      if (layout_object->NotifyOfSubtreeChange()) {
        mark_ancestors = true;
      }
    }
    if (mark_ancestors) {
      layout_object_element->MarkAncestorsWithChildNeedsStyleRecalc();
    }
  }
  parent_for_detached_subtree_ = nullptr;
}

void StyleEngine::InvalidateSVGResourcesAfterDetach() {
  GetDocument().InvalidatePendingSVGResources();
}

bool StyleEngine::AllowSkipStyleRecalcForScope() const {
  if (InContainerQueryStyleRecalc()) {
    return true;
  }
  if (LocalFrameView* view = GetDocument().View()) {
    // Existing layout roots before starting style recalc may end up being
    // inside skipped subtrees if we allowed skipping. If we start out with an
    // empty list, any added ones will be a result of an element style recalc,
    // which means the will not be inside a skipped subtree.
    return !view->IsSubtreeLayout();
  }
  return true;
}

void StyleEngine::AddCachedFillOrClipPathURIValue(const AtomicString& string,
                                                  const CSSValue& value) {
  fill_or_clip_path_uri_value_cache_.insert(string, &value);
}

const CSSValue* StyleEngine::GetCachedFillOrClipPathURIValue(
    const AtomicString& string) {
  auto it = fill_or_clip_path_uri_value_cache_.find(string);
  if (it == fill_or_clip_path_uri_value_cache_.end()) {
    return nullptr;
  }
  return it->value;
}

void StyleEngine::BaseURLChanged() {
  fill_or_clip_path_uri_value_cache_.clear();
}

void StyleEngine::UpdateViewportSize() {
  viewport_size_ =
      CSSToLengthConversionData::ViewportSize(GetDocument().GetLayoutView());
}

namespace {

bool UpdateLastSuccessfulPositionFallback(Element& element) {
  if (OutOfFlowData* out_of_flow_data = element.GetOutOfFlowData()) {
    LayoutObject* layout_object = element.GetLayoutObject();
    if (out_of_flow_data->ApplyPendingSuccessfulPositionFallback(
            layout_object) &&
        layout_object) {
      layout_object->SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kAnchorPositioning);
      return true;
    }
  }
  return false;
}

bool InvalidatePositionTryNames(Element* root,
                                const HashSet<AtomicString>& try_names) {
  bool invalidated = false;
  Node* current = root;
  while (current) {
    if (auto* element = DynamicTo<Element>(current)) {
      if (OutOfFlowData* data = element->GetOutOfFlowData()) {
        if (data->InvalidatePositionTryNames(try_names)) {
          LayoutObject* layout_object = element->GetLayoutObject();
          CHECK(layout_object);
          layout_object->SetNeedsLayoutAndFullPaintInvalidation(
              layout_invalidation_reason::kAnchorPositioning);
          invalidated = true;
        }
      }
      if (ComputedStyle::NullifyEnsured(element->GetComputedStyle()) ==
          nullptr) {
        current =
            LayoutTreeBuilderTraversal::NextSkippingChildren(*element, root);
        continue;
      }
    }
    current = LayoutTreeBuilderTraversal::Next(*current, root);
  }
  return invalidated;
}

}  // namespace

bool StyleEngine::UpdateLastSuccessfulPositionFallbacks() {
  bool invalidated = false;
  if (!dirty_position_try_names_.empty()) {
    // Added, removed, or modified @position-try rules.
    // Walk the whole tree and invalidate last successful position for elements
    // with position-try-fallbacks referring those names.
    if (InvalidatePositionTryNames(GetDocument().documentElement(),
                                   dirty_position_try_names_)) {
      invalidated = true;
    }
    dirty_position_try_names_.clear();
  }

  if (!last_successful_option_dirty_set_.empty()) {
    for (Element* element : last_successful_option_dirty_set_) {
      if (UpdateLastSuccessfulPositionFallback(*element)) {
        invalidated = true;
      }
    }
    last_successful_option_dirty_set_.clear();
  }
  return invalidated;
}

void StyleEngine::RevisitActiveStyleSheetsForInspector() {
  // TODO(crbug.com/337076014): Also revisit other stylesheets such as those in
  // shadow trees, user sheets, and UA sheets.
  const RuleFeatureSet& global_features = GetRuleFeatureSet();
  const ActiveStyleSheetVector& active_style_sheets =
      GetDocumentStyleSheetCollection().ActiveStyleSheets();
  for (const ActiveStyleSheet& sheet : active_style_sheets) {
    // We need to revisit each sheet twice, once with the global rule set and
    // once with the sheet's associated rule set.
    // The global rule set contains the rule invalidation data we're currently
    // using for style invalidations. However, if a stylesheet change occurs,
    // we may throw out the global rule set data and rebuild it from the
    // individual sheets' data, so the inspector needs to know about both.
    StyleSheetContents* contents = sheet.first->Contents();
    RevisitStyleRulesForInspector(global_features, contents->ChildRules());
    if (contents->HasRuleSet()) {
      RevisitStyleRulesForInspector(contents->GetRuleSet().Features(),
                                    contents->ChildRules());
    }
  }
}

void StyleEngine::RevisitStyleRulesForInspector(
    const RuleFeatureSet& features,
    const HeapVector<Member<StyleRuleBase>>& rules) {
  for (StyleRuleBase* rule : rules) {
    if (StyleRule* style_rule = DynamicTo<StyleRule>(rule)) {
      for (const CSSSelector* selector = style_rule->FirstSelector(); selector;
           selector = CSSSelectorList::Next(*selector)) {
        InvalidationSetToSelectorMap::SelectorScope selector_scope(
            style_rule, style_rule->SelectorIndex(*selector));
        features.RevisitSelectorForInspector(*selector);
      }
    } else if (StyleRuleGroup* style_rule_group =
                   DynamicTo<StyleRuleGroup>(rule)) {
      RevisitStyleRulesForInspector(features, style_rule_group->ChildRules());
    }
  }
}

}  // namespace blink
