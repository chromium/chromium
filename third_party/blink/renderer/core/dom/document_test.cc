/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/document.h"

#include <algorithm>
#include <memory>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/css/media_query_list_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/scripted_animation_controller.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "url/url_util.h"

namespace blink {

using network::mojom::ContentSecurityPolicySource;
using network::mojom::ContentSecurityPolicyType;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

class DocumentTest : public PageTestBase {
 public:
  static void SimulateTrustTokenQueryAnswererConnectionError(
      Document* document) {
    document->TrustTokenQueryAnswererConnectionError();
  }

 protected:
  void TearDown() override {
    ThreadState::Current()->CollectAllGarbageForTesting();
    PageTestBase::TearDown();
  }

  void SetHtmlInnerHTML(const char*);

  // Note: callers must mock any urls that are referred to in `html_content`,
  // with the exception of foo.html, which can be assumed to be defined by this
  // function.
  // Note: callers must not use double-quotes in the `html_content` string,
  // since that will conflict with the srcdoc attribute assignment in the
  // javascript below.
  enum SandboxState { kIsSandboxed, kIsNotSandboxed };
  enum UseCountedExpectation { kIsUseCounted, kIsNotUseCounted };
  void NavigateSrcdocMaybeSandboxed(
      const String& base_url,
      const std::string& html_content,
      const SandboxState sandbox_state,
      const UseCountedExpectation use_counted_expectation) {
    WebURL mocked_mainframe_url =
        url_test_helpers::RegisterMockedURLLoadFromBase(
            base_url, test::CoreTestDataPath(),
            WebString::FromUTF8("foo.html"));

    frame_test_helpers::WebViewHelper web_view_helper;
    // Load a non-about:blank simple mainframe page.
    web_view_helper.InitializeAndLoad(mocked_mainframe_url.GetString().Utf8());

    WebLocalFrame* main_frame = web_view_helper.LocalMainFrame();
    const char js_template[] =
        R"( javascript:
            var frm = document.createElement('iframe');
            %s
            frm.srcdoc = "%s";
            document.body.appendChild(frm);
        )";
    frame_test_helpers::LoadFrame(
        main_frame,
        base::StringPrintf(
            js_template,
            sandbox_state == kIsSandboxed ? "frm.sandbox = '';" : "",
            html_content.c_str()));
    EXPECT_NE(nullptr, main_frame->FirstChild());
    WebLocalFrame* iframe = main_frame->FirstChild()->ToWebLocalFrame();

    Document* srcdoc_document = iframe->GetDocument();
    KURL url("about:srcdoc");
    EXPECT_EQ(url, srcdoc_document->Url());
    switch (use_counted_expectation) {
      case kIsUseCounted:
        EXPECT_TRUE(srcdoc_document->IsUseCounted(
            WebFeature::kSandboxedSrcdocFrameResolvesRelativeURL));
        break;
      case kIsNotUseCounted:
        EXPECT_FALSE(srcdoc_document->IsUseCounted(
            WebFeature::kSandboxedSrcdocFrameResolvesRelativeURL));
    }
    url_test_helpers::RegisterMockedURLUnregister(mocked_mainframe_url);
  }

  void NavigateWithSandbox(const KURL& url) {
    auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
    MockPolicyContainerHost mock_policy_container_host;
    params->policy_container = std::make_unique<blink::WebPolicyContainer>(
        blink::WebPolicyContainerPolicies(),
        mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
    params->policy_container->policies.sandbox_flags =
        network::mojom::blink::WebSandboxFlags::kAll;
    GetFrame().Loader().CommitNavigation(std::move(params),
                                         /*extra_data=*/nullptr);
    test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }
};

void DocumentTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

class DocumentSimTest : public SimTest {};

namespace {

class TestSynchronousMutationObserver
    : public GarbageCollected<TestSynchronousMutationObserver>,
      public SynchronousMutationObserver {
 public:
  struct MergeTextNodesRecord : GarbageCollected<MergeTextNodesRecord> {
    Member<const Text> node_;
    Member<Node> node_to_be_removed_;
    unsigned offset_ = 0;

    MergeTextNodesRecord(const Text* node,
                         const NodeWithIndex& node_with_index,
                         unsigned offset)
        : node_(node),
          node_to_be_removed_(node_with_index.GetNode()),
          offset_(offset) {}

    void Trace(Visitor* visitor) const {
      visitor->Trace(node_);
      visitor->Trace(node_to_be_removed_);
    }
  };

  struct UpdateCharacterDataRecord
      : GarbageCollected<UpdateCharacterDataRecord> {
    Member<CharacterData> node_;
    unsigned offset_ = 0;
    unsigned old_length_ = 0;
    unsigned new_length_ = 0;

    UpdateCharacterDataRecord(CharacterData* node,
                              unsigned offset,
                              unsigned old_length,
                              unsigned new_length)
        : node_(node),
          offset_(offset),
          old_length_(old_length),
          new_length_(new_length) {}

    void Trace(Visitor* visitor) const { visitor->Trace(node_); }
  };

  explicit TestSynchronousMutationObserver(Document&);
  TestSynchronousMutationObserver(const TestSynchronousMutationObserver&) =
      delete;
  TestSynchronousMutationObserver& operator=(
      const TestSynchronousMutationObserver&) = delete;
  virtual ~TestSynchronousMutationObserver() = default;

  int CountContextDestroyedCalled() const {
    return on_document_shutdown_called_counter_;
  }

  const HeapVector<Member<const ContainerNode>>& ChildrenChangedNodes() const {
    return children_changed_nodes_;
  }

  const HeapVector<Member<MergeTextNodesRecord>>& MergeTextNodesRecords()
      const {
    return merge_text_nodes_records_;
  }

  const HeapVector<Member<const Node>>& MoveTreeToNewDocumentNodes() const {
    return move_tree_to_new_document_nodes_;
  }

  const HeapVector<Member<ContainerNode>>& RemovedChildrenNodes() const {
    return removed_children_nodes_;
  }

  const HeapVector<Member<Node>>& RemovedNodes() const {
    return removed_nodes_;
  }

  const HeapVector<Member<const Text>>& SplitTextNodes() const {
    return split_text_nodes_;
  }

  const HeapVector<Member<UpdateCharacterDataRecord>>&
  UpdatedCharacterDataRecords() const {
    return updated_character_data_records_;
  }

  void Trace(Visitor*) const override;

 private:
  // Implement |SynchronousMutationObserver| member functions.
  void ContextDestroyed() final;
  void DidChangeChildren(const ContainerNode&,
                         const ContainerNode::ChildrenChange&) final;
  void DidMergeTextNodes(const Text&, const NodeWithIndex&, unsigned) final;
  void DidMoveTreeToNewDocument(const Node& root) final;
  void DidSplitTextNode(const Text&) final;
  void DidUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned old_length,
                              unsigned new_length) final;
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node&) final;

  int on_document_shutdown_called_counter_ = 0;
  HeapVector<Member<const ContainerNode>> children_changed_nodes_;
  HeapVector<Member<MergeTextNodesRecord>> merge_text_nodes_records_;
  HeapVector<Member<const Node>> move_tree_to_new_document_nodes_;
  HeapVector<Member<ContainerNode>> removed_children_nodes_;
  HeapVector<Member<Node>> removed_nodes_;
  HeapVector<Member<const Text>> split_text_nodes_;
  HeapVector<Member<UpdateCharacterDataRecord>> updated_character_data_records_;
};

TestSynchronousMutationObserver::TestSynchronousMutationObserver(
    Document& document) {
  SetDocument(&document);
}

void TestSynchronousMutationObserver::ContextDestroyed() {
  ++on_document_shutdown_called_counter_;
}

void TestSynchronousMutationObserver::DidChangeChildren(
    const ContainerNode& container,
    const ContainerNode::ChildrenChange&) {
  children_changed_nodes_.push_back(&container);
}

void TestSynchronousMutationObserver::DidMergeTextNodes(
    const Text& node,
    const NodeWithIndex& node_with_index,
    unsigned offset) {
  merge_text_nodes_records_.push_back(
      MakeGarbageCollected<MergeTextNodesRecord>(&node, node_with_index,
                                                 offset));
}

void TestSynchronousMutationObserver::DidMoveTreeToNewDocument(
    const Node& root) {
  move_tree_to_new_document_nodes_.push_back(&root);
}

void TestSynchronousMutationObserver::DidSplitTextNode(const Text& node) {
  split_text_nodes_.push_back(&node);
}

void TestSynchronousMutationObserver::DidUpdateCharacterData(
    CharacterData* character_data,
    unsigned offset,
    unsigned old_length,
    unsigned new_length) {
  updated_character_data_records_.push_back(
      MakeGarbageCollected<UpdateCharacterDataRecord>(character_data, offset,
                                                      old_length, new_length));
}

void TestSynchronousMutationObserver::NodeChildrenWillBeRemoved(
    ContainerNode& container) {
  removed_children_nodes_.push_back(&container);
}

void TestSynchronousMutationObserver::NodeWillBeRemoved(Node& node) {
  removed_nodes_.push_back(&node);
}

void TestSynchronousMutationObserver::Trace(Visitor* visitor) const {
  visitor->Trace(children_changed_nodes_);
  visitor->Trace(merge_text_nodes_records_);
  visitor->Trace(move_tree_to_new_document_nodes_);
  visitor->Trace(removed_children_nodes_);
  visitor->Trace(removed_nodes_);
  visitor->Trace(split_text_nodes_);
  visitor->Trace(updated_character_data_records_);
  SynchronousMutationObserver::Trace(visitor);
}

class MockDocumentValidationMessageClient
    : public GarbageCollected<MockDocumentValidationMessageClient>,
      public ValidationMessageClient {
 public:
  MockDocumentValidationMessageClient() { Reset(); }
  void Reset() {
    show_validation_message_was_called = false;
    document_detached_was_called = false;
  }
  bool show_validation_message_was_called;
  bool document_detached_was_called;

  // ValidationMessageClient functions.
  void ShowValidationMessage(Element& anchor,
                             const String& main_message,
                             TextDirection,
                             const String& sub_message,
                             TextDirection) override {
    show_validation_message_was_called = true;
  }
  void HideValidationMessage(const Element& anchor) override {}
  bool IsValidationMessageVisible(const Element& anchor) override {
    return true;
  }
  void DocumentDetached(const Document&) override {
    document_detached_was_called = true;
  }
  void DidChangeFocusTo(const Element*) override {}
  void WillBeDestroyed() override {}

  // virtual void Trace(Visitor* visitor) const {
  // ValidationMessageClient::trace(visitor); }
};

class PrefersColorSchemeTestListener final : public MediaQueryListListener {
 public:
  void NotifyMediaQueryChanged() override { notified_ = true; }
  bool IsNotified() const { return notified_; }

 private:
  bool notified_ = false;
};

bool IsDOMException(ScriptState* script_state,
                    ScriptValue value,
                    DOMExceptionCode code) {
  auto* dom_exception =
      V8DOMException::ToWrappable(script_state->GetIsolate(), value.V8Value());
  if (!dom_exception)
    return false;

  // Unfortunately, it's not enough to check |dom_exception->code() == code|,
  // as DOMException::code is only populated for the DOMExceptionCodes with
  // "legacy code" numeric values.
  return dom_exception->name() == DOMException(code).name();
}
}  // anonymous namespace

TEST_F(DocumentTest, CreateRangeAdjustedToTreeScopeWithPositionInShadowTree) {
  GetDocument().body()->setInnerHTML("<div><select><option>012</option></div>");
  Element* const select_element =
      GetDocument().QuerySelector(AtomicString("select"));
  const Position& position =
      Position(*select_element->UserAgentShadowRoot(),
               select_element->UserAgentShadowRoot()->CountChildren());
  Range* const range =
      Document::CreateRangeAdjustedToTreeScope(GetDocument(), position);
  EXPECT_EQ(range->startContainer(), select_element->parentNode());
  EXPECT_EQ(static_cast<unsigned>(range->startOffset()),
            select_element->NodeIndex());
  EXPECT_TRUE(range->collapsed());
}

TEST_F(DocumentTest, DomTreeVersionForRemoval) {
  // ContainerNode::CollectChildrenAndRemoveFromOldParentWithCheck assumes this
  // behavior.
  Document& doc = GetDocument();
  {
    DocumentFragment* fragment = DocumentFragment::Create(doc);
    fragment->appendChild(
        MakeGarbageCollected<Element>(html_names::kDivTag, &doc));
    fragment->appendChild(
        MakeGarbageCollected<Element>(html_names::kSpanTag, &doc));
    uint64_t original_version = doc.DomTreeVersion();
    fragment->RemoveChildren();
    EXPECT_EQ(original_version + 1, doc.DomTreeVersion())
        << "RemoveChildren() should increase DomTreeVersion by 1.";
  }

  {
    DocumentFragment* fragment = DocumentFragment::Create(doc);
    Node* child = MakeGarbageCollected<Element>(html_names::kDivTag, &doc);
    child->appendChild(
        MakeGarbageCollected<Element>(html_names::kSpanTag, &doc));
    fragment->appendChild(child);
    uint64_t original_version = doc.DomTreeVersion();
    fragment->removeChild(child);
    EXPECT_EQ(original_version + 1, doc.DomTreeVersion())
        << "removeChild() should increase DomTreeVersion by 1.";
  }
}

// This tests that we properly resize and re-layout pages for printing in the
// presence of media queries effecting elements in a subtree layout boundary
TEST_F(DocumentTest, PrintRelayout) {
  SetHtmlInnerHTML(R"HTML(
    <style>
        div {
            width: 100px;
            height: 100px;
            overflow: hidden;
        }
        span {
            width: 50px;
            height: 50px;
        }
        @media screen {
            span {
                width: 20px;
            }
        }
    </style>
    <p><div><span></span></div></p>
  )HTML");
  gfx::SizeF page_size(400, 400);
  float maximum_shrink_ratio = 1.6;

  GetDocument().GetFrame()->StartPrinting(WebPrintParams(page_size),
                                          maximum_shrink_ratio);
  EXPECT_EQ(GetDocument().documentElement()->OffsetWidth(), 400);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(GetDocument().documentElement()->OffsetWidth(), 800);
}

// This tests whether we properly set the bits for indicating if a media feature
// has been evaluated.
TEST_F(DocumentTest, MediaFeatureEvaluated) {
  GetDocument().SetMediaFeatureEvaluated(
      static_cast<int>(IdentifiableSurface::MediaFeatureName::kForcedColors));
  for (int i = 0; i < 64; i++) {
    if (i == static_cast<int>(
                 IdentifiableSurface::MediaFeatureName::kForcedColors)) {
      EXPECT_TRUE(GetDocument().WasMediaFeatureEvaluated(i));
    } else {
      EXPECT_FALSE(GetDocument().WasMediaFeatureEvaluated(i));
    }
  }
  GetDocument().SetMediaFeatureEvaluated(
      static_cast<int>(IdentifiableSurface::MediaFeatureName::kAnyHover));
  for (int i = 0; i < 64; i++) {
    if ((i == static_cast<int>(
                  IdentifiableSurface::MediaFeatureName::kForcedColors)) ||
        (i ==
         static_cast<int>(IdentifiableSurface::MediaFeatureName::kAnyHover))) {
      EXPECT_TRUE(GetDocument().WasMediaFeatureEvaluated(i));
    } else {
      EXPECT_FALSE(GetDocument().WasMediaFeatureEvaluated(i));
    }
  }
}

// This test checks that Documunt::linkManifest() returns a value conform to the
// specification.
TEST_F(DocumentTest, LinkManifest) {
  // Test the default result.
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());

  // Check that we use the first manifest with <link rel=manifest>
  auto* link = MakeGarbageCollected<HTMLLinkElement>(GetDocument(),
                                                     CreateElementFlags());
  link->setAttribute(blink::html_names::kRelAttr, AtomicString("manifest"));
  link->setAttribute(blink::html_names::kHrefAttr, AtomicString("foo.json"));
  GetDocument().head()->AppendChild(link);
  EXPECT_EQ(link, GetDocument().LinkManifest());

  auto* link2 = MakeGarbageCollected<HTMLLinkElement>(GetDocument(),
                                                      CreateElementFlags());
  link2->setAttribute(blink::html_names::kRelAttr, AtomicString("manifest"));
  link2->setAttribute(blink::html_names::kHrefAttr, AtomicString("bar.json"));
  GetDocument().head()->InsertBefore(link2, link);
  EXPECT_EQ(link2, GetDocument().LinkManifest());
  GetDocument().head()->AppendChild(link2);
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that crazy URLs are accepted.
  link->setAttribute(blink::html_names::kHrefAttr,
                     AtomicString("http:foo.json"));
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that empty URLs are accepted.
  link->setAttribute(blink::html_names::kHrefAttr, g_empty_atom);
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that URLs from different origins are accepted.
  link->setAttribute(blink::html_names::kHrefAttr,
                     AtomicString("http://example.org/manifest.json"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kHrefAttr,
                     AtomicString("http://foo.example.org/manifest.json"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kHrefAttr,
                     AtomicString("http://foo.bar/manifest.json"));
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // More than one token in @rel is accepted.
  link->setAttribute(blink::html_names::kRelAttr,
                     AtomicString("foo bar manifest"));
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Such as spaces around the token.
  link->setAttribute(blink::html_names::kRelAttr, AtomicString(" manifest "));
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that rel=manifest actually matters.
  link->setAttribute(blink::html_names::kRelAttr, g_empty_atom);
  EXPECT_EQ(link2, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kRelAttr, AtomicString("manifest"));

  // Check that link outside of the <head> are ignored.
  GetDocument().head()->RemoveChild(link);
  GetDocument().head()->RemoveChild(link2);
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());
  GetDocument().body()->AppendChild(link);
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());
  GetDocument().head()->AppendChild(link);
  GetDocument().head()->AppendChild(link2);

  // Check that some attribute values do not have an effect.
  link->setAttribute(blink::html_names::kCrossoriginAttr,
                     AtomicString("use-credentials"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kHreflangAttr, AtomicString("klingon"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kTypeAttr, AtomicString("image/gif"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kSizesAttr, AtomicString("16x16"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::html_names::kMediaAttr, AtomicString("print"));
  EXPECT_EQ(link, GetDocument().LinkManifest());
}

TEST_F(DocumentTest, StyleVersion) {
  SetHtmlInnerHTML(R"HTML(
    <style>
        .a * { color: green }
        .b .c { color: green }
    </style>
    <div id='x'><span class='c'></span></div>
  )HTML");

  Element* element = GetDocument().getElementById(AtomicString("x"));
  EXPECT_TRUE(element);

  uint64_t previous_style_version = GetDocument().StyleVersion();
  element->setAttribute(blink::html_names::kClassAttr,
                        AtomicString("notfound"));
  EXPECT_EQ(previous_style_version, GetDocument().StyleVersion());

  UpdateAllLifecyclePhasesForTest();

  previous_style_version = GetDocument().StyleVersion();
  element->setAttribute(blink::html_names::kClassAttr, AtomicString("a"));
  EXPECT_NE(previous_style_version, GetDocument().StyleVersion());

  UpdateAllLifecyclePhasesForTest();

  previous_style_version = GetDocument().StyleVersion();
  element->setAttribute(blink::html_names::kClassAttr, AtomicString("a b"));
  EXPECT_NE(previous_style_version, GetDocument().StyleVersion());
}

TEST_F(DocumentTest, SynchronousMutationNotifier) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());

  EXPECT_EQ(GetDocument(), observer.GetDocument());
  EXPECT_EQ(0, observer.CountContextDestroyedCalled());

  Element* div_node = GetDocument().CreateRawElement(html_names::kDivTag);
  GetDocument().body()->AppendChild(div_node);

  Element* bold_node = GetDocument().CreateRawElement(html_names::kBTag);
  div_node->AppendChild(bold_node);

  Element* italic_node = GetDocument().CreateRawElement(html_names::kITag);
  div_node->AppendChild(italic_node);

  Node* text_node = GetDocument().createTextNode("0123456789");
  bold_node->AppendChild(text_node);
  EXPECT_TRUE(observer.RemovedNodes().empty());

  text_node->remove();
  ASSERT_EQ(1u, observer.RemovedNodes().size());
  EXPECT_EQ(text_node, observer.RemovedNodes()[0]);

  div_node->RemoveChildren();
  EXPECT_EQ(1u, observer.RemovedNodes().size())
      << "ContainerNode::removeChildren() doesn't call nodeWillBeRemoved()";
  ASSERT_EQ(1u, observer.RemovedChildrenNodes().size());
  EXPECT_EQ(div_node, observer.RemovedChildrenNodes()[0]);

  GetDocument().Shutdown();
  EXPECT_EQ(nullptr, observer.GetDocument());
  EXPECT_EQ(1, observer.CountContextDestroyedCalled());
}

TEST_F(DocumentTest, SynchronousMutationNotifieAppendChild) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());
  GetDocument().body()->AppendChild(GetDocument().createTextNode("a123456789"));
  ASSERT_EQ(1u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().body(), observer.ChildrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieInsertBefore) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());
  GetDocument().documentElement()->InsertBefore(
      GetDocument().createTextNode("a123456789"), GetDocument().body());
  ASSERT_EQ(1u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().documentElement(),
            observer.ChildrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierMergeTextNodes) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());

  Text* merge_sample_a = GetDocument().createTextNode("a123456789");
  GetDocument().body()->AppendChild(merge_sample_a);

  Text* merge_sample_b = GetDocument().createTextNode("b123456789");
  GetDocument().body()->AppendChild(merge_sample_b);

  EXPECT_EQ(0u, observer.MergeTextNodesRecords().size());
  GetDocument().body()->normalize();

  ASSERT_EQ(1u, observer.MergeTextNodesRecords().size());
  EXPECT_EQ(merge_sample_a, observer.MergeTextNodesRecords()[0]->node_);
  EXPECT_EQ(merge_sample_b,
            observer.MergeTextNodesRecords()[0]->node_to_be_removed_);
  EXPECT_EQ(10u, observer.MergeTextNodesRecords()[0]->offset_);
}

TEST_F(DocumentTest, SynchronousMutationNotifierMoveTreeToNewDocument) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());

  Node* move_sample = GetDocument().CreateRawElement(html_names::kDivTag);
  move_sample->appendChild(GetDocument().createTextNode("a123"));
  move_sample->appendChild(GetDocument().createTextNode("b456"));
  GetDocument().body()->AppendChild(move_sample);

  ScopedNullExecutionContext execution_context;
  Document& another_document =
      *Document::CreateForTest(execution_context.GetExecutionContext());
  another_document.AppendChild(move_sample);

  EXPECT_EQ(1u, observer.MoveTreeToNewDocumentNodes().size());
  EXPECT_EQ(move_sample, observer.MoveTreeToNewDocumentNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieRemoveChild) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());
  GetDocument().documentElement()->RemoveChild(GetDocument().body());
  ASSERT_EQ(1u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().documentElement(),
            observer.ChildrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieReplaceChild) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());
  Element* const replaced_node = GetDocument().body();
  GetDocument().documentElement()->ReplaceChild(
      GetDocument().CreateRawElement(html_names::kDivTag),
      GetDocument().body());
  ASSERT_EQ(2u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().documentElement(),
            observer.ChildrenChangedNodes()[0]);
  EXPECT_EQ(GetDocument().documentElement(),
            observer.ChildrenChangedNodes()[1]);

  ASSERT_EQ(1u, observer.RemovedNodes().size());
  EXPECT_EQ(replaced_node, observer.RemovedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierSplitTextNode) {
  V8TestingScope scope;
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());

  Text* split_sample = GetDocument().createTextNode("0123456789");
  GetDocument().body()->AppendChild(split_sample);

  split_sample->splitText(4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(1u, observer.SplitTextNodes().size());
  EXPECT_EQ(split_sample, observer.SplitTextNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierUpdateCharacterData) {
  auto& observer =
      *MakeGarbageCollected<TestSynchronousMutationObserver>(GetDocument());

  Text* append_sample = GetDocument().createTextNode("a123456789");
  GetDocument().body()->AppendChild(append_sample);

  Text* delete_sample = GetDocument().createTextNode("b123456789");
  GetDocument().body()->AppendChild(delete_sample);

  Text* insert_sample = GetDocument().createTextNode("c123456789");
  GetDocument().body()->AppendChild(insert_sample);

  Text* replace_sample = GetDocument().createTextNode("c123456789");
  GetDocument().body()->AppendChild(replace_sample);

  EXPECT_EQ(0u, observer.UpdatedCharacterDataRecords().size());

  append_sample->appendData("abc");
  ASSERT_EQ(1u, observer.UpdatedCharacterDataRecords().size());
  EXPECT_EQ(append_sample, observer.UpdatedCharacterDataRecords()[0]->node_);
  EXPECT_EQ(10u, observer.UpdatedCharacterDataRecords()[0]->offset_);
  EXPECT_EQ(0u, observer.UpdatedCharacterDataRecords()[0]->old_length_);
  EXPECT_EQ(3u, observer.UpdatedCharacterDataRecords()[0]->new_length_);

  delete_sample->deleteData(3, 4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(2u, observer.UpdatedCharacterDataRecords().size());
  EXPECT_EQ(delete_sample, observer.UpdatedCharacterDataRecords()[1]->node_);
  EXPECT_EQ(3u, observer.UpdatedCharacterDataRecords()[1]->offset_);
  EXPECT_EQ(4u, observer.UpdatedCharacterDataRecords()[1]->old_length_);
  EXPECT_EQ(0u, observer.UpdatedCharacterDataRecords()[1]->new_length_);

  insert_sample->insertData(3, "def", ASSERT_NO_EXCEPTION);
  ASSERT_EQ(3u, observer.UpdatedCharacterDataRecords().size());
  EXPECT_EQ(insert_sample, observer.UpdatedCharacterDataRecords()[2]->node_);
  EXPECT_EQ(3u, observer.UpdatedCharacterDataRecords()[2]->offset_);
  EXPECT_EQ(0u, observer.UpdatedCharacterDataRecords()[2]->old_length_);
  EXPECT_EQ(3u, observer.UpdatedCharacterDataRecords()[2]->new_length_);

  replace_sample->replaceData(6, 4, "ghi", ASSERT_NO_EXCEPTION);
  ASSERT_EQ(4u, observer.UpdatedCharacterDataRecords().size());
  EXPECT_EQ(replace_sample, observer.UpdatedCharacterDataRecords()[3]->node_);
  EXPECT_EQ(6u, observer.UpdatedCharacterDataRecords()[3]->offset_);
  EXPECT_EQ(4u, observer.UpdatedCharacterDataRecords()[3]->old_length_);
  EXPECT_EQ(3u, observer.UpdatedCharacterDataRecords()[3]->new_length_);
}

// This tests that meta-theme-color can be found correctly
TEST_F(DocumentTest, ThemeColor) {
  {
    SetHtmlInnerHTML(
        "<meta name=\"theme-color\" content=\"#00ff00\">"
        "<body>");
    EXPECT_EQ(Color(0, 255, 0), GetDocument().ThemeColor())
        << "Theme color should be bright green.";
  }

  {
    SetHtmlInnerHTML(
        "<body>"
        "<meta name=\"theme-color\" content=\"#00ff00\">");
    EXPECT_EQ(Color(0, 255, 0), GetDocument().ThemeColor())
        << "Theme color should be bright green.";
  }
}

TEST_F(DocumentTest, ValidationMessageCleanup) {
  ValidationMessageClient* original_client =
      &GetPage().GetValidationMessageClient();
  MockDocumentValidationMessageClient* mock_client =
      MakeGarbageCollected<MockDocumentValidationMessageClient>();
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetPage().SetValidationMessageClientForTesting(mock_client);
  // ImplicitOpen()-CancelParsing() makes Document.loadEventFinished()
  // true. It's necessary to kick unload process.
  GetDocument().ImplicitOpen(kForceSynchronousParsing);
  GetDocument().CancelParsing();
  GetDocument().AppendChild(
      GetDocument().CreateRawElement(html_names::kHTMLTag));
  SetHtmlInnerHTML("<body><input required></body>");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(
      "window.onunload = function() {"
      "document.querySelector('input').reportValidity(); };");
  GetDocument().body()->AppendChild(script);
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstChild());
  DVLOG(0) << GetDocument().body()->outerHTML();

  // Sanity check.
  input->reportValidity();
  EXPECT_TRUE(mock_client->show_validation_message_was_called);
  mock_client->Reset();

  // DetachDocument() unloads the document, and shutdowns.
  GetDocument().GetFrame()->DetachDocument();
  EXPECT_TRUE(mock_client->document_detached_was_called);
  // Unload handler tried to show a validation message, but it should fail.
  EXPECT_FALSE(mock_client->show_validation_message_was_called);

  GetPage().SetValidationMessageClientForTesting(original_client);
}

// Verifies that calling EnsurePaintLocationDataValidForNode cleans compositor
// inputs only when necessary. We generally want to avoid cleaning the inputs,
// as it is more expensive than just doing layout.
TEST_F(DocumentTest,
       EnsurePaintLocationDataValidForNodeCompositingInputsOnlyWhenNecessary) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id='ancestor'>
      <div id='sticky' style='position:sticky;'>
        <div id='stickyChild'></div>
      </div>
      <div id='nonSticky'></div>
    </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_EQ(DocumentLifecycle::kStyleClean,
            GetDocument().Lifecycle().GetState());

  // Asking for any element that is not affected by a sticky element should only
  // advance the lifecycle to layout clean.
  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById(AtomicString("ancestor")),
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            GetDocument().Lifecycle().GetState());

  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById(AtomicString("nonSticky")),
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            GetDocument().Lifecycle().GetState());

  // However, asking for either the sticky element or it's descendents should
  // clean compositing inputs as well.
  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById(AtomicString("sticky")),
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            GetDocument().Lifecycle().GetState());

  // Dirty layout.
  GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                     AtomicString("background: red;"));
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            GetDocument().Lifecycle().GetState());

  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById(AtomicString("stickyChild")),
      DocumentUpdateReason::kTest);
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            GetDocument().Lifecycle().GetState());
}

// Tests that the difference in computed style of direction on the html and body
// elements does not trigger a style recalc for viewport style propagation when
// the computed style for another element in the document is recalculated.
TEST_F(DocumentTest, ViewportPropagationNoRecalc) {
  SetHtmlInnerHTML(R"HTML(
    <body style='direction:rtl'>
      <div id=recalc></div>
    </body>
  )HTML");

  int old_element_count = GetDocument().GetStyleEngine().StyleForElementCount();

  Element* div = GetDocument().getElementById(AtomicString("recalc"));
  div->setAttribute(html_names::kStyleAttr, AtomicString("color:green"));
  GetDocument().UpdateStyleAndLayoutTree();

  int new_element_count = GetDocument().GetStyleEngine().StyleForElementCount();

  EXPECT_EQ(1, new_element_count - old_element_count);
}

// A relative url in a sandboxed, srcdoc frame should trigger a usecount.
TEST_F(DocumentTest, SandboxedSrcdocUserCounts_BasicRelativeUrl) {
  String base_url("https://example.com/");
  WebURL mocked_url = url_test_helpers::RegisterMockedURLLoadFromBase(
      base_url, test::CoreTestDataPath(), "white-1x1.png", "image/png");
  std::string content =
      R"(<html><body><img src='white-1x1.png'></body></html>)";
  NavigateSrcdocMaybeSandboxed(base_url, content, kIsSandboxed, kIsUseCounted);
  url_test_helpers::RegisterMockedURLUnregister(mocked_url);
}

// A relative url in a sandboxed, srcdoc frame should not trigger a usecount
// if the srcdoc document has defined a base element.
TEST_F(DocumentTest,
       SandboxedSrcdocUserCounts_BasicRelativeUrlWithBaseElement) {
  String base_url("https://example.com/");
  WebURL mocked_url = url_test_helpers::RegisterMockedURLLoadFromBase(
      base_url, test::CoreTestDataPath(), "white-1x1.png", "image/png");
  static constexpr char kSrcdocTemplate[] =
      R"(<html><head><base href='%s' /></head>
               <body><img src='white-1x1.png'></body></html>)";
  std::string content =
      base::StringPrintf(kSrcdocTemplate, base_url.Utf8().c_str());
  NavigateSrcdocMaybeSandboxed(base_url, content, kIsSandboxed,
                               kIsNotUseCounted);
  url_test_helpers::RegisterMockedURLUnregister(mocked_url);
}

// An absolute url in a sandboxed, srcdoc frame should not trigger a usecount.
TEST_F(DocumentTest, SandboxedSrcdocUserCounts_BasicAbsoluteUrl) {
  String base_url("https://example.com/");
  WebURL mocked_url = url_test_helpers::RegisterMockedURLLoadFromBase(
      base_url, test::CoreTestDataPath(), "white-1x1.png", "image/png");
  std::string content =
      R"(<html>
           <body>
             <img src='https://example.com/white-1x1.png'>
          </body>
        </html>)";
  NavigateSrcdocMaybeSandboxed(base_url, content, kIsSandboxed,
                               kIsNotUseCounted);
  url_test_helpers::RegisterMockedURLUnregister(mocked_url);
}

// As in BasicRelativeUrl, but this time the url is for an iframe.
TEST_F(DocumentTest, SandboxedSrcdocUserCounts_BasicRelativeUrlInIframe) {
  String base_url("https://example.com/");
  std::string content = R"(<html><body><iframe src='foo.html'></body></html>)";
  NavigateSrcdocMaybeSandboxed(base_url, content, kIsSandboxed, kIsUseCounted);
}

// Non-sandboxed srcdoc frames with relative urls shouldn't trigger the use
// count.
TEST_F(DocumentTest,
       SandboxedSrcdocUserCounts_BasicRelativeUrlInNonSandboxedIframe) {
  String base_url("https://example.com/");
  std::string content = R"(<html><body><iframe src='foo.html'></body></html>)";
  NavigateSrcdocMaybeSandboxed(base_url, content, kIsNotSandboxed,
                               kIsNotUseCounted);
}

// As in BasicAbsoluteUrl, but this time the url is for an iframe.
TEST_F(DocumentTest, SandboxedSrcdocUserCounts_BasicAbsoluteUrlInIframe) {
  String base_url("https://example.com/");
  std::string content =
      R"(<html>
           <body>
             <iframe src='https://example.com/foo.html'>
           </body>
         </html>)";
  NavigateSrcdocMaybeSandboxed(base_url, content, kIsSandboxed,
                               kIsNotUseCounted);
}

TEST_F(DocumentTest, CanExecuteScriptsWithSandboxAndIsolatedWorld) {
  NavigateWithSandbox(KURL("https://www.example.com/"));

  LocalFrame* frame = GetDocument().GetFrame();
  frame->GetSettings()->SetScriptEnabled(true);
  ScriptState* main_world_script_state = ToScriptStateForMainWorld(frame);
  v8::Isolate* isolate = main_world_script_state->GetIsolate();

  constexpr int kIsolatedWorldWithoutCSPId = 1;
  DOMWrapperWorld* world_without_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithoutCSPId);
  ScriptState* isolated_world_without_csp_script_state =
      ToScriptState(frame, *world_without_csp);
  ASSERT_TRUE(world_without_csp->IsIsolatedWorld());
  EXPECT_FALSE(IsolatedWorldCSP::Get().HasContentSecurityPolicy(
      kIsolatedWorldWithoutCSPId));

  constexpr int kIsolatedWorldWithCSPId = 2;
  DOMWrapperWorld* world_with_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithCSPId);
  IsolatedWorldCSP::Get().SetContentSecurityPolicy(
      kIsolatedWorldWithCSPId, String::FromUTF8("script-src *"),
      SecurityOrigin::Create(KURL("chrome-extension://123")));
  ScriptState* isolated_world_with_csp_script_state =
      ToScriptState(frame, *world_with_csp);
  ASSERT_TRUE(world_with_csp->IsIsolatedWorld());
  EXPECT_TRUE(IsolatedWorldCSP::Get().HasContentSecurityPolicy(
      kIsolatedWorldWithCSPId));

  {
    // Since the page is sandboxed, main world script execution shouldn't be
    // allowed.
    ScriptState::Scope scope(main_world_script_state);
    EXPECT_FALSE(frame->DomWindow()->CanExecuteScripts(kAboutToExecuteScript));
  }
  {
    // Isolated worlds without a dedicated CSP should also not be allowed to
    // run scripts.
    ScriptState::Scope scope(isolated_world_without_csp_script_state);
    EXPECT_FALSE(frame->DomWindow()->CanExecuteScripts(kAboutToExecuteScript));
  }
  {
    // An isolated world with a CSP should bypass the main world CSP, and be
    // able to run scripts.
    ScriptState::Scope scope(isolated_world_with_csp_script_state);
    EXPECT_TRUE(frame->DomWindow()->CanExecuteScripts(kAboutToExecuteScript));
  }
}

TEST_F(DocumentTest, ElementFromPointOnScrollbar) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  // This test requires that scrollbars take up space.
  ScopedMockOverlayScrollbars no_overlay_scrollbars(false);

  SetHtmlInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div id='content'>content</div>
  )HTML");

  // A hit test close to the bottom of the page without scrollbars should hit
  // the body element.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 590), GetDocument().body());

  // Add width which will cause a horizontal scrollbar.
  auto* content = GetDocument().getElementById(AtomicString("content"));
  content->setAttribute(html_names::kStyleAttr, AtomicString("width: 101%;"));

  // A hit test on the horizontal scrollbar should not return an element because
  // it is outside the viewport.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 590), nullptr);
  // A hit test above the horizontal scrollbar should hit the body element.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 580), GetDocument().body());
}

TEST_F(DocumentTest, ElementFromPointWithPageZoom) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  // This test requires that scrollbars take up space.
  ScopedMockOverlayScrollbars no_overlay_scrollbars(false);

  SetHtmlInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div id='content' style='height: 10px;'>content</div>
  )HTML");

  // A hit test on the content div should hit it.
  auto* content = GetDocument().getElementById(AtomicString("content"));
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 8), content);
  // A hit test below the content div should not hit it.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 12), GetDocument().body());

  // Zoom the page by 2x,
  GetDocument().GetFrame()->SetLayoutZoomFactor(2);

  // A hit test on the content div should hit it.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 8), content);
  // A hit test below the content div should not hit it.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 12), GetDocument().body());
}

TEST_F(DocumentTest, PrefersColorSchemeChanged) {
  ColorSchemeHelper color_scheme_helper(GetDocument());
  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kLight);
  UpdateAllLifecyclePhasesForTest();

  auto* list = GetDocument().GetMediaQueryMatcher().MatchMedia(
      "(prefers-color-scheme: dark)");
  auto* listener = MakeGarbageCollected<PrefersColorSchemeTestListener>();
  list->AddListener(listener);

  EXPECT_FALSE(listener->IsNotified());

  color_scheme_helper.SetPreferredColorScheme(
      mojom::blink::PreferredColorScheme::kDark);

  UpdateAllLifecyclePhasesForTest();
  PageAnimator::ServiceScriptedAnimations(
      base::TimeTicks(),
      {{GetDocument().GetScriptedAnimationController(), false}});

  EXPECT_TRUE(listener->IsNotified());
}

TEST_F(DocumentTest, FindInPageUkm) {
  ukm::TestAutoSetUkmRecorder recorder;

  EXPECT_EQ(recorder.entries_count(), 0u);
  GetDocument().MarkHasFindInPageRequest();
  EXPECT_EQ(recorder.entries_count(), 1u);
  GetDocument().MarkHasFindInPageRequest();
  EXPECT_EQ(recorder.entries_count(), 1u);

  auto entries = recorder.GetEntriesByName("Blink.FindInPage");
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entries[0], "DidSearch"));
  EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(entries[0], "DidSearch"), 1);
  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[0], "DidHaveRenderSubtreeMatch"));

  GetDocument().MarkHasFindInPageContentVisibilityActiveMatch();
  EXPECT_EQ(recorder.entries_count(), 2u);
  GetDocument().MarkHasFindInPageContentVisibilityActiveMatch();
  EXPECT_EQ(recorder.entries_count(), 2u);
  entries = recorder.GetEntriesByName("Blink.FindInPage");
  EXPECT_EQ(entries.size(), 2u);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entries[0], "DidSearch"));
  EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(entries[0], "DidSearch"), 1);
  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[0], "DidHaveRenderSubtreeMatch"));

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[1], "DidHaveRenderSubtreeMatch"));
  EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(entries[1],
                                                  "DidHaveRenderSubtreeMatch"),
            1);
  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(entries[1], "DidSearch"));
}

TEST_F(DocumentTest, FindInPageUkmInFrame) {
  std::string base_url = "http://internal.test/";

  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("visible_iframe.html"));
  url_test_helpers::RegisterMockedURLLoadFromBase(
      WebString::FromUTF8(base_url), test::CoreTestDataPath(),
      WebString::FromUTF8("single_iframe.html"));

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeAndLoad(base_url + "single_iframe.html");

  web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  Document* top_doc = web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
  auto* iframe =
      To<HTMLIFrameElement>(top_doc->QuerySelector(AtomicString("iframe")));
  Document* document = iframe->contentDocument();
  ASSERT_TRUE(document);
  ASSERT_FALSE(document->IsInMainFrame());

  ukm::TestAutoSetUkmRecorder recorder;
  EXPECT_EQ(recorder.entries_count(), 0u);
  document->MarkHasFindInPageRequest();
  EXPECT_EQ(recorder.entries_count(), 1u);
  document->MarkHasFindInPageRequest();
  EXPECT_EQ(recorder.entries_count(), 1u);

  auto entries = recorder.GetEntriesByName("Blink.FindInPage");
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entries[0], "DidSearch"));
  EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(entries[0], "DidSearch"), 1);
  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[0], "DidHaveRenderSubtreeMatch"));

  document->MarkHasFindInPageContentVisibilityActiveMatch();
  EXPECT_EQ(recorder.entries_count(), 2u);
  document->MarkHasFindInPageContentVisibilityActiveMatch();
  EXPECT_EQ(recorder.entries_count(), 2u);
  entries = recorder.GetEntriesByName("Blink.FindInPage");
  EXPECT_EQ(entries.size(), 2u);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entries[0], "DidSearch"));
  EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(entries[0], "DidSearch"), 1);
  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[0], "DidHaveRenderSubtreeMatch"));

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(
      entries[1], "DidHaveRenderSubtreeMatch"));
  EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(entries[1],
                                                  "DidHaveRenderSubtreeMatch"),
            1);
  EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(entries[1], "DidSearch"));
}

TEST_F(DocumentTest, AtPageMarginWithDeviceScaleFactor) {
  GetDocument().GetFrame()->SetLayoutZoomFactor(2);
  SetBodyInnerHTML("<style>@page { margin: 50px; size: 400px 10in; }</style>");

  constexpr gfx::SizeF initial_page_size(800, 600);

  GetDocument().GetFrame()->StartPrinting(WebPrintParams(initial_page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  WebPrintPageDescription description = GetDocument().GetPageDescription(0);

  EXPECT_EQ(50, description.margin_top);
  EXPECT_EQ(50, description.margin_right);
  EXPECT_EQ(50, description.margin_bottom);
  EXPECT_EQ(50, description.margin_left);
  EXPECT_EQ(gfx::SizeF(400, 960), description.size);
}

TEST_F(DocumentTest, HandlesDisconnectDuringHasPrivateToken) {
  // Check that a Mojo handle disconnecting during hasPrivateToken operation
  // execution results in the promise getting rejected with the proper
  // exception.
  V8TestingScope scope(KURL("https://trusttoken.example"));

  Document& document = scope.GetDocument();

  auto promise =
      document.hasPrivateToken(scope.GetScriptState(), "https://issuer.example",
                               scope.GetExceptionState());
  DocumentTest::SimulateTrustTokenQueryAnswererConnectionError(&document);
  ScriptPromiseTester promise_tester(scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(scope.GetScriptState(), promise_tester.Value(),
                             DOMExceptionCode::kOperationError));
}

TEST_F(DocumentTest, RejectsHasPrivateTokenCallFromNonHttpNonHttpsDocument) {
  // Check that hasPrivateToken getting called from a secure, but
  // non-http/non-https, document results in an exception being thrown.
  V8TestingScope scope(KURL("file:///trusttoken.txt"));

  Document& document = scope.GetDocument();
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasPrivateToken");

  auto promise = document.hasPrivateToken(
      script_state, "https://issuer.example", exception_state);
  EXPECT_TRUE(promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotAllowedError);
}

namespace {
class MockTrustTokenQueryAnswerer
    : public network::mojom::blink::TrustTokenQueryAnswerer {
 public:
  enum Outcome { kError, kInvalidArgument, kResourceExhausted, kTrue, kFalse };
  explicit MockTrustTokenQueryAnswerer(Outcome outcome) : outcome_(outcome) {}

  void HasTrustTokens(
      const ::scoped_refptr<const ::blink::SecurityOrigin>& issuer,
      HasTrustTokensCallback callback) override {
    auto result = network::mojom::blink::HasTrustTokensResult::New();
    result->status = network::mojom::blink::TrustTokenOperationStatus::kOk;
    switch (outcome_) {
      case kTrue: {
        result->has_trust_tokens = true;
        std::move(callback).Run(std::move(result));
        return;
      }
      case kFalse: {
        result->has_trust_tokens = false;
        std::move(callback).Run(std::move(result));
        return;
      }
      case kInvalidArgument: {
        result->status =
            network::mojom::blink::TrustTokenOperationStatus::kInvalidArgument;
        std::move(callback).Run(std::move(result));
        return;
      }
      case kResourceExhausted: {
        result->status = network::mojom::blink::TrustTokenOperationStatus::
            kResourceExhausted;
        std::move(callback).Run(std::move(result));
        return;
      }
      case kError: {
        result->status =
            network::mojom::blink::TrustTokenOperationStatus::kUnknownError;
        std::move(callback).Run(std::move(result));
      }
    }
  }

  void HasRedemptionRecord(
      const ::scoped_refptr<const ::blink::SecurityOrigin>& issuer,
      HasRedemptionRecordCallback callback) override {
    auto result = network::mojom::blink::HasRedemptionRecordResult::New();
    result->status = network::mojom::blink::TrustTokenOperationStatus::kOk;
    switch (outcome_) {
      case kTrue: {
        result->has_redemption_record = true;
        break;
      }
      case kFalse: {
        result->has_redemption_record = false;
        break;
      }
      case kInvalidArgument: {
        result->status =
            network::mojom::blink::TrustTokenOperationStatus::kInvalidArgument;
        break;
      }
      case kResourceExhausted: {
        result->status = network::mojom::blink::TrustTokenOperationStatus::
            kResourceExhausted;
        break;
      }
      case kError: {
        result->status =
            network::mojom::blink::TrustTokenOperationStatus::kUnknownError;
        break;
      }
    }
    std::move(callback).Run(std::move(result));
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<network::mojom::blink::TrustTokenQueryAnswerer>(
            std::move(handle)));
  }

 private:
  Outcome outcome_;
  mojo::Receiver<network::mojom::blink::TrustTokenQueryAnswerer> receiver_{
      this};
};
}  // namespace

TEST_F(DocumentTest, HasPrivateTokenSuccess) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(MockTrustTokenQueryAnswerer::kTrue);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasPrivateToken");

  auto promise = document.hasPrivateToken(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_TRUE(promise_tester.Value().V8Value()->IsTrue());

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasPrivateTokenSuccessWithFalseValue) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(MockTrustTokenQueryAnswerer::kFalse);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasPrivateToken");

  auto promise = document.hasPrivateToken(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_TRUE(promise_tester.Value().V8Value()->IsFalse());

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasPrivateTokenOperationError) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(MockTrustTokenQueryAnswerer::kError);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasPrivateToken");

  auto promise = document.hasPrivateToken(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kOperationError));

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasPrivateTokenInvalidArgument) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(
      MockTrustTokenQueryAnswerer::kInvalidArgument);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasPrivateToken");

  auto promise = document.hasPrivateToken(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kOperationError));

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasPrivateTokenResourceExhausted) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(
      MockTrustTokenQueryAnswerer::kResourceExhausted);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasPrivateToken");

  auto promise = document.hasPrivateToken(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kOperationError));

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasRedemptionRecordSuccess) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(MockTrustTokenQueryAnswerer::kTrue);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasRedemptionRecord");

  auto promise = document.hasRedemptionRecord(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_TRUE(promise_tester.Value().V8Value()->IsTrue());

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasRedemptionRecordSuccessWithFalseValue) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(MockTrustTokenQueryAnswerer::kFalse);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasRedemptionRecord");

  auto promise = document.hasRedemptionRecord(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_TRUE(promise_tester.Value().V8Value()->IsFalse());

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasRedemptionRecordOperationError) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(MockTrustTokenQueryAnswerer::kError);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasRedemptionRecord");

  auto promise = document.hasRedemptionRecord(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kOperationError));

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HasRedemptionRecordInvalidArgument) {
  V8TestingScope scope(KURL("https://secure.example"));

  MockTrustTokenQueryAnswerer answerer(
      MockTrustTokenQueryAnswerer::kInvalidArgument);

  Document& document = scope.GetDocument();
  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_,
      WTF::BindRepeating(&MockTrustTokenQueryAnswerer::Bind,
                         WTF::Unretained(&answerer)));

  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasRedemptionRecord");

  auto promise = document.hasRedemptionRecord(
      script_state, "https://issuer.example", exception_state);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kOperationError));

  document.GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      network::mojom::blink::TrustTokenQueryAnswerer::Name_, {});
}

TEST_F(DocumentTest, HandlesDisconnectDuringHasRedemptionRecord) {
  // Check that a Mojo handle disconnecting during hasRedemptionRecord
  // operation execution results in the promise getting rejected with
  // the proper exception.
  V8TestingScope scope(KURL("https://trusttoken.example"));

  Document& document = scope.GetDocument();

  auto promise = document.hasRedemptionRecord(scope.GetScriptState(),
                                              "https://issuer.example",
                                              scope.GetExceptionState());
  DocumentTest::SimulateTrustTokenQueryAnswererConnectionError(&document);
  ScriptPromiseTester promise_tester(scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(scope.GetScriptState(), promise_tester.Value(),
                             DOMExceptionCode::kOperationError));
}

TEST_F(DocumentTest,
       RejectsHasRedemptionRecordCallFromNonHttpNonHttpsDocument) {
  // Check that hasRedemptionRecord getting called from a secure, but
  // non-http/non-https, document results in an exception being thrown.
  V8TestingScope scope(KURL("file:///trusttoken.txt"));

  Document& document = scope.GetDocument();
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kOperation, "Document",
                                 "hasRedemptionRecord");

  auto promise = document.hasRedemptionRecord(
      script_state, "https://issuer.example", exception_state);
  EXPECT_TRUE(promise.IsEmpty());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotAllowedError);
}

/**
 * Tests for viewport-fit propagation.
 */

class ViewportFitDocumentTest : public DocumentTest,
                                private ScopedDisplayCutoutAPIForTest {
 public:
  ViewportFitDocumentTest() : ScopedDisplayCutoutAPIForTest(true) {}
  void SetUp() override {
    DocumentTest::SetUp();
    GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  }

  mojom::ViewportFit GetViewportFit() const {
    return GetDocument().GetViewportData().GetCurrentViewportFitForTests();
  }
};

// Test meta viewport present but no viewport-fit.
TEST_F(ViewportFitDocumentTest, MetaViewportButNoFit) {
  SetHtmlInnerHTML("<meta name='viewport' content='initial-scale=1'>");

  EXPECT_EQ(mojom::ViewportFit::kAuto, GetViewportFit());
}

// Test overriding the viewport fit using SetExpandIntoDisplayCutout.
TEST_F(ViewportFitDocumentTest, ForceExpandIntoCutout) {
  SetHtmlInnerHTML("<meta name='viewport' content='viewport-fit=contain'>");
  EXPECT_EQ(mojom::ViewportFit::kContain, GetViewportFit());

  // Now override the viewport fit value and expect it to be kCover.
  GetDocument().GetViewportData().SetExpandIntoDisplayCutout(true);
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, GetViewportFit());

  // Test that even if we change the value we ignore it.
  SetHtmlInnerHTML("<meta name='viewport' content='viewport-fit=auto'>");
  EXPECT_EQ(mojom::ViewportFit::kCoverForcedByUserAgent, GetViewportFit());

  // Now remove the override and check that it went back to the previous value.
  GetDocument().GetViewportData().SetExpandIntoDisplayCutout(false);
  EXPECT_EQ(mojom::ViewportFit::kAuto, GetViewportFit());
}

// This is a test case for testing a combination of viewport-fit meta value,
// viewport CSS value and the expected outcome.
using ViewportTestCase = std::tuple<const char*, mojom::ViewportFit>;

class ParameterizedViewportFitDocumentTest
    : public ViewportFitDocumentTest,
      public testing::WithParamInterface<ViewportTestCase> {
 protected:
  void LoadTestHTML() {
    const char* kMetaValue = std::get<0>(GetParam());
    StringBuilder html;

    if (kMetaValue) {
      html.Append("<meta name='viewport' content='viewport-fit=");
      html.Append(kMetaValue);
      html.Append("'>");
    }

    GetDocument().documentElement()->setInnerHTML(html.ReleaseString());
    UpdateAllLifecyclePhasesForTest();
  }
};

TEST_P(ParameterizedViewportFitDocumentTest, EffectiveViewportFit) {
  LoadTestHTML();
  EXPECT_EQ(std::get<1>(GetParam()), GetViewportFit());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParameterizedViewportFitDocumentTest,
    testing::Values(
        // Test the default case.
        ViewportTestCase(nullptr, mojom::ViewportFit::kAuto),
        // Test the different values set through the meta tag.
        ViewportTestCase("auto", mojom::ViewportFit::kAuto),
        ViewportTestCase("contain", mojom::ViewportFit::kContain),
        ViewportTestCase("cover", mojom::ViewportFit::kCover),
        ViewportTestCase("invalid", mojom::ViewportFit::kAuto)));

namespace {
class MockReportingContext final : public ReportingContext {
 public:
  explicit MockReportingContext(ExecutionContext& ec) : ReportingContext(ec) {}

  void QueueReport(Report* report, const Vector<String>& endpoint) override {
    report_count++;
  }

  unsigned report_count = 0;
};

}  // namespace

TEST_F(DocumentSimTest, LastModified) {
  const char kLastModified[] = "Tue, 15 Nov 1994 12:45:26 GMT";
  SimRequest::Params params;
  params.response_http_headers = {{"Last-Modified", kLastModified}};
  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  // We test lastModifiedTime() instead of lastModified() because the latter
  // returns a string in the local time zone.
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(kLastModified, &time));
  EXPECT_EQ(time, GetDocument().lastModifiedTime());
}

TEST_F(DocumentSimTest, DuplicatedDocumentPolicyViolationsAreIgnored) {
  SimRequest::Params params;
  params.response_http_headers = {{"Document-Policy", "force-load-at-top=?0"}};
  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  MockReportingContext* mock_reporting_context =
      MakeGarbageCollected<MockReportingContext>(*execution_context);
  Supplement<ExecutionContext>::ProvideTo(*execution_context,
                                          mock_reporting_context);

  EXPECT_FALSE(execution_context->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop,
      PolicyValue::CreateBool(true), ReportOptions::kReportOnFailure));

  EXPECT_EQ(mock_reporting_context->report_count, 1u);

  EXPECT_FALSE(execution_context->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop,
      PolicyValue::CreateBool(true), ReportOptions::kReportOnFailure));

  EXPECT_EQ(mock_reporting_context->report_count, 1u);
}

// Tests getting the unassociated listed elements.
class UnassociatedListedElementTest : public DocumentTest {
 protected:
  ListedElement* GetElement(const char* id) {
    Element* element = GetElementById(id);
    return ListedElement::From(*element);
  }
};

// Check if the unassociated listed elements are properly extracted.
// Listed elements are: button, fieldset, input, textarea, output, select,
// object and form-associated custom elements.
TEST_F(UnassociatedListedElementTest, GetUnassociatedListedElements) {
  SetHtmlInnerHTML(R"HTML(
    <button id='unassociated_button'>Unassociated button</button>
    <fieldset id='unassociated_fieldset'>
      <label>Unassociated fieldset</label>
    </fieldset>
    <input id='unassociated_input'>
    <textarea id='unassociated_textarea'>I am unassociated</textarea>
    <output id='unassociated_output'>Unassociated output</output>
    <select id='unassociated_select'>
      <option value='first'>first</option>
      <option value='second' selected>second</option>
    </select>
    <object id='unassociated_object'></object>

    <form id='form'>
      <button id='form_button'>Form button</button>
      <fieldset id='form_fieldset'>
        <label>Form fieldset</label>
      </fieldset>
      <input id='form_input'>
      <textarea id='form_textarea'>I am in a form</textarea>
      <output id='form_output'>Form output</output>
      <select name='form_select' id='form_select'>
        <option value='june'>june</option>
        <option value='july' selected>july</option>
      </select>
      <object id='form_object'></object>
    </form>
 )HTML");

  // Add unassociated form-associated custom element.
  Element* unassociated_custom_element =
      CreateElement(AtomicString("input")).WithIsValue(AtomicString("a-b"));
  unassociated_custom_element->SetIdAttribute(
      AtomicString("unassociated_custom_element"));
  GetDocument().body()->AppendChild(unassociated_custom_element);
  ASSERT_TRUE(GetDocument().getElementById(
      AtomicString("unassociated_custom_element")));

  // Add associated form-associated custom element.
  Element* associated_custom_element =
      CreateElement(AtomicString("input")).WithIsValue(AtomicString("a-b"));
  associated_custom_element->SetIdAttribute(
      AtomicString("associated_custom_element"));
  GetDocument()
      .getElementById(AtomicString("form"))
      ->AppendChild(associated_custom_element);
  ASSERT_TRUE(
      GetDocument().getElementById(AtomicString("associated_custom_element")));

  auto expected_elements = [&] {
    return ElementsAre(
        GetElement("unassociated_button"), GetElement("unassociated_fieldset"),
        GetElement("unassociated_input"), GetElement("unassociated_textarea"),
        GetElement("unassociated_output"), GetElement("unassociated_select"),
        /*Button inside <object> Shadow DOM*/ _,
        GetElement("unassociated_custom_element"));
  };
  EXPECT_THAT(GetDocument().UnassociatedListedElements(), expected_elements());

  // Try getting the cached unassociated listed elements again (calling
  // UnassociatedListedElements() again will not re-extract them).
  EXPECT_THAT(GetDocument().UnassociatedListedElements(), expected_elements());
}

// We extract unassociated listed element in a shadow DOM.
TEST_F(UnassociatedListedElementTest,
       GetUnassociatedListedElementsFromShadowTree) {
  ShadowRoot& shadow_root =
      GetDocument().body()->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  HTMLInputElement* input =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input->SetIdAttribute(AtomicString("unassociated_input"));
  shadow_root.AppendChild(input);
  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements,
              ElementsAre(ListedElement::From(*shadow_root.getElementById(
                  AtomicString("unassociated_input")))));
}

// Check if the dynamically added unassociated listed element is properly
// extracted.
TEST_F(UnassociatedListedElementTest,
       GetDynamicallyAddedUnassociatedListedElements) {
  SetHtmlInnerHTML(R"HTML(
    <form id="form_id">
      <input id='form_input_1'>
    </form>
  )HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());

  auto* input = MakeGarbageCollected<HTMLInputElement>(GetDocument());
  input->SetIdAttribute(AtomicString("unassociated_input"));
  GetDocument().body()->AppendChild(input);

  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements, ElementsAre(GetElement("unassociated_input")));
}

// Check if the dynamically removed unassociated listed element from the
// Document is no longer extracted.
TEST_F(UnassociatedListedElementTest,
       GetDynamicallyRemovedUnassociatedListedElement) {
  SetHtmlInnerHTML(R"HTML(
    <form id='form_id'></form>
    <input id='input_id'>
  )HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements, ElementsAre(GetElement("input_id")));

  GetDocument().getElementById(AtomicString("input_id"))->remove();
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());
}

// Check if dynamically assigning an unassociated listed element to a form by
// changing its form attribute is no longer extracted as an unassociated listed
// element.
TEST_F(UnassociatedListedElementTest,
       GetUnassociatedListedElementAfterAddingFormAttr) {
  SetHtmlInnerHTML(R"HTML(
    <form id='form_id'></form>
    <input id='input_id'>
  )HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements, ElementsAre(GetElement("input_id")));

  GetDocument()
      .getElementById(AtomicString("input_id"))
      ->setAttribute(html_names::kFormAttr, AtomicString("form_id"));
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());
}

// Check if dynamically removing the form attribute from an associated listed
// element makes it unassociated.
TEST_F(UnassociatedListedElementTest,
       GetUnassociatedListedElementAfterRemovingFormAttr) {
  SetHtmlInnerHTML(R"HTML(
    <form id='form_id'></form>
    <input id='input_id' form='form_id'>
  )HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());

  GetDocument()
      .getElementById(AtomicString("input_id"))
      ->removeAttribute(html_names::kFormAttr);
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements, ElementsAre(GetElement("input_id")));
}

// Check if after dynamically setting an associated listed element's form
// attribute to a non-existent one, the element becomes unassociated even if
// inside a <form> element.
TEST_F(UnassociatedListedElementTest,
       GetUnassociatedListedElementAfterSettingFormAttrToNonexistent) {
  SetHtmlInnerHTML(
      R"HTML(<form id='form_id'><input id='input_id'></form>)HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());

  GetDocument()
      .getElementById(AtomicString("input_id"))
      ->setAttribute(html_names::kFormAttr, AtomicString("nonexistent_id"));
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements, ElementsAre(GetElement("input_id")));
}

// Check if dynamically adding an unassociated listed element to an element
// that is not in the Document won't be extracted.
TEST_F(UnassociatedListedElementTest,
       GeDynamicallyAddedUnassociatedListedElementThatIsNotInTheDocument) {
  SetHtmlInnerHTML(R"HTML(<body></body>)HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());

  HTMLDivElement* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  HTMLInputElement* input =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());
  div->AppendChild(input);
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());
}

// Check if an unassociated listed element added as a nested element will be
// extracted.
TEST_F(UnassociatedListedElementTest,
       GetAttachedNestedUnassociatedFormFieldElements) {
  SetHtmlInnerHTML(R"HTML(<body></body>)HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());

  HTMLDivElement* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  HTMLInputElement* input =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());
  div->AppendChild(input);
  GetDocument().body()->AppendChild(div);
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_EQ(listed_elements[0]->ToHTMLElement(), input);
}

// Check when removing the ancestor element of an unassociated listed element
// won't make the unassociated element extracted.
TEST_F(UnassociatedListedElementTest,
       GetDetachedNestedUnassociatedFormFieldElements) {
  SetHtmlInnerHTML(R"HTML(<div id='div_id'><input id='input_id'></div>)HTML");

  ListedElement::List listed_elements =
      GetDocument().UnassociatedListedElements();
  EXPECT_THAT(listed_elements, ElementsAre(GetElement("input_id")));

  auto* div = GetDocument().getElementById(AtomicString("div_id"));
  div->remove();
  listed_elements = GetDocument().UnassociatedListedElements();
  EXPECT_EQ(0u, listed_elements.size());
}

class TopLevelFormsListTest : public DocumentTest {
 public:
  HTMLFormElement* GetFormElement(const char* id) {
    return DynamicTo<HTMLFormElement>(GetElementById(id));
  }
  HTMLFormElement* GetFormElement(const char* id, ShadowRoot& shadow_root) {
    return DynamicTo<HTMLFormElement>(
        shadow_root.getElementById(AtomicString(id)));
  }
};

// Tests that `GetTopLevelForms` correctly lists forms in the light DOM.
TEST_F(TopLevelFormsListTest, FormsInLightDom) {
  SetHtmlInnerHTML(R"HTML(
    <form id="f1">
      <input type="text">
    </form>
    <div>
      <form id="f2">
        <input type="text">
      </form>
    </div>
  )HTML");
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), GetFormElement("f2")));
  // A second call has the same result.
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), GetFormElement("f2")));
}

// Tests that `GetTopLevelForms` functions correctly after dynamic form element
// insertion and removal.
TEST_F(TopLevelFormsListTest, FormsInLightDomInsertionAndRemoval) {
  SetHtmlInnerHTML(R"HTML(
    <form id="f1">
      <input type="text">
    </form>
    <div>
      <form id="f2">
        <input type="text">
      </form>
    </div>
  )HTML");
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), GetFormElement("f2")));

  // Adding a new form element invalidates the cache.
  Element* new_form = CreateElement(AtomicString("form"));
  new_form->SetIdAttribute(AtomicString("f3"));
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), GetFormElement("f2")));
  GetDocument().body()->AppendChild(new_form);
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), GetFormElement("f3"),
                          GetFormElement("f2")));

  // Removing a form element invalidates the cache.
  GetFormElement("f2")->remove();
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), GetFormElement("f3")));
}

// Tests that top level forms inside shadow DOM are listed correctly and
// insertion and removal updates the cache.
TEST_F(TopLevelFormsListTest, FormsInShadowDomInsertionAndRemoval) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <form id="f1">
      <input type="text">
    </form>
    <div id="d">
      <template shadowrootmode=open>
        <form id="f2">
          <input type="text">
        </form>
      </template>
    </div>
  )HTML");
  HTMLFormElement* f2 =
      GetFormElement("f2", *GetElementById("d")->GetShadowRoot());
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1"), f2));

  // Removing f1 updates the cache.
  GetFormElement("f1")->remove();
  EXPECT_THAT(GetDocument().GetTopLevelForms(), ElementsAre(f2));

  // Removing f2 also updates the cache.
  f2->remove();
  EXPECT_THAT(GetDocument().GetTopLevelForms(), IsEmpty());
}

// Tests that nested forms across shadow DOM are ignored by `GetTopLevelForms`.
TEST_F(TopLevelFormsListTest, GetTopLevelFormsIgnoresNestedChildren) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <form id="f1">
      <input type="text">
      <div id="d">
        <template shadowrootmode=open>
          <form id="f2">
            <input type="text">
          </form>
        </template>
      </div>
    </form>
  )HTML");
  EXPECT_THAT(GetDocument().GetTopLevelForms(),
              ElementsAre(GetFormElement("f1")));
}

TEST_F(DocumentTest, DocumentDefiningElementWithMultipleBodies) {
  SetHtmlInnerHTML(R"HTML(
    <body style="overflow: auto; height: 100%">
      <div style="height: 10000px"></div>
    </body>
  )HTML");

  Element* body1 = GetDocument().body();
  EXPECT_EQ(body1, GetDocument().ViewportDefiningElement());
  EXPECT_FALSE(body1->GetLayoutBox()->GetScrollableArea());

  Element* body2 = To<Element>(body1->cloneNode(true));
  GetDocument().documentElement()->appendChild(body2);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(body1, GetDocument().ViewportDefiningElement());
  EXPECT_FALSE(body1->GetLayoutBox()->GetScrollableArea());
  EXPECT_TRUE(body2->GetLayoutBox()->GetScrollableArea());

  GetDocument().documentElement()->appendChild(body1);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(body2, GetDocument().ViewportDefiningElement());
  EXPECT_TRUE(body1->GetLayoutBox()->GetScrollableArea());
  EXPECT_FALSE(body2->GetLayoutBox()->GetScrollableArea());
}

TEST_F(DocumentTest, LayoutReplacedUseCounterNoStyles) {
  SetHtmlInnerHTML(R"HTML(
    <img>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

TEST_F(DocumentTest, LayoutReplacedUseCounterExplicitlyHidden) {
  SetHtmlInnerHTML(R"HTML(
    <style> .tag { overflow: hidden } </style>
    <img class=tag>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

TEST_F(DocumentTest, LayoutReplacedUseCounterExplicitlyVisible) {
  SetHtmlInnerHTML(R"HTML(
    <style> .tag { overflow: visible } </style>
    <img class=tag>
  )HTML");

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

TEST_F(DocumentTest, LayoutReplacedUseCounterExplicitlyVisibleWithObjectFit) {
  SetHtmlInnerHTML(R"HTML(
    <style> .tag { overflow: visible; object-fit: cover; } </style>
    <img class=tag>
  )HTML");

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

TEST_F(DocumentTest, LayoutReplacedUseCounterExplicitlyVisibleLaterHidden) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      img { overflow: visible; }
      .tag { overflow: hidden; }
    </style>
    <img class=tag>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

TEST_F(DocumentTest, LayoutReplacedUseCounterIframe) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      iframe { overflow: visible; }
    </style>
    <iframe></iframe>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

TEST_F(DocumentTest, LayoutReplacedUseCounterSvg) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      svg { overflow: visible; }
    </style>
    <svg></svg>
  )HTML");

  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElement));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kExplicitOverflowVisibleOnReplacedElementWithObjectProp));
}

// https://crbug.com/1311370
TEST_F(DocumentSimTest, HeaderPreloadRemoveReaddClient) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Link", "<https://example.com/sheet.css>;rel=preload;as=style;"}};

  SimRequest main_resource("https://example.com", "text/html", main_params);
  SimSubresourceRequest css_resource("https://example.com/sheet.css",
                                     "text/css");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <link rel="stylesheet" href="sheet.css">
  )HTML");

  // Remove and garbage-collect the pending stylesheet link element, which will
  // remove it from the list of ResourceClients of the Resource being preloaded.
  GetDocument().QuerySelector(AtomicString("link"))->remove();
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Removing the ResourceClient should not affect the preloading.
  css_resource.Complete(".target { width: 100px; }");

  // After the preload finishes, when a new ResourceClient is added, it should
  // be able to use the Resource immediately.
  main_resource.Complete(R"HTML(
    <link rel="stylesheet" href="sheet.css">
    <div class="target"></div>
  )HTML");

  Element* target = GetDocument().QuerySelector(AtomicString(".target"));
  EXPECT_EQ(100, target->OffsetWidth());
}

TEST_F(DocumentTest, ActiveModalDialog) {
  SetHtmlInnerHTML(R"HTML(
    <dialog id="modal"></dialog>
    <dialog popover id="popover"></dialog>
  )HTML");

  HTMLDialogElement* modal = DynamicTo<HTMLDialogElement>(
      GetDocument().getElementById(AtomicString("modal")));
  HTMLDialogElement* popover = DynamicTo<HTMLDialogElement>(
      GetDocument().getElementById(AtomicString("popover")));

  ASSERT_TRUE(modal);
  ASSERT_TRUE(popover);

  EXPECT_EQ(GetDocument().ActiveModalDialog(), nullptr);

  NonThrowableExceptionState exception_state;
  modal->showModal(exception_state);

  EXPECT_EQ(GetDocument().ActiveModalDialog(), modal);
  ASSERT_FALSE(GetDocument().TopLayerElements().empty());
  EXPECT_EQ(GetDocument().TopLayerElements().back(), modal);

  popover->showPopover(exception_state);

  // The popover is the last of the top layer elements, but it's not modal.
  ASSERT_FALSE(GetDocument().TopLayerElements().empty());
  EXPECT_EQ(GetDocument().TopLayerElements().back(), popover);
  EXPECT_EQ(GetDocument().ActiveModalDialog(), modal);
}

TEST_F(DocumentTest, LifecycleState_DirtyStyle_NoBody) {
  GetDocument().body()->remove();
  UpdateAllLifecyclePhasesForTest();
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("color:pink"));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_EQ(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kVisualUpdatePending);
}

class TestPaymentLinkHandler
    : public payments::facilitated::mojom::blink::PaymentLinkHandler {
 public:
  void HandlePaymentLink(const KURL& url) override {
    ++payment_link_handled_counter_;
    handled_url_ = url;
    std::move(on_link_handled_callback_).Run();
  }

  int get_payment_link_handled_counter() const {
    return payment_link_handled_counter_;
  }

  const KURL& get_handled_url() const { return handled_url_; }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<
                   payments::facilitated::mojom::blink::PaymentLinkHandler>(
        std::move(handle)));
  }

  void set_on_link_handled_callback(
      base::OnceClosure on_link_handled_callback) {
    on_link_handled_callback_ = std::move(on_link_handled_callback);
  }

 private:
  int payment_link_handled_counter_ = 0;
  KURL handled_url_;
  mojo::Receiver<payments::facilitated::mojom::blink::PaymentLinkHandler>
      receiver_{this};
  base::OnceClosure on_link_handled_callback_;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(DocumentTest, PaymentLinkHandling_SinglePaymentLink) {
  TestPaymentLinkHandler test_payment_link_handler;
  base::RunLoop run_loop;
  test_payment_link_handler.set_on_link_handled_callback(
      run_loop.QuitClosure());

  GetDocument().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      payments::facilitated::mojom::blink::PaymentLinkHandler::Name_,
      base::BindRepeating(&TestPaymentLinkHandler::Bind,
                          base::Unretained(&test_payment_link_handler)));

  ScopedPaymentLinkDetectionForTest payment_link_detection(true);

  SetHtmlInnerHTML(R"HTML(
    <head>
      <link rel="payment" href="upi://payment_link_1">
    </head>
  )HTML");

  // Run the message loop to ensure Mojo messages are dispatched.
  run_loop.Run();

  // Check if the correct payment link was handled.
  EXPECT_EQ(test_payment_link_handler.get_payment_link_handled_counter(), 1);
  EXPECT_EQ(test_payment_link_handler.get_handled_url(),
            KURL("upi://payment_link_1"));
}

TEST_F(DocumentTest, PaymentLinkHandling_MultiplePaymentLink) {
  TestPaymentLinkHandler test_payment_link_handler;
  base::RunLoop run_loop;
  test_payment_link_handler.set_on_link_handled_callback(
      run_loop.QuitClosure());

  GetDocument().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
      payments::facilitated::mojom::blink::PaymentLinkHandler::Name_,
      base::BindRepeating(&TestPaymentLinkHandler::Bind,
                          base::Unretained(&test_payment_link_handler)));

  ScopedPaymentLinkDetectionForTest payment_link_detection(true);

  SetHtmlInnerHTML(R"HTML(
    <head>
      <link rel="payment" href="upi://payment_link_1">
      <link rel="payment" href="upi://payment_link_2">
    </head>
  )HTML");

  // Run the message loop to ensure Mojo messages are dispatched.
  run_loop.Run();

  // Check if the correct payment link was handled and the payment link handling
  // was invoked only once.
  EXPECT_EQ(test_payment_link_handler.get_payment_link_handled_counter(), 1);
  EXPECT_EQ(test_payment_link_handler.get_handled_url(),
            KURL("upi://payment_link_1"));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace blink
