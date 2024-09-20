// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_cg_mac.h"

#include <stdint.h>

#include <algorithm>
#include <numbers>

#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "printing/mojom/print.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using base::apple::ScopedCFTypeRef;

namespace {

// Rotate a page by `num_rotations` * 90 degrees, counter-clockwise.
void RotatePage(CGContextRef context, const CGRect& rect, int num_rotations) {
  switch (num_rotations) {
    case 0:
      break;
    case 1:
      // After rotating by 90 degrees with the axis at the origin, the page
      // content is now "off screen". Shift it right to move it back on screen.
      CGContextTranslateCTM(context, rect.size.width, 0);
      // Rotates counter-clockwise by 90 degrees.
      CGContextRotateCTM(context, std::numbers::pi / 2);
      break;
    case 2:
      // After rotating by 180 degrees with the axis at the origin, the page
      // content is now "off screen". Shift it right and up to move it back on
      // screen.
      CGContextTranslateCTM(context, rect.size.width, rect.size.height);
      // Rotates counter-clockwise by 90 degrees.
      CGContextRotateCTM(context, std::numbers::pi);
      break;
    case 3:
      // After rotating by 270 degrees with the axis at the origin, the page
      // content is now "off screen". Shift it right to move it back on screen.
      CGContextTranslateCTM(context, 0, rect.size.height);
      // Rotates counter-clockwise by 90 degrees.
      CGContextRotateCTM(context, -std::numbers::pi / 2);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace printing {

PdfMetafileCg::PdfMetafileCg() = default;

PdfMetafileCg::~PdfMetafileCg() = default;

bool PdfMetafileCg::Init() {
  // Ensure that Init hasn't already been called.
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, 0));
  if (!pdf_data_.get()) {
    LOG(ERROR) << "Failed to create pdf data for metafile";
    return false;
  }
  ScopedCFTypeRef<CGDataConsumerRef> pdf_consumer(
      CGDataConsumerCreateWithCFData(pdf_data_.get()));
  if (!pdf_consumer.get()) {
    LOG(ERROR) << "Failed to create data consumer for metafile";
    pdf_data_.reset();
    return false;
  }
  context_.reset(CGPDFContextCreate(pdf_consumer.get(), nullptr, nullptr));
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create pdf context for metafile";
    pdf_data_.reset();
  }

  return true;
}

bool PdfMetafileCg::InitFromData(base::span<const uint8_t> data) {
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  if (data.empty())
    return false;

  if (!base::IsValueInRangeForNumericType<CFIndex>(data.size()))
    return false;

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, data.size()));
  CFDataAppendBytes(pdf_data_.get(), data.data(), data.size());
  return true;
}

void PdfMetafileCg::StartPage(const gfx::Size& page_size,
                              const gfx::Rect& content_area,
                              float scale_factor,
                              mojom::PageOrientation page_orientation) {
  DCHECK_EQ(page_orientation, mojom::PageOrientation::kUpright)
      << "Not implemented";
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  page_is_open_ = true;
  float height = page_size.height();
  float width = page_size.width();

  CGRect bounds = CGRectMake(0, 0, width, height);
  CGContextBeginPage(context_.get(), &bounds);
  CGContextSaveGState(context_.get());

  // Move to the context origin.
  CGContextTranslateCTM(context_.get(), content_area.x(), -content_area.y());

  // Flip the context.
  CGContextTranslateCTM(context_.get(), 0, height);
  CGContextScaleCTM(context_.get(), scale_factor, -scale_factor);
}

bool PdfMetafileCg::FinishPage() {
  DCHECK(context_);
  DCHECK(page_is_open_);

  CGContextRestoreGState(context_.get());
  CGContextEndPage(context_.get());
  page_is_open_ = false;
  return true;
}

bool PdfMetafileCg::FinishDocument() {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

#ifndef NDEBUG
  // Check that the context will be torn down properly; if it's not, `pdf_data`
  // will be incomplete and generate invalid PDF files/documents.
  if (context_.get()) {
    CFIndex extra_retain_count = CFGetRetainCount(context_.get()) - 1;
    if (extra_retain_count > 0) {
      LOG(ERROR) << "Metafile context has " << extra_retain_count
                 << " extra retain(s) on Close";
    }
  }
#endif
  CGPDFContextClose(context_.get());
  context_.reset();
  return true;
}

bool PdfMetafileCg::RenderPage(unsigned int page_number,
                               CGContextRef context,
                               const CGRect& rect,
                               bool autorotate,
                               bool fit_to_page) const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  if (!pdf_doc) {
    LOG(ERROR) << "Unable to create PDF document from data";
    return false;
  }

  const unsigned int page_count = GetPageCount();
  DCHECK_NE(page_count, 0U);
  DCHECK_NE(page_number, 0U);
  DCHECK_LE(page_number, page_count);

  CGPDFPageRef pdf_page = CGPDFDocumentGetPage(pdf_doc, page_number);
  CGRect source_rect = CGPDFPageGetBoxRect(pdf_page, kCGPDFCropBox);
  const int pdf_src_rotation = CGPDFPageGetRotationAngle(pdf_page);
  const bool source_is_landscape =
      (source_rect.size.width > source_rect.size.height);
  const bool dest_is_landscape = (rect.size.width > rect.size.height);
  const bool rotate = autorotate && (source_is_landscape != dest_is_landscape);
  const float source_width =
      rotate ? source_rect.size.height : source_rect.size.width;
  const float source_height =
      rotate ? source_rect.size.width : source_rect.size.height;

  // See if we need to scale the output.
  float scaling_factor = 1.0;
  const bool scaling_needed =
      fit_to_page && ((source_width != rect.size.width) ||
                      (source_height != rect.size.height));
  if (scaling_needed) {
    float x_scaling_factor = rect.size.width / source_width;
    float y_scaling_factor = rect.size.height / source_height;
    scaling_factor = std::min(x_scaling_factor, y_scaling_factor);
  }

  CGContextSaveGState(context);

  int num_rotations = 0;
  if (rotate) {
    if (pdf_src_rotation == 0 || pdf_src_rotation == 270) {
      num_rotations = 1;
    } else {
      num_rotations = 3;
    }
  } else {
    if (pdf_src_rotation == 180 || pdf_src_rotation == 270) {
      num_rotations = 2;
    }
  }
  RotatePage(context, rect, num_rotations);

  CGContextScaleCTM(context, scaling_factor, scaling_factor);

  // Some PDFs have a non-zero origin. Need to take that into account and align
  // the PDF to the CoreGraphics's coordinate system origin. Also realign the
  // contents from the bottom-left of the page to top-left in order to stay
  // consistent with Print Preview.
  // A rotational vertical offset is calculated to determine how much to offset
  // the y-component of the origin to move the origin from bottom-left to
  // top-right. When the source is not rotated, the offset is simply the
  // difference between the paper height and the source height. When rotated,
  // the y-axis of the source falls along the width of the source and paper, so
  // the offset becomes the difference between the paper width and the source
  // width.
  const float rotational_vertical_offset =
      rotate ? (rect.size.width - (scaling_factor * source_width))
             : (rect.size.height - (scaling_factor * source_height));
  const float x_origin_offset = -1 * source_rect.origin.x;
  const float y_origin_offset =
      rotational_vertical_offset - source_rect.origin.y;
  CGContextTranslateCTM(context, x_origin_offset, y_origin_offset);

  CGContextDrawPDFPage(context, pdf_page);
  CGContextRestoreGState(context);

  return true;
}

unsigned int PdfMetafileCg::GetPageCount() const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  return pdf_doc ? CGPDFDocumentGetNumberOfPages(pdf_doc) : 0;
}

gfx::Rect PdfMetafileCg::GetPageBounds(unsigned int page_number) const {
  CGPDFDocumentRef pdf_doc = GetPDFDocument();
  if (!pdf_doc) {
    LOG(ERROR) << "Unable to create PDF document from data";
    return gfx::Rect();
  }
  if (page_number == 0 || page_number > GetPageCount()) {
    LOG(ERROR) << "Invalid page number: " << page_number;
    return gfx::Rect();
  }
  CGPDFPageRef pdf_page = CGPDFDocumentGetPage(pdf_doc, page_number);
  CGRect page_rect = CGPDFPageGetBoxRect(pdf_page, kCGPDFMediaBox);
  return gfx::Rect(page_rect);
}

uint32_t PdfMetafileCg::GetDataSize() const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);

  if (!pdf_data_)
    return 0;
  return static_cast<uint32_t>(CFDataGetLength(pdf_data_.get()));
}

bool PdfMetafileCg::GetData(void* dst_buffer, uint32_t dst_buffer_size) const {
  // PDF data is only valid/complete once the context is released.
  DCHECK(!context_);
  DCHECK(pdf_data_);
  DCHECK(dst_buffer);
  DCHECK_GT(dst_buffer_size, 0U);

  uint32_t data_size = GetDataSize();
  if (dst_buffer_size > data_size) {
    return false;
  }

  CFDataGetBytes(pdf_data_.get(), CFRangeMake(0, dst_buffer_size),
                 static_cast<UInt8*>(dst_buffer));
  return true;
}

bool PdfMetafileCg::ShouldCopySharedMemoryRegionData() const {
  // Since `InitFromData()` copies the data, the caller doesn't have to.
  return false;
}

mojom::MetafileDataType PdfMetafileCg::GetDataType() const {
  return mojom::MetafileDataType::kPDF;
}

CGContextRef PdfMetafileCg::context() const {
  return context_.get();
}

CGPDFDocumentRef PdfMetafileCg::GetPDFDocument() const {
  // Make sure that we have data, and that it's not being modified any more.
  DCHECK(pdf_data_.get());
  DCHECK(!context_.get());

  if (!pdf_doc_.get()) {
    ScopedCFTypeRef<CGDataProviderRef> pdf_data_provider(
        CGDataProviderCreateWithCFData(pdf_data_.get()));
    pdf_doc_.reset(CGPDFDocumentCreateWithProvider(pdf_data_provider.get()));
  }
  return pdf_doc_.get();
}

}  // namespace printing
