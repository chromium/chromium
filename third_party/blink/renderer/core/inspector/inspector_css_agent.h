/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_CSS_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_CSS_AGENT_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"
#include "third_party/blink/renderer/core/inspector/protocol/CSS.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class FontCustomPlatformData;
class FontFace;
class InspectedFrames;
class InspectorNetworkAgent;
class InspectorResourceContainer;
class InspectorResourceContentLoader;
class MediaList;
class Node;
class LayoutObject;
class StyleRuleUsageTracker;

class CORE_EXPORT InspectorCSSAgent final
    : public InspectorBaseAgent<protocol::CSS::Metainfo>,
      public InspectorDOMAgent::DOMListener,
      public InspectorStyleSheetBase::Listener {
  USING_GARBAGE_COLLECTED_MIXIN(InspectorCSSAgent);

 public:
  enum MediaListSource {
    kMediaListSourceLinkedSheet,
    kMediaListSourceInlineSheet,
    kMediaListSourceMediaRule,
    kMediaListSourceImportRule
  };

  class InlineStyleOverrideScope {
    STACK_ALLOCATED();

   public:
    InlineStyleOverrideScope(SecurityContext* context)
        : content_security_policy_(context->GetContentSecurityPolicy()) {
      content_security_policy_->SetOverrideAllowInlineStyle(true);
    }

    ~InlineStyleOverrideScope() {
      content_security_policy_->SetOverrideAllowInlineStyle(false);
    }

   private:
    Member<ContentSecurityPolicy> content_security_policy_;
  };

  static CSSStyleRule* AsCSSStyleRule(CSSRule*);
  static CSSMediaRule* AsCSSMediaRule(CSSRule*);

  static void CollectAllDocumentStyleSheets(Document*,
                                            HeapVector<Member<CSSStyleSheet>>&);

  static void GetBackgroundColors(Element* element,
                                  Vector<Color>* background_colors,
                                  String* computed_font_size,
                                  String* computed_font_weight);

  InspectorCSSAgent(InspectorDOMAgent*,
                    InspectedFrames*,
                    InspectorNetworkAgent*,
                    InspectorResourceContentLoader*,
                    InspectorResourceContainer*);
  ~InspectorCSSAgent() override;
  void Trace(blink::Visitor*) override;

  void ForcePseudoState(Element*, CSSSelector::PseudoType, bool* result);
  void DidCommitLoadForLocalFrame(LocalFrame*) override;
  void Restore() override;
  void FlushPendingProtocolNotifications() override;
  void Reset();
  void MediaQueryResultChanged();

  void ActiveStyleSheetsUpdated(Document*);
  void DocumentDetached(Document*);
  void FontsUpdated(const FontFace*,
                    const String& src,
                    const FontCustomPlatformData*);
  void SetCoverageEnabled(bool);
  void WillChangeStyleElement(Element*);

  void enable(std::unique_ptr<EnableCallback>) override;
  protocol::Response disable() override;
  protocol::Response getMatchedStylesForNode(
      int node_id,
      protocol::Maybe<protocol::CSS::CSSStyle>* inline_style,
      protocol::Maybe<protocol::CSS::CSSStyle>* attributes_style,
      protocol::Maybe<protocol::Array<protocol::CSS::RuleMatch>>*
          matched_css_rules,
      protocol::Maybe<protocol::Array<protocol::CSS::PseudoElementMatches>>*,
      protocol::Maybe<protocol::Array<protocol::CSS::InheritedStyleEntry>>*,
      protocol::Maybe<protocol::Array<protocol::CSS::CSSKeyframesRule>>*)
      override;
  protocol::Response getInlineStylesForNode(
      int node_id,
      protocol::Maybe<protocol::CSS::CSSStyle>* inline_style,
      protocol::Maybe<protocol::CSS::CSSStyle>* attributes_style) override;
  protocol::Response getComputedStyleForNode(
      int node_id,
      std::unique_ptr<
          protocol::Array<protocol::CSS::CSSComputedStyleProperty>>*) override;
  protocol::Response getPlatformFontsForNode(
      int node_id,
      std::unique_ptr<protocol::Array<protocol::CSS::PlatformFontUsage>>* fonts)
      override;
  protocol::Response collectClassNames(
      const String& style_sheet_id,
      std::unique_ptr<protocol::Array<String>>* class_names) override;
  protocol::Response getStyleSheetText(const String& style_sheet_id,
                                       String* text) override;
  protocol::Response setStyleSheetText(
      const String& style_sheet_id,
      const String& text,
      protocol::Maybe<String>* source_map_url) override;
  protocol::Response setRuleSelector(
      const String& style_sheet_id,
      std::unique_ptr<protocol::CSS::SourceRange>,
      const String& selector,
      std::unique_ptr<protocol::CSS::SelectorList>*) override;
  protocol::Response setKeyframeKey(
      const String& style_sheet_id,
      std::unique_ptr<protocol::CSS::SourceRange>,
      const String& key_text,
      std::unique_ptr<protocol::CSS::Value>* out_key_text) override;
  protocol::Response setStyleTexts(
      std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>
          edits,
      std::unique_ptr<protocol::Array<protocol::CSS::CSSStyle>>* styles)
      override;
  protocol::Response setMediaText(
      const String& style_sheet_id,
      std::unique_ptr<protocol::CSS::SourceRange>,
      const String& text,
      std::unique_ptr<protocol::CSS::CSSMedia>*) override;
  protocol::Response createStyleSheet(const String& frame_id,
                                      String* style_sheet_id) override;
  protocol::Response addRule(const String& style_sheet_id,
                             const String& rule_text,
                             std::unique_ptr<protocol::CSS::SourceRange>,
                             std::unique_ptr<protocol::CSS::CSSRule>*) override;
  protocol::Response forcePseudoState(
      int node_id,
      std::unique_ptr<protocol::Array<String>> forced_pseudo_classes) override;
  protocol::Response getMediaQueries(
      std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>>*) override;
  protocol::Response setEffectivePropertyValueForNode(
      int node_id,
      const String& property_name,
      const String& value) override;
  protocol::Response getBackgroundColors(
      int node_id,
      protocol::Maybe<protocol::Array<String>>* background_colors,
      protocol::Maybe<String>* computed_font_size,
      protocol::Maybe<String>* computed_font_weight) override;

  protocol::Response startRuleUsageTracking() override;
  protocol::Response takeCoverageDelta(
      std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>* result)
      override;
  protocol::Response stopRuleUsageTracking(
      std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>* result)
      override;

  void CollectMediaQueriesFromRule(CSSRule*,
                                   protocol::Array<protocol::CSS::CSSMedia>*);
  void CollectMediaQueriesFromStyleSheet(
      CSSStyleSheet*,
      protocol::Array<protocol::CSS::CSSMedia>*);
  std::unique_ptr<protocol::CSS::CSSMedia> BuildMediaObject(const MediaList*,
                                                            MediaListSource,
                                                            const String&,
                                                            CSSStyleSheet*);
  std::unique_ptr<protocol::Array<protocol::CSS::CSSMedia>> BuildMediaListChain(
      CSSRule*);

  CSSStyleDeclaration* FindEffectiveDeclaration(
      const CSSProperty&,
      const HeapVector<Member<CSSStyleDeclaration>>& styles);

  HeapVector<Member<CSSStyleDeclaration>> MatchingStyles(Element*);
  String StyleSheetId(CSSStyleSheet*);

 private:
  class StyleSheetAction;
  class SetStyleSheetTextAction;
  class ModifyRuleAction;
  class SetElementStyleAction;
  class AddRuleAction;

  static void CollectStyleSheets(CSSStyleSheet*,
                                 HeapVector<Member<CSSStyleSheet>>&);

  typedef HeapHashMap<String, Member<InspectorStyleSheet>>
      IdToInspectorStyleSheet;
  typedef HeapHashMap<String, Member<InspectorStyleSheetForInlineStyle>>
      IdToInspectorStyleSheetForInlineStyle;
  typedef HeapHashMap<Member<Node>, Member<InspectorStyleSheetForInlineStyle>>
      NodeToInspectorStyleSheet;  // bogus "stylesheets" with elements' inline
                                  // styles
  typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

  void ResourceContentLoaded(std::unique_ptr<EnableCallback>);
  void CompleteEnabled();
  void ResetNonPersistentData();
  InspectorStyleSheetForInlineStyle* AsInspectorStyleSheet(Element* element);

  void UpdateActiveStyleSheets(Document*);
  void SetActiveStyleSheets(Document*,
                            const HeapVector<Member<CSSStyleSheet>>&);
  protocol::Response SetStyleText(InspectorStyleSheetBase*,
                                  const SourceRange&,
                                  const String&,
                                  CSSStyleDeclaration*&);
  protocol::Response MultipleStyleTextsActions(
      std::unique_ptr<protocol::Array<protocol::CSS::StyleDeclarationEdit>>,
      HeapVector<Member<StyleSheetAction>>* actions);

  std::unique_ptr<protocol::Array<protocol::CSS::CSSKeyframesRule>>
  AnimationsForNode(Element*);

  void CollectPlatformFontsForLayoutObject(
      LayoutObject*,
      HashCountedSet<std::pair<int, String>>*,
      unsigned descendants_depth);

  InspectorStyleSheet* BindStyleSheet(CSSStyleSheet*);
  String UnbindStyleSheet(InspectorStyleSheet*);
  InspectorStyleSheet* InspectorStyleSheetForRule(CSSStyleRule*);

  InspectorStyleSheet* ViaInspectorStyleSheet(Document*);

  protocol::Response AssertEnabled();
  protocol::Response AssertInspectorStyleSheetForId(const String&,
                                                    InspectorStyleSheet*&);
  protocol::Response AssertStyleSheetForId(const String&,
                                           InspectorStyleSheetBase*&);
  String DetectOrigin(CSSStyleSheet* page_style_sheet,
                      Document* owner_document);

  std::unique_ptr<protocol::CSS::CSSRule> BuildObjectForRule(CSSStyleRule*);
  std::unique_ptr<protocol::CSS::RuleUsage> BuildCoverageInfo(CSSStyleRule*,
                                                              bool);
  std::unique_ptr<protocol::Array<protocol::CSS::RuleMatch>>
  BuildArrayForMatchedRuleList(RuleIndexList*, Element*, PseudoId);
  std::unique_ptr<protocol::CSS::CSSStyle> BuildObjectForAttributesStyle(
      Element*);

  // InspectorDOMAgent::DOMListener implementation
  void DidAddDocument(Document*) override;
  void DidRemoveDocument(Document*) override;
  void DidRemoveDOMNode(Node*) override;
  void DidModifyDOMAttr(Element*) override;

  // InspectorStyleSheet::Listener implementation
  void StyleSheetChanged(InspectorStyleSheetBase*) override;

  void ResetPseudoStates();

  Member<InspectorDOMAgent> dom_agent_;
  Member<InspectedFrames> inspected_frames_;
  Member<InspectorNetworkAgent> network_agent_;
  Member<InspectorResourceContentLoader> resource_content_loader_;
  Member<InspectorResourceContainer> resource_container_;

  IdToInspectorStyleSheet id_to_inspector_style_sheet_;
  IdToInspectorStyleSheetForInlineStyle
      id_to_inspector_style_sheet_for_inline_style_;
  HeapHashMap<Member<CSSStyleSheet>, Member<InspectorStyleSheet>>
      css_style_sheet_to_inspector_style_sheet_;
  typedef HeapHashMap<Member<Document>,
                      Member<HeapHashSet<Member<CSSStyleSheet>>>>
      DocumentStyleSheets;
  DocumentStyleSheets document_to_css_style_sheets_;
  HeapHashSet<Member<Document>> invalidated_documents_;

  NodeToInspectorStyleSheet node_to_inspector_style_sheet_;
  NodeIdToForcedPseudoState node_id_to_forced_pseudo_state_;

  Member<StyleRuleUsageTracker> tracker_;

  Member<CSSStyleSheet> inspector_user_agent_style_sheet_;

  int resource_content_loader_client_id_;
  InspectorAgentState::Boolean enable_requested_;
  bool enable_completed_;
  InspectorAgentState::Boolean coverage_enabled_;

  friend class InspectorResourceContentLoaderCallback;
  friend class StyleSheetBinder;
  DISALLOW_COPY_AND_ASSIGN(InspectorCSSAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_CSS_AGENT_H_
