// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_on_demand_searchifier.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_range.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace {

void WaitUntilIdle(chrome_pdf::PDFiumOnDemandSearchifier* searchifier,
                   base::OnceClosure callback) {
  if (searchifier->IsIdleForTesting()) {
    std::move(callback).Run();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitUntilIdle, searchifier, std::move(callback)),
      base::Milliseconds(100));
}

void WaitUntilFailure(chrome_pdf::PDFiumOnDemandSearchifier* searchifier,
                      base::OnceClosure callback) {
  if (searchifier->HasFailed()) {
    std::move(callback).Run();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitUntilFailure, searchifier, std::move(callback)),
      base::Milliseconds(100));
}

screen_ai::mojom::VisualAnnotationPtr CreateDummyAnnotation(int call_number) {
  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  auto line_box = screen_ai::mojom::LineBox::New();
  line_box->baseline_box = gfx::Rect(0, 0, 100, 100);
  line_box->baseline_box_angle = 0;
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

namespace chrome_pdf {

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

  void TearDown() override {
    // PDFium gets uninitialized via `FPDF_DestroyLibrary`. If `engine_` is not
    // destroyed here, its destruction results in a crash later.
    engine_.reset();
    PDFiumTestBase::TearDown();
  }

  void StartSearchify() {
    // `engine_` is owned by this class, safe to use as unretained.
    engine_->StartSearchify(
        base::BindRepeating(&PDFiumOnDemandSearchifierTest::MockPerformOcr,
                            base::Unretained(this)));
  }

  void MockPerformOcr(
      const SkBitmap& image,
      base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)>
          callback) {
    // Reply with delay, as done through mojo connection to the OCR service.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceCallback<void(screen_ai::mojom::VisualAnnotationPtr)>
                   callback,
               int call_number) {
              std::move(callback).Run(CreateDummyAnnotation(call_number));
            },
            std::move(callback), performed_ocrs_),
        base::Milliseconds(100));

    performed_ocrs_++;
  }

  // Returns all characters in the page.
  std::string GetPageText(chrome_pdf::PDFiumPage& page) {
    return base::UTF16ToUTF8(
        chrome_pdf::PDFiumRange::AllTextOnPage(&page).GetText());
  }

  int performed_ocrs() const { return performed_ocrs_; }
  PDFiumEngine* engine() { return engine_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PDFiumEngine> engine_;
  TestClient client_;
  int performed_ocrs_ = 0;
};

TEST_P(PDFiumOnDemandSearchifierTest, NoImage) {
  CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);

  // Load the page to trigger searchify checking.
  page.GetPage();
  ASSERT_FALSE(engine()->PageNeedsSearchify(0));
  EXPECT_FALSE(page.IsPageSearchified());

  // Searchifier should not be created as it's not needed yet.
  ASSERT_FALSE(engine()->GetSearchifierForTesting());
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

  StartSearchify();

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(performed_ocrs(), 2);
  EXPECT_TRUE(page.IsPageSearchified());

  // The page has two images.
  std::string page_text = GetPageText(page);
  ASSERT_EQ(page_text, "OCR Text 0\r\nOCR Text 1");
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

  StartSearchify();

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(performed_ocrs(), 4);
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 0)), "OCR Text 0");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 1)), "OCR Text 1");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 2)), "OCR Text 2");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 3)), "OCR Text 3");
}

TEST_P(PDFiumOnDemandSearchifierTest, MultiplePagesWithUnload) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load for all.
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(GetPDFiumPageForTest(*engine(), page).GetPage());
  }

  PDFiumPage& page = GetPDFiumPageForTest(*engine(), 0);
  page.Unload();

  PDFiumOnDemandSearchifier* searchifier = engine()->GetSearchifierForTesting();
  ASSERT_TRUE(searchifier);
  ASSERT_FALSE(searchifier->IsPageScheduled(0));

  StartSearchify();

  base::test::TestFuture<void> future;
  WaitUntilIdle(searchifier, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(performed_ocrs(), kPageCount - 1);

  // First page is not searchified.
  std::string page_text = GetPageText(page);
  EXPECT_TRUE(page_text.empty());

  // Other pages are searchified.
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 1)), "OCR Text 0");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 2)), "OCR Text 1");
  EXPECT_EQ(GetPageText(GetPDFiumPageForTest(*engine(), 3)), "OCR Text 2");
}

TEST_P(PDFiumOnDemandSearchifierTest, OcrCancellation) {
  constexpr int kPageCount = 4;
  CreateEngine(FILE_PATH_LITERAL("multi_page_no_text.pdf"));

  // Trigger page load for all.
  for (int page = 0; page < kPageCount; page++) {
    ASSERT_TRUE(GetPDFiumPageForTest(*engine(), page).GetPage());
  }

  StartSearchify();
  engine()->GetOcrDisconnectHandler().Run();

  base::test::TestFuture<void> future;
  WaitUntilFailure(engine()->GetSearchifierForTesting(), future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // Performing OCR is async and has some delay. It is expected that
  // cancellation takes effect before all pages are OCRed.
  ASSERT_LT(performed_ocrs(), kPageCount);
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumOnDemandSearchifierTest, testing::Bool());

}  // namespace chrome_pdf
