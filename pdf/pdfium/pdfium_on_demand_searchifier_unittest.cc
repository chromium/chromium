// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_on_demand_searchifier.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_print.h"
#include "pdf/pdfium/pdfium_range.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace chrome_pdf {

namespace {

constexpr uint32_t kMaxOcrImageDimension = 2048;
const char kPageHasTextHistogram[] = "PDF.PageHasText";
const char kSearchifyAddedTextHistogram[] = "PDF.SearchifyAddedText";

using VisualAnnotationPtr = screen_ai::mojom::VisualAnnotationPtr;

constexpr base::TimeDelta kOcrDelay = base::Milliseconds(100);

base::FilePath GetReferenceFilePathForPrint(std::string_view test_filename) {
  return base::FilePath(FILE_PATH_LITERAL("pdfium_print"))
      .AppendASCII(test_filename);
}

class SearchifierTestClient : public TestClient {
 public:
  SearchifierTestClient() = default;
  SearchifierTestClient(const SearchifierTestClient&) = delete;
  SearchifierTestClient& operator=(const SearchifierTestClient&) = delete;
  ~SearchifierTestClient() override = default;

  bool IsPrintPreview() const override { return is_print_preview_; }

  void OnSearchifyStateChange(bool busy) override {
    if (busy) {
      busy_state_changed_count_++;
    } else {
      idle_state_changed_count_++;
    }
  }

  void set_for_print_preview() { is_print_preview_ = true; }

  int busy_state_changed_count() const { return busy_state_changed_count_; }
  int idle_state_changed_count() const { return idle_state_changed_count_; }

 private:
  bool is_print_preview_ = false;
  int busy_state_changed_count_ = 0;
  int idle_state_changed_count_ = 0;
};

void WaitUntilIdle(PDFiumOnDemandSearchifier* searchifier,
                   base::OnceClosure callback) {
  if (searchifier->IsIdleForTesting()) {
    std::move(callback).Run();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitUntilIdle, searchifier, std::move(callback)),
      kOcrDelay);
}

void WaitUntilFailure(PDFiumOnDemandSearchifier* searchifier,
                      base::OnceClosure callback) {
  if (searchifier->HasFailed()) {
    std::move(callback).Run();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitUntilFailure, searchifier, std::move(callback)),
      kOcrDelay);
}

void WaitForOneTimingCycle(base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback), kOcrDelay);
}

VisualAnnotationPtr CreateEmptyAnnotation() {
  return screen_ai::mojom::VisualAnnotation::New();
}

VisualAnnotationPtr CreateSampleAnnotation(int call_number) {
  auto annotation = CreateEmptyAnnotation();
  auto line_box = screen_ai::mojom::LineBox::New();
  line_box->bounding_box = gfx::Rect(0, 0, 100, 100);
  line_box->bounding_box_angle = 0;
  auto word_box = screen_ai::mojom::WordBox::New();
  word_box->word = base::StringPrintf("OCR Text %i", call_number);
  word_box->bounding_box = gfx::Rect(0, 0, 100, 100);
  word_box->bounding_box_angle = 0;
  line_box->words.push_back(std::move(word_box));
  annotation->lines.push_back(std::move(line_box));
  return annotation;
}

}  // namespace

class PDFiumOnDemandSearchifierTest : public PDFiumTestBase {
 public:
  PDFiumOnDemandSearchifierTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chrome_pdf::features::kPdfSearchify);
  }

  void CreateEngine(const base::FilePath::CharType* test_filename) {
    engine_ = InitializeEngine(&client_, test_filename);
    ASSERT_TRUE(engine_) << test_filename;
  }

  void CreatePreviewEngine(const base::FilePath::CharType* test_filename) {
    client_.set_for_print_preview();
    CreateEngine(test_filename);
  }

  void TearDown() override {
    // PDFium gets uninitialized via `FPDF_DestroyLibrary`. If `engine_` is not
    // destroyed here, its destruction results in a crash later.
    engine_.reset();
    PDFiumTestBase::TearDown();
  }

  void StartSearchify(bool empty_results) {
    // `engine_` is owned by this class, safe to use as unretained.
    engine_->StartSearchify(
        base::BindOnce(
            &PDFiumOnDemandSearchifierTest::MockGetOcrMaxImageDimension,
            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PDFiumOnDemandSearchifierTest::MockPerformOcr,
                            weak_factory_.GetWeakPtr(), empty_results));
  }

  void MockGetOcrMaxImageDimension(
      base::OnceCallback<void(uint32_t)> callback) {
    // Reply with delay, as done through mojo connection to the OCR service.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PDFiumOnDemandSearchifierTest::ReplyGetOcrMaxImageDimension,
            weak_factory_.GetWeakPtr(), std::move(callback)),
        base::Milliseconds(100));
  }

  void MockPerformOcr(bool empty_results,
                      const SkBitmap& /*image*/,
                      base::OnceCallback<void(VisualAnnotationPtr)> callback) {
    VisualAnnotationPtr results = empty_results
                                      ? CreateEmptyAnnotation()
                                      : CreateSampleAnnotation(performed_ocrs_);
    // Reply with delay, as done through mojo connection to the OCR service.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(results)),
        base::Milliseconds(100));

    performed_ocrs_++;
  }

  void WaitUntilMaxImageDimensionReplied() {
    EXPECT_TRUE(max_image_dimension_replied_.Wait());
  }

  void WaitUntilPerformedOcrCount(int expected_performed_ocrs) {
    if (performed_ocrs() == expected_performed_ocrs) {
      return;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PDFiumOnDemandSearchifierTest::WaitUntilPerformedOcrCount,
            base::Unretained(this), expected_performed_ocrs),
        kOcrDelay);
  }

  // Returns all characters in the page.
  std::string GetPageText(PDFiumPage& page) {
    return base::UTF16ToUTF8(PDFiumRange::AllTextOnPage(&page).GetText());
  }

  void ReplyGetOcrMaxImageDimension(
      base::OnceCallback<void(uint32_t)> callback) {
    std::move(callback).Run(kMaxOcrImageDimension);
    std::move(max_image_dimension_replied_.GetCallback()).Run();
  }

  int performed_ocrs() const { return performed_ocrs_; }
  PDFiumEngine* engine() { return engine_.get(); }
  int busy_state_changed_count() const {
    return client_.busy_state_changed_count();
  }
  int idle_state_changed_count() const {
    return client_.idle_state_changed_count();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PDFiumEngine> engine_;
  SearchifierTestClient client_;
  int performed_ocrs_ = 0;
  base::test::TestFuture<void> max_image_dimension_replied_;

  base::WeakPtrFactory<PDFiumOnDemandSearchifierTest> weak_factory_{this};
};

TEST_P(PDFiumOnDemandSearchifierTest, NoImage) {
  base::HistogramTester histogram_tester;
  CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_FALSE(engine()->PageNeedsSearchify(0));
  EXPECT_FALSE(page.IsPageSearchified());

  // Searchifier should not be created as it's not needed yet.
  ASSERT_FALSE(engine()->GetSearchifierForTesting());

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 1);
  histogram_tester.ExpectBucketCount(kPageHasTextHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 0);
}

TEST_P(PDFiumOnDemandSearchifierTest, OnePageWithImages) {
  CreateEngine(FILE_PATH_LITERAL("image_alt_text.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_TRUE(engine()->PageNeedsSearchify(0));

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);

  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  StartSearchify(/*empty_results=*/false);

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // The page has 2 images, so the text contains 2 fake OCR results.
  ASSERT_EQ(performed_ocrs(), 2);
  EXPECT_TRUE(page.IsPageSearchified());
  ASSERT_EQ(GetPageText(page), "OCR Text 0\r\nOCR Text 1");
}

TEST_P(PDFiumOnDemandSearchifierTest, PageWithImagesNoRecognizableText) {
  CreateEngine(FILE_PATH_LITERAL("image_alt_text.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_TRUE(engine()->PageNeedsSearchify(0));

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);

  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  StartSearchify(/*empty_results=*/true);

  {
    base::test::TestFuture<void> future;
    WaitUntilIdle(searchifier, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    ASSERT_EQ(performed_ocrs(), 2);
    EXPECT_TRUE(page.IsPageSearchified());
  }

  // The page has two images, but no recognizable text.
  EXPECT_TRUE(GetPageText(page).empty());

  // Unload the page where Searchify did not add any text.
  page.Unload();

  // Get the text from the page, which reloads the page.
  EXPECT_EQ(GetPageText(page), "");

  {
    // Wait for idle. This should not crash.
    base::test::TestFuture<void> future;
    WaitUntilIdle(searchifier, future.GetCallback());
    ASSERT_TRUE(future.Wait());

    // The number of performed OCRs has not changed.
    ASSERT_EQ(performed_ocrs(), 2);
    EXPECT_TRUE(page.IsPageSearchified());
  }
}

TEST_P(PDFiumOnDemandSearchifierTest, MultiplePagesWithImages) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load and verify needing searchify.
  for (int page = 0; page < kPageCount; page++) {
    GetPDFiumPageForTest(*engine(), page).GetPage();
    ASSERT_TRUE(engine()->PageNeedsSearchify(page));
  }

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);

  // Ensure they are scheduled.
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(searchifier->IsPageScheduled(page)) << page;
  }

  StartSearchify(/*empty_results=*/false);

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(performed_ocrs(), 4);
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 0)), "OCR Text 0");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 1)), "OCR Text 1");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 2)), "OCR Text 2");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 3)), "OCR Text 3");
}

TEST_P(PDFiumOnDemandSearchifierTest, AddedTextPreservedAfterUnload) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load.
  for (int page = 0; page < kPageCount; page++) {
    GetPDFiumPageForTest(*engine(), page).GetPage();
  }

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);

  StartSearchify(/*empty_results=*/false);

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(performed_ocrs(), 4);

  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 0)), "OCR Text 0");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 1)), "OCR Text 1");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 2)), "OCR Text 2");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 3)), "OCR Text 3");

  for (int page = 0; page < kPageCount; page++) {
    GetPDFiumPageForTest(*engine(), page).Unload();
  }
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 0)), "OCR Text 0");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 1)), "OCR Text 1");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 2)), "OCR Text 2");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 3)), "OCR Text 3");
}

TEST_P(PDFiumOnDemandSearchifierTest, MultipleImagesWithUnload) {
  CreateEngine(FILE_PATH_LITERAL("image_alt_text.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_TRUE(engine()->PageNeedsSearchify(0));

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);

  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  ASSERT_EQ(performed_ocrs(), 0);
  StartSearchify(/*empty_results=*/false);
  WaitUntilPerformedOcrCount(1);

  // Check the partially Searchified state after performing 1 of 2 OCRs. There
  // is no text, considering the OCR result has not arrived yet.
  EXPECT_FALSE(page.IsPageSearchified());
  ASSERT_EQ(GetPageText(page), "");

  {
    // Wait for the first OCR result to arrive.
    base::test::TestFuture<void> future;
    WaitForOneTimingCycle(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // OCR result arrived, but the second OCR has not finished, so there is still
  // nothing added to the page.
  EXPECT_FALSE(page.IsPageSearchified());
  ASSERT_EQ(GetPageText(page), "");

  // Unloading the page, doesn't result in canceling the task in `searchifier`.
  page.Unload();
  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  // Let `searchifier` finish.
  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Searchify finished, OCR results are added to the page.
  ASSERT_EQ(performed_ocrs(), 2);
  EXPECT_TRUE(page.IsPageSearchified());
  ASSERT_EQ(GetPageText(page), "OCR Text 0\r\nOCR Text 1");
}

TEST_P(PDFiumOnDemandSearchifierTest, MultiplePagesWithUnload) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load for all.
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(GetPDFiumPageForTest(*engine(), page).GetPage());
  }

  PDFiumPage& page0 = GetPDFiumPageForTest(*engine(), 0);
  page0.Unload();

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(searchifier->IsPageScheduled(page));
  }

  StartSearchify(/*empty_results=*/false);

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(performed_ocrs(), kPageCount);

  // Pages 0 is still not loaded.
  EXPECT_EQ(page0.page(), nullptr);

  // All pages are searchified.
  EXPECT_EQ(GetPageText(page0), "OCR Text 0");
  EXPECT_TRUE(page0.IsPageSearchified());
  std::optional<AccessibilityTextRunInfo> page0_info = page0.GetTextRunInfo(0);
  ASSERT_TRUE(page0_info.has_value());
  EXPECT_TRUE(page0_info.value().is_searchified);

  PDFiumPage& page1 = GetPDFiumPageForTest(*engine(), 1);
  EXPECT_EQ(GetPageText(page1), "OCR Text 1");
  EXPECT_TRUE(page1.IsPageSearchified());
  std::optional<AccessibilityTextRunInfo> page1_info = page1.GetTextRunInfo(0);
  ASSERT_TRUE(page1_info.has_value());
  EXPECT_TRUE(page1_info.value().is_searchified);

  PDFiumPage& page2 = GetPDFiumPageForTest(*engine(), 2);
  EXPECT_EQ(GetPageText(page2), "OCR Text 2");
  EXPECT_TRUE(page2.IsPageSearchified());
  std::optional<AccessibilityTextRunInfo> page2_info = page2.GetTextRunInfo(0);
  ASSERT_TRUE(page2_info.has_value());
  EXPECT_TRUE(page2_info.value().is_searchified);

  PDFiumPage& page3 = GetPDFiumPageForTest(*engine(), 3);
  EXPECT_EQ(GetPageText(page3), "OCR Text 3");
  EXPECT_TRUE(page3.IsPageSearchified());
  std::optional<AccessibilityTextRunInfo> page3_info = page3.GetTextRunInfo(0);
  ASSERT_TRUE(page3_info.has_value());
  EXPECT_TRUE(page3_info.value().is_searchified);

  // Unload a Searchified page.
  page3.Unload();

  // Get the text from the page, which reloads the page. It still has the
  // Searchified text because OCR finished and the text has been committed into
  // the page.
  EXPECT_EQ(GetPageText(page3), "OCR Text 3");
  EXPECT_TRUE(page3.IsPageSearchified());

  // Fetch `page3_info` again.
  page3_info = page3.GetTextRunInfo(0);
  ASSERT_TRUE(page3_info.has_value());
  EXPECT_TRUE(page3_info.value().is_searchified);
}

TEST_P(PDFiumOnDemandSearchifierTest, OnePageWithImagesInPrintPreview) {
  CreatePreviewEngine(FILE_PATH_LITERAL("image_alt_text.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger Searchify, but it should not do anything for Print
  // Preview.
  page.GetPage();
  ASSERT_FALSE(engine()->PageNeedsSearchify(0));
  ASSERT_FALSE(engine()->GetSearchifierForTesting());
}

TEST_P(PDFiumOnDemandSearchifierTest,
       OcrDisconnectionBeforeGettingMaxImageDimension) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load for all.
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(GetPDFiumPageForTest(*engine(), page).GetPage());
  }

  StartSearchify(/*empty_results=*/false);
  engine()->GetOcrDisconnectHandler().Run();

  base::test::TestFuture<void> future;
  WaitUntilFailure(engine()->GetSearchifierForTesting(), future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Sice OCR service got disconnected before arrival of max image dimension, no
  // OCR should be performed.
  ASSERT_EQ(performed_ocrs(), 0);
}

TEST_P(PDFiumOnDemandSearchifierTest,
       OcrDisconnectionAfterGettingMaxImageDimension) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load for all.
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(GetPDFiumPageForTest(*engine(), page).GetPage());
  }

  StartSearchify(/*empty_results=*/false);
  WaitUntilMaxImageDimensionReplied();
  engine()->GetOcrDisconnectHandler().Run();

  base::test::TestFuture<void> future;
  WaitUntilFailure(engine()->GetSearchifierForTesting(), future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Performing OCR is async and has some delay. It is expected that
  // cancellation takes effect before all pages are OCRed.
  ASSERT_LT(performed_ocrs(), kPageCount);
}

TEST_P(PDFiumOnDemandSearchifierTest, SearchifyStateChanges) {
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger one page load.
  GetPDFiumPageForTest(*engine(), 0).GetPage();

  EXPECT_EQ(busy_state_changed_count(), 1);
  EXPECT_EQ(idle_state_changed_count(), 0);

  StartSearchify(/*empty_results=*/false);

  EXPECT_EQ(busy_state_changed_count(), 1);
  EXPECT_EQ(idle_state_changed_count(), 0);

  // Wait for searchifier to process all pending tasks.
  {
    base::test::TestFuture<void> future;
    WaitUntilIdle(engine()->GetSearchifierForTesting(), future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  EXPECT_EQ(busy_state_changed_count(), 1);
  EXPECT_EQ(idle_state_changed_count(), 1);

  // Trigger more page loads.
  GetPDFiumPageForTest(*engine(), 1).GetPage();
  GetPDFiumPageForTest(*engine(), 2).GetPage();

  EXPECT_EQ(busy_state_changed_count(), 2);
  EXPECT_EQ(idle_state_changed_count(), 1);

  // Wait for searchifier to process all pending tasks.
  {
    base::test::TestFuture<void> future;
    WaitUntilIdle(engine()->GetSearchifierForTesting(), future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  EXPECT_EQ(busy_state_changed_count(), 2);
  EXPECT_EQ(idle_state_changed_count(), 2);

  // Trigger more page loads.
  GetPDFiumPageForTest(*engine(), 3).GetPage();

  EXPECT_EQ(busy_state_changed_count(), 3);
  EXPECT_EQ(idle_state_changed_count(), 2);

  // Disconnect OCR before searchifier processes the pending task.
  engine()->GetOcrDisconnectHandler().Run();

  EXPECT_EQ(busy_state_changed_count(), 3);
  EXPECT_EQ(idle_state_changed_count(), 3);
}

TEST_P(PDFiumOnDemandSearchifierTest, MetricsProcessedPageWithoutText) {
  base::HistogramTester histogram_tester;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 0);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 0);

  // Trigger one page load.
  GetPDFiumPageForTest(*engine(), 0).GetPage();

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 1);
  histogram_tester.ExpectBucketCount(kPageHasTextHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 0);

  StartSearchify(/*empty_results=*/false);

  // Wait for searchifier to process all pending tasks.
  {
    base::test::TestFuture<void> future;
    WaitUntilIdle(engine()->GetSearchifierForTesting(), future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 1);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 1);
  histogram_tester.ExpectBucketCount(kSearchifyAddedTextHistogram, true, 1);
}

TEST_P(PDFiumOnDemandSearchifierTest, MetricsCanceledPageWithoutText) {
  base::HistogramTester histogram_tester;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 0);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 0);

  // Trigger one page load.
  GetPDFiumPageForTest(*engine(), 0).GetPage();

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 1);
  histogram_tester.ExpectBucketCount(kPageHasTextHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 0);

  StartSearchify(/*empty_results=*/false);
  engine()->GetOcrDisconnectHandler().Run();

  // Wait for searchifier to process all pending tasks.
  {
    base::test::TestFuture<void> future;
    WaitUntilFailure(engine()->GetSearchifierForTesting(),
                     future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  histogram_tester.ExpectTotalCount(kPageHasTextHistogram, 1);
  histogram_tester.ExpectTotalCount(kSearchifyAddedTextHistogram, 0);
}

TEST_P(PDFiumOnDemandSearchifierTest, SelectPageBeforeSearchify) {
  CreateEngine(FILE_PATH_LITERAL("image_alt_text.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_TRUE(engine()->PageNeedsSearchify(0));
  engine()->SelectAll();
  ASSERT_TRUE(engine()->GetSelectedText().empty());

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);

  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  StartSearchify(/*empty_results=*/false);

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Perform SelectAll again to select extracted text.
  engine()->SelectAll();

  // The page has 2 images, so the text contains 2 fake OCR results.
#if BUILDFLAG(IS_WIN)
  const char kExpectedSelection[] = "OCR Text 0\r\nOCR Text 1";
#else
  const char kExpectedSelection[] = "OCR Text 0\nOCR Text 1";
#endif
  ASSERT_EQ(engine()->GetSelectedText(), kExpectedSelection);
}

TEST_P(PDFiumOnDemandSearchifierTest, UpdateWithUnloadedPage) {
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  PDFiumPage& page0 = GetPDFiumPageForTest(*engine(), 0);
  PDFiumPage& page1 = GetPDFiumPageForTest(*engine(), 1);

  // Load page 0 to schedule it for searchify.
  EXPECT_TRUE(page0.GetPage());

  page1.Unload();

  StartSearchify(/*empty_results=*/false);
  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();

  // Wait until searchify is finished.
  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(performed_ocrs(), 1);

  EXPECT_EQ(page1.GetImageObjectIndices().size(), 1u);
}

TEST_P(PDFiumOnDemandSearchifierTest, Bug405433817) {
  CreateEngine(FILE_PATH_LITERAL("bug_405433817.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_TRUE(engine()->PageNeedsSearchify(0));

  PDFiumPrint print(engine());

  static constexpr std::array<int, 1> kPageIndices = {0};
  const blink::WebPrintParams print_params = GetDefaultPrintParams();
  std::vector<uint8_t> pdf_data =
      print.PrintPagesAsPdf(kPageIndices, print_params);
  CheckFuzzyPdfRendering(pdf_data, 0, /*size_in_points=*/{200, 300},
                         GetReferenceFilePathForPrint("bug_405433817.png"));

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);
  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  StartSearchify(/*empty_results=*/true);
  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ASSERT_EQ(performed_ocrs(), 1);
  EXPECT_TRUE(page.IsPageSearchified());

  pdf_data = print.PrintPagesAsPdf(kPageIndices, print_params);
  CheckFuzzyPdfRendering(pdf_data, 0, /*size_in_points=*/{200, 300},
                         GetReferenceFilePathForPrint("bug_405433817.png"));
}

TEST_P(PDFiumOnDemandSearchifierTest, Bug406530484) {
  CreateEngine(FILE_PATH_LITERAL("bug_406530484.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_TRUE(engine()->PageNeedsSearchify(0));

  PDFiumPrint print(engine());

  static constexpr std::array<int, 1> kPageIndices = {0};
  const blink::WebPrintParams print_params = GetDefaultPrintParams();
  std::vector<uint8_t> pdf_data =
      print.PrintPagesAsPdf(kPageIndices, print_params);
  CheckPdfRendering(pdf_data, 0, /*size_in_points=*/{200, 300},
                    GetReferenceFilePathForPrint("bug_406530484.png"));

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);
  ASSERT_TRUE(searchifier->IsPageScheduled(0));

  StartSearchify(/*empty_results=*/true);
  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ASSERT_EQ(performed_ocrs(), 1);
  EXPECT_TRUE(page.IsPageSearchified());

  pdf_data = print.PrintPagesAsPdf(kPageIndices, print_params);
  CheckPdfRendering(pdf_data, 0, /*size_in_points=*/{200, 300},
                    GetReferenceFilePathForPrint("bug_406530484.png"));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumOnDemandSearchifierTest, testing::Bool());

}  // namespace chrome_pdf
