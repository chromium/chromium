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

#include <memory>

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class DocumentTest : public PageTestBase {
 protected:
  void TearDown() override { ThreadState::Current()->CollectAllGarbage(); }

  void SetHtmlInnerHTML(const char*);
};

void DocumentTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      String::FromUTF8(html_content));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

namespace {

class TestSynchronousMutationObserver
    : public GarbageCollectedFinalized<TestSynchronousMutationObserver>,
      public SynchronousMutationObserver {
  USING_GARBAGE_COLLECTED_MIXIN(TestSynchronousMutationObserver);

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

    void Trace(blink::Visitor* visitor) {
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

    void Trace(blink::Visitor* visitor) { visitor->Trace(node_); }
  };

  TestSynchronousMutationObserver(Document&);
  virtual ~TestSynchronousMutationObserver() = default;

  int CountContextDestroyedCalled() const {
    return context_destroyed_called_counter_;
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

  void Trace(blink::Visitor*) override;

 private:
  // Implement |SynchronousMutationObserver| member functions.
  void ContextDestroyed(Document*) final;
  void DidChangeChildren(const ContainerNode&) final;
  void DidMergeTextNodes(const Text&, const NodeWithIndex&, unsigned) final;
  void DidMoveTreeToNewDocument(const Node& root) final;
  void DidSplitTextNode(const Text&) final;
  void DidUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned old_length,
                              unsigned new_length) final;
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node&) final;

  int context_destroyed_called_counter_ = 0;
  HeapVector<Member<const ContainerNode>> children_changed_nodes_;
  HeapVector<Member<MergeTextNodesRecord>> merge_text_nodes_records_;
  HeapVector<Member<const Node>> move_tree_to_new_document_nodes_;
  HeapVector<Member<ContainerNode>> removed_children_nodes_;
  HeapVector<Member<Node>> removed_nodes_;
  HeapVector<Member<const Text>> split_text_nodes_;
  HeapVector<Member<UpdateCharacterDataRecord>> updated_character_data_records_;

  DISALLOW_COPY_AND_ASSIGN(TestSynchronousMutationObserver);
};

TestSynchronousMutationObserver::TestSynchronousMutationObserver(
    Document& document) {
  SetContext(&document);
}

void TestSynchronousMutationObserver::ContextDestroyed(Document*) {
  ++context_destroyed_called_counter_;
}

void TestSynchronousMutationObserver::DidChangeChildren(
    const ContainerNode& container) {
  children_changed_nodes_.push_back(&container);
}

void TestSynchronousMutationObserver::DidMergeTextNodes(
    const Text& node,
    const NodeWithIndex& node_with_index,
    unsigned offset) {
  merge_text_nodes_records_.push_back(
      new MergeTextNodesRecord(&node, node_with_index, offset));
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
  updated_character_data_records_.push_back(new UpdateCharacterDataRecord(
      character_data, offset, old_length, new_length));
}

void TestSynchronousMutationObserver::NodeChildrenWillBeRemoved(
    ContainerNode& container) {
  removed_children_nodes_.push_back(&container);
}

void TestSynchronousMutationObserver::NodeWillBeRemoved(Node& node) {
  removed_nodes_.push_back(&node);
}

void TestSynchronousMutationObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(children_changed_nodes_);
  visitor->Trace(merge_text_nodes_records_);
  visitor->Trace(move_tree_to_new_document_nodes_);
  visitor->Trace(removed_children_nodes_);
  visitor->Trace(removed_nodes_);
  visitor->Trace(split_text_nodes_);
  visitor->Trace(updated_character_data_records_);
  SynchronousMutationObserver::Trace(visitor);
}

class TestDocumentShutdownObserver
    : public GarbageCollectedFinalized<TestDocumentShutdownObserver>,
      public DocumentShutdownObserver {
  USING_GARBAGE_COLLECTED_MIXIN(TestDocumentShutdownObserver);

 public:
  TestDocumentShutdownObserver(Document&);
  virtual ~TestDocumentShutdownObserver() = default;

  int CountContextDestroyedCalled() const {
    return context_destroyed_called_counter_;
  }

  void Trace(blink::Visitor*) override;

 private:
  // Implement |DocumentShutdownObserver| member functions.
  void ContextDestroyed(Document*) final;

  int context_destroyed_called_counter_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDocumentShutdownObserver);
};

TestDocumentShutdownObserver::TestDocumentShutdownObserver(Document& document) {
  SetContext(&document);
}

void TestDocumentShutdownObserver::ContextDestroyed(Document*) {
  ++context_destroyed_called_counter_;
}

void TestDocumentShutdownObserver::Trace(blink::Visitor* visitor) {
  DocumentShutdownObserver::Trace(visitor);
}

class MockDocumentValidationMessageClient
    : public GarbageCollectedFinalized<MockDocumentValidationMessageClient>,
      public ValidationMessageClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockDocumentValidationMessageClient);

 public:
  MockDocumentValidationMessageClient() { Reset(); }
  void Reset() {
    show_validation_message_was_called = false;
    document_detached_was_called = false;
  }
  bool show_validation_message_was_called;
  bool document_detached_was_called;

  // ValidationMessageClient functions.
  void ShowValidationMessage(const Element& anchor,
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
  void WillBeDestroyed() override {}

  // virtual void Trace(blink::Visitor* visitor) {
  // ValidationMessageClient::trace(visitor); }
};

class MockWebApplicationCacheHost : public blink::WebApplicationCacheHost {
 public:
  MockWebApplicationCacheHost() = default;
  ~MockWebApplicationCacheHost() override = default;

  void SelectCacheWithoutManifest() override {
    without_manifest_was_called_ = true;
  }
  bool SelectCacheWithManifest(const blink::WebURL& manifestURL) override {
    with_manifest_was_called_ = true;
    return true;
  }

  bool with_manifest_was_called_ = false;
  bool without_manifest_was_called_ = false;
};

}  // anonymous namespace

TEST_F(DocumentTest, CreateRangeAdjustedToTreeScopeWithPositionInShadowTree) {
  GetDocument().body()->SetInnerHTMLFromString(
      "<div><select><option>012</option></div>");
  Element* const select_element = GetDocument().QuerySelector("select");
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
    fragment->appendChild(Element::Create(HTMLNames::divTag, &doc));
    fragment->appendChild(Element::Create(HTMLNames::spanTag, &doc));
    uint64_t original_version = doc.DomTreeVersion();
    fragment->RemoveChildren();
    EXPECT_EQ(original_version + 1, doc.DomTreeVersion())
        << "RemoveChildren() should increase DomTreeVersion by 1.";
  }

  {
    DocumentFragment* fragment = DocumentFragment::Create(doc);
    Node* child = Element::Create(HTMLNames::divTag, &doc);
    child->appendChild(Element::Create(HTMLNames::spanTag, &doc));
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
  FloatSize page_size(400, 400);
  float maximum_shrink_ratio = 1.6;

  GetDocument().GetFrame()->StartPrinting(page_size, page_size,
                                          maximum_shrink_ratio);
  EXPECT_EQ(GetDocument().documentElement()->OffsetWidth(), 400);
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_EQ(GetDocument().documentElement()->OffsetWidth(), 800);
}

// This test checks that Documunt::linkManifest() returns a value conform to the
// specification.
TEST_F(DocumentTest, LinkManifest) {
  // Test the default result.
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());

  // Check that we use the first manifest with <link rel=manifest>
  auto* link = HTMLLinkElement::Create(GetDocument(), CreateElementFlags());
  link->setAttribute(blink::HTMLNames::relAttr, "manifest");
  link->setAttribute(blink::HTMLNames::hrefAttr, "foo.json");
  GetDocument().head()->AppendChild(link);
  EXPECT_EQ(link, GetDocument().LinkManifest());

  auto* link2 = HTMLLinkElement::Create(GetDocument(), CreateElementFlags());
  link2->setAttribute(blink::HTMLNames::relAttr, "manifest");
  link2->setAttribute(blink::HTMLNames::hrefAttr, "bar.json");
  GetDocument().head()->InsertBefore(link2, link);
  EXPECT_EQ(link2, GetDocument().LinkManifest());
  GetDocument().head()->AppendChild(link2);
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that crazy URLs are accepted.
  link->setAttribute(blink::HTMLNames::hrefAttr, "http:foo.json");
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that empty URLs are accepted.
  link->setAttribute(blink::HTMLNames::hrefAttr, "");
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that URLs from different origins are accepted.
  link->setAttribute(blink::HTMLNames::hrefAttr,
                     "http://example.org/manifest.json");
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::hrefAttr,
                     "http://foo.example.org/manifest.json");
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::hrefAttr,
                     "http://foo.bar/manifest.json");
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // More than one token in @rel is accepted.
  link->setAttribute(blink::HTMLNames::relAttr, "foo bar manifest");
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Such as spaces around the token.
  link->setAttribute(blink::HTMLNames::relAttr, " manifest ");
  EXPECT_EQ(link, GetDocument().LinkManifest());

  // Check that rel=manifest actually matters.
  link->setAttribute(blink::HTMLNames::relAttr, "");
  EXPECT_EQ(link2, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::relAttr, "manifest");

  // Check that link outside of the <head> are ignored.
  GetDocument().head()->RemoveChild(link);
  GetDocument().head()->RemoveChild(link2);
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());
  GetDocument().body()->AppendChild(link);
  EXPECT_EQ(nullptr, GetDocument().LinkManifest());
  GetDocument().head()->AppendChild(link);
  GetDocument().head()->AppendChild(link2);

  // Check that some attribute values do not have an effect.
  link->setAttribute(blink::HTMLNames::crossoriginAttr, "use-credentials");
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::hreflangAttr, "klingon");
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::typeAttr, "image/gif");
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::sizesAttr, "16x16");
  EXPECT_EQ(link, GetDocument().LinkManifest());
  link->setAttribute(blink::HTMLNames::mediaAttr, "print");
  EXPECT_EQ(link, GetDocument().LinkManifest());
}

TEST_F(DocumentTest, referrerPolicyParsing) {
  EXPECT_EQ(kReferrerPolicyDefault, GetDocument().GetReferrerPolicy());

  struct TestCase {
    const char* policy;
    ReferrerPolicy expected;
    bool is_legacy;
  } tests[] = {
      {"", kReferrerPolicyDefault, false},
      // Test that invalid policy values are ignored.
      {"not-a-real-policy", kReferrerPolicyDefault, false},
      {"not-a-real-policy,also-not-a-real-policy", kReferrerPolicyDefault,
       false},
      {"not-a-real-policy,unsafe-url", kReferrerPolicyAlways, false},
      {"unsafe-url,not-a-real-policy", kReferrerPolicyAlways, false},
      // Test parsing each of the policy values.
      {"always", kReferrerPolicyAlways, true},
      {"default", kReferrerPolicyNoReferrerWhenDowngrade, true},
      {"never", kReferrerPolicyNever, true},
      {"no-referrer", kReferrerPolicyNever, false},
      {"default", kReferrerPolicyNoReferrerWhenDowngrade, true},
      {"no-referrer-when-downgrade", kReferrerPolicyNoReferrerWhenDowngrade,
       false},
      {"origin", kReferrerPolicyOrigin, false},
      {"origin-when-crossorigin", kReferrerPolicyOriginWhenCrossOrigin, true},
      {"origin-when-cross-origin", kReferrerPolicyOriginWhenCrossOrigin, false},
      {"same-origin", kReferrerPolicySameOrigin, false},
      {"strict-origin", kReferrerPolicyStrictOrigin, false},
      {"strict-origin-when-cross-origin",
       kReferrerPolicyStrictOriginWhenCrossOrigin, false},
      {"unsafe-url", kReferrerPolicyAlways},
  };

  for (auto test : tests) {
    GetDocument().SetReferrerPolicy(kReferrerPolicyDefault);
    if (test.is_legacy) {
      // Legacy keyword support must be explicitly enabled for the policy to
      // parse successfully.
      GetDocument().ParseAndSetReferrerPolicy(test.policy);
      EXPECT_EQ(kReferrerPolicyDefault, GetDocument().GetReferrerPolicy());
      GetDocument().ParseAndSetReferrerPolicy(test.policy, true);
    } else {
      GetDocument().ParseAndSetReferrerPolicy(test.policy);
    }
    EXPECT_EQ(test.expected, GetDocument().GetReferrerPolicy()) << test.policy;
  }
}

TEST_F(DocumentTest, OutgoingReferrer) {
  GetDocument().SetURL(KURL("https://www.example.com/hoge#fuga?piyo"));
  GetDocument().SetSecurityOrigin(
      SecurityOrigin::Create(KURL("https://www.example.com/")));
  EXPECT_EQ("https://www.example.com/hoge", GetDocument().OutgoingReferrer());
}

TEST_F(DocumentTest, OutgoingReferrerWithUniqueOrigin) {
  GetDocument().SetURL(KURL("https://www.example.com/hoge#fuga?piyo"));
  GetDocument().SetSecurityOrigin(SecurityOrigin::CreateUniqueOpaque());
  EXPECT_EQ(String(), GetDocument().OutgoingReferrer());
}

TEST_F(DocumentTest, StyleVersion) {
  SetHtmlInnerHTML(R"HTML(
    <style>
        .a * { color: green }
        .b .c { color: green }
    </style>
    <div id='x'><span class='c'></span></div>
  )HTML");

  Element* element = GetDocument().getElementById("x");
  EXPECT_TRUE(element);

  uint64_t previous_style_version = GetDocument().StyleVersion();
  element->setAttribute(blink::HTMLNames::classAttr, "notfound");
  EXPECT_EQ(previous_style_version, GetDocument().StyleVersion());

  GetDocument().View()->UpdateAllLifecyclePhases();

  previous_style_version = GetDocument().StyleVersion();
  element->setAttribute(blink::HTMLNames::classAttr, "a");
  EXPECT_NE(previous_style_version, GetDocument().StyleVersion());

  GetDocument().View()->UpdateAllLifecyclePhases();

  previous_style_version = GetDocument().StyleVersion();
  element->setAttribute(blink::HTMLNames::classAttr, "a b");
  EXPECT_NE(previous_style_version, GetDocument().StyleVersion());
}

TEST_F(DocumentTest, EnforceSandboxFlags) {
  scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("http://example.test");
  GetDocument().SetSecurityOrigin(origin);
  SandboxFlags mask = kSandboxNavigation;
  GetDocument().EnforceSandboxFlags(mask);
  EXPECT_EQ(origin, GetDocument().GetSecurityOrigin());
  EXPECT_FALSE(GetDocument().GetSecurityOrigin()->IsPotentiallyTrustworthy());

  mask |= kSandboxOrigin;
  GetDocument().EnforceSandboxFlags(mask);
  EXPECT_TRUE(GetDocument().GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(GetDocument().GetSecurityOrigin()->IsPotentiallyTrustworthy());

  // A unique origin does not bypass secure context checks unless it
  // is also potentially trustworthy.
  url::AddStandardScheme("very-special-scheme",
                         url::SchemeType::SCHEME_WITH_HOST);
  SchemeRegistry::RegisterURLSchemeBypassingSecureContextCheck(
      "very-special-scheme");
  origin =
      SecurityOrigin::CreateFromString("very-special-scheme://example.test");
  GetDocument().SetSecurityOrigin(origin);
  GetDocument().EnforceSandboxFlags(mask);
  EXPECT_TRUE(GetDocument().GetSecurityOrigin()->IsOpaque());
  EXPECT_FALSE(GetDocument().GetSecurityOrigin()->IsPotentiallyTrustworthy());

  SchemeRegistry::RegisterURLSchemeAsSecure("very-special-scheme");
  GetDocument().SetSecurityOrigin(origin);
  GetDocument().EnforceSandboxFlags(mask);
  EXPECT_TRUE(GetDocument().GetSecurityOrigin()->IsOpaque());
  EXPECT_TRUE(GetDocument().GetSecurityOrigin()->IsPotentiallyTrustworthy());

  origin = SecurityOrigin::CreateFromString("https://example.test");
  GetDocument().SetSecurityOrigin(origin);
  GetDocument().EnforceSandboxFlags(mask);
  EXPECT_TRUE(GetDocument().GetSecurityOrigin()->IsOpaque());
  EXPECT_TRUE(GetDocument().GetSecurityOrigin()->IsPotentiallyTrustworthy());
}

TEST_F(DocumentTest, SynchronousMutationNotifier) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());

  EXPECT_EQ(GetDocument(), observer.LifecycleContext());
  EXPECT_EQ(0, observer.CountContextDestroyedCalled());

  Element* div_node = GetDocument().CreateRawElement(HTMLNames::divTag);
  GetDocument().body()->AppendChild(div_node);

  Element* bold_node = GetDocument().CreateRawElement(HTMLNames::bTag);
  div_node->AppendChild(bold_node);

  Element* italic_node = GetDocument().CreateRawElement(HTMLNames::iTag);
  div_node->AppendChild(italic_node);

  Node* text_node = GetDocument().createTextNode("0123456789");
  bold_node->AppendChild(text_node);
  EXPECT_TRUE(observer.RemovedNodes().IsEmpty());

  text_node->remove();
  ASSERT_EQ(1u, observer.RemovedNodes().size());
  EXPECT_EQ(text_node, observer.RemovedNodes()[0]);

  div_node->RemoveChildren();
  EXPECT_EQ(1u, observer.RemovedNodes().size())
      << "ContainerNode::removeChildren() doesn't call nodeWillBeRemoved()";
  ASSERT_EQ(1u, observer.RemovedChildrenNodes().size());
  EXPECT_EQ(div_node, observer.RemovedChildrenNodes()[0]);

  GetDocument().Shutdown();
  EXPECT_EQ(nullptr, observer.LifecycleContext());
  EXPECT_EQ(1, observer.CountContextDestroyedCalled());
}

TEST_F(DocumentTest, SynchronousMutationNotifieAppendChild) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());
  GetDocument().body()->AppendChild(GetDocument().createTextNode("a123456789"));
  ASSERT_EQ(1u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().body(), observer.ChildrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieInsertBefore) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());
  GetDocument().documentElement()->InsertBefore(
      GetDocument().createTextNode("a123456789"), GetDocument().body());
  ASSERT_EQ(1u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().documentElement(),
            observer.ChildrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierMergeTextNodes) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());

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
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());

  Node* move_sample = GetDocument().CreateRawElement(HTMLNames::divTag);
  move_sample->appendChild(GetDocument().createTextNode("a123"));
  move_sample->appendChild(GetDocument().createTextNode("b456"));
  GetDocument().body()->AppendChild(move_sample);

  Document& another_document = *Document::CreateForTest();
  another_document.AppendChild(move_sample);

  EXPECT_EQ(1u, observer.MoveTreeToNewDocumentNodes().size());
  EXPECT_EQ(move_sample, observer.MoveTreeToNewDocumentNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieRemoveChild) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());
  GetDocument().documentElement()->RemoveChild(GetDocument().body());
  ASSERT_EQ(1u, observer.ChildrenChangedNodes().size());
  EXPECT_EQ(GetDocument().documentElement(),
            observer.ChildrenChangedNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifieReplaceChild) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());
  Element* const replaced_node = GetDocument().body();
  GetDocument().documentElement()->ReplaceChild(
      GetDocument().CreateRawElement(HTMLNames::divTag), GetDocument().body());
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
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());

  Text* split_sample = GetDocument().createTextNode("0123456789");
  GetDocument().body()->AppendChild(split_sample);

  split_sample->splitText(4, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(1u, observer.SplitTextNodes().size());
  EXPECT_EQ(split_sample, observer.SplitTextNodes()[0]);
}

TEST_F(DocumentTest, SynchronousMutationNotifierUpdateCharacterData) {
  auto& observer = *new TestSynchronousMutationObserver(GetDocument());

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

TEST_F(DocumentTest, DocumentShutdownNotifier) {
  auto& observer = *new TestDocumentShutdownObserver(GetDocument());

  EXPECT_EQ(GetDocument(), observer.LifecycleContext());
  EXPECT_EQ(0, observer.CountContextDestroyedCalled());

  GetDocument().Shutdown();
  EXPECT_EQ(nullptr, observer.LifecycleContext());
  EXPECT_EQ(1, observer.CountContextDestroyedCalled());
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
      new MockDocumentValidationMessageClient();
  GetDocument().GetSettings()->SetScriptEnabled(true);
  GetPage().SetValidationMessageClientForTesting(mock_client);
  // ImplicitOpen()-CancelParsing() makes Document.loadEventFinished()
  // true. It's necessary to kick unload process.
  GetDocument().ImplicitOpen(kForceSynchronousParsing);
  GetDocument().CancelParsing();
  GetDocument().AppendChild(GetDocument().CreateRawElement(HTMLNames::htmlTag));
  SetHtmlInnerHTML("<body><input required></body>");
  Element* script = GetDocument().CreateRawElement(HTMLNames::scriptTag);
  script->setTextContent(
      "window.onunload = function() {"
      "document.querySelector('input').reportValidity(); };");
  GetDocument().body()->AppendChild(script);
  HTMLInputElement* input =
      ToHTMLInputElement(GetDocument().body()->firstChild());
  DVLOG(0) << GetDocument().body()->OuterHTMLAsString();

  // Sanity check.
  input->reportValidity();
  EXPECT_TRUE(mock_client->show_validation_message_was_called);
  mock_client->Reset();

  // prepareForCommit() unloads the document, and shutdown.
  GetDocument().GetFrame()->PrepareForCommit();
  EXPECT_TRUE(mock_client->document_detached_was_called);
  // Unload handler tried to show a validation message, but it should fail.
  EXPECT_FALSE(mock_client->show_validation_message_was_called);

  GetPage().SetValidationMessageClientForTesting(original_client);
}

TEST_F(DocumentTest, SandboxDisablesAppCache) {
  scoped_refptr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://test.com");
  GetDocument().SetSecurityOrigin(origin);
  SandboxFlags mask = kSandboxOrigin;
  GetDocument().EnforceSandboxFlags(mask);
  GetDocument().SetURL(KURL("https://test.com/foobar/document"));

  ApplicationCacheHost* appcache_host =
      GetDocument().Loader()->GetApplicationCacheHost();
  appcache_host->host_ = std::make_unique<MockWebApplicationCacheHost>();
  appcache_host->SelectCacheWithManifest(
      KURL("https://test.com/foobar/manifest"));
  MockWebApplicationCacheHost* mock_web_host =
      static_cast<MockWebApplicationCacheHost*>(appcache_host->host_.get());
  EXPECT_FALSE(mock_web_host->with_manifest_was_called_);
  EXPECT_TRUE(mock_web_host->without_manifest_was_called_);
}

// Verifies that calling EnsurePaintLocationDataValidForNode cleans compositor
// inputs only when necessary. We generally want to avoid cleaning the inputs,
// as it is more expensive than just doing layout.
TEST_F(DocumentTest,
       EnsurePaintLocationDataValidForNodeCompositingInputsOnlyWhenNecessary) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
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
      GetDocument().getElementById("ancestor"));
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            GetDocument().Lifecycle().GetState());

  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById("nonSticky"));
  EXPECT_EQ(DocumentLifecycle::kLayoutClean,
            GetDocument().Lifecycle().GetState());

  // However, asking for either the sticky element or it's descendents should
  // clean compositing inputs as well.
  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById("sticky"));
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            GetDocument().Lifecycle().GetState());

  // Dirty layout.
  GetDocument().body()->setAttribute("style", "background: red;");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            GetDocument().Lifecycle().GetState());

  GetDocument().EnsurePaintLocationDataValidForNode(
      GetDocument().getElementById("stickyChild"));
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
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

  Element* div = GetDocument().getElementById("recalc");
  div->setAttribute("style", "color:green");
  GetDocument().UpdateStyleAndLayoutTree();

  int new_element_count = GetDocument().GetStyleEngine().StyleForElementCount();

  EXPECT_EQ(1, new_element_count - old_element_count);
}

class InvalidatorObserver : public InterfaceInvalidator::Observer {
 public:
  void OnInvalidate() override { ++invalidate_called_counter_; }

  int CountInvalidateCalled() const { return invalidate_called_counter_; }

 private:
  int invalidate_called_counter_ = 0;
};

TEST_F(DocumentTest, InterfaceInvalidatorDestruction) {
  InvalidatorObserver obs;
  InterfaceInvalidator* invalidator = GetDocument().GetInterfaceInvalidator();
  invalidator->AddObserver(&obs);
  EXPECT_EQ(obs.CountInvalidateCalled(), 0);

  GetDocument().Shutdown();
  EXPECT_FALSE(GetDocument().GetInterfaceInvalidator());
  EXPECT_EQ(1, obs.CountInvalidateCalled());
}

TEST_F(DocumentTest, CanExecuteScriptsWithSandboxAndIsolatedWorld) {
  constexpr SandboxFlags kSandboxMask = kSandboxScripts;
  GetDocument().EnforceSandboxFlags(kSandboxMask);

  LocalFrame* frame = GetDocument().GetFrame();
  frame->GetSettings()->SetScriptEnabled(true);
  ScriptState* main_world_script_state = ToScriptStateForMainWorld(frame);
  v8::Isolate* isolate = main_world_script_state->GetIsolate();

  constexpr int kIsolatedWorldWithoutCSPId = 1;
  scoped_refptr<DOMWrapperWorld> world_without_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithoutCSPId);
  ScriptState* isolated_world_without_csp_script_state =
      ToScriptState(frame, *world_without_csp);
  ASSERT_TRUE(world_without_csp->IsIsolatedWorld());
  EXPECT_FALSE(world_without_csp->IsolatedWorldHasContentSecurityPolicy());

  constexpr int kIsolatedWorldWithCSPId = 2;
  scoped_refptr<DOMWrapperWorld> world_with_csp =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldWithCSPId);
  DOMWrapperWorld::SetIsolatedWorldContentSecurityPolicy(
      kIsolatedWorldWithCSPId, String::FromUTF8("script-src *"));
  ScriptState* isolated_world_with_csp_script_state =
      ToScriptState(frame, *world_with_csp);
  ASSERT_TRUE(world_with_csp->IsIsolatedWorld());
  EXPECT_TRUE(world_with_csp->IsolatedWorldHasContentSecurityPolicy());

  {
    // Since the page is sandboxed, main world script execution shouldn't be
    // allowed.
    ScriptState::Scope scope(main_world_script_state);
    EXPECT_FALSE(GetDocument().CanExecuteScripts(kAboutToExecuteScript));
  }
  {
    // Isolated worlds without a dedicated CSP should also not be allowed to
    // run scripts.
    ScriptState::Scope scope(isolated_world_without_csp_script_state);
    EXPECT_FALSE(GetDocument().CanExecuteScripts(kAboutToExecuteScript));
  }
  {
    // An isolated world with a CSP should bypass the main world CSP, and be
    // able to run scripts.
    ScriptState::Scope scope(isolated_world_with_csp_script_state);
    EXPECT_TRUE(GetDocument().CanExecuteScripts(kAboutToExecuteScript));
  }
}

// Android does not support non-overlay top-level scrollbars.
#if !defined(OS_ANDROID)
TEST_F(DocumentTest, ElementFromPointOnScrollbar) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  // This test requires that scrollbars take up space.
  ScopedOverlayScrollbarsForTest no_overlay_scrollbars(false);

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
  auto* content = GetDocument().getElementById("content");
  content->setAttribute("style", "width: 101%;");

  // A hit test on the horizontal scrollbar should not return an element because
  // it is outside the viewport.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 590), nullptr);
  // A hit test above the horizontal scrollbar should hit the body element.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 580), GetDocument().body());
}
#endif  // defined(OS_ANDROID)

TEST_F(DocumentTest, ElementFromPointWithPageZoom) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  // This test requires that scrollbars take up space.
  ScopedOverlayScrollbarsForTest no_overlay_scrollbars(false);

  SetHtmlInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div id='content' style='height: 10px;'>content</div>
  )HTML");

  // A hit test on the content div should hit it.
  auto* content = GetDocument().getElementById("content");
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 8), content);
  // A hit test below the content div should not hit it.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 12), GetDocument().body());

  // Zoom the page by 2x,
  GetDocument().GetFrame()->SetPageZoomFactor(2);

  // A hit test on the content div should hit it.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 8), content);
  // A hit test below the content div should not hit it.
  EXPECT_EQ(GetDocument().ElementFromPoint(1, 12), GetDocument().body());
}

/**
 * Tests for viewport-fit propagation.
 */

class ViewportFitDocumentTest : public DocumentTest {
 public:
  void SetUp() override {
    DocumentTest::SetUp();

    RuntimeEnabledFeatures::SetDisplayCutoutAPIEnabled(true);
    GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  }

  mojom::ViewportFit GetViewportFit() const {
    return GetDocument().GetViewportData().GetCurrentViewportFitForTests();
  }
};

// Test both meta and @viewport present but no viewport-fit.
TEST_F(ViewportFitDocumentTest, MetaCSSViewportButNoFit) {
  SetHtmlInnerHTML(
      "<style>@viewport { min-width: 100px; }</style>"
      "<meta name='viewport' content='initial-scale=1'>");

  EXPECT_EQ(mojom::ViewportFit::kAuto, GetViewportFit());
}

// Test @viewport present but no viewport-fit.
TEST_F(ViewportFitDocumentTest, CSSViewportButNoFit) {
  SetHtmlInnerHTML("<style>@viewport { min-width: 100px; }</style>");

  EXPECT_EQ(mojom::ViewportFit::kAuto, GetViewportFit());
}

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
using ViewportTestCase =
    std::tuple<const char*, const char*, mojom::ViewportFit>;

class ParameterizedViewportFitDocumentTest
    : public ViewportFitDocumentTest,
      public testing::WithParamInterface<ViewportTestCase> {
 protected:
  void LoadTestHTML() {
    const char* kMetaValue = std::get<0>(GetParam());
    const char* kCSSValue = std::get<1>(GetParam());
    StringBuilder html;

    if (kCSSValue) {
      html.Append("<style>@viewport { viewport-fit: ");
      html.Append(kCSSValue);
      html.Append("; }</style>");
    }

    if (kMetaValue) {
      html.Append("<meta name='viewport' content='viewport-fit=");
      html.Append(kMetaValue);
      html.Append("'>");
    }

    GetDocument().documentElement()->SetInnerHTMLFromString(html.ToString());
    GetDocument().View()->UpdateAllLifecyclePhases();
  }
};

TEST_P(ParameterizedViewportFitDocumentTest, EffectiveViewportFit) {
  LoadTestHTML();
  EXPECT_EQ(std::get<2>(GetParam()), GetViewportFit());
}

INSTANTIATE_TEST_CASE_P(
    All,
    ParameterizedViewportFitDocumentTest,
    testing::Values(
        // Test the default case.
        ViewportTestCase(nullptr, nullptr, mojom::ViewportFit::kAuto),
        // Test the different values set through CSS.
        ViewportTestCase(nullptr, "auto", mojom::ViewportFit::kAuto),
        ViewportTestCase(nullptr, "contain", mojom::ViewportFit::kContain),
        ViewportTestCase(nullptr, "cover", mojom::ViewportFit::kCover),
        ViewportTestCase(nullptr, "invalid", mojom::ViewportFit::kAuto),
        // Test the different values set through the meta tag.
        ViewportTestCase("auto", nullptr, mojom::ViewportFit::kAuto),
        ViewportTestCase("contain", nullptr, mojom::ViewportFit::kContain),
        ViewportTestCase("cover", nullptr, mojom::ViewportFit::kCover),
        ViewportTestCase("invalid", nullptr, mojom::ViewportFit::kAuto),
        // Test that the CSS should override the meta tag.
        ViewportTestCase("cover", "auto", mojom::ViewportFit::kAuto),
        ViewportTestCase("cover", "contain", mojom::ViewportFit::kContain)));

}  // namespace blink
