// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"

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
  std::unique_ptr<BackgroundHTMLScanner> CreateScanner(int num_streamers,
                                                       TestParser* parser) {
    BackgroundHTMLScanner::InlineScriptStreamerVector streamers;
    for (int i = 0; i < num_streamers; i++)
      streamers.emplace_back(MakeGarbageCollected<InlineScriptStreamer>());
    return std::make_unique<BackgroundHTMLScanner>(
        std::make_unique<HTMLTokenizer>(HTMLParserOptions()),
        std::move(streamers), parser, task_runner_);
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
  auto scanner = CreateScanner(2, parser);
  scanner->Scan("<script>foo</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, ScriptWithScriptTag) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(2, parser);
  scanner->Scan("<script>foo = '<script>'</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo = '<script>'"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, TooManyScripts) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(2, parser);
  scanner->Scan("<script>foo</script><script>bar</script><script>baz</script>");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
  EXPECT_NE(parser->TakeInlineScriptStreamer("bar"), nullptr);
  EXPECT_EQ(parser->TakeInlineScriptStreamer("baz"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, ScriptAcrossMultipleScans) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(2, parser);
  scanner->Scan("Some stuff<div></div><script>f");
  scanner->Scan("oo</script> and some other stuff");
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, String16Key) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(2, parser);
  scanner->Scan("<script>foo</script>");
  FlushTaskRunner();
  String key = "foo";
  key.Ensure16Bit();
  EXPECT_NE(parser->TakeInlineScriptStreamer(key), nullptr);
}

TEST_F(BackgroundHTMLScannerTest, String16Source) {
  auto* parser = MakeGarbageCollected<TestParser>(GetDocument());
  auto scanner = CreateScanner(2, parser);
  String source = "<script>foo</script>";
  source.Ensure16Bit();
  scanner->Scan(source);
  FlushTaskRunner();
  EXPECT_NE(parser->TakeInlineScriptStreamer("foo"), nullptr);
}

}  // namespace
}  // namespace blink
