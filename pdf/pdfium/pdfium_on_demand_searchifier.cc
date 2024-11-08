// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_on_demand_searchifier.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "pdf/pdfium/pdfium_searchify.h"

namespace {

// A delay to wait between page searchify tasks to give more priority to other
// PDF tasks.
constexpr base::TimeDelta kSearchifyPageDelay = base::Milliseconds(100);

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

void PDFiumOnDemandSearchifier::Start(PerformOcrCallbackAsync callback) {
  CHECK(!callback.is_null());
  CHECK_EQ(state_, State::kIdle);

  // Expected to be called only once.
  CHECK(perform_ocr_callback_.is_null());

  font_ = CreateFont(engine_->doc());
  perform_ocr_callback_ = std::move(callback);

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
  if (state_ == State::kWaitingForResults || !perform_ocr_callback_) {
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

void PDFiumOnDemandSearchifier::CancelPage(int page_index) {
  if (current_page_ && current_page_->index() == page_index) {
    current_page_ = nullptr;
    return;
  }
  base::Erase(pages_queue_, page_index);
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
  pages_queue_.pop_front();

  current_page_image_object_indices_ = current_page_->GetImageObjectIndices();
  current_page_ocr_results_.clear();
  current_page_ocr_results_.reserve(current_page_image_object_indices_.size());
  SearchifyNextImage();
}

void PDFiumOnDemandSearchifier::SearchifyNextImage() {
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
  bool not_reported =
      searchify_added_text_metric_reported_.insert(current_page_->index())
          .second;
  if (not_reported) {
    base::UmaHistogramBoolean("PDF.SearchifyAddedText",
                              !current_page_ocr_results_.empty());
  }

  if (!current_page_ocr_results_.empty()) {
    // It is expected that the page would be still loaded.
    FPDF_PAGE page = current_page_->page();
    CHECK(page);
    bool added_text = false;
    for (auto& result : current_page_ocr_results_) {
      FPDF_PAGEOBJECT image = FPDFPage_GetObject(page, result.image_index);
      std::vector<FPDF_PAGEOBJECT> added_text_objects =
          AddTextOnImage(engine_->doc(), page, font_.get(), image,
                         std::move(result.annotation), result.image_size);
      current_page_->OnSearchifyGotOcrResult(added_text_objects);
      added_text |= !added_text_objects.empty();
    }
    if (added_text) {
      engine_->OnHasSearchifyText();
    }
    current_page_ocr_results_.clear();

    current_page_->ReloadTextPage();
    if (!FPDFPage_GenerateContent(page)) {
      LOG(ERROR) << "Failed to generate content";
    }
  }

  current_page_ = nullptr;

  // Searchify next page.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PDFiumOnDemandSearchifier::SearchifyNextPage,
                     weak_factory_.GetWeakPtr()),
      kSearchifyPageDelay);
}

std::optional<PDFiumOnDemandSearchifier::BitmapResult>
PDFiumOnDemandSearchifier::GetNextBitmap() {
  while (!current_page_image_object_indices_.empty()) {
    int image_index = current_page_image_object_indices_.back();
    current_page_image_object_indices_.pop_back();
    SkBitmap bitmap = current_page_->GetImageForOcr(image_index);
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

  // If current request got canceled while OCR was running, ignore the result
  // and move to the next page.
  if (!current_page_) {
    current_page_ocr_results_.clear();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PDFiumOnDemandSearchifier::SearchifyNextPage,
                       weak_factory_.GetWeakPtr()),
        kSearchifyPageDelay);
    return;
  }

  if (annotation) {
    current_page_ocr_results_.emplace_back(image_index, std::move(annotation),
                                           image_size);
  }
  SearchifyNextImage();
}

}  // namespace chrome_pdf
