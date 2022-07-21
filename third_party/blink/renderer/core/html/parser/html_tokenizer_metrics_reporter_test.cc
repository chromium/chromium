// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer_metrics_reporter.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

TEST(HTMLTokenizerMetricsReporterTest, FindNullChar) {
  HTMLTokenizerMetricsReporter::BackgroundReporter reporter;
  reporter.WillAppend(String("123\0", 4u));
  EXPECT_EQ(3u, reporter.index_of_null_char());
}

TEST(HTMLTokenizerMetricsReporterTest, FindNullCharSecondChunk) {
  HTMLTokenizerMetricsReporter::BackgroundReporter reporter;
  reporter.WillAppend("abc");
  reporter.WillAppend(String("d\0f", 3u));
  EXPECT_EQ(4u, reporter.index_of_null_char());
}

TEST(HTMLTokenizerMetricsReporterTest, SimpleCData) {
  HTMLTokenizerMetricsReporter::BackgroundReporter reporter;
  reporter.WillAppend("abcdef<<![CDATA[");
  EXPECT_EQ(7u, reporter.index_of_cdata_section());
}

TEST(HTMLTokenizerMetricsReporterTest, SplitCData) {
  HTMLTokenizerMetricsReporter::BackgroundReporter reporter;
  reporter.WillAppend("abc<![");
  reporter.WillAppend("CD");
  reporter.WillAppend("ATA[");
  EXPECT_EQ(3u, reporter.index_of_cdata_section());
}

TEST(HTMLTokenizerMetricsReporterTest, IncompleteCData) {
  HTMLTokenizerMetricsReporter::BackgroundReporter reporter;
  reporter.WillAppend("abcdef<<![CDATA");
  EXPECT_EQ(std::numeric_limits<unsigned>::max(),
            reporter.index_of_cdata_section());
}

TEST(HTMLTokenizerMetricsReporterTest, SplitWithPartialThenFull) {
  HTMLTokenizerMetricsReporter::BackgroundReporter reporter;
  reporter.WillAppend("abc<![");
  reporter.WillAppend("<![CDATA[");
  EXPECT_EQ(6u, reporter.index_of_cdata_section());
}

class HTMLTokenizerMetricsReporterSimTest : public SimTest {
 public:
  void LoadPageWithContentForReporter(const String& string) {
    // To ensure metrics reporter is created.
    Document::SetForceSynchronousParsingForTesting(false);
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    HTMLTokenizerMetricsReporter::metrics_logged_callback_for_test_ =
        &quit_closure;

    String source("https://example.com/p1");
    SimRequest main_resource(source, "text/html");
    LoadURL(source);
    main_resource.Complete(string);

    // Because SetForceSynchronousParsingForTesting(false) was called,
    // tokenizing doesn't happen immediately. Use this to ensure it runs.
    base::RunLoop().RunUntilIdle();

    String source2("https://example.com/p2");
    SimRequest resource2(source2, "text/html");
    Document::SetForceSynchronousParsingForTesting(true);
    LoadURL(source2);
    resource2.Complete("empty");

    run_loop.Run();
    HTMLTokenizerMetricsReporter::metrics_logged_callback_for_test_ = nullptr;
  }
};

TEST_F(HTMLTokenizerMetricsReporterSimTest, UnexpectedState) {
  base::HistogramTester tester;
  LoadPageWithContentForReporter("<svg><style></style></svg>");
  // Bucket 2 means a state mismatch.
  tester.ExpectUniqueSample("Blink.Tokenizer.MainDocument.ATypicalStates", 2,
                            1);
}

TEST_F(HTMLTokenizerMetricsReporterSimTest, DocumentWrite) {
  base::HistogramTester tester;
  LoadPageWithContentForReporter("<script>document.write('test');</script>");
  // Bucket 1 is for document.write().
  tester.ExpectUniqueSample("Blink.Tokenizer.MainDocument.ATypicalStates", 1,
                            1);
}

TEST_F(HTMLTokenizerMetricsReporterSimTest, EmbeddedNull) {
  base::HistogramTester tester;
  LoadPageWithContentForReporter(String("<div>t\0ext", 10u));
  tester.ExpectUniqueSample("Blink.Tokenizer.MainDocument.ATypicalStates", 4,
                            1);
}

}  // namespace blink
