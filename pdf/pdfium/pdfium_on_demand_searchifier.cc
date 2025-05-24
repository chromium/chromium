// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_on_demand_searchifier.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "pdf/pdfium/pdfium_searchify.h"

namespace {

// A delay to wait between page searchify tasks to give more priority to other
// PDF tasks. The longer delay is used when the next task seems to be not urgent
// and its helpful to reduce CPU load.
constexpr base::TimeDelta kSearchifyPageDelay = base::Milliseconds(100);
constexpr base::TimeDelta kSearchifyPageLongDelay = base::Milliseconds(300);

}  // namespace

namespace chrome_pdf {

PDFiumOnDemandSearchifier::OcrResult::OcrResult(
    int image_index,
    screen_ai::mojom::VisualAnnotationPtr annotation,
    const gfx::Size& image_size)
    : image_index(image_index),
      annotation(std::move(annotation)),
      image_size(image_size) {}

PDFiumOnDemandSearchifier::OcrResult::OcrResult(
    PDFiumOnDemandSearchifier::OcrResult&& other) noexcept = default;

PDFiumOnDemandSearchifier::OcrResult::~OcrResult() = default;

PDFiumOnDemandSearchifier::PDFiumOnDemandSearchifier(PDFiumEngine* engine)
    : engine_(raw_ref<PDFiumEngine>::from_ptr(engine)) {}

PDFiumOnDemandSearchifier::~PDFiumOnDemandSearchifier() = default;

void PDFiumOnDemandSearchifier::Start(
    GetOcrMaxImageDimensionCallbackAsync get_max_dimension_callback,
    PerformOcrCallbackAsync perform_ocr_callback) {
  CHECK(perform_ocr_callback);
  CHECK(get_max_dimension_callback);
  CHECK_EQ(state_, State::kIdle);

  // Expected to be called only once.
  CHECK(perform_ocr_callback_.is_null());

  font_ = CreateFont(engine_->doc());
  perform_ocr_callback_ = std::move(perform_ocr_callback);

  std::move(get_max_dimension_callback)
      .Run(base::BindOnce(&PDFiumOnDemandSearchifier::OnGotOcrMaxImageDimension,
                          weak_factory_.GetWeakPtr()));
  state_ = State::kWaitingForResults;
}

void PDFiumOnDemandSearchifier::OnGotOcrMaxImageDimension(
    uint32_t max_image_dimension) {
  // A state changed while waiting for max image dimension indicates that OCR
  // got disconnnected and cannot be used.
  if (state_ != State::kWaitingForResults) {
    return;
  }

  CHECK(max_image_dimension);
  max_image_dimension_ = max_image_dimension;

  state_ = State::kIdle;
  SearchifyNextPage();
}

void PDFiumOnDemandSearchifier::OnOcrDisconnected() {
  switch (state_) {
    case State::kIdle:
      // No need to change state, if another request comes up, the OCR provider
      // will try to connect to the service again.
      return;

    case State::kWaitingForResults:
      // Assume OCR cannot be used anymore if it gets disconnected while
      // waiting for results. Therefore cancel all pending requests and move
      // to failed state.
      current_page_ = nullptr;
      pages_queue_.clear();
      state_ = State::kFailed;
      engine_->OnSearchifyStateChange(/*busy=*/false);
      return;

    case State::kFailed:
      // `kFailed` is the end state and searchifier does not accept any requests
      // after it. So no need to react to OCR disconnection.
      return;
  }
  NOTREACHED();
}

bool PDFiumOnDemandSearchifier::IsPageScheduled(int page_index) const {
  if (current_page_ && current_page_->index() == page_index) {
    return true;
  }

  return base::Contains(pages_queue_, page_index);
}

void PDFiumOnDemandSearchifier::SchedulePage(int page_index) {
  CHECK_GE(page_index, 0);
  CHECK_NE(state_, State::kFailed);
  if (IsPageScheduled(page_index)) {
    return;
  }
  if (!current_page_ && pages_queue_.empty() && state_ == State::kIdle) {
    engine_->OnSearchifyStateChange(/*busy=*/true);
  }
  pages_queue_.push_back(page_index);
  // OCR service cannot be used before max image dimension is received.
  if (state_ == State::kWaitingForResults || !max_image_dimension_) {
    return;
  }

  CHECK_EQ(state_, State::kIdle);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PDFiumOnDemandSearchifier::SearchifyNextPage,
                     weak_factory_.GetWeakPtr()),
      kSearchifyPageDelay);

  // Avoid posting `SearchifyNextPage` more than once.
  state_ = State::kWaitingForResults;
}

void PDFiumOnDemandSearchifier::SearchifyNextPage() {
  // Do not proceed if OCR got disconnected.
  if (state_ == State::kFailed) {
    return;
  }

  if (pages_queue_.empty()) {
    state_ = State::kIdle;
    engine_->OnSearchifyStateChange(/*busy=*/false);
    return;
  }

  state_ = State::kWaitingForResults;
  current_page_ = engine_->GetPage(pages_queue_.front());
  CHECK(current_page_);
  current_page_was_loaded_ = !!current_page_->page();
  pages_queue_.pop_front();

  // Load the page if needed.
  current_page_->GetPage();
  current_page_image_object_indices_ = current_page_->GetImageObjectIndices();
  current_page_ocr_results_.clear();
  current_page_ocr_results_.reserve(current_page_image_object_indices_.size());
  SearchifyNextImage();
}

void PDFiumOnDemandSearchifier::SearchifyNextImage() {
  CHECK(current_page_);
  std::optional<BitmapResult> bitmap_result = GetNextBitmap();
  if (bitmap_result.has_value()) {
    const auto& bitmap = bitmap_result.value().bitmap;
    perform_ocr_callback_.Run(
        bitmap, base::BindOnce(&PDFiumOnDemandSearchifier::OnGotOcrResult,
                               weak_factory_.GetWeakPtr(),
                               bitmap_result.value().image_index,
                               gfx::Size(bitmap.width(), bitmap.height())));
    return;
  }

  // Report metric only once for each page.
  CHECK(!current_page_->IsPageSearchified());
  base::UmaHistogramBoolean("PDF.SearchifyAddedText",
                            !current_page_ocr_results_.empty());

  CommitResultsToPage();
}

void PDFiumOnDemandSearchifier::CommitResultsToPage() {
  // Ignore the results if the page got unloaded before committing them.
  if (!current_page_) {
    current_page_ocr_results_.clear();
  }

  if (!current_page_ocr_results_.empty()) {
    // If the page is being painted, wait for paint to finish.
    if (engine_->IsPageScheduledForPaint(current_page_->index())) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PDFiumOnDemandSearchifier::CommitResultsToPage,
                         weak_factory_.GetWeakPtr()),
          kSearchifyPageDelay);
      return;
    }

    // Reload page if needed.
    FPDF_PAGE page = current_page_->GetPage();
    bool added_text = false;
    for (auto& result : current_page_ocr_results_) {
      FPDF_PAGEOBJECT image = FPDFPage_GetObject(page, result.image_index);
      added_text |=
          AddTextOnImage(engine_->doc(), page, font_.get(), image,
                         std::move(result.annotation), result.image_size);
    }
    current_page_ocr_results_.clear();
    current_page_->OnSearchifyGotOcrResult(added_text);
    current_page_->ReloadTextPage();
    if (!FPDFPage_GenerateContent(page)) {
      LOG(ERROR) << "Failed to generate content";
    }
  }

  if (!current_page_was_loaded_) {
    engine_->MaybeUnloadPage(current_page_->index());
  }
  current_page_ = nullptr;

  // Searchify next page.
  // If none of the scheduled pages are visible, post the task with more delay
  // to reduce CPU load.
  bool long_delay = std::ranges::none_of(pages_queue_, [this](int page_index) {
    return this->engine_->IsPageVisible(page_index);
  });
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PDFiumOnDemandSearchifier::SearchifyNextPage,
                     weak_factory_.GetWeakPtr()),
      long_delay ? kSearchifyPageLongDelay : kSearchifyPageDelay);
}

std::optional<PDFiumOnDemandSearchifier::BitmapResult>
PDFiumOnDemandSearchifier::GetNextBitmap() {
  while (!current_page_image_object_indices_.empty()) {
    int image_index = current_page_image_object_indices_.back();
    current_page_image_object_indices_.pop_back();
    SkBitmap bitmap =
        current_page_->GetImageForOcr(image_index, max_image_dimension_);
    if (!bitmap.drawsNothing()) {
      return BitmapResult{bitmap, image_index};
    }
  }
  return std::nullopt;
}

void PDFiumOnDemandSearchifier::OnGotOcrResult(
    int image_index,
    const gfx::Size& image_size,
    screen_ai::mojom::VisualAnnotationPtr annotation) {
  CHECK_EQ(state_, State::kWaitingForResults);
  CHECK(current_page_);

  if (annotation) {
    current_page_ocr_results_.emplace_back(image_index, std::move(annotation),
                                           image_size);
  }
  SearchifyNextImage();
}

}  // namespace chrome_pdf
