// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"

#include <memory>
#include <optional>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder_builder.h"
#include "third_party/blink/renderer/core/loader/no_state_prefetch_client.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class MockNoStatePrefetchClient : public NoStatePrefetchClient {
 public:
  MockNoStatePrefetchClient(Page& page, bool is_prefetch_only)
      : NoStatePrefetchClient(page, nullptr),
        is_prefetch_only_(is_prefetch_only) {}

 private:
  bool IsPrefetchOnly() override { return is_prefetch_only_; }

  bool is_prefetch_only_;
};

class HTMLDocumentParserTest
    : public PageTestBase,
      public testing::WithParamInterface<ParserSynchronizationPolicy> {
 public:
  ParserSynchronizationPolicy Policy() const { return GetParam(); }

 protected:
  HTMLDocumentParserTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        original_force_synchronous_parsing_for_testing_(
            Document::ForceSynchronousParsingForTesting()) {
    Document::SetForceSynchronousParsingForTesting(Policy() ==
                                                   kForceSynchronousParsing);
  }
  ~HTMLDocumentParserTest() override {
    // Finish the pending tasks which may require the runtime enabled flags,
    // before restoring the flags.
    base::RunLoop().RunUntilIdle();
    Document::SetForceSynchronousParsingForTesting(
        original_force_synchronous_parsing_for_testing_);
  }

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetURL(KURL("https://example.test"));
  }

  HTMLDocumentParser* CreateParser(HTMLDocument& document) {
    auto* parser =
        MakeGarbageCollected<HTMLDocumentParser>(document, GetParam());
    std::unique_ptr<TextResourceDecoder> decoder(
        BuildTextResourceDecoder(document.GetFrame(), document.Url(),
                                 AtomicString("text/html"), g_null_atom));
    parser->SetDecoder(std::move(decoder));
    return parser;
  }

 private:
  bool original_force_synchronous_parsing_for_testing_;
};

// Calls DocumentParser::Detach() in the destructor. Used to ensure detach is
// called, as otherwise some assertions may be triggered.
class ScopedParserDetacher {
 public:
  explicit ScopedParserDetacher(DocumentParser* parser) : parser_(parser) {}

  explicit ScopedParserDetacher(HTMLDocumentParser* parser)
      : ScopedParserDetacher(static_cast<DocumentParser*>(parser)) {}

  ~ScopedParserDetacher() { parser_->Detach(); }

 private:
  UntracedMember<DocumentParser> parser_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(HTMLDocumentParserTest,
                         HTMLDocumentParserTest,
                         testing::Values(kForceSynchronousParsing,
                                         kAllowDeferredParsing));

TEST_P(HTMLDocumentParserTest, StopThenPrepareToStopShouldNotCrash) {
  auto& document = To<HTMLDocument>(GetDocument());
  DocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);
  parser->AppendBytes(base::byte_span_from_cstring("<html>"));
  // These methods are not supposed to be called one after the other, but in
  // practice it can happen (e.g. if navigation is aborted).
  parser->StopParsing();
  parser->PrepareToStopParsing();
}

TEST_P(HTMLDocumentParserTest, DeferTreeBuilderFlush) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["initial_interval"] = "16ms";
  params["max_interval"] = "160ms";
  params["multiplier"] = "2.0";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kDeferTreeBuilderFlush, params);
  HTMLTreeBuilder::ResetCachedFeaturesForTesting();
  auto& document = To<HTMLDocument>(GetDocument());
  if (document.documentElement()) {
    document.removeChild(document.documentElement());
  }
  auto* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  // Append a chunk of data with a script tag (kTextMode) and run the
  // tasks that were queued by Flush() without advancing the clock.
  parser->AppendBytes(base::byte_span_from_cstring("<script>aaa"));
  parser->Flush();
  task_environment().FastForwardBy(base::TimeDelta());

  // The first flush in kTextMode is allowed immediately.
  Element* script = document.QuerySelector(AtomicString("script"));
  ASSERT_TRUE(script);
  ASSERT_TRUE(script->firstChild());
  EXPECT_EQ(Node::kTextNode, script->firstChild()->getNodeType());
  EXPECT_EQ("aaa", script->firstChild()->nodeValue());

  // Append a second chunk. Flushes should now be deferred.
  parser->AppendBytes(base::byte_span_from_cstring("bbb"));
  parser->Flush();
  task_environment().FastForwardBy(base::TimeDelta());

  // Text node should still only contain "aaa".
  EXPECT_EQ("aaa", script->firstChild()->nodeValue());

  // Fast forward by more than the initial interval (16ms).
  task_environment().FastForwardBy(base::Milliseconds(20));
  parser->AppendBytes(base::byte_span_from_cstring("ccc"));
  task_environment().FastForwardBy(base::TimeDelta());

  // Now it should contain "aaabbbccc".
  EXPECT_EQ(1u, script->childNodes()->length());
  EXPECT_EQ("aaabbbccc", script->firstChild()->nodeValue());

  // Append a fourth chunk. Flushes should now be deferred again because the
  // interval doubled to 32ms.
  parser->AppendBytes(base::byte_span_from_cstring("ddd"));
  // Check that it is deferred.
  EXPECT_EQ("aaabbbccc", script->firstChild()->nodeValue());

  // Now close the script. Leaving kTextMode should cause everything to be
  // flushed.
  parser->AppendBytes(base::byte_span_from_cstring("</script>"));
  parser->Flush();
  task_environment().FastForwardBy(base::TimeDelta());

  ASSERT_TRUE(script->firstChild());
  EXPECT_EQ(Node::kTextNode, script->firstChild()->getNodeType());
  EXPECT_EQ("aaabbbcccddd", script->firstChild()->nodeValue());
}

TEST_P(HTMLDocumentParserTest, DeferTreeBuilderFlushEOF) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["initial_interval"] = "16ms";
  params["max_interval"] = "160ms";
  params["multiplier"] = "2.0";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kDeferTreeBuilderFlush, params);
  HTMLTreeBuilder::ResetCachedFeaturesForTesting();
  auto& document = To<HTMLDocument>(GetDocument());
  if (document.documentElement()) {
    document.removeChild(document.documentElement());
  }
  auto* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  // Append a chunk of data with a script tag (kTextMode) and run the tasks
  // that were queued by Flush() without advancing the clock.
  parser->AppendBytes(base::byte_span_from_cstring("<script>aaa"));
  parser->Flush();
  task_environment().FastForwardBy(base::TimeDelta());

  // The first flush in kTextMode is allowed immediately.
  Element* script = document.QuerySelector(AtomicString("script"));
  ASSERT_TRUE(script);
  ASSERT_TRUE(script->firstChild());
  EXPECT_EQ("aaa", script->firstChild()->nodeValue());

  // Append a second chunk (this should be deferred).
  parser->AppendBytes(base::byte_span_from_cstring("bbb"));
  parser->Flush();
  task_environment().FastForwardBy(base::TimeDelta());

  // Should still be "aaa".
  EXPECT_EQ("aaa", script->firstChild()->nodeValue());

  // Close the stream without an explicit end tag to make sure the text node is
  // still created.
  static_cast<DocumentParser*>(parser)->Finish();

  // Run any queued tasks without advancing the clock.
  task_environment().FastForwardBy(base::TimeDelta());

  ASSERT_TRUE(script->firstChild());
  EXPECT_EQ(Node::kTextNode, script->firstChild()->getNodeType());
  EXPECT_EQ("aaabbb", script->firstChild()->nodeValue());
}

TEST_P(HTMLDocumentParserTest, DeferTreeBuilderFlushDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kDeferTreeBuilderFlush);
  HTMLTreeBuilder::ResetCachedFeaturesForTesting();
  auto& document = To<HTMLDocument>(GetDocument());
  if (document.documentElement()) {
    document.removeChild(document.documentElement());
  }
  auto* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  // With the flush deferring feature disabled, flushes in kTextMode should
  // happen immediately.
  parser->AppendBytes(base::byte_span_from_cstring("<script>aaa"));
  parser->Flush();
  // Run the tasks that were queued by Flush() without advancing the clock.
  task_environment().FastForwardBy(base::TimeDelta());

  Element* script = document.QuerySelector(AtomicString("script"));
  ASSERT_TRUE(script);
  ASSERT_TRUE(script->firstChild());
  EXPECT_EQ(Node::kTextNode, script->firstChild()->getNodeType());
  EXPECT_EQ("aaa", script->firstChild()->nodeValue());

  // Append a second chunk. It should also flush immediately.
  parser->AppendBytes(base::byte_span_from_cstring("bbb"));
  parser->Flush();
  task_environment().FastForwardBy(base::TimeDelta());

  EXPECT_EQ("aaabbb", script->firstChild()->nodeValue());
}

TEST_P(HTMLDocumentParserTest, HasNoPendingWorkAfterStopParsing) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  DocumentParser* control_parser = static_cast<DocumentParser*>(parser);
  ScopedParserDetacher detacher(control_parser);
  control_parser->AppendBytes(base::byte_span_from_cstring("<html>"));
  control_parser->StopParsing();
  EXPECT_FALSE(parser->HasPendingWorkScheduledForTesting());
}

TEST_P(HTMLDocumentParserTest, HasNoPendingWorkAfterStopParsingThenAppend) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  DocumentParser* control_parser = static_cast<DocumentParser*>(parser);
  ScopedParserDetacher detacher(control_parser);
  control_parser->AppendBytes(base::byte_span_from_cstring("<html>"));
  control_parser->StopParsing();
  control_parser->AppendBytes(base::byte_span_from_cstring("<head>"));
  EXPECT_FALSE(parser->HasPendingWorkScheduledForTesting());
}

TEST_P(HTMLDocumentParserTest, HasNoPendingWorkAfterDetach) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  DocumentParser* control_parser = static_cast<DocumentParser*>(parser);
  control_parser->AppendBytes(base::byte_span_from_cstring("<html>"));
  control_parser->Detach();
  EXPECT_FALSE(parser->HasPendingWorkScheduledForTesting());
}

TEST_P(HTMLDocumentParserTest, AppendPrefetch) {
  auto& document = To<HTMLDocument>(GetDocument());
  ProvideNoStatePrefetchClientTo(
      *document.GetPage(), MakeGarbageCollected<MockNoStatePrefetchClient>(
                               *document.GetPage(), true));
  EXPECT_TRUE(document.IsPrefetchOnly());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<httttttt"));
  // The bytes are forwarded to the preload scanner, not to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_TRUE(script_runner_host->HasPreloadScanner());
  // Finishing should not cause parsing to start (verified via an internal
  // DCHECK).
  EXPECT_FALSE(parser->DidPumpTokenizerForTesting());
  static_cast<DocumentParser*>(parser)->Finish();
  EXPECT_FALSE(parser->DidPumpTokenizerForTesting());
  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, AppendNoPrefetch) {
  auto& document = To<HTMLDocument>(GetDocument());
  EXPECT_FALSE(document.IsPrefetchOnly());
  // Use ForceSynchronousParsing to allow calling append().
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<htttttt"));
  test::RunPendingTasks();
  // The bytes are forwarded to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_EQ(script_runner_host->HasPreloadScanner(),
            GetParam() == kAllowDeferredParsing);
  EXPECT_TRUE(parser->DidPumpTokenizerForTesting());
  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, ProcessingInstruction) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<?target data?>"));
  test::RunPendingTasks();

  Node* last = document.lastChild();
  ASSERT_TRUE(last);
  EXPECT_EQ(Node::kProcessingInstructionNode, last->getNodeType());
  ProcessingInstruction* pi = To<ProcessingInstruction>(last);
  EXPECT_EQ("target", pi->target());
  EXPECT_EQ("data", pi->data());

  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, ProcessingInstructionSimpleAttribute) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<?target name=\"n\">"));
  test::RunPendingTasks();

  Node* last = document.lastChild();
  ASSERT_TRUE(last);
  EXPECT_EQ(Node::kProcessingInstructionNode, last->getNodeType());
  ProcessingInstruction* pi = To<ProcessingInstruction>(last);
  EXPECT_EQ("target", pi->target());
  EXPECT_EQ("name=\"n\"", pi->data());
  EXPECT_EQ("n", pi->getAttribute(AtomicString("name")));

  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, ProcessingInstructionAttributeChange) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<?target name=\"n\">"));
  test::RunPendingTasks();

  Node* last = document.lastChild();
  ASSERT_TRUE(last);
  EXPECT_EQ(Node::kProcessingInstructionNode, last->getNodeType());
  ProcessingInstruction* pi = To<ProcessingInstruction>(last);
  EXPECT_EQ("target", pi->target());
  EXPECT_EQ("name=\"n\"", pi->data());
  EXPECT_EQ("n", pi->getAttribute(AtomicString("name")));
  pi->setData("name=\"m\" value=v");
  EXPECT_EQ("name=\"m\" value=v", pi->data());
  EXPECT_EQ("m", pi->getAttribute(AtomicString("name")));
  EXPECT_EQ("v", pi->getAttribute(AtomicString("value")));

  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, ProcessingInstructionGTSign) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<?target name=\"n\">"));
  test::RunPendingTasks();

  Node* last = document.lastChild();
  ASSERT_TRUE(last);
  EXPECT_EQ(Node::kProcessingInstructionNode, last->getNodeType());
  ProcessingInstruction* pi = To<ProcessingInstruction>(last);
  pi->setData("name=n > value=v");
  EXPECT_EQ("target", pi->target());
  EXPECT_EQ("name=n > value=v", pi->data());
  EXPECT_EQ("n", pi->getAttribute(AtomicString("name")));
  EXPECT_EQ(g_null_atom, pi->getAttribute(AtomicString("value")));

  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, ProcessingInstructionEmptyAttribute) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<?target name=\"\">"));
  test::RunPendingTasks();

  Node* last = document.lastChild();
  ASSERT_TRUE(last);
  EXPECT_EQ(Node::kProcessingInstructionNode, last->getNodeType());
  ProcessingInstruction* pi = To<ProcessingInstruction>(last);
  EXPECT_EQ("target", pi->target());
  EXPECT_EQ("name=\"\"", pi->data());
  EXPECT_EQ("", pi->getAttribute(AtomicString("name")));

  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_P(HTMLDocumentParserTest, ProcessingInstructionttributeNoQuotes) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  parser->AppendBytes(base::byte_span_from_cstring("<?target name=v>"));
  test::RunPendingTasks();

  Node* last = document.lastChild();
  ASSERT_TRUE(last);
  EXPECT_EQ(Node::kProcessingInstructionNode, last->getNodeType());
  ProcessingInstruction* pi = To<ProcessingInstruction>(last);
  EXPECT_EQ("target", pi->target());
  EXPECT_EQ("name=v", pi->data());
  EXPECT_EQ("v", pi->getAttribute(AtomicString("name")));

  static_cast<DocumentParser*>(parser)->StopParsing();
}

class HTMLDocumentParserThreadedPreloadYieldModeScannerTest
    : public HTMLDocumentParserTest {
 public:
  HTMLDocumentParserThreadedPreloadYieldModeScannerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kThreadedPreloadScanner,
          {{"preload-processing-mode", "yield"}}}},
        /*disabled_features=*/
        {});
    HTMLDocumentParser::ResetCachedFeaturesForTesting();
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(features::kThreadedPreloadScanner));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(HTMLDocumentParserThreadedPreloadYieldModeScannerTest,
                         HTMLDocumentParserThreadedPreloadYieldModeScannerTest,
                         testing::Values(kForceSynchronousParsing,
                                         kAllowDeferredParsing));

TEST_P(HTMLDocumentParserThreadedPreloadYieldModeScannerTest,
       BasicPendingPreloadsBehaviour) {
  auto& document = To<HTMLDocument>(GetDocument());
  document.Fetcher()->EnableIsPreloadedForTest();
  HTMLDocumentParser* parser = CreateParser(document);
  EXPECT_FALSE(parser->HasPendingPreloads());
  parser->AppendBytes(base::byte_span_from_cstring("<img src='preload.png'>"));

  const auto preload_url =
      document.CompleteURL("https://example.test/preload.png");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return document.Fetcher()->AllResources().Contains(preload_url);
  }));
  HTMLDocumentParser::FlushPreloadScannerThreadForTesting();

  EXPECT_FALSE(parser->HasPendingPreloads());
  if (Policy() == kAllowDeferredParsing) {
    // There are no more pending preloads, but ensure we had one and it has been
    // processed OR the preload URL has been regularly loaded.
    EXPECT_TRUE(document.Fetcher()->IsPreloadedForTest(preload_url) ||
                document.Fetcher()->AllResources().Contains(preload_url));
  } else {
    EXPECT_FALSE(document.Fetcher()->IsPreloadedForTest(preload_url));
  }
}

class HTMLDocumentParserThreadedPreloadScannerTest : public PageTestBase {
 protected:
  HTMLDocumentParserThreadedPreloadScannerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kThreadedPreloadScanner, features::kPrecompileInlineScripts},
        {});
    HTMLDocumentParser::ResetCachedFeaturesForTesting();
  }

  ~HTMLDocumentParserThreadedPreloadScannerTest() override {
    scoped_feature_list_.Reset();
    HTMLDocumentParser::ResetCachedFeaturesForTesting();
  }

  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetURL(KURL("https://example.test"));
  }

  HTMLDocumentParser* CreateParser(HTMLDocument& document) {
    return MakeGarbageCollected<HTMLDocumentParser>(document,
                                                    kAllowDeferredParsing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HTMLDocumentParserThreadedPreloadScannerTest,
       TakeBackgroundScanCallback) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);

  // First append "foo" script which should be passed through to the scanner.
  parser->AppendDecodedData("<script>foo</script>", DocumentEncodingData());
  HTMLDocumentParser::FlushPreloadScannerThreadForTesting();
  EXPECT_TRUE(parser->HasInlineScriptStreamerForTesting("foo"));

  // Now take the callback.
  auto callback =
      static_cast<DocumentParser*>(parser)->TakeBackgroundScanCallback();

  // Append "bar" script which should not be passed to the scanner.
  parser->AppendDecodedData("<script>bar</script>", DocumentEncodingData());
  HTMLDocumentParser::FlushPreloadScannerThreadForTesting();
  EXPECT_FALSE(parser->HasInlineScriptStreamerForTesting("bar"));

  // Append "baz" script to the callback which should be passed to the scanner.
  callback.Run("<script>baz</script>");
  HTMLDocumentParser::FlushPreloadScannerThreadForTesting();
  EXPECT_TRUE(parser->HasInlineScriptStreamerForTesting("baz"));

  static_cast<DocumentParser*>(parser)->StopParsing();
}

class HTMLDocumentParserProcessImmediatelyTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().SetURL(KURL("https://example.test"));
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

  static HTMLDocumentParser* CreateParser(HTMLDocument& document) {
    auto* parser = MakeGarbageCollected<HTMLDocumentParser>(
        document, kAllowDeferredParsing);
    std::unique_ptr<TextResourceDecoder> decoder(
        BuildTextResourceDecoder(document.GetFrame(), document.Url(),
                                 AtomicString("text/html"), g_null_atom));
    parser->SetDecoder(std::move(decoder));
    return parser;
  }

  static HTMLDocumentParser* ConfigureWebViewHelperForChildFrameAndCreateParser(
      frame_test_helpers::WebViewHelper& web_view_helper) {
    std::string base_url = "http://internal.test/";
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUtf8(base_url), test::CoreTestDataPath(),
        WebString("visible_iframe.html"));
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUtf8(base_url), test::CoreTestDataPath(),
        WebString("single_iframe.html"));

    WebViewImpl* web_view_impl =
        web_view_helper.InitializeAndLoad(base_url + "single_iframe.html");

    web_view_impl->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);

    Document* top_doc =
        web_view_impl->MainFrameImpl()->GetFrame()->GetDocument();
    auto* iframe =
        To<HTMLIFrameElement>(top_doc->QuerySelector(AtomicString("iframe")));
    Document* child_document = iframe->contentDocument();
    return child_document ? CreateParser(To<HTMLDocument>(*child_document))
                          : nullptr;
  }
};

TEST_F(HTMLDocumentParserProcessImmediatelyTest, FirstChunk) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kProcessHtmlDataImmediately,
      {{features::kProcessHtmlDataImmediatelyFirstChunk.name, "true"},
       {features::kProcessHtmlDataImmediatelyMainFrame.name, "true"}});
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);
  parser->AppendBytes(base::byte_span_from_cstring("<htttttt"));
  // Because kProcessHtmlDataImmediatelyFirstChunk is set,
  // DidPumpTokenizerForTesting() should be true.
  EXPECT_TRUE(parser->DidPumpTokenizerForTesting());
  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_F(HTMLDocumentParserProcessImmediatelyTest, SecondChunk) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kProcessHtmlDataImmediately,
      {{features::kProcessHtmlDataImmediatelySubsequentChunks.name, "true"},
       {features::kProcessHtmlDataImmediatelyMainFrame.name, "true"}});
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  ScopedParserDetacher detacher(parser);
  const auto kBytes = base::byte_span_from_cstring("<div><div><div>");
  parser->AppendBytes(kBytes);
  // The first chunk should not have been processed yet (it was scheduled).
  EXPECT_FALSE(parser->DidPumpTokenizerForTesting());
  test::RunPendingTasks();
  EXPECT_TRUE(parser->DidPumpTokenizerForTesting());
  EXPECT_EQ(1u, parser->GetChunkCountForTesting());
  parser->AppendBytes(kBytes);
  // As kProcessHtmlDataImmediatelySubsequentChunks is true, the second chunk
  // should be processed immediately.
  EXPECT_EQ(2u, parser->GetChunkCountForTesting());
  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_F(HTMLDocumentParserProcessImmediatelyTest, FirstChunkChildFrame) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kProcessHtmlDataImmediately,
      {{features::kProcessHtmlDataImmediatelyChildFrame.name, "true"},
       {features::kProcessHtmlDataImmediatelyFirstChunk.name, "true"}});
  frame_test_helpers::WebViewHelper web_view_helper;
  HTMLDocumentParser* parser =
      ConfigureWebViewHelperForChildFrameAndCreateParser(web_view_helper);
  ASSERT_TRUE(parser);
  ScopedParserDetacher detacher(parser);
  parser->AppendBytes(base::byte_span_from_cstring("<div><div><div>"));
  // The first chunk should been processed.
  EXPECT_TRUE(parser->DidPumpTokenizerForTesting());

  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  static_cast<DocumentParser*>(parser)->StopParsing();
}

TEST_F(HTMLDocumentParserProcessImmediatelyTest, FirstChunkDelayedChildFrame) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kProcessHtmlDataImmediately,
      {{features::kProcessHtmlDataImmediatelyChildFrame.name, "true"},
       {features::kProcessHtmlDataImmediatelyFirstChunk.name, "false"}});
  frame_test_helpers::WebViewHelper web_view_helper;
  HTMLDocumentParser* parser =
      ConfigureWebViewHelperForChildFrameAndCreateParser(web_view_helper);
  ASSERT_TRUE(parser);
  ScopedParserDetacher detacher(parser);
  parser->AppendBytes(base::byte_span_from_cstring("<div><div><div>"));
  // The first chunk should not been processed.
  EXPECT_FALSE(parser->DidPumpTokenizerForTesting());

  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  static_cast<DocumentParser*>(parser)->StopParsing();
}

}  // namespace blink
