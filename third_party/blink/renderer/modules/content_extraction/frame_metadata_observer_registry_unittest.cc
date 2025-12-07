// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/frame_metadata_observer_registry.h"

#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom-blink.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class MockPaidContentMetadataObserver
    : public mojom::blink::PaidContentMetadataObserver {
 public:
  MockPaidContentMetadataObserver() = default;
  ~MockPaidContentMetadataObserver() override = default;

  mojo::PendingRemote<mojom::blink::PaidContentMetadataObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::blink::PaidContentMetadataObserver:
  void OnPaidContentMetadataChanged(bool has_paid_content) override {
    future_.SetValue(has_paid_content);
  }

  base::test::TestFuture<bool>& future() { return future_; }

 private:
  base::test::TestFuture<bool> future_;
  mojo::Receiver<mojom::blink::PaidContentMetadataObserver> receiver_{this};
};

class MockMetaTagsObserver : public mojom::blink::MetaTagsObserver {
 public:
  MockMetaTagsObserver() = default;
  ~MockMetaTagsObserver() override = default;

  mojo::PendingRemote<mojom::blink::MetaTagsObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::blink::MetaTagsObserver:
  void OnMetaTagsChanged(Vector<mojom::blink::MetaTagPtr> meta_tags) override {
    future_.SetValue(std::move(meta_tags));
  }

  base::test::TestFuture<Vector<mojom::blink::MetaTagPtr>>& future() {
    return future_;
  }

 private:
  base::test::TestFuture<Vector<mojom::blink::MetaTagPtr>> future_;
  mojo::Receiver<mojom::blink::MetaTagsObserver> receiver_{this};
};

class FrameMetadataObserverRegistryTest : public testing::Test {
 public:
  void SetUp() override { helper_.Initialize(); }

  void BindRegistry() {
    mojo::Remote<mojom::blink::FrameMetadataObserverRegistry> remote;
    FrameMetadataObserverRegistry::BindReceiver(
        helper_.LocalMainFrame()->GetFrame(),
        remote.BindNewPipeAndPassReceiver());
    registry_ = FrameMetadataObserverRegistry::From(*GetDocument());
    ASSERT_TRUE(registry_);
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(
        helper_.LocalMainFrame(), html,
        url_test_helpers::ToKURL("https://example.com/"));
  }

  Document* GetDocument() {
    return helper_.LocalMainFrame()->GetFrame()->GetDocument();
  }

  void VerifyAuthorMetaTag(const Vector<mojom::blink::MetaTagPtr>& meta_tags) {
    ASSERT_EQ(meta_tags.size(), 1u);
    EXPECT_EQ(meta_tags[0]->name, "author");
    EXPECT_EQ(meta_tags[0]->content, "Gary");
  }

  void VerifyAuthorMetaTagNoContent(
      const Vector<mojom::blink::MetaTagPtr>& meta_tags) {
    ASSERT_EQ(meta_tags.size(), 1u);
    EXPECT_EQ(meta_tags[0]->name, "author");
    EXPECT_EQ(meta_tags[0]->content, "");
  }

 protected:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
  Persistent<FrameMetadataObserverRegistry> registry_;
};

TEST_F(FrameMetadataObserverRegistryTest, PaidContent) {
  LoadHTML(R"HTML(
    <head>
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false
      }</script>
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Get());
}

TEST_F(FrameMetadataObserverRegistryTest, NoPaidContent) {
  LoadHTML("<body></body>");
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  EXPECT_FALSE(observer.future().IsReady());
}

TEST_F(FrameMetadataObserverRegistryTest, LateObserver) {
  LoadHTML(R"HTML(
    <head>
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false
      }</script>
    </head>
    <body></body>
  )HTML");
  // Ensure DOM is fully loaded before adding observer.
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Get());
}

TEST_F(FrameMetadataObserverRegistryTest, PaidContentAddedDynamically) {
  LoadHTML(R"HTML(
    <head>
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  // No paid content initially.
  EXPECT_FALSE(observer.future().IsReady());

  // Dynamically add paid content.
  auto* script = MakeGarbageCollected<HTMLScriptElement>(*GetDocument(),
                                                         CreateElementFlags());
  script->setAttribute(html_names::kTypeAttr,
                       AtomicString("application/ld+json"));
  script->setTextContent(R"JSON({
    "@context": "http://schema.org",
    "@type": "NewsArticle",
    "isAccessibleForFree": false
  })JSON");
  GetDocument()->head()->AppendChild(script);
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Get());
}

TEST_F(FrameMetadataObserverRegistryTest,
       PaidContentUnaffectedByOtherElements) {
  LoadHTML(R"HTML(
    <head>
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false
      }</script>
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Get());

  // Add a meta element, which should not trigger the observer.
  observer.future().Clear();
  auto* meta_element = MakeGarbageCollected<HTMLMetaElement>(
      *GetDocument(), CreateElementFlags());
  meta_element->setAttribute(html_names::kNameAttr, AtomicString("author"));
  meta_element->setAttribute(html_names::kContentAttr, AtomicString("Gary"));
  GetDocument()->head()->AppendChild(meta_element);
  test::RunPendingTasks();

  EXPECT_FALSE(observer.future().IsReady());
}

TEST_F(FrameMetadataObserverRegistryTest,
       PaidContentWithSchemaOrgTrailingSlash) {
  LoadHTML(R"HTML(
    <head>
      <script type="application/ld+json">{
        "@context": "http://schema.org/",
        "@type": "NewsArticle",
        "isAccessibleForFree": false
      }</script>
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Get());
}

TEST_F(FrameMetadataObserverRegistryTest, PaidContentWithUnescapedNewlines) {
  LoadHTML(R"HTML(
    <head>
      <script type="application/ld+json">{
        "@context": "http://schema.org",
        "@type": "NewsArticle",
        "isAccessibleForFree": false,
        "description": "This is a description
with unescaped newlines."
      }</script>
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockPaidContentMetadataObserver observer;
  registry_->AddPaidContentMetadataObserver(
      observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Get());
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTags) {
  LoadHTML(R"HTML(
    <head>
      <meta name="author" content="Gary">
      <meta name="keywords" content="test">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");
  names_to_observe.push_back("subject");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  auto meta_tags = observer.future().Take();
  VerifyAuthorMetaTag(meta_tags);
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTagsLateObserver) {
  LoadHTML(R"HTML(
    <head>
      <meta name="author" content="Gary">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");
  names_to_observe.push_back("subject");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  VerifyAuthorMetaTag(observer.future().Take());
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTagsNameMismatch) {
  LoadHTML(R"HTML(
    <head>
      <meta name="author" content="Gary">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("subject");
  names_to_observe.push_back("category");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Take().empty());
}

TEST_F(FrameMetadataObserverRegistryTest, NoMetaTags) {
  LoadHTML("<body></body>");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");
  names_to_observe.push_back("subject");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Take().empty());
}

TEST_F(FrameMetadataObserverRegistryTest,
       MetaTagsInitialUpdateWithNoMatchingTags) {
  LoadHTML("<body></body>");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Take().empty());
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTagsUpdated) {
  LoadHTML(R"HTML(
    <head>
      <meta name="author" content="Gary">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");
  names_to_observe.push_back("subject");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  // Initial state.
  ASSERT_TRUE(observer.future().IsReady());
  VerifyAuthorMetaTag(observer.future().Take());

  // Modify an existing tag.
  observer.future().Clear();
  auto* meta_element = To<HTMLMetaElement>(
      GetDocument()->head()->QuerySelector(AtomicString("meta[name=author]")));
  ASSERT_TRUE(meta_element);
  meta_element->setAttribute(html_names::kContentAttr, AtomicString("Val"));
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  auto meta_tags1 = observer.future().Take();
  EXPECT_EQ(meta_tags1.size(), 1u);
  EXPECT_EQ(meta_tags1[0]->name, "author");
  EXPECT_EQ(meta_tags1[0]->content, "Val");

  // Add a new tag.
  observer.future().Clear();
  auto* new_meta = MakeGarbageCollected<HTMLMetaElement>(*GetDocument(),
                                                         CreateElementFlags());
  new_meta->setAttribute(html_names::kNameAttr, AtomicString("subject"));
  new_meta->setAttribute(html_names::kContentAttr, AtomicString("testing"));
  GetDocument()->head()->AppendChild(new_meta);
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  auto meta_tags2 = observer.future().Take();
  EXPECT_EQ(meta_tags2.size(), 2u);
  bool author_found = false;
  bool subject_found = false;
  for (const auto& tag : meta_tags2) {
    if (tag->name == "author") {
      author_found = true;
      EXPECT_EQ(tag->content, "Val");
    } else if (tag->name == "subject") {
      subject_found = true;
      EXPECT_EQ(tag->content, "testing");
    }
  }
  EXPECT_TRUE(author_found);
  EXPECT_TRUE(subject_found);

  // Remove a tag.
  observer.future().Clear();
  meta_element = To<HTMLMetaElement>(
      GetDocument()->head()->QuerySelector(AtomicString("meta[name=author]")));
  ASSERT_TRUE(meta_element);
  meta_element->remove();
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  auto meta_tags3 = observer.future().Take();
  EXPECT_EQ(meta_tags3.size(), 1u);
  EXPECT_EQ(meta_tags3[0]->name, "subject");
  EXPECT_EQ(meta_tags3[0]->content, "testing");
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTagsUnaffectedByOtherElements) {
  LoadHTML(R"HTML(
    <head>
      <meta name="author" content="Gary">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  // Initial state.
  ASSERT_TRUE(observer.future().IsReady());
  VerifyAuthorMetaTag(observer.future().Take());

  // Add a script element, which should not trigger the observer.
  observer.future().Clear();
  auto* script_element = MakeGarbageCollected<HTMLScriptElement>(
      *GetDocument(), CreateElementFlags());
  script_element->setTextContent("console.log('hello');");
  GetDocument()->head()->AppendChild(script_element);
  test::RunPendingTasks();

  EXPECT_FALSE(observer.future().IsReady());
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTagsWithNamelessTag) {
  LoadHTML(R"HTML(
    <head>
      <meta charset="UTF-8">
      <meta name="author" content="Gary">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  VerifyAuthorMetaTag(observer.future().Take());
}

TEST_F(FrameMetadataObserverRegistryTest, MetaTagsWithNoContent) {
  LoadHTML(R"HTML(
    <head>
      <meta charset="UTF-8">
      <meta name="author">
    </head>
    <body></body>
  )HTML");
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  VerifyAuthorMetaTagNoContent(observer.future().Take());
}

// Re-enable this test once we support observing head elements that are added
// dynamically.
TEST_F(FrameMetadataObserverRegistryTest, MetaTagsAddedWithHead) {
  LoadHTML("<body></body>");
  // Remove the head that was automatically added by the parser, to simulate a
  // document that starts without one.
  GetDocument()->head()->remove();
  ASSERT_FALSE(GetDocument()->head());
  BindRegistry();

  MockMetaTagsObserver observer;
  Vector<String> names_to_observe;
  names_to_observe.push_back("author");

  registry_->AddMetaTagsObserver(names_to_observe,
                                 observer.BindNewPipeAndPassRemote());
  test::RunPendingTasks();

  // Initially, no head and no meta tags. An empty update should be sent.
  ASSERT_TRUE(observer.future().IsReady());
  EXPECT_TRUE(observer.future().Take().empty());

  // Dynamically add a head and meta tag.
  auto* head = MakeGarbageCollected<HTMLHeadElement>(*GetDocument());
  auto* meta = MakeGarbageCollected<HTMLMetaElement>(*GetDocument(),
                                                     CreateElementFlags());
  meta->setAttribute(html_names::kNameAttr, AtomicString("author"));
  meta->setAttribute(html_names::kContentAttr, AtomicString("Gary"));
  head->AppendChild(meta);
  GetDocument()->documentElement()->AppendChild(head);
  test::RunPendingTasks();

  ASSERT_TRUE(observer.future().IsReady());
  VerifyAuthorMetaTag(observer.future().Take());
}

}  // namespace

}  // namespace blink
