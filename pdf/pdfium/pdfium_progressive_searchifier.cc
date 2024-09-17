// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_progressive_searchifier.h"

#include "base/check.h"
#include "base/compiler_specific.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_searchify.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace {

int GetBlockForJpeg(void* param,
                    unsigned long pos,
                    unsigned char* buf,
                    unsigned long size) {
  auto data_vector = *static_cast<base::span<const uint8_t>*>(param);
  if (pos + size < pos || pos + size > data_vector.size()) {
    return 0;
  }
  // TODO(tsepez): spanify arguments to remove the error.
  base::span<uint8_t> UNSAFE_TODO(buf_span(buf, size));
  buf_span.copy_from(data_vector.subspan(pos, size));
  return 1;
}

}  // namespace

namespace chrome_pdf {

PdfiumProgressiveSearchifier::ScopedSdkInitializer::ScopedSdkInitializer() {
  // TODO(thestig): Check the default value of `use_skia`.
  InitializeSDK(false, false, FontMappingMode::kNoMapping);
}

PdfiumProgressiveSearchifier::ScopedSdkInitializer::~ScopedSdkInitializer() {
  ShutdownSDK();
}

PdfiumProgressiveSearchifier::PdfiumProgressiveSearchifier()
    : doc_(FPDF_CreateNewDocument()), font_(CreateFont(doc_.get())) {
  CHECK(doc_);
  CHECK(font_);
}

PdfiumProgressiveSearchifier::~PdfiumProgressiveSearchifier() = default;

// TODO(chuhsuan): Return bool instead of crashing on error.
void PdfiumProgressiveSearchifier::AddPage(
    const SkBitmap& bitmap,
    uint32_t page_index,
    screen_ai::mojom::VisualAnnotationPtr annotation) {
  CHECK(annotation);
  // Replace the page if it already exists.
  DeletePage(page_index);
  int width = bitmap.width();
  int height = bitmap.height();
  ScopedFPDFPage page(FPDFPage_New(doc_.get(), page_index, width, height));
  CHECK(page);
  ScopedFPDFPageObject image(FPDFPageObj_NewImageObj(doc_.get()));
  CHECK(image);
  std::vector<uint8_t> encoded;
  CHECK(gfx::JPEGCodec::Encode(bitmap, 100, &encoded));
  FPDF_FILEACCESS file_access{
      .m_FileLen = static_cast<unsigned long>(encoded.size()),
      .m_GetBlock = &GetBlockForJpeg,
      .m_Param = &encoded};
  CHECK(FPDFImageObj_LoadJpegFileInline(nullptr, 0, image.get(), &file_access));
  CHECK(FPDFImageObj_SetMatrix(image.get(), width, 0, 0, height, 0, 0));
  AddTextOnImage(doc_.get(), page.get(), font_.get(), image.get(),
                 std::move(annotation), gfx::Size(width, height));
  FPDFPage_InsertObject(page.get(), image.release());
  CHECK(FPDFPage_GenerateContent(page.get()));
}

void PdfiumProgressiveSearchifier::DeletePage(uint32_t page_index) {
  FPDFPage_Delete(doc_.get(), page_index);
}

std::vector<uint8_t> PdfiumProgressiveSearchifier::Save() {
  PDFiumMemBufferFileWrite output_file_write;
  CHECK(FPDF_SaveAsCopy(doc_.get(), &output_file_write, 0));
  return output_file_write.TakeBuffer();
}

}  // namespace chrome_pdf
