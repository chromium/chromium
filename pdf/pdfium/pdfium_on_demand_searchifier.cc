// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_on_demand_searchifier.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "pdf/pdfium/pdfium_searchify.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace {

// A delay to wait between page searchify tasks to give more priority to other
// PDF tasks.
constexpr base::TimeDelta kSearchifyPageDelay = base::Milliseconds(100);

}  // namespace

namespace chrome_pdf {

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

void PDFiumOnDemandSearchifier::RemovePageFromQueue(int page_index) {
  base::Erase(pages_queue_, page_index);
}

void PDFiumOnDemandSearchifier::SearchifyNextPage() {
  // Do not proceed if OCR got disconnected.
  if (state_ == State::kFailed) {
    return;
  }

  if (pages_queue_.empty()) {
    state_ = State::kIdle;
    return;
  }

  state_ = State::kWaitingForResults;
  current_page_ = engine_->GetPage(pages_queue_.front());
  CHECK(current_page_);
  pages_queue_.pop_front();

  current_page_image_object_indices_ = current_page_->GetImageObjectIndices();
  SearchifyNextImage();
}

void PDFiumOnDemandSearchifier::SearchifyNextImage() {
  std::optional<BitmapResult> result = GetNextBitmap();
  if (!result.has_value()) {
    current_page_->ReloadTextPage();
    if (!FPDFPage_GenerateContent(current_page_->GetPage())) {
      LOG(ERROR) << "Failed to generate content";
    }
    current_page_ = nullptr;

    // Searchify next page.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PDFiumOnDemandSearchifier::SearchifyNextPage,
                       weak_factory_.GetWeakPtr()),
        kSearchifyPageDelay);
    return;
  }

  const auto& bitmap = result.value().bitmap;
  perform_ocr_callback_.Run(
      bitmap,
      base::BindOnce(&PDFiumOnDemandSearchifier::OnGotOcrResult,
                     weak_factory_.GetWeakPtr(), result.value().image_index,
                     gfx::Size(bitmap.width(), bitmap.height())));
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
  if (annotation) {
    current_page_->OnSearchifyGotOcrResult();
    FPDF_PAGEOBJECT image =
        FPDFPage_GetObject(current_page_->GetPage(), image_index);
    AddTextOnImage(engine_->doc(), current_page_->GetPage(), font_.get(), image,
                   std::move(annotation), image_size);
  }
  SearchifyNextImage();
}

}  // namespace chrome_pdf
