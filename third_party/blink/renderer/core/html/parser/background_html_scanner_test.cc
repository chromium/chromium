// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class TestParser : public ScriptableDocumentParser {
 public:
  explicit TestParser(Document& document)
      : ScriptableDocumentParser(document) {}

  void Trace(Visitor* visitor) const override {
    ScriptableDocumentParser::Trace(visitor);
  }

  void ExecuteScriptsWaitingForResources() override {}
  bool IsWaitingForScripts() const override { return false; }
  void DidAddPendingParserBlockingStylesheet() override {}
  void DidLoadAllPendingParserBlockingStylesheets() override {}
  OrdinalNumber LineNumber() const override { return OrdinalNumber::First(); }
  TextPosition GetTextPosition() const override {
    return TextPosition::MinimumPosition();
  }
  void insert(const String&) override {}
  void Append(const String&) override {}
  void Finish() override {}
};

class BackgroundHTMLScannerTest : public PageTestBase {
 public:
  BackgroundHTMLScannerTest()
      : task_runner_(worker_pool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_BLOCKING})) {}

 protected:
  std::unique_ptr<BackgroundHTMLScanner> CreateScanner(
      TestParser* parser,
      wtf_size_t min_script_size = 0u) {
    return std::make_unique<BackgroundHTMLScanner>(
        std::make_unique<HTMLTokenizer>(HTMLParserOptions()),
        std::make_unique<BackgroundHTMLScanner::ScriptTokenScanner>(
            parser, task_runner_, min_script_size));
  }

  void FlushTaskRunner() {
    base::RunLoop r;
    task_runner_->PostTask(FROM_HERE, r.QuitClosure());
    r.Run();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

TEST_F(BackgroundHTMLScannerTest, SimpleScript) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>foo</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, InsideHTMLPreloadScanner) {
  GetDocument().SetURL(KURL("https://www.example.com"));
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto background_scanner = CreateScanner(parser);
  HTMLPreloadScanner preload_scanner(
      std::make_unique<HTMLTokenizer>(HTMLParserOptions()), GetDocument().Url(),
      std::make_unique<CachedDocumentParameters>(&GetDocument()),
      std::make_unique<MediaValuesCached::MediaValuesCachedData>(GetDocument()),
      TokenPreloadScanner::ScannerType::kMainDocument,
      std::make_unique<BackgroundHTMLScanner::ScriptTokenScanner>(
          parser, task_runner_, 0),
      CrossThreadBindRepeating([](std::unique_ptr<PendingPreloadData>) {}));
  preload_scanner.ScanInBackground("<script>foo</script>",
                                   GetDocument().ValidBaseElementURL());
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, MultipleScripts) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>foo</script><script>bar</script><script>baz</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("bar"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("baz"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, ScriptSizeLimit) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser, /*min_script_size=*/3u);
  scanner->Scan("<script>ba</script><script>long</script>");
  FlushTaskRunner();
  EXPECT_EQ(parser->TakeInlineScriptStreamer("ba"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("long"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, ScriptWithScriptTag) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>foo = '<script>'</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo = '<script>'"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, ScriptAcrossMultipleScans) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("Some stuff<div></div><script>f");
  scanner->Scan("oo</script> and some other stuff");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, String16Key) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>foo</script>");
  FlushTaskRunner();
  String key = "foo";
  key.Ensure16Bit();
  EXPECT_NE(parser->TakeInlineScriptStreamer(key), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, String16Source) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  String source = "<script>foo</script>";
  source.Ensure16Bit();
  scanner->Scan(source);
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, UTF16Characters) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  String source = u"<script>hello \u3042</script>";
  EXPECT_FALSE(source.Is8Bit());
  scanner->Scan(source);
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer(u"hello \u3042"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, MismatchedScriptEndTags) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>foo</style></script></style>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo</style>"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, ExtraStartTag) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>foo<script>bar</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo<script>bar"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, HistogramEmission) {
  base::HistogramTester histogram_tester;

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>function test() { return 42; }</script>");
  FlushTaskRunner();

  EXPECT_NE(parser->TakeInlineScriptStreamer("function test() { return 42; }"),
            nullptr);

  // Verify the histogram was emitted
  histogram_tester.ExpectTotalCount("WebCore.Scripts.InlineStreamerTimedOut",
                                    1);
}

TEST_F(BackgroundHTMLScannerTest, HistogramEmissionWithTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPrecompileInlineScripts, {{"inline-script-timeout", "1000"}});

  base::HistogramTester histogram_tester;

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script>var x = 1; var y = 2;</script>");
  FlushTaskRunner();

  EXPECT_NE(parser->TakeInlineScriptStreamer("var x = 1; var y = 2;"), nullptr);
  histogram_tester.ExpectTotalCount("WebCore.Scripts.InlineStreamerTimedOut",
                                    1);
}

TEST_F(BackgroundHTMLScannerTest, MainThreadTaskRunnerForHistograms) {
  base::HistogramTester histogram_tester;

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  // Use a sequenced task runner (non-null)
  auto scanner = CreateScanner(parser);

  // Scan multiple scripts to test both code paths (with/without task_runner_)
  scanner->Scan("<script>foo</script><script>bar</script>");
  FlushTaskRunner();

  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("bar"), nullptr);

  // Should emit one histogram per script
  histogram_tester.ExpectTotalCount("WebCore.Scripts.InlineStreamerTimedOut",
                                    2);
}

TEST_F(BackgroundHTMLScannerTest, EmptyScriptNoHistogram) {
  base::HistogramTester histogram_tester;

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<script></script>");
  FlushTaskRunner();

  // Empty scripts shouldn't create streamers or emit histograms
  histogram_tester.ExpectTotalCount("WebCore.Scripts.InlineStreamerTimedOut",
                                    0);
}

TEST_F(BackgroundHTMLScannerTest, SmallScriptNoHistogram) {
  base::HistogramTester histogram_tester;

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser, /*min_script_size=*/100u);
  scanner->Scan("<script>x</script>");
  FlushTaskRunner();

  // Scripts below min size shouldn't create streamers or emit histograms
  histogram_tester.ExpectTotalCount("WebCore.Scripts.InlineStreamerTimedOut",
                                    0);
}

TEST_F(BackgroundHTMLScannerTest, CompileStrategyLazy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPrecompileInlineScripts, {{"compile-strategy", "lazy"}});

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);

  scanner->Scan("<script>first</script><script>second</script>");
  FlushTaskRunner();

  EXPECT_NE(parser->TakeInlineScriptStreamer("first"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("second"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, CompileStrategyFirstScriptLazy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPrecompileInlineScripts,
      {{"compile-strategy", "first-script-lazy"}});

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);

  scanner->Scan("<script>first_lazy</script><script>second_eager</script>");
  FlushTaskRunner();

  EXPECT_NE(parser->TakeInlineScriptStreamer("first_lazy"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("second_eager"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, CompileStrategyEager) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPrecompileInlineScripts, {{"compile-strategy", "eager"}});

  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);

  scanner->Scan("<script>eager_first</script><script>eager_second</script>");
  FlushTaskRunner();

  EXPECT_NE(parser->TakeInlineScriptStreamer("eager_first"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("eager_second"), nullptr);
}

}  // namespace
}  // namespace blink
