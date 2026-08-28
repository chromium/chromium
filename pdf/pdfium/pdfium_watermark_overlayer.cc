// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_watermark_overlayer.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_save.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

namespace {

// Tolerance in points when comparing page dimensions, to account for minor
// rounding differences during float-to-text PDF serialization and parsing.
constexpr float kDimensionTolerance = 0.001f;

ScopedUnsupportedFeature CreateScopedUnsupportedFeature() {
  // Avoid loading V8 if the PDF contains embedded JavaScript.
  return ScopedUnsupportedFeature(ScopedUnsupportedFeature::kNoEngine);
}

}  // namespace

// static
std::unique_ptr<PdfiumWatermarkOverlayer> PdfiumWatermarkOverlayer::Create(
    base::span<const uint8_t> pdf_buffer) {
  auto overlayer = base::WrapUnique(new PdfiumWatermarkOverlayer());
  if (!overlayer->Initialize(pdf_buffer)) {
    return nullptr;
  }

  return overlayer;
}

PdfiumWatermarkOverlayer::ScopedSdkInitializer::ScopedSdkInitializer() {
  InitializeSDK(/*enable_v8=*/false, /*use_skia=*/false,
                FontMappingMode::kNoMapping);
}

PdfiumWatermarkOverlayer::ScopedSdkInitializer::~ScopedSdkInitializer() {
  ShutdownSDK();
}

PdfiumWatermarkOverlayer::PdfiumWatermarkOverlayer() = default;

PdfiumWatermarkOverlayer::~PdfiumWatermarkOverlayer() = default;

bool PdfiumWatermarkOverlayer::Initialize(
    base::span<const uint8_t> pdf_buffer) {
  ScopedUnsupportedFeature scoped_unsupported_feature =
      CreateScopedUnsupportedFeature();
  base_doc_ = LoadPdfData(pdf_buffer);
  if (!base_doc_) {
    return false;
  }

  CHECK_EQ(FPDF_GetFormType(base_doc_.get()), FORMTYPE_NONE);
  page_count_ = FPDF_GetPageCount(base_doc_.get());
  return page_count_ > 0;
}

std::vector<gfx::SizeF> PdfiumWatermarkOverlayer::GetPageSizes() const {
  ScopedUnsupportedFeature scoped_unsupported_feature =
      CreateScopedUnsupportedFeature();
  std::vector<gfx::SizeF> page_sizes;
  page_sizes.reserve(page_count_);

  for (int i = 0; i < page_count_; ++i) {
    FS_SIZEF size;
    if (!FPDF_GetPageSizeByIndexF(base_doc_.get(), i, &size) ||
        size.width <= 0.0f || size.height <= 0.0f) {
      LOG(ERROR) << "Failed to get page size for page " << i;
      return {};
    }
    page_sizes.emplace_back(size.width, size.height);
  }

  return page_sizes;
}

std::vector<uint8_t> PdfiumWatermarkOverlayer::OverlayPages(
    base::span<const uint8_t> overlay_pdf_buffer) {
  ScopedUnsupportedFeature scoped_unsupported_feature =
      CreateScopedUnsupportedFeature();
  ScopedFPDFDocument overlay_doc = LoadPdfData(overlay_pdf_buffer);
  if (!overlay_doc) {
    LOG(ERROR) << "OverlayPages: Failed to load overlay PDF document.";
    return {};
  }

  int overlay_page_count = FPDF_GetPageCount(overlay_doc.get());

  if (page_count_ != overlay_page_count) {
    LOG(ERROR) << "OverlayPages: Mismatched page counts. Base page count: "
               << page_count_
               << ", Overlay page count: " << overlay_page_count;
    return {};
  }

  for (int i = 0; i < page_count_; ++i) {
    FS_SIZEF base_size;
    FS_SIZEF overlay_size;
    if (!FPDF_GetPageSizeByIndexF(base_doc_.get(), i, &base_size) ||
        !FPDF_GetPageSizeByIndexF(overlay_doc.get(), i, &overlay_size) ||
        !base::IsApproximatelyEqual(base_size.width, overlay_size.width,
                                    kDimensionTolerance) ||
        !base::IsApproximatelyEqual(base_size.height, overlay_size.height,
                                    kDimensionTolerance)) {
      LOG(ERROR) << "OverlayPages: Mismatched page dimensions for page " << i;
      return {};
    }

    ScopedFPDFPage base_page(FPDF_LoadPage(base_doc_.get(), i));
    if (!base_page) {
      LOG(ERROR) << "OverlayPages: Failed to load page " << i
                 << " from base PDF.";
      return {};
    }

    FPDF_XOBJECT xobject =
        FPDF_NewXObjectFromPage(base_doc_.get(), overlay_doc.get(), i);
    if (!xobject) {
      LOG(ERROR) << "OverlayPages: Failed to create xobject from overlay page "
                 << i;
      return {};
    }

    ScopedFPDFPageObject form_obj(FPDF_NewFormObjectFromXObject(xobject));
    FPDF_CloseXObject(xobject);
    if (!form_obj) {
      LOG(ERROR) << "OverlayPages: Failed to create form object for page " << i;
      return {};
    }

    if (!FPDFPage_InsertObject(base_page.get(), form_obj.release())) {
      LOG(ERROR) << "OverlayPages: Failed to insert form object for page " << i;
      return {};
    }

    if (!FPDFPage_GenerateContent(base_page.get())) {
      LOG(ERROR) << "OverlayPages: Failed to generate content for page " << i;
      return {};
    }
  }

  PDFiumMemBufferFileWrite file_write;
  if (!FPDF_SaveAsCopy(base_doc_.get(), &file_write, 0)) {
    LOG(ERROR) << "OverlayPages: Failed to serialize modified PDF.";
    return {};
  }

  return file_write.TakeBuffer();
}

}  // namespace chrome_pdf

