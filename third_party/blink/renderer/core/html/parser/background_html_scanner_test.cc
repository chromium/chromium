// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"

#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"

namespace blink {
namespace {

using OptimizationParams =
    BackgroundHTMLScanner::ScriptTokenScanner::OptimizationParams;

constexpr char kStyleText[] = ".foo { color: red; }";

class TestParser : public ScriptableDocumentParser {
 public:
  explicit TestParser(Document& document)
      : ScriptableDocumentParser(document) {}

  void Trace(Visitor* visitor) const override {
    ScriptableDocumentParser::Trace(visitor);
  }

  void ExecuteScriptsWaitingForResources() override {}
  void NotifyNoRemainingAsyncScripts() override {}
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
      bool precompile_scripts = true,
      bool pretokenize_css = true,
      wtf_size_t min_script_size = 0u,
      wtf_size_t min_css_size = 0u) {
    auto token_scanner =
        std::make_unique<BackgroundHTMLScanner::ScriptTokenScanner>(
            parser,
            /*precompile_scripts_params=*/
            OptimizationParams{.task_runner = task_runner_,
                               .min_size = min_script_size,
                               .enabled = precompile_scripts},
            /*pretokenize_css_params=*/
            OptimizationParams{.task_runner = task_runner_,
                               .min_size = min_css_size,
                               .enabled = pretokenize_css});
    return std::make_unique<BackgroundHTMLScanner>(
        std::make_unique<HTMLTokenizer>(HTMLParserOptions()),
        std::move(token_scanner));
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

TEST_F(BackgroundHTMLScannerTest, PrecompileTurnedOff) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser, false);
  scanner->Scan("<script>foo</script>");
  FlushTaskRunner();
  EXPECT_EQ(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, InsideHTMLPreloadScanner) {
  GetDocument().SetURL(KURL("https://www.example.com"));
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto background_scanner = CreateScanner(parser);
  HTMLPreloadScanner preload_scanner(
      std::make_unique<HTMLTokenizer>(HTMLParserOptions()), false,
      GetDocument().Url(),
      std::make_unique<CachedDocumentParameters>(&GetDocument()),
      MediaValuesCached::MediaValuesCachedData(GetDocument()),
      TokenPreloadScanner::ScannerType::kMainDocument,
      std::make_unique<BackgroundHTMLScanner::ScriptTokenScanner>(
          parser,
          /*precompile_scripts_params=*/
          OptimizationParams{
              .task_runner = task_runner_, .min_size = 0u, .enabled = true},
          /*pretokenize_css_params=*/
          OptimizationParams{
              .task_runner = task_runner_, .min_size = 0u, .enabled = true}),
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
  auto scanner = CreateScanner(parser, true, true, /*min_script_size=*/3u);
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

TEST_F(BackgroundHTMLScannerTest, SimpleStyle) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan(String("<style>") + kStyleText + "</style>");
  FlushTaskRunner();
  auto tokenizer = parser->TakeCSSTokenizer(kStyleText);
  EXPECT_NE(tokenizer, nullptr);
  // Finish tokenizing and grab the token count.
  while (tokenizer->TokenizeSingle().GetType() != kEOFToken) {
  }
  EXPECT_GT(tokenizer->TokenCount(), 1u);
}

TEST_F(BackgroundHTMLScannerTest, CSSSizeLimit) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser, true, true, /*min_script_size=*/0u,
                               /*min_css_size=*/3u);
  scanner->Scan("<style>ba</style><style>long</style>");
  FlushTaskRunner();
  EXPECT_EQ(parser->TakeCSSTokenizer("ba"), nullptr);
  EXPECT_NE(parser->TakeCSSTokenizer("long"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, DuplicateSheets) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan(String("<style>") + kStyleText + "</style>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeCSSTokenizer(kStyleText), nullptr);

  scanner->Scan(String("<style>") + kStyleText + "</style>");
  FlushTaskRunner();
  // Tokenizer should not be created a second time.
  EXPECT_EQ(parser->TakeCSSTokenizer(kStyleText), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, PrecompileScriptsTurnedOff) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser, false);
  scanner->Scan(String("<script>foo</script><style>") + kStyleText +
                "</style>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeCSSTokenizer(kStyleText), nullptr);
  EXPECT_EQ(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, PretokenizeCSSTurnedOff) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser, true, false);
  scanner->Scan(String("<script>foo</script><style>") + kStyleText +
                "</style>");
  FlushTaskRunner();
  EXPECT_EQ(parser->TakeCSSTokenizer(kStyleText), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, StyleAndScript) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan(String("<style>") + kStyleText +
                "</style><script>foo</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeCSSTokenizer(kStyleText), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, MismatchedStyleEndTags) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(parser);
  scanner->Scan("<style>foo</script></style></script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeCSSTokenizer("foo</script>"), nullptr);
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

}  // namespace
}  // namespace blink
