/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENGINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENGINE_H_

#include <memory>
#include <utility>
#include "base/auto_reset.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-shared.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/animation/css/css_scroll_timeline.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_global_rule_set.h"
#include "third_party/blink/renderer/core/css/document_style_sheet_collection.h"
#include "third_party/blink/renderer/core/css/invalidation/pending_invalidations.h"
#include "third_party/blink/renderer/core/css/invalidation/style_invalidator.h"
#include "third_party/blink/renderer/core/css/layout_tree_rebuild_root.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/style_engine_context.h"
#include "third_party/blink/renderer/core/css/style_invalidation_root.h"
#include "third_party/blink/renderer/core/css/style_recalc_root.h"
#include "third_party/blink/renderer/core/css/vision_deficiency.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/tree_ordered_list.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/layout/geometry/axis.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CounterStyle;
class CounterStyleMap;
class CSSFontSelector;
class CSSStyleSheet;
class FontSelector;
class HTMLSelectElement;
class MediaQueryEvaluator;
class Node;
class RuleFeatureSet;
class ShadowTreeStyleSheetCollection;
class DocumentStyleEnvironmentVariables;
class StyleRuleFontFace;
class StyleRuleUsageTracker;
class StyleSheetContents;
class StyleInitialData;
class ViewportStyleResolver;
struct LogicalSize;

enum InvalidationScope { kInvalidateCurrentScope, kInvalidateAllScopes };

using StyleSheetKey = AtomicString;

// The StyleEngine class manages style-related state for the document. There is
// a 1-1 relationship of Document to StyleEngine. The document calls the
// StyleEngine when the document is updated in a way that impacts styles.
class CORE_EXPORT StyleEngine final : public GarbageCollected<StyleEngine>,
                                      public FontSelectorClient,
                                      public NameClient {
 public:

  class DOMRemovalScope {
    STACK_ALLOCATED();

   public:
    explicit DOMRemovalScope(StyleEngine& engine)
        : in_removal_(&engine.in_dom_removal_, true) {}

   private:
    base::AutoReset<bool> in_removal_;
  };

  // There are a few instances where we are marking nodes style dirty from
  // within style recalc. That is generally not allowed, and if allowed we must
  // make sure we mark inside the subtree we are currently traversing, be sure
  // we will traverse the marked node as part of the current traversal. The
  // current instances of this situation is marked with this scope object to
  // skip DCHECKs. Do not introduce new functionality that requires introducing
  // more such scopes.
  class AllowMarkStyleDirtyFromRecalcScope {
    STACK_ALLOCATED();

   public:
    explicit AllowMarkStyleDirtyFromRecalcScope(StyleEngine& engine)
        : allow_marking_(&engine.allow_mark_style_dirty_from_recalc_, true) {}

   private:
    base::AutoReset<bool> allow_marking_;
  };

  // We postpone updating ::first-letter styles until layout tree rebuild to
  // know which text node contains the first letter. If we need to re-attach the
  // ::first-letter element as a result means we mark for re-attachment during
  // layout tree rebuild. That is not generally allowed, and we make sure we
  // explicitly allow it for that case.
  class AllowMarkForReattachFromRebuildLayoutTreeScope {
    STACK_ALLOCATED();

   public:
    explicit AllowMarkForReattachFromRebuildLayoutTreeScope(StyleEngine& engine)
        : allow_marking_(
              &engine.allow_mark_for_reattach_from_rebuild_layout_tree_,
              true) {}

   private:
    base::AutoReset<bool> allow_marking_;
  };

  explicit StyleEngine(Document&);
  ~StyleEngine() override;

  const HeapVector<Member<StyleSheet>>& StyleSheetsForStyleSheetList(
      TreeScope&);

  const HeapVector<std::pair<StyleSheetKey, Member<CSSStyleSheet>>>&
  InjectedAuthorStyleSheets() const {
    return injected_author_style_sheets_;
  }

  CSSStyleSheet* InspectorStyleSheet() const { return inspector_style_sheet_; }

  void AddTextTrack(TextTrack*);
  void RemoveTextTrack(TextTrack*);

  // An Element with no tag name, IDs, classes (etc), as described by the
  // WebVTT spec:
  // https://w3c.github.io/webvtt/#obtaining-css-boxes
  //
  // TODO(https://github.com/w3c/webvtt/issues/477): Make originating element
  // actually featureless.
  Element* EnsureVTTOriginatingElement();

  const ActiveStyleSheetVector ActiveStyleSheetsForInspector();

  bool NeedsActiveStyleUpdate() const;
  void SetNeedsActiveStyleUpdate(TreeScope&);
  void AddStyleSheetCandidateNode(Node&);
  void RemoveStyleSheetCandidateNode(Node&, ContainerNode& insertion_point);
  void ModifiedStyleSheetCandidateNode(Node&);
  void AdoptedStyleSheetsWillChange(
      TreeScope&,
      const HeapVector<Member<CSSStyleSheet>>& old_sheets,
      const HeapVector<Member<CSSStyleSheet>>& new_sheets);
  void AddedCustomElementDefaultStyles(
      const HeapVector<Member<CSSStyleSheet>>& default_styles);
  void WatchedSelectorsChanged();
  void InitialStyleChanged();
  void ColorSchemeChanged();
  void SetOwnerColorScheme(mojom::blink::ColorScheme);
  mojom::blink::ColorScheme GetOwnerColorScheme() const {
    return owner_color_scheme_;
  }
  void InitialViewportChanged();
  void ViewportRulesChanged();

  void InjectSheet(const StyleSheetKey&, StyleSheetContents*,
                   WebDocument::CSSOrigin =
                       WebDocument::kAuthorOrigin);
  void RemoveInjectedSheet(const StyleSheetKey&,
                           WebDocument::CSSOrigin =
                               WebDocument::kAuthorOrigin);
  CSSStyleSheet& EnsureInspectorStyleSheet();
  RuleSet* WatchedSelectorsRuleSet() {
    DCHECK(global_rule_set_);
    return global_rule_set_->WatchedSelectorsRuleSet();
  }

  RuleSet* RuleSetForSheet(CSSStyleSheet&);
  void MediaQueryAffectingValueChanged(MediaValueChange change);
  void UpdateActiveStyle();

  String PreferredStylesheetSetName() const {
    return preferred_stylesheet_set_name_;
  }
  void SetPreferredStylesheetSetNameIfNotSet(const String&);
  void SetHttpDefaultStyle(const String&);

  void AddPendingSheet(StyleEngineContext&);
  void RemovePendingSheet(Node& style_sheet_candidate_node,
                          const StyleEngineContext&);

  bool HasPendingScriptBlockingSheets() const {
    return pending_script_blocking_stylesheets_ > 0;
  }
  bool HasPendingRenderBlockingSheets() const {
    return pending_render_blocking_stylesheets_ > 0;
  }
  bool HaveScriptBlockingStylesheetsLoaded() const {
    return !HasPendingScriptBlockingSheets();
  }
  bool HaveRenderBlockingStylesheetsLoaded() const {
    return !HasPendingRenderBlockingSheets();
  }

  unsigned MaxDirectAdjacentSelectors() const {
    return GetRuleFeatureSet().MaxDirectAdjacentSelectors();
  }
  bool UsesFirstLineRules() const {
    return GetRuleFeatureSet().UsesFirstLineRules();
  }
  bool UsesWindowInactiveSelector() const {
    return GetRuleFeatureSet().UsesWindowInactiveSelector();
  }
  bool UsesContainerQueries() const {
    return GetRuleFeatureSet().UsesContainerQueries() ||
           uses_container_relative_units_;
  }

  void SetUsesContainerRelativeUnits() {
    uses_container_relative_units_ = true;
  }

  bool UsesRemUnits() const { return uses_rem_units_; }
  void SetUsesRemUnit(bool uses_rem_units) { uses_rem_units_ = uses_rem_units; }
  bool UpdateRemUnits(const ComputedStyle* old_root_style,
                      const ComputedStyle* new_root_style);

  void ResetCSSFeatureFlags(const RuleFeatureSet&);

  void ShadowRootInsertedToDocument(ShadowRoot&);
  void ShadowRootRemovedFromDocument(ShadowRoot*);
  void ResetAuthorStyle(TreeScope&);

  StyleResolver& GetStyleResolver() const {
    DCHECK(resolver_);
    return *resolver_;
  }

  void SetRuleUsageTracker(StyleRuleUsageTracker*);

  void ComputeFont(Element& element,
                   ComputedStyle* font_style,
                   const CSSPropertyValueSet& font_properties) {
    UpdateActiveStyle();
    GetStyleResolver().ComputeFont(element, font_style, font_properties);
  }

  PendingInvalidations& GetPendingNodeInvalidations() {
    return pending_invalidations_;
  }
  // Push all pending invalidations on the document.
  void InvalidateStyle();
  bool MediaQueryAffectedByViewportChange();
  bool MediaQueryAffectedByDeviceChange();
  bool HasViewportDependentMediaQueries() {
    DCHECK(global_rule_set_);
    UpdateActiveStyle();
    return !global_rule_set_->GetRuleFeatureSet()
                .ViewportDependentMediaQueryResults()
                .IsEmpty();
  }

  void UpdateStyleInvalidationRoot(ContainerNode* ancestor, Node* dirty_node);
  void UpdateStyleRecalcRoot(ContainerNode* ancestor, Node* dirty_node);
  void UpdateLayoutTreeRebuildRoot(ContainerNode* ancestor, Node* dirty_node);

  CSSFontSelector* GetFontSelector() { return font_selector_; }

  void RemoveFontFaceRules(const HeapVector<Member<const StyleRuleFontFace>>&);
  // updateGenericFontFamilySettings is used from WebSettingsImpl.
  void UpdateGenericFontFamilySettings();

  void DidDetach();

  CSSStyleSheet* CreateSheet(Element&,
                             const String& text,
                             TextPosition start_position,
                             StyleEngineContext&);

  void CollectFeaturesTo(RuleFeatureSet& features) {
    CollectUserStyleFeaturesTo(features);
    CollectScopedStyleFeaturesTo(features);
    for (CSSStyleSheet* sheet : custom_element_default_style_sheets_) {
      if (!sheet)
        continue;
      if (RuleSet* rule_set = RuleSetForSheet(*sheet))
        features.Add(rule_set->Features());
    }
  }

  void EnsureUAStyleForFullscreen();
  void EnsureUAStyleForXrOverlay();
  void EnsureUAStyleForElement(const Element&);
  void EnsureUAStyleForPseudoElement(PseudoId);
  void EnsureUAStyleForForcedColors();

  void PlatformColorsChanged();

  bool HasRulesForId(const AtomicString& id) const;
  void ClassChangedForElement(const SpaceSplitString& changed_classes,
                              Element&);
  void ClassChangedForElement(const SpaceSplitString& old_classes,
                              const SpaceSplitString& new_classes,
                              Element&);
  void AttributeChangedForElement(const QualifiedName& attribute_name,
                                  Element&);
  void IdChangedForElement(const AtomicString& old_id,
                           const AtomicString& new_id,
                           Element&);
  void PseudoStateChangedForElement(CSSSelector::PseudoType, Element&);
  void PartChangedForElement(Element&);
  void ExportpartsChangedForElement(Element&);

  void ScheduleSiblingInvalidationsForElement(Element&,
                                              ContainerNode& scheduling_parent,
                                              unsigned min_direct_adjacent);
  void ScheduleInvalidationsForInsertedSibling(Element* before_element,
                                               Element& inserted_element);
  void ScheduleInvalidationsForRemovedSibling(Element* before_element,
                                              Element& removed_element,
                                              Element& after_element);
  void ScheduleNthPseudoInvalidations(ContainerNode&);
  void ScheduleInvalidationsForRuleSets(TreeScope&,
                                        const HeapHashSet<Member<RuleSet>>&,
                                        InvalidationScope =
                                            kInvalidateCurrentScope);
  void ScheduleCustomElementInvalidations(HashSet<AtomicString> tag_names);

  void NodeWillBeRemoved(Node&);
  void ChildrenRemoved(ContainerNode& parent);
  void RemovedFromFlatTree(Node& node) {
    style_recalc_root_.RemovedFromFlatTree(node);
  }
  void PseudoElementRemoved(Element& originating_element) {
    layout_tree_rebuild_root_.SubtreeModified(originating_element);
  }

  unsigned StyleForElementCount() const { return style_for_element_count_; }
  void IncStyleForElementCount() { style_for_element_count_++; }

  StyleResolverStats* Stats() { return style_resolver_stats_.get(); }
  void SetStatsEnabled(bool);

  void ApplyRuleSetChanges(TreeScope&,
                           const ActiveStyleSheetVector& old_style_sheets,
                           const ActiveStyleSheetVector& new_style_sheets);
  void ApplyUserRuleSetChanges(const ActiveStyleSheetVector& old_style_sheets,
                               const ActiveStyleSheetVector& new_style_sheets);

  void VisionDeficiencyChanged();
  void ApplyVisionDeficiencyStyle(
      scoped_refptr<ComputedStyle> layout_view_style);

  void CollectMatchingUserRules(ElementRuleCollector&) const;

  void PropertyRegistryChanged();

  void EnvironmentVariableChanged();

  // Called when the set of @scroll-timeline rules changes. E.g. if a new
  // @scroll-timeline rule was inserted.
  //
  // Not to be confused with ScrollTimelineInvalidated, which is called when
  // elements *referenced* by @scroll-timeline rules change.
  void ScrollTimelinesChanged();

  bool NeedsWhitespaceReattachment() const {
    return !whitespace_reattach_set_.IsEmpty();
  }
  bool NeedsWhitespaceReattachment(Element* element) const {
    return whitespace_reattach_set_.Contains(element);
  }
  void ClearNeedsWhitespaceReattachmentFor(Element* element) {
    whitespace_reattach_set_.erase(element);
  }
  void ClearWhitespaceReattachSet() { whitespace_reattach_set_.clear(); }
  void MarkForWhitespaceReattachment();
  void MarkAllElementsForStyleRecalc(const StyleChangeReasonForTracing& reason);
  void MarkViewportStyleDirty();
  bool IsViewportStyleDirty() const { return viewport_style_dirty_; }

  void MarkFontsNeedUpdate();
  void InvalidateStyleAndLayoutForFontUpdates();

  void MarkCounterStylesNeedUpdate();
  void UpdateCounterStyles();

  StyleRuleKeyframes* KeyframeStylesForAnimation(
      const AtomicString& animation_name);

  void UpdateTimelines();

  CSSScrollTimeline* FindScrollTimeline(const AtomicString& name);

  // Called when information a @scroll-timeline depends on changes, e.g.
  // when we have source:selector(#foo), and the element referenced by
  // #foo changes.
  //
  // Not to be confused with ScrollTimelinesChanged, which is called when
  // @scroll-timeline rules themselves change.
  void ScrollTimelineInvalidated(CSSScrollTimeline&);

  CounterStyleMap* GetUserCounterStyleMap() { return user_counter_style_map_; }
  const CounterStyle& FindCounterStyleAcrossScopes(const AtomicString&,
                                                   const TreeScope*) const;

  DocumentStyleEnvironmentVariables& EnsureEnvironmentVariables();

  scoped_refptr<StyleInitialData> MaybeCreateAndGetInitialData();

  bool NeedsStyleInvalidation() const {
    return style_invalidation_root_.GetRootNode();
  }
  bool NeedsStyleRecalc() const { return style_recalc_root_.GetRootNode(); }
  bool NeedsLayoutTreeRebuild() const {
    return layout_tree_rebuild_root_.GetRootNode();
  }
  bool NeedsFullStyleUpdate() const;

  void UpdateViewport();
  void UpdateViewportStyle();
  void UpdateStyleAndLayoutTree();
  // To be called from layout when container queries change for the container.
  void UpdateStyleAndLayoutTreeForContainer(Element& container,
                                            const LogicalSize&,
                                            LogicalAxes contained_axes);
  void RecalcStyle();

  void ClearEnsuredDescendantStyles(Element& element);
  void RebuildLayoutTree();
  bool InRebuildLayoutTree() const { return in_layout_tree_rebuild_; }
  bool InDOMRemoval() const { return in_dom_removal_; }
  bool InContainerQueryStyleRecalc() const {
    return in_container_query_style_recalc_;
  }
  void ChangeRenderingForHTMLSelect(HTMLSelectElement& select);

  void SetColorSchemeFromMeta(const CSSValue* color_scheme);
  const CSSValue* GetMetaColorSchemeValue() const { return meta_color_scheme_; }
  mojom::PreferredColorScheme GetPreferredColorScheme() const {
    return preferred_color_scheme_;
  }
  ForcedColors GetForcedColors() const { return forced_colors_; }
  void UpdateColorSchemeBackground(bool color_scheme_changed = false);
  Color ForcedBackgroundColor() const { return forced_background_color_; }
  Color ColorAdjustBackgroundColor() const;

  void Trace(Visitor*) const override;
  const char* NameInHeapSnapshot() const override { return "StyleEngine"; }

 private:
  // FontSelectorClient implementation.
  void FontsNeedUpdate(FontSelector*, FontInvalidationReason) override;

  void LoadVisionDeficiencyFilter();

 private:
  bool NeedsActiveStyleSheetUpdate() const {
    return tree_scopes_removed_ || document_scope_dirty_ ||
           dirty_tree_scopes_.size() || user_style_dirty_;
  }

  TreeScopeStyleSheetCollection& EnsureStyleSheetCollectionFor(TreeScope&);
  TreeScopeStyleSheetCollection* StyleSheetCollectionFor(TreeScope&);
  bool ShouldUpdateDocumentStyleSheetCollection() const;
  bool ShouldUpdateShadowTreeStyleSheetCollection() const;

  void MarkDocumentDirty();
  void MarkTreeScopeDirty(TreeScope&);
  void MarkUserStyleDirty();

  Document& GetDocument() const { return *document_; }

  typedef HeapHashSet<Member<TreeScope>> UnorderedTreeScopeSet;

  bool MediaQueryAffectingValueChanged(const ActiveStyleSheetVector&,
                                       MediaValueChange);
  void MediaQueryAffectingValueChanged(TreeScope&, MediaValueChange);
  void MediaQueryAffectingValueChanged(UnorderedTreeScopeSet&,
                                       MediaValueChange);
  void MediaQueryAffectingValueChanged(HeapHashSet<Member<TextTrack>>&,
                                       MediaValueChange);

  const RuleFeatureSet& GetRuleFeatureSet() const {
    DCHECK(global_rule_set_);
    return global_rule_set_->GetRuleFeatureSet();
  }

  void ClearResolvers();

  void CollectUserStyleFeaturesTo(RuleFeatureSet&) const;
  void CollectScopedStyleFeaturesTo(RuleFeatureSet&) const;

  CSSStyleSheet* ParseSheet(Element&,
                            const String& text,
                            TextPosition start_position);

  const DocumentStyleSheetCollection& GetDocumentStyleSheetCollection() const {
    DCHECK(document_style_sheet_collection_);
    return *document_style_sheet_collection_;
  }

  DocumentStyleSheetCollection& GetDocumentStyleSheetCollection() {
    DCHECK(document_style_sheet_collection_);
    return *document_style_sheet_collection_;
  }

  void UpdateActiveStyleSheetsInShadow(
      TreeScope*,
      UnorderedTreeScopeSet& tree_scopes_removed);

  bool ShouldSkipInvalidationFor(const Element&) const;
  void ScheduleRuleSetInvalidationsForElement(
      Element&,
      const HeapHashSet<Member<RuleSet>>&);
  void ScheduleTypeRuleSetInvalidations(ContainerNode&,
                                        const HeapHashSet<Member<RuleSet>>&);
  void InvalidateSlottedElements(HTMLSlotElement&);
  void InvalidateForRuleSetChanges(
      TreeScope& tree_scope,
      const HeapHashSet<Member<RuleSet>>& changed_rule_sets,
      unsigned changed_rule_flags,
      InvalidationScope invalidation_scope);
  void InvalidateInitialData();

  void UpdateActiveUserStyleSheets();
  void UpdateActiveStyleSheets();
  void UpdateGlobalRuleSet() {
    DCHECK(!NeedsActiveStyleSheetUpdate());
    if (global_rule_set_)
      global_rule_set_->Update(GetDocument());
  }
  const MediaQueryEvaluator& EnsureMediaQueryEvaluator();
  void UpdateStyleSheetList(TreeScope&);

  // Returns true if any @font-face rules are added or removed.
  bool ClearFontFaceCacheAndAddUserFonts(
      const ActiveStyleSheetVector& user_sheets);

  void ClearKeyframeRules() { keyframes_rule_map_.clear(); }
  void ClearPropertyRules();
  void ClearScrollTimelineRules();

  void AddPropertyRulesFromSheets(const ActiveStyleSheetVector&);
  void AddScrollTimelineRulesFromSheets(const ActiveStyleSheetVector&);

  // Returns true if any @font-face rules are added.
  bool AddUserFontFaceRules(const RuleSet&);
  void AddUserKeyframeRules(const RuleSet&);
  void AddUserKeyframeStyle(StyleRuleKeyframes*);
  void AddPropertyRules(const RuleSet&);
  void AddScrollTimelineRules(const RuleSet&);

  CounterStyleMap& EnsureUserCounterStyleMap();

  void UpdateColorScheme();
  bool SupportsDarkColorScheme();
  void UpdateForcedBackgroundColor();

  void UpdateColorSchemeMetrics();

  void ViewportDefiningElementDidChange();
  void PropagateWritingModeAndDirectionToHTMLRoot();

  void RecalcStyle(StyleRecalcChange, const StyleRecalcContext&);

  Member<Document> document_;

  // Tracks the number of currently loading top-level stylesheets. Sheets loaded
  // using the @import directive are not included in this count. We use this
  // count of pending sheets to detect when it is safe to execute scripts
  // (parser-inserted scripts may not run until all pending stylesheets have
  // loaded). See:
  // https://html.spec.whatwg.org/multipage/semantics.html#interactions-of-styling-and-scripting
  int pending_script_blocking_stylesheets_{0};

  // Tracks the number of currently loading top-level stylesheets which block
  // rendering (the "Update the rendering" step of the event loop processing
  // model) from starting. Sheets loaded using the @import directive are not
  // included in this count. See:
  // https://html.spec.whatwg.org/multipage/webappapis.html#event-loop-processing-model
  // Once all of these sheets have loaded, rendering begins.
  int pending_render_blocking_stylesheets_{0};

  // Tracks the number of currently loading top-level stylesheets which block
  // the HTML parser. Sheets loaded using the @import directive are not included
  // in this count. Once all of these sheets have loaded, the parser may
  // continue.
  int pending_parser_blocking_stylesheets_{0};

  Member<CSSStyleSheet> inspector_style_sheet_;

  Member<DocumentStyleSheetCollection> document_style_sheet_collection_;

  Member<StyleRuleUsageTracker> tracker_;

  using StyleSheetCollectionMap =
      HeapHashMap<WeakMember<TreeScope>,
                  Member<ShadowTreeStyleSheetCollection>>;
  StyleSheetCollectionMap style_sheet_collection_map_;

  bool document_scope_dirty_{true};
  bool tree_scopes_removed_{false};
  bool user_style_dirty_{false};
  UnorderedTreeScopeSet dirty_tree_scopes_;
  UnorderedTreeScopeSet active_tree_scopes_;

  String preferred_stylesheet_set_name_;

  bool uses_rem_units_{false};
  bool uses_container_relative_units_{false};
  bool in_layout_tree_rebuild_{false};
  bool in_container_query_style_recalc_{false};
  bool in_dom_removal_{false};
  bool viewport_style_dirty_{false};
  bool fonts_need_update_{false};
  bool counter_styles_need_update_{false};
  bool timelines_need_update_{false};

  // Set to true if we allow marking style dirty from style recalc. Ideally, we
  // should get rid of this, but we keep track of where we allow it with
  // AllowMarkStyleDirtyFromRecalcScope.
  bool allow_mark_style_dirty_from_recalc_{false};

  // Set to true if we allow marking for reattachment from layout tree rebuild.
  // AllowMarkStyleDirtyFromRecalcScope.
  bool allow_mark_for_reattach_from_rebuild_layout_tree_{false};

  VisionDeficiency vision_deficiency_{VisionDeficiency::kNoVisionDeficiency};
  Member<ReferenceFilterOperation> vision_deficiency_filter_;

  Member<StyleResolver> resolver_;
  Member<ViewportStyleResolver> viewport_resolver_;
  Member<MediaQueryEvaluator> media_query_evaluator_;
  Member<CSSGlobalRuleSet> global_rule_set_;

  PendingInvalidations pending_invalidations_;

  StyleInvalidationRoot style_invalidation_root_;
  StyleRecalcRoot style_recalc_root_;
  LayoutTreeRebuildRoot layout_tree_rebuild_root_;

  // This is a set of rendered elements which had one or more of its rendered
  // children removed since the last lifecycle update. For such elements we need
  // to re-attach whitespace children. Also see reattach_all_whitespace_nodes_
  // in the WhitespaceAttacher class.
  HeapHashSet<Member<Element>> whitespace_reattach_set_;

  Member<CSSFontSelector> font_selector_;

  HeapHashMap<AtomicString, WeakMember<StyleSheetContents>>
      text_to_sheet_cache_;
  HeapHashMap<WeakMember<StyleSheetContents>, AtomicString>
      sheet_to_text_cache_;

  std::unique_ptr<StyleResolverStats> style_resolver_stats_;
  unsigned style_for_element_count_{0};

  HeapVector<std::pair<StyleSheetKey, Member<CSSStyleSheet>>>
      injected_user_style_sheets_;
  HeapVector<std::pair<StyleSheetKey, Member<CSSStyleSheet>>>
      injected_author_style_sheets_;

  ActiveStyleSheetVector active_user_style_sheets_;

  HeapHashSet<WeakMember<CSSStyleSheet>> custom_element_default_style_sheets_;
  using KeyframesRuleMap =
      HeapHashMap<AtomicString, Member<StyleRuleKeyframes>>;
  KeyframesRuleMap keyframes_rule_map_;

  Member<CounterStyleMap> user_counter_style_map_;

  HeapHashMap<AtomicString, Member<StyleRuleScrollTimeline>>
      scroll_timeline_rule_map_;
  HeapHashMap<AtomicString, Member<CSSScrollTimeline>> scroll_timeline_map_;

  scoped_refptr<DocumentStyleEnvironmentVariables> environment_variables_;

  scoped_refptr<StyleInitialData> initial_data_;

  // Color schemes explicitly supported by the author through the viewport meta
  // tag. E.g. <meta name="color-scheme" content="light dark">. A dark color-
  // scheme is used to opt-out of forced darkening.
  Member<const CSSValue> meta_color_scheme_;

  // The preferred color scheme is set in settings, but may be overridden by the
  // ForceDarkMode setting where the preferred_color_scheme_ will be set to
  // kLight to avoid dark styling to be applied before auto darkening.
  mojom::PreferredColorScheme preferred_color_scheme_{
      mojom::PreferredColorScheme::kLight};

  // We pass the used value of color-scheme from the iframe element in the
  // embedding document. If the color-scheme of the owner element and the root
  // element in the embedded document differ, use a solid backdrop color instead
  // of the default transparency of an iframe.
  mojom::blink::ColorScheme owner_color_scheme_;

  // The color of the canvas backdrop for the used color-scheme.
  Color color_scheme_background_;

  // Forced colors is set in WebThemeEngine.
  ForcedColors forced_colors_{ForcedColors::kNone};

  Color forced_background_color_;

  friend class NodeTest;
  friend class StyleEngineTest;
  friend class WhitespaceAttacherTest;
  friend class StyleCascadeTest;

  HeapHashSet<Member<TextTrack>> text_tracks_;
  Member<Element> vtt_originating_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENGINE_H_
