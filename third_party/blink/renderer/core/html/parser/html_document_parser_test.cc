// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder_builder.h"
#include "third_party/blink/renderer/core/loader/no_state_prefetch_client.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

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
 protected:
  HTMLDocumentParserTest()
      : original_force_synchronous_parsing_for_testing_(
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
        BuildTextResourceDecoderFor(&document, "text/html", g_null_atom));
    parser->SetDecoder(std::move(decoder));
    return parser;
  }

 private:
  ParserSynchronizationPolicy Policy() const { return GetParam(); }

  bool original_force_synchronous_parsing_for_testing_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(HTMLDocumentParserTest,
                         HTMLDocumentParserTest,
                         testing::Values(kForceSynchronousParsing,
                                         kAllowDeferredParsing));

TEST_P(HTMLDocumentParserTest, StopThenPrepareToStopShouldNotCrash) {
  auto& document = To<HTMLDocument>(GetDocument());
  DocumentParser* parser = CreateParser(document);
  const char kBytes[] = "<html>";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  // These methods are not supposed to be called one after the other, but in
  // practice it can happen (e.g. if navigation is aborted).
  parser->StopParsing();
  parser->PrepareToStopParsing();
}

TEST_P(HTMLDocumentParserTest, HasNoPendingWorkAfterStopParsing) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  DocumentParser* control_parser = static_cast<DocumentParser*>(parser);
  const char kBytes[] = "<html>";
  control_parser->AppendBytes(kBytes, sizeof(kBytes));
  control_parser->StopParsing();
  EXPECT_FALSE(parser->HasPendingWorkScheduledForTesting());
}

TEST_P(HTMLDocumentParserTest, HasNoPendingWorkAfterStopParsingThenAppend) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  DocumentParser* control_parser = static_cast<DocumentParser*>(parser);
  const char kBytes1[] = "<html>";
  control_parser->AppendBytes(kBytes1, sizeof(kBytes1));
  control_parser->StopParsing();
  const char kBytes2[] = "<head>";
  control_parser->AppendBytes(kBytes2, sizeof(kBytes2));
  EXPECT_FALSE(parser->HasPendingWorkScheduledForTesting());
}

TEST_P(HTMLDocumentParserTest, HasNoPendingWorkAfterDetach) {
  auto& document = To<HTMLDocument>(GetDocument());
  HTMLDocumentParser* parser = CreateParser(document);
  DocumentParser* control_parser = static_cast<DocumentParser*>(parser);
  const char kBytes[] = "<html>";
  control_parser->AppendBytes(kBytes, sizeof(kBytes));
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

  const char kBytes[] = "<httttttt";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  // The bytes are forwarded to the preload scanner, not to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_TRUE(script_runner_host->HasPreloadScanner());
  EXPECT_EQ(HTMLTokenizer::kDataState, parser->Tokenizer()->GetState());
  // Finishing should not cause parsing to start (verified via an internal
  // DCHECK).
  static_cast<DocumentParser*>(parser)->Finish();
  EXPECT_EQ(HTMLTokenizer::kDataState, parser->Tokenizer()->GetState());
  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  (static_cast<DocumentParser*>(parser))->StopParsing();
}

TEST_P(HTMLDocumentParserTest, AppendNoPrefetch) {
  auto& document = To<HTMLDocument>(GetDocument());
  EXPECT_FALSE(document.IsPrefetchOnly());
  // Use ForceSynchronousParsing to allow calling append().
  HTMLDocumentParser* parser = CreateParser(document);

  const char kBytes[] = "<htttttt";
  parser->AppendBytes(kBytes, sizeof(kBytes));
  test::RunPendingTasks();
  // The bytes are forwarded to the tokenizer.
  HTMLParserScriptRunnerHost* script_runner_host =
      parser->AsHTMLParserScriptRunnerHostForTesting();
  EXPECT_EQ(script_runner_host->HasPreloadScanner(),
            GetParam() == kAllowDeferredParsing);
  EXPECT_EQ(HTMLTokenizer::kTagNameState, parser->Tokenizer()->GetState());
  // Cancel any pending work to make sure that RuntimeFeatures DCHECKs do not
  // fire.
  (static_cast<DocumentParser*>(parser))->StopParsing();
}

}  // namespace blink
