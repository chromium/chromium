// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PDF_METAFILE_CG_MAC_H_
#define PRINTING_PDF_METAFILE_CG_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/component_export.h"
#include "printing/metafile.h"

namespace printing {

// This class creates a graphics context that renders into a PDF data stream.
class COMPONENT_EXPORT(PRINTING_METAFILE) PdfMetafileCg : public Metafile {
 public:
  PdfMetafileCg();
  PdfMetafileCg(const PdfMetafileCg&) = delete;
  PdfMetafileCg& operator=(const PdfMetafileCg&) = delete;
  ~PdfMetafileCg() override;

  // Metafile methods.
  bool Init() override;
  bool InitFromData(base::span<const uint8_t> data) override;
  void StartPage(const gfx::Size& page_size,
                 const gfx::Rect& content_area,
                 float scale_factor,
                 mojom::PageOrientation page_orientation) override;
  bool FinishPage() override;
  bool FinishDocument() override;

  uint32_t GetDataSize() const override;
  bool GetData(void* dst_buffer, uint32_t dst_buffer_size) const override;
  bool ShouldCopySharedMemoryRegionData() const override;
  mojom::MetafileDataType GetDataType() const override;

  gfx::Rect GetPageBounds(unsigned int page_number) const override;
  unsigned int GetPageCount() const override;

  // Note: The returned context *must not be retained* past Close(). If it is,
  // the data returned from GetData will not be valid PDF data.
  CGContextRef context() const override;

  bool RenderPage(unsigned int page_number,
                  printing::NativeDrawingContext context,
                  const CGRect& rect,
                  bool autorotate,
                  bool fit_to_page) const override;

 private:
  // Returns a CGPDFDocumentRef version of `pdf_data_`.
  CGPDFDocumentRef GetPDFDocument() const;

  // Context for rendering to the pdf.
  base::apple::ScopedCFTypeRef<CGContextRef> context_;

  // PDF backing store.
  base::apple::ScopedCFTypeRef<CFMutableDataRef> pdf_data_;

  // Lazily-created CGPDFDocument representation of `pdf_data_`.
  mutable base::apple::ScopedCFTypeRef<CGPDFDocumentRef> pdf_doc_;

  // Whether or not a page is currently open.
  bool page_is_open_ = false;
};

}  // namespace printing

#endif  // PRINTING_PDF_METAFILE_CG_MAC_H_
