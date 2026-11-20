// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"

#include <algorithm>

#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"
#include "third_party/blink/renderer/core/inspector/protocol/protocol.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

// Triggers InspectorCSSAgent::FontsUpdated() synchronously when font resolution
// looks up this source during Element::EnsureComputedStyle().
class ReentrantFontFaceSource final : public CSSFontFaceSource {
 public:
  explicit ReentrantFontFaceSource(InspectorCSSAgent* agent) : agent_(agent) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(agent_);
    CSSFontFaceSource::Trace(visitor);
  }

 protected:
  const SimpleFontData* CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override {
    if (agent_) {
      agent_->FontsUpdated(nullptr, g_null_atom, nullptr);
    }
    return nullptr;
  }

 private:
  WeakMember<InspectorCSSAgent> agent_;
};

// Mirrors DevToolsSession::FlushProtocolNotifications(), triggered when an
// attached agent (e.g. InspectorAuditsAgent) calls GetFrontend()->flush().
class FlushingFrontendChannel : public protocol::FrontendChannel {
 public:
  void SetCSSAgent(InspectorCSSAgent* css_agent) { css_agent_ = css_agent; }
  void SendProtocolResponse(int,
                            std::unique_ptr<protocol::Serializable>) override {}
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable>) override {}
  void FlushProtocolNotifications() override {
    if (css_agent_) {
      css_agent_->FlushPendingProtocolNotifications();
    }
  }

 private:
  Persistent<InspectorCSSAgent> css_agent_;
};

}  // namespace

class InspectorCSSAgentTest : public PageTestBase {
 public:
  HeapHashSet<Member<CSSStyleSheet>> CollectAllDocumentStyleSheets() {
    HeapVector<Member<CSSStyleSheet>> sheets;
    InspectorCSSAgent::CollectAllDocumentStyleSheets(&GetDocument(), sheets);

    HeapHashSet<Member<CSSStyleSheet>> sheets_set;
    for (Member<CSSStyleSheet>& sheet : sheets) {
      sheets_set.insert(sheet);
    }
    return sheets_set;
  }

  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
  CollectReferencedFunctionRules(const char* selector) {
    HeapHashSet<Member<CSSStyleSheet>> sheets = CollectAllDocumentStyleSheets();

    Element* e = GetDocument().querySelector(AtomicString(selector),
                                             ASSERT_NO_EXCEPTION);
    CHECK(e);
    InspectorStyleResolver resolver(e, kPseudoIdNone,
                                    /*pseudo_argument=*/g_null_atom);

    HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
        function_rules;
    InspectorCSSAgent::CollectReferencedFunctionRules(
        InspectorCSSAgent::BuildFunctionRuleMap(sheets),
        *resolver.MatchedRules(), function_rules);
    return function_rules;
  }

  InspectorCSSAgent* CreateInspectorCSSAgent() {
    LocalFrame* frame = GetDocument().GetFrame();
    InspectedFrames* inspected_frames =
        MakeGarbageCollected<InspectedFrames>(frame);
    auto* dom_agent = MakeGarbageCollected<InspectorDOMAgent>(
        GetDocument().GetExecutionContext()->GetIsolate(), inspected_frames);
    dom_agent->FrameDocumentUpdated(frame);
    InspectorCSSAgent* agent = MakeGarbageCollected<InspectorCSSAgent>(
        dom_agent, inspected_frames,
        MakeGarbageCollected<InspectorNetworkAgent>(inspected_frames, nullptr,
                                                    /*client_is_trusted=*/true),
        MakeGarbageCollected<InspectorResourceContentLoader>(
            GetDocument().GetFrame()),
        MakeGarbageCollected<InspectorResourceContainer>(inspected_frames));
    agent->UpdateActiveStyleSheets(&GetDocument());
    return agent;
  }

  using AtRules = std::unique_ptr<protocol::Array<protocol::CSS::CSSAtRule>>;
  AtRules CollectFontAtRules(const char* selector,
                             std::vector<PseudoId> pseudo_ids) {
    Element* e = GetDocument().querySelector(AtomicString(selector),
                                             ASSERT_NO_EXCEPTION);
    CHECK(e);
    HeapVector<Member<Element>> elements = {e};
    for (PseudoId pseudo_id : pseudo_ids) {
      Element* pseudo_element = e->GetPseudoElement(pseudo_id);
      CHECK(pseudo_element);
      elements.push_back(pseudo_element);
    }
    return CreateInspectorCSSAgent()->FontAtRulesForNodes(elements);
  }

  AtRules CollectFontAtRules(const char* selector) {
    return CollectFontAtRules(selector, {});
  }

  AtRules CollectCounterAtRules(const char* selector) {
    Element* e = GetDocument().querySelector(AtomicString(selector),
                                             ASSERT_NO_EXCEPTION);
    CHECK(e);
    return CreateInspectorCSSAgent()->CounterAtRulesForElement(e);
  }

  CSSStyleRule* GetSingleNestedStyleRule(TreeScope* tree_scope) {
    CHECK_EQ(tree_scope->StyleSheets().length(), 1);
    StyleSheet* sheet = tree_scope->StyleSheets().item(0);
    CHECK(sheet->IsCSSStyleSheet());
    CSSRuleList* css_rules =
        To<CSSStyleSheet>(*sheet).cssRules(ASSERT_NO_EXCEPTION);
    CHECK_EQ(css_rules->length(), 1);
    CSSRule* rule = css_rules->item(0);
    return DynamicTo<CSSStyleRule>(rule);
  }

  CSSStyleRule* GetInnermostStyleRule(CSSStyleSheet* sheet) {
    CSSRuleList* css_rules = sheet->cssRules(ASSERT_NO_EXCEPTION);
    CHECK_EQ(css_rules->length(), 1u);
    CSSRule* rule = css_rules->item(0);

    while (rule->cssRules()->length() > 0) {
      rule = rule->cssRules()->item(0);
      CHECK_LE(css_rules->length(), 1u);
    }
    return DynamicTo<CSSStyleRule>(rule);
  }

  std::unique_ptr<protocol::CSS::CSSRule> BuildObjectForInnermostRule(
      const char* selector) {
    Element* e = GetDocument().querySelector(AtomicString(selector),
                                             ASSERT_NO_EXCEPTION);
    CHECK(e);
    TreeScope* tree_scope = &(e->GetTreeScope());
    CHECK(tree_scope);
    CHECK_EQ(tree_scope->StyleSheets().length(), 1u);
    StyleSheet* sheet = tree_scope->StyleSheets().item(0);
    CHECK(sheet->IsCSSStyleSheet());
    CSSStyleRule* rule = GetInnermostStyleRule(DynamicTo<CSSStyleSheet>(sheet));
    CHECK(rule);
    return CreateInspectorCSSAgent()->BuildObjectForRule(
        rule, e, kPseudoIdNone, /*pseudo_argument=*/g_null_atom, tree_scope);
  }

  CSSFunctionRule* FindFunctionRule(
      const HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>&
          function_rules,
      const ScopedCSSName* scoped_name) {
    auto it = function_rules.find(scoped_name);
    return it != function_rules.end() ? it->value.Get() : nullptr;
  }

  CSSFunctionRule* FindFunctionRule(
      const HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>&
          function_rules,
      const char* name) {
    auto* scoped_name = MakeGarbageCollected<ScopedCSSName>(
        AtomicString(name), /*tree_scope=*/&GetDocument());
    return FindFunctionRule(function_rules, scoped_name);
  }

  String GetComputedStyle(Element* element, CSSPropertyID property_id) {
    const CSSProperty& property = CSSProperty::Get(property_id);
    const ComputedStyle* computed_style = element->GetComputedStyle();
    const LayoutObject* layout_object = element->GetLayoutObject();
    const CSSValue* computed_value =
        property.CSSProperty::CSSValueFromComputedStyle(
            *computed_style, layout_object, /* allow_visited_style */ false,
            CSSValuePhase::kResolvedValue);
    return computed_value->CssText();
  }

  String InspectorResolvePercentageValues(Element* element,
                                          CSSPropertyID property_id,
                                          String value_str) {
    const CSSPropertyName property_name =
        CSSProperty::Get(property_id).GetCSSPropertyName();
    ScopedNullExecutionContext execution_context;
    auto* document =
        Document::CreateForTest(execution_context.GetExecutionContext());
    const CSSValue* value = css_test_helpers::ParseValue(
        *document, "<length-percentage>", value_str);
    return InspectorCSSAgent::ResolvePercentagesValues(element, property_name,
                                                       value, value_str);
  }

  std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>>
  StopRuleUsageTracking(InspectorCSSAgent* agent) {
    agent->UpdateActiveStyleSheets(&GetDocument());
    std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>> result;
    agent->stopRuleUsageTracking(&result);
    return result;
  }

  void TearDown() override {
    channel_.SetCSSAgent(nullptr);
    if (audits_agent_) {
      audits_agent_->Dispose();
      audits_agent_ = nullptr;
    }
    if (attached_agent_) {
      attached_agent_->Dispose();
      attached_agent_ = nullptr;
    }
    PageTestBase::TearDown();
  }

  // Wires a CSS agent and Audits agent to a flushing frontend channel and probe
  // sink so FontsUpdated() and Audits.issueAdded synchronously flush pending
  // stylesheet updates as in a live CDP session.
  InspectorCSSAgent* CreateAttachedInspectorCSSAgent() {
    CHECK(!attached_agent_);
    attached_agent_ = CreateInspectorCSSAgent();
    LocalFrame* frame = GetDocument().GetFrame();
    channel_.SetCSSAgent(attached_agent_);
    attached_agent_->Init(frame->GetProbeSink(), &dispatcher_, &session_state_,
                          V8SessionHolder(), ClientIsTrusted(true));
    frame->GetProbeSink()->AddInspectorCSSAgent(attached_agent_);

    audits_agent_ = MakeGarbageCollected<InspectorAuditsAgent>(
        attached_agent_->network_agent_,
        &frame->GetPage()->GetInspectorIssueStorage(),
        attached_agent_->inspected_frames_,
        /*web_autofill_client=*/nullptr);
    audits_agent_->Init(frame->GetProbeSink(), &dispatcher_, &session_state_,
                        V8SessionHolder(), ClientIsTrusted(true));
    audits_agent_->enable();
    return attached_agent_;
  }

  // Sets up an attached agent with a 9-document map (4 invalidated) and a
  // `"Reentrant"` font that calls FontsUpdated() during EnsureComputedStyle().
  InspectorCSSAgent* SetUpEnsureComputedStyleReentrancy() {
    InspectorCSSAgent* agent = CreateAttachedInspectorCSSAgent();
    SeedMapForShrinkOnFlush(agent);
    // An unsupported `.eot` source leaves `sources_` empty without triggering a
    // network/local font load or a CSS src parse error, so our custom source
    // runs directly on lookup.
    auto* font_face = FontFace::Create(
        GetDocument().GetExecutionContext(), AtomicString("Reentrant"),
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            String("url('https://example.invalid/font.eot')")),
        FontFaceDescriptors::Create());
    font_face->CssFontFace()->AddSource(
        MakeGarbageCollected<ReentrantFontFaceSource>(agent));
    GetDocument()
        .GetStyleEngine()
        .GetFontSelector()
        ->GetFontFaceCache()
        ->AddFontFace(font_face, /*css_connected=*/false);
    return agent;
  }

  // Sets up an attached agent with a 9-document map (4 invalidated) and unbound
  // stylesheets so the first BindStyleSheet() parses the sheet and triggers
  // Audits.issueAdded -> FlushPendingProtocolNotifications().
  InspectorCSSAgent* SetUpBindStyleSheetReentrancy() {
    InspectorCSSAgent* agent = CreateAttachedInspectorCSSAgent();
    agent->css_style_sheet_to_inspector_style_sheet_.clear();
    agent->id_to_inspector_style_sheet_.clear();
    SeedMapForShrinkOnFlush(agent);
    return agent;
  }

  // Seeds 8 dummy documents alongside `&GetDocument()` (9 keys -> 32 buckets)
  // and marks 4 invalidated so FlushPendingProtocolNotifications() erases them
  // (9 -> 5 keys -> 16 buckets), triggering a rehash.
  void SeedMapForShrinkOnFlush(InspectorCSSAgent* agent) {
    for (int i = 0; i < 8; ++i) {
      Document* doc =
          Document::CreateForTest(*GetDocument().GetExecutionContext());
      agent->document_to_css_style_sheets_.Set(
          doc, MakeGarbageCollected<GCedHeapHashSet<Member<CSSStyleSheet>>>());
      if (i < 4) {
        agent->ActiveStyleSheetsUpdated(doc);
      }
    }
  }

  void ExpectFlushedAndShrunk(const InspectorCSSAgent* agent) {
    EXPECT_TRUE(agent->invalidated_documents_.empty());
    EXPECT_EQ(5u, agent->document_to_css_style_sheets_.size());
  }

  auto CustomPropertiesForNode(InspectorCSSAgent* agent, Element* element) {
    return agent->CustomPropertiesForNode(element);
  }
  AtRules FontAtRulesForNodes(InspectorCSSAgent* agent,
                              HeapVector<Member<Element>>& elements) {
    return agent->FontAtRulesForNodes(elements);
  }

 private:
  FlushingFrontendChannel channel_;
  protocol::UberDispatcher dispatcher_{&channel_};
  InspectorSessionState session_state_{
      mojom::blink::DevToolsSessionState::New()};
  Persistent<InspectorCSSAgent> attached_agent_;
  Persistent<InspectorAuditsAgent> audits_agent_;
};

TEST_F(InspectorCSSAgentTest, NoFunctions) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #e { width: 1px; }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_TRUE(function_rules.empty());
}

TEST_F(InspectorCSSAgentTest, UnreferencedFunction) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { width: 1px; }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_TRUE(function_rules.empty());
}

TEST_F(InspectorCSSAgentTest, ElementSpecificFunctionReferences) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e1 { width: 1px; }
      #e2 { width: --a(); }
    </style>
    <div id=e1></div>
    <div id=e2></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      e1_function_rules = CollectReferencedFunctionRules("#e1");
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      e2_function_rules = CollectReferencedFunctionRules("#e2");
  EXPECT_TRUE(e1_function_rules.empty());
  EXPECT_EQ(1u, e2_function_rules.size());
  EXPECT_TRUE(FindFunctionRule(e2_function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, MultipleFunctions_Declaration) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @function --b() { result: 2px; }
      @function --c() { result: 1000px; }
      #e { width: calc(--a() + --b()); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(2u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--c"));
}

TEST_F(InspectorCSSAgentTest, KeyNameVsFunctionName) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @function --b() { result: 2px; }
      @function --c() { result: 3px; }
      #e { width: calc(--a() + --b() + --c()); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  // Check that the names held by the keys correspond to names held
  // by the values:
  EXPECT_EQ(3u, function_rules.size());
  ASSERT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_EQ("--a", FindFunctionRule(function_rules, "--a")->name());
  ASSERT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_EQ("--b", FindFunctionRule(function_rules, "--b")->name());
  ASSERT_TRUE(FindFunctionRule(function_rules, "--c"));
  EXPECT_EQ("--c", FindFunctionRule(function_rules, "--c")->name());
}

TEST_F(InspectorCSSAgentTest, MultipleFunctions_Rules) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @function --b() { result: 2px; }
      @function --c() { result: 1000px; }
      #e { width: --a(); }
      #e { height: --b(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(2u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--c"));
}

TEST_F(InspectorCSSAgentTest, FunctionsInShorthand) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { padding: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionInMedia) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      @media (width) {
        #e { width: --a(); }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionNested) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e {
        & {
          width: --a();
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, TransitiveFunction) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() {
        result: --b();
      }
      @function --b() {
        result: 2px;
      }
      @function --c() {
        result: 1000px;
      }
      #e { width: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(2u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--c"));
}

TEST_F(InspectorCSSAgentTest, TransitiveFunctionBranches) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() {
        @media (width > 0px) {
          --x: --b();
        }
        @media (width < -9000px) {
          /* Branch not taken, but referenced functions are still relevant. */
          --x: --c();
        }
        result: var(--x);
      }
      @function --b() {
        result: 2px;
      }
      @function --c() {
        result: 3px;
      }
      @function --d() {
        result: 3px;
      }
      #e { width: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(3u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--b"));
  EXPECT_TRUE(FindFunctionRule(function_rules, "--c"));
  EXPECT_FALSE(FindFunctionRule(function_rules, "--d"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionDedup) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { left: --a(); }
      div { top: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  // There should only be one entry, despite --a() appearing twice.
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
}

TEST_F(InspectorCSSAgentTest, DashedFunctionUnknown) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() { result: 1px; }
      #e { left: --unknown(); right: --a(); }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#e");
  EXPECT_EQ(1u, function_rules.size());
  EXPECT_TRUE(FindFunctionRule(function_rules, "--a"));
  // The presence of a reference to a function that does not exist
  // should not cause a crash.
}

TEST_F(InspectorCSSAgentTest, SameFunctionNamesAcrossShadows) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @function --a() {
        result: 10px;
      }
      @function --b() {
        result: --a(); /* Outer --a() */
      }
    </style>
    <div id=host>
      <template shadowrootmode=open>
        <style>
          @function --a() {
            result: --b();
          }
          :host {
             width: --a(); /* Inner --a() */
          }
        </style>
      </template>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  HeapHashMap<Member<const ScopedCSSName>, Member<CSSFunctionRule>>
      function_rules = CollectReferencedFunctionRules("#host");
  EXPECT_EQ(3u, function_rules.size());

  Element* host = GetElementById("host");
  ASSERT_TRUE(host);
  const TreeScope* document_scope = &GetDocument();
  const TreeScope* inner_scope = host->GetShadowRoot();
  ASSERT_TRUE(inner_scope);

  CSSFunctionRule* inner_a = FindFunctionRule(
      function_rules,
      MakeGarbageCollected<ScopedCSSName>(AtomicString("--a"), inner_scope));
  CSSFunctionRule* outer_a = FindFunctionRule(
      function_rules,
      MakeGarbageCollected<ScopedCSSName>(AtomicString("--a"), document_scope));
  CSSFunctionRule* outer_b = FindFunctionRule(
      function_rules,
      MakeGarbageCollected<ScopedCSSName>(AtomicString("--b"), inner_scope));

  EXPECT_TRUE(inner_a);
  EXPECT_TRUE(outer_a);
  EXPECT_TRUE(outer_b);
  EXPECT_NE(inner_a, outer_a);

  // `function_rules` maps invocations to the corresponding invoked functions,
  // and there was no invocation of --b() in the outer scope.
  EXPECT_FALSE(FindFunctionRule(
      function_rules, MakeGarbageCollected<ScopedCSSName>(AtomicString("--b"),
                                                          document_scope)));
}

TEST_F(InspectorCSSAgentTest, GetFontFaceRule) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-face {
        font-family: ExampleFontForTest;
        src: url(//nonexistent.ttf);
      }
      #e {
        font-family: ExampleFontForTest;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFace);
  EXPECT_FALSE(rules->at(0)->getSubsection());
  EXPECT_FALSE(rules->at(0)->getName());
  EXPECT_GE(rules->at(0)->getStyle()->getCssProperties()->size(), 2u);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "font-family");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getValue(),
            "ExampleFontForTest");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getName(),
            "src");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getValue(),
            "url(//nonexistent.ttf)");
}

TEST_F(InspectorCSSAgentTest, GetFontFaceRuleNoMatch) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-face {
        font-family: ExampleFontForTest;
        src: url(//nonexistent.ttf);
      }
      #e {
        font-family: Papyrus;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(0u, rules->size());
}

TEST_F(InspectorCSSAgentTest, GetFontFaceRuleFromPseudoElement) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-face {
        font-family: ExampleFontForTest;
        src: url(//nonexistent.ttf);
      }
      #e {
        font-family: Papyrus;
      }
      #e::before {
        content: "before";
        font-family: ExampleFontForTest;
      }
      #e::after {
        content: "after";
        font-family: ExampleFontForTest;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules(
      "#e", {PseudoId::kPseudoIdBefore, PseudoId::kPseudoIdAfter});
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFace);
  EXPECT_FALSE(rules->at(0)->getSubsection());
  EXPECT_FALSE(rules->at(0)->getName());
  EXPECT_GE(rules->at(0)->getStyle()->getCssProperties()->size(), 2u);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "font-family");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getValue(),
            "ExampleFontForTest");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getName(),
            "src");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getValue(),
            "url(//nonexistent.ttf)");
}

TEST_F(InspectorCSSAgentTest, GetFontPaletteValuesRule) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-palette-values --palette {
        font-family: Bixa;
        override-colors: 0 red;
      }
      #e {
        font-family: Bixa;
        font-palette: --palette;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontPaletteValues);
  EXPECT_FALSE(rules->at(0)->getSubsection());
  EXPECT_TRUE(rules->at(0)->getName());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "--palette");
  // The backend currently reports these twice, once with source info and once
  // without.
  EXPECT_GE(rules->at(0)->getStyle()->getCssProperties()->size(), 2u);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "font-family");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getValue(),
            "Bixa");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getName(),
            "override-colors");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getValue(),
            "0 red");
}

TEST_F(InspectorCSSAgentTest, GetCounterStyleRuleSimple) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @counter-style --my-style {
        system: cyclic;
        symbols: 'a' 'b' 'c';
      }
      #e {
        display: list-item;
        list-style-type: --my-style;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectCounterAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::CounterStyle);
  EXPECT_TRUE(rules->at(0)->getName());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "--my-style");
  EXPECT_GE(rules->at(0)->getStyle()->getCssProperties()->size(), 2u);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "system");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getValue(),
            "cyclic");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getName(),
            "symbols");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getValue(),
            "'a' 'b' 'c'");
}

TEST_F(InspectorCSSAgentTest, GetCounterStyleRuleTwoFallbacks) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @counter-style style-b {
        system: cyclic;
        symbols: 'x' 'y';
      }
      @counter-style style-a {
        system: cyclic;
        symbols: 'a' 'b';
        fallback: style-b;
      }
      #e {
        display: list-item;
        list-style-type: style-a;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectCounterAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(2u, rules->size());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "style-a");
  EXPECT_EQ(rules->at(1)->getName()->getText(), "style-b");
}

TEST_F(InspectorCSSAgentTest, GetCounterStyleRuleCycle) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @counter-style --my-style {
        system: cyclic;
        symbols: 'a' 'b';
        fallback: --my-style;
      }
      #e {
        display: list-item;
        list-style-type: --my-style;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectCounterAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "--my-style");
}

TEST_F(InspectorCSSAgentTest, GetCounterStyleRuleNonExistentFallback) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @counter-style --my-style {
        system: cyclic;
        symbols: 'a' 'b';
        fallback: --non-existent-style;
      }
      #e {
        display: list-item;
        list-style-type: --my-style;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectCounterAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "--my-style");
}

TEST_F(InspectorCSSAgentTest, GetCounterStyleRuleBuiltInFallback) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @counter-style --my-style {
        system: cyclic;
        symbols: 'a' 'b';
        fallback: decimal;
      }
      #e {
        display: list-item;
        list-style-type: --my-style;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectCounterAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "--my-style");
}

TEST_F(InspectorCSSAgentTest, GetCounterStyleRuleSymbolsFunctionHasNoAtRule) {
  ScopedCSSCounterStyleSymbolsFunctionForTest scoped_feature(true);
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      #e {
        display: list-item;
        list-style-type: symbols('a' 'b' 'c');
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  // An anonymous symbols() counter style has no name, so there is no
  // corresponding @counter-style at-rule to report to DevTools.
  AtRules rules = CollectCounterAtRules("#e");
  EXPECT_FALSE(rules);
}

TEST_F(InspectorCSSAgentTest, GetFontPaletteValuesRuleNoMatchPalette) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-palette-values --palette {
        font-family: Bixa;
        override-colors: 0 red;
      }
      #e {
        font-family: Bixa;
        font-palette: --other-palette;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(0u, rules->size());
}

TEST_F(InspectorCSSAgentTest, GetFontPaletteValuesRuleNoMatchFont) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-palette-values --palette {
        font-family: Bixa;
        override-colors: 0 red;
      }
      #e {
        font-family: Papyrus;
        font-palette: --palette;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(0u, rules->size());
}

TEST_F(InspectorCSSAgentTest, GetFontPaletteValuesRuleFromPseudoElement) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-palette-values --palette {
        font-family: Bixa;
        override-colors: 0 red;
      }
      #e {
        font-family: Bixa;
      }
      #e::before {
        content: "before";
        font-palette: --palette;
      }
      #e::after {
        content: "after";
        font-palette: --palette;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules(
      "#e", {PseudoId::kPseudoIdBefore, PseudoId::kPseudoIdAfter});
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontPaletteValues);
  EXPECT_FALSE(rules->at(0)->getSubsection());
  EXPECT_TRUE(rules->at(0)->getName());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "--palette");
  // The backend currently reports these twice, once with source info and once
  // without.
  EXPECT_GE(rules->at(0)->getStyle()->getCssProperties()->size(), 2u);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "font-family");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getValue(),
            "Bixa");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getName(),
            "override-colors");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(1)->getValue(),
            "0 red");
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleSwash) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @swash {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: swash(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Swash);
  EXPECT_TRUE(rules->at(0)->getName());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "Bixa");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "fancy");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getValue(),
            "1");
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleNoMatchFont) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @swash {
          fancy: 1;
        }
      }
      #e {
        font-family: Papyrus;
        font-variant-alternates: swash(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(0u, rules->size());
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleNoMatchFeature) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @swash {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: styleset(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(0u, rules->size());
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleAnnotation) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @annotation {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: annotation(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Annotation);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleOrnaments) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @ornaments {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: ornaments(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Ornaments);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleStylistic) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @stylistic {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: stylistic(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Stylistic);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleStyleset) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @styleset {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: styleset(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Styleset);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleCharacterVariant) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @character-variant {
          fancy: 1;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: character-variant(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::CharacterVariant);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleMultipleFamilies) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa, Papyrus {
        @character-variant {
          fancy: 1;
        }
      }
      #e {
        font-family: Papyrus;
        font-variant-alternates: character-variant(fancy);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
  EXPECT_EQ(rules->at(0)->getType(),
            protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::CharacterVariant);
  EXPECT_TRUE(rules->at(0)->getName());
  EXPECT_EQ(rules->at(0)->getName()->getText(), "Bixa, Papyrus");
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->size(), 1u);
}

TEST_F(InspectorCSSAgentTest, GetFontFeatureValuesRuleMultipleFeatures) {
  GetDocument().body()->SetHTMLUnsafeWithoutTrustedTypes(R"HTML(
    <style>
      @font-feature-values Bixa {
        @swash {
          fancy: 1;
        }
        @ornaments {
          nope: 3;
        }
      }
      @font-feature-values Bixa {
        @swash {
          swishy: 4;
        }
        @annotation {
          extra: 5;
        }
      }
      #e {
        font-family: Bixa;
        font-variant-alternates: swash(swishy) historical-forms annotation(extra) character-variant(oops);
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  AtRules rules = CollectFontAtRules("#e");
  EXPECT_TRUE(rules);
  EXPECT_EQ(3u, rules->size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(rules->at(i)->getType(),
              protocol::CSS::CSSAtRule::TypeEnum::FontFeatureValues);
    EXPECT_TRUE(rules->at(i)->getName());
    EXPECT_EQ(rules->at(i)->getName()->getText(), "Bixa");
    EXPECT_EQ(rules->at(i)->getStyle()->getCssProperties()->size(), 1u);
  }
  EXPECT_EQ(rules->at(0)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Swash);
  EXPECT_EQ(rules->at(0)->getStyle()->getCssProperties()->at(0)->getName(),
            "fancy");
  EXPECT_EQ(rules->at(1)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Swash);
  EXPECT_EQ(rules->at(1)->getStyle()->getCssProperties()->at(0)->getName(),
            "swishy");
  EXPECT_EQ(rules->at(2)->getSubsection(),
            protocol::CSS::CSSAtRule::SubsectionEnum::Annotation);
  EXPECT_EQ(rules->at(2)->getStyle()->getCssProperties()->at(0)->getName(),
            "extra");
}

TEST_F(InspectorCSSAgentTest, BuildSimpleRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #e {
        color: red;
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  EXPECT_FALSE(rule->getNestingSelectors());
  EXPECT_EQ(rule->getRuleTypes()->size(), 0u);
  EXPECT_EQ(rule->getMedia()->size(), 0u);
  EXPECT_EQ(rule->getSupports()->size(), 0u);
  EXPECT_EQ(rule->getContainerQueries()->size(), 0u);
  EXPECT_EQ(rule->getScopes()->size(), 0u);
  EXPECT_EQ(rule->getLayers()->size(), 0u);
  EXPECT_EQ(rule->getStartingStyles()->size(), 0u);
}

TEST_F(InspectorCSSAgentTest, BuildNestedStyleRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      div {
        #e {
          color: red;
        }
      }
    </style>
    <div><div id=e></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "& #e");
  ASSERT_TRUE(rule->getNestingSelectors());
  EXPECT_EQ(rule->getNestingSelectors()->size(), 1u);
  EXPECT_EQ(rule->getNestingSelectors()->at(0), "div");
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::StyleRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedMediaRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @media screen and (min-width: 1px) {
        #e {
          color: red;
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getMedia()->size(), 1u);
  EXPECT_EQ(rule->getMedia()->at(0)->getText(), "screen and (min-width: 1px)");
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::MediaRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedSupportsRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @supports (display: grid) {
        #e {
          color: red;
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getSupports()->size(), 1u);
  EXPECT_EQ(rule->getSupports()->at(0)->getText(), "(display: grid)");
  EXPECT_TRUE(rule->getSupports()->at(0)->getActive());
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::SupportsRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedContainerRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @container (min-width: 1px) {
        #e {
          color: red;
        }
      }
    </style>
    <div style="container-type: inline-size;"><div id=e></div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getContainerQueries()->size(), 1u);
  EXPECT_EQ(rule->getContainerQueries()->at(0)->getText(), "(min-width: 1px)");
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::ContainerRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedLayerRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @layer my-layer {
        #e {
          color: red;
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getLayers()->size(), 1u);
  EXPECT_EQ(rule->getLayers()->at(0)->getText(), "my-layer");
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::LayerRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedScopeRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @scope (.scope) {
        #e {
          color: red;
        }
      }
    </style>
    <div class=scope>
        <div id=e></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getScopes()->size(), 1u);
  EXPECT_EQ(rule->getScopes()->at(0)->getText(), "(.scope)");
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::ScopeRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedStartingStyleRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @starting-style {
        #e {
          color: red;
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getStartingStyles()->size(), 1u);
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::StartingStyleRule);
}

TEST_F(InspectorCSSAgentTest, BuildNestedNavigationRule) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      @navigation not (at: url-pattern("http:*xxx*")) {
        #e {
          color: red;
        }
      }
    </style>
    <div id=e></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  std::unique_ptr<protocol::CSS::CSSRule> rule =
      BuildObjectForInnermostRule("#e");
  EXPECT_TRUE(rule);
  EXPECT_EQ(rule->getSelectorList()->getText(), "#e");
  ASSERT_EQ(rule->getNavigations()->size(), 1u);
  EXPECT_EQ(rule->getNavigations()->at(0)->getText(),
            "not (at: url-pattern(\"http:*xxx*\"))");
  EXPECT_TRUE(rule->getNavigations()->at(0)->getActive());
  ASSERT_EQ(rule->getRuleTypes()->size(), 1u);
  EXPECT_EQ(rule->getRuleTypes()->at(0),
            protocol::CSS::CSSRuleTypeEnum::NavigationRule);
}

const CSSPropertyID DirectionAwareConverterTestData[] = {
    // clang-format off
  CSSPropertyID::kWidth,
  CSSPropertyID::kHeight,
  CSSPropertyID::kMarginLeft,
  CSSPropertyID::kMarginTop,
  CSSPropertyID::kMarginRight,
  CSSPropertyID::kMarginBottom,
  CSSPropertyID::kPaddingLeft,
  CSSPropertyID::kPaddingTop,
  CSSPropertyID::kPaddingRight,
  CSSPropertyID::kPaddingBottom,
  CSSPropertyID::kLeft,
  CSSPropertyID::kTop,
  CSSPropertyID::kRight,
  CSSPropertyID::kBottom,
    // clang-format on
};

class PercentageResolutionTest
    : public InspectorCSSAgentTest,
      public testing::WithParamInterface<CSSPropertyID> {};

INSTANTIATE_TEST_SUITE_P(InspectorCSSAgentTest,
                         PercentageResolutionTest,
                         testing::ValuesIn(DirectionAwareConverterTestData));

TEST_P(PercentageResolutionTest, ResolvePercentagesSimple) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #outer {
        width: 100px;
        height: 300px;
      }
    </style>
    <div id=outer>
    </div>
  )HTML");

  String value("calc(1px + 10%)");

  CSSPropertyID property_id = GetParam();
  AtomicString property_name =
      CSSProperty::Get(property_id).GetCSSPropertyName().ToAtomicString();

  StringBuilder html_string;
  html_string.Append("<div id=inner style=\"position: absolute; ");
  html_string.Append(property_name);
  html_string.Append(": ");
  html_string.Append(value);
  html_string.Append(";\"></div>");
  GetElementById("outer")->SetInnerHTMLWithoutTrustedTypes(
      html_string.ToString());

  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("inner");
  String expected = GetComputedStyle(element, property_id);
  String actual = InspectorResolvePercentageValues(element, property_id, value);
  EXPECT_EQ(actual, expected);
}

TEST_P(PercentageResolutionTest, ResolvePercentagesEffectiveZoom) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #outer {
        width: 100px;
        height: 300px;
        zoom: 3;
      }
    </style>
    <div id=outer>
    </div>
  )HTML");

  String value("10%");

  CSSPropertyID property_id = GetParam();
  AtomicString property_name =
      CSSProperty::Get(property_id).GetCSSPropertyName().ToAtomicString();

  StringBuilder html_string;
  html_string.Append("<div id=inner style=\"position: absolute; ");
  html_string.Append(property_name);
  html_string.Append(": ");
  html_string.Append(value);
  html_string.Append(";\"></div>");
  GetElementById("outer")->SetInnerHTMLWithoutTrustedTypes(
      html_string.ToString());

  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("inner");
  String expected = GetComputedStyle(element, property_id);
  String actual = InspectorResolvePercentageValues(element, property_id, value);
  EXPECT_EQ(actual, expected);
}

TEST_F(InspectorCSSAgentTest, ResolvePercentagesSizingProperties) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #outer {
        width: 100px;
        height: 300px
      }
      #inner {
        width: calc(1px + 10%);
        height: calc(1px + 10%);
      }
    </style>
    <div id=outer>
      <div id=inner></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("inner");
  String value("calc(1px + 10%)");

  String expected_inline = GetComputedStyle(element, CSSPropertyID::kWidth);
  String expected_block = GetComputedStyle(element, CSSPropertyID::kHeight);

  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMinWidth,
                                             value),
            expected_inline);
  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMaxWidth,
                                             value),
            expected_inline);
  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMinHeight,
                                             value),
            expected_block);
  EXPECT_EQ(InspectorResolvePercentageValues(element, CSSPropertyID::kMaxHeight,
                                             value),
            expected_block);
}

TEST_F(InspectorCSSAgentTest, ResolvePercentagesAnchorPositioning) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #cb {
        width: 300px;
        height: 150px;
        position: relative;
      }
      #anchor {
        position: absolute;
        left: 50px;
        top: 20px;
        anchor-name: --a;
        width: 50px;
        height: 50px;
      }
      #anchored {
        position-anchor: --a;
        position-area: center right;
        position: absolute;
        width: 10%;
        height: 10%;
      }
    </style>
    <div id=cb>
      <div id=anchor>
        Anchor
      </div>
      <div id=anchored>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("anchored");
  String value("10%");

  String expected_inline = GetComputedStyle(element, CSSPropertyID::kWidth);
  String expected_block = GetComputedStyle(element, CSSPropertyID::kHeight);

  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kWidth, value),
      expected_inline);
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kWidth, value),
      "20px");
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kHeight, value),
      expected_block);
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kHeight, value),
      "5px");
}

TEST_F(InspectorCSSAgentTest, ResolvePercentagesDisplayTable) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .table {
        display: table;
        width: 100px;
        height: 300px;
      }
      #column1 {
        display: table-column;
      }
      #column2 {
        display: table-column;
      }
      .row {
        display: table-row;
      }
      .cell {
        display: table-cell;
      }
    </style>

    <div class="table">
        <div id="column1"></div>
        <div id="column2"></div>
        <div class="row">
          <div class="cell">row 1, cell 1</div>
          <div class="cell">row 1, cell 2</div>
        </div>
        <div class="row">
          <div class="cell">row 2, cell 1</div>
          <div class="cell">row 2, cell 2</div>
        </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* element = GetElementById("column1");
  String value("10%");

  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kWidth, value),
      value);
  EXPECT_EQ(
      InspectorResolvePercentageValues(element, CSSPropertyID::kHeight, value),
      value);
}

TEST_F(InspectorCSSAgentTest, TakeCoverageDeltaNestedStyleRules) {
  InspectorCSSAgent* agent = CreateInspectorCSSAgent();
  agent->SetCoverageEnabled(true);

  String css = R"CSS(#parent1 {
  color: red;
  .unused { color: yellow; }
  .used1 { color: blue; }
  .used2 { color: green; }
  font-size: 12px;
}
#parent2 {
  .unused { color: pink; }
  .used { color: cyan; }
})CSS";
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<style>" + css +
                                                        R"HTML(</style>
    <div id="parent1">
      <span class="used1"></span>
      <span class="used2"></span>
    </div>
    <div id="parent2">
      <span class="used"></span>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  std::unique_ptr<protocol::Array<protocol::CSS::RuleUsage>> coverage =
      StopRuleUsageTracking(agent);
  ASSERT_TRUE(coverage);

  std::vector<std::pair<int, int>> ranges;
  for (const std::unique_ptr<protocol::CSS::RuleUsage>& rule : *coverage) {
    EXPECT_TRUE(rule->getUsed());
    ranges.emplace_back(rule->getStartOffset(), rule->getEndOffset());
  }
  std::sort(ranges.begin(), ranges.end());

  Vector<String> used_rule_texts;
  for (const auto& [start, end] : ranges) {
    used_rule_texts.push_back(css.substr(start, end - start));
  }

  Vector<String> expected = {
      "#parent1 {\n  color: red;",
      ".used1 { color: blue; }",
      ".used2 { color: green; }",
      "font-size: 12px;",
      "#parent2 ",
      ".used { color: cyan; }",
  };
  EXPECT_EQ(used_rule_texts, expected);
}

TEST_F(InspectorCSSAgentTest,
       CustomPropertiesForNodeReentrancyDuringEnsureComputedStyle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @property --x { syntax: "<number>"; inherits: false; initial-value: 0; }
      #scope { --x: 1; font-family: Reentrant; width: 10ch; }
    </style>
    <div style="display: none"><div id=scope></div></div>
  )HTML");
  InspectorCSSAgent* agent = SetUpEnsureComputedStyleReentrancy();
  Element* scope = GetElementById("scope");
  ASSERT_FALSE(scope->GetComputedStyle());

  auto [property_rules, registrations] = CustomPropertiesForNode(agent, scope);

  ExpectFlushedAndShrunk(agent);
  ASSERT_EQ(1u, property_rules->size());
  EXPECT_EQ("--x", property_rules->at(0)->getPropertyName()->getText());
  EXPECT_TRUE(registrations->empty());
}

TEST_F(InspectorCSSAgentTest,
       CustomPropertiesForNodeReentrancyDuringBindStyleSheet) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @property invalid {}
      @property --a { syntax: "<number>"; inherits: false; initial-value: 0; }
      @property --b { syntax: "<number>"; inherits: false; initial-value: 0; }
      #scope { --a: 1; --b: 2; }
    </style>
    <div id=scope></div>
  )HTML");
  InspectorCSSAgent* agent = SetUpBindStyleSheetReentrancy();

  auto [property_rules, registrations] =
      CustomPropertiesForNode(agent, GetElementById("scope"));

  ExpectFlushedAndShrunk(agent);
  EXPECT_EQ(2u, property_rules->size());
  EXPECT_TRUE(registrations->empty());
}

TEST_F(InspectorCSSAgentTest,
       FontAtRulesForNodesReentrancyDuringEnsureComputedStyle) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @font-face {
        font-family: CustomFont;
        src: url('https://example.invalid/font.eot');
      }
      #styled { font-family: CustomFont; }
      #scope { font-family: Reentrant, CustomFont; width: 10ch; }
    </style>
    <div id=styled></div>
    <div style="display: none"><div id=scope></div></div>
  )HTML");
  InspectorCSSAgent* agent = SetUpEnsureComputedStyleReentrancy();
  Element* scope = GetElementById("scope");
  ASSERT_FALSE(scope->GetComputedStyle());
  HeapVector<Member<Element>> elements = {GetElementById("styled"), scope};

  AtRules rules = FontAtRulesForNodes(agent, elements);

  ExpectFlushedAndShrunk(agent);
  ASSERT_TRUE(rules);
  EXPECT_EQ(1u, rules->size());
}

TEST_F(InspectorCSSAgentTest,
       FontAtRulesForNodesReentrancyDuringBindStyleSheet) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @property invalid {}
      @font-palette-values --pal { font-family: CustomFont; base-palette: 0; }
      @font-feature-values CustomFont { @swash { fancy: 1; } }
      #scope {
        font-family: CustomFont;
        font-palette: --pal;
        font-variant-alternates: swash(fancy);
      }
    </style>
    <div id=scope></div>
  )HTML");
  InspectorCSSAgent* agent = SetUpBindStyleSheetReentrancy();
  HeapVector<Member<Element>> elements = {GetElementById("scope")};

  AtRules rules = FontAtRulesForNodes(agent, elements);

  ExpectFlushedAndShrunk(agent);
  ASSERT_TRUE(rules);
  EXPECT_EQ(2u, rules->size());
}

}  // namespace blink
