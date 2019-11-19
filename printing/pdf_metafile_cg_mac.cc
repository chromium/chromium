// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_metafile_cg_mac.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using base::ScopedCFTypeRef;

namespace {

// Rotate a page by |num_rotations| * 90 degrees, counter-clockwise.
void RotatePage(CGContextRef context, const CGRect rect, int num_rotations) {
  switch (num_rotations) {
    case 0:
      break;
    case 1:
      // After rotating by 90 degrees with the axis at the origin, the page
      // content is now "off screen". Shift it right to move it back on screen.
      CGContextTranslateCTM(context, rect.size.width, 0);
      // Rotates counter-clockwise by 90 degrees.
      CGContextRotateCTM(context, base::kPiDouble / 2);
      break;
    case 2:
      // After rotating by 180 degrees with the axis at the origin, the page
      // content is now "off screen". Shift it right and up to move it back on
      // screen.
      CGContextTranslateCTM(context, rect.size.width, rect.size.height);
      // Rotates counter-clockwise by 90 degrees.
      CGContextRotateCTM(context, base::kPiDouble);
      break;
    case 3:
      // After rotating by 270 degrees with the axis at the origin, the page
      // content is now "off screen". Shift it right to move it back on screen.
      CGContextTranslateCTM(context, 0, rect.size.height);
      // Rotates counter-clockwise by 90 degrees.
      CGContextRotateCTM(context, -base::kPiDouble / 2);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

namespace printing {

PdfMetafileCg::PdfMetafileCg() : page_is_open_(false) {}

PdfMetafileCg::~PdfMetafileCg() {}

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
      CGDataConsumerCreateWithCFData(pdf_data_));
  if (!pdf_consumer.get()) {
    LOG(ERROR) << "Failed to create data consumer for metafile";
    pdf_data_.reset();
    return false;
  }
  context_.reset(CGPDFContextCreate(pdf_consumer, nullptr, nullptr));
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create pdf context for metafile";
    pdf_data_.reset();
  }

  return true;
}

bool PdfMetafileCg::InitFromData(const void* src_buffer,
                                 size_t src_buffer_size) {
  DCHECK(!context_.get());
  DCHECK(!pdf_data_.get());

  if (!src_buffer || !src_buffer_size)
    return false;

  if (!base::IsValueInRangeForNumericType<CFIndex>(src_buffer_size))
    return false;

  pdf_data_.reset(CFDataCreateMutable(kCFAllocatorDefault, src_buffer_size));
  CFDataAppendBytes(pdf_data_, static_cast<const UInt8*>(src_buffer),
                    src_buffer_size);
  return true;
}

void PdfMetafileCg::StartPage(const gfx::Size& page_size,
                              const gfx::Rect& content_area,
                              const float& scale_factor) {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

  double height = page_size.height();
  double width = page_size.width();

  CGRect bounds = CGRectMake(0, 0, width, height);
  CGContextBeginPage(context_, &bounds);
  page_is_open_ = true;
  CGContextSaveGState(context_);

  // Move to the context origin.
  CGContextTranslateCTM(context_, content_area.x(), -content_area.y());

  // Flip the context.
  CGContextTranslateCTM(context_, 0, height);
  CGContextScaleCTM(context_, scale_factor, -scale_factor);
}

bool PdfMetafileCg::FinishPage() {
  DCHECK(context_.get());
  DCHECK(page_is_open_);

  CGContextRestoreGState(context_);
  CGContextEndPage(context_);
  page_is_open_ = false;
  return true;
}

bool PdfMetafileCg::FinishDocument() {
  DCHECK(context_.get());
  DCHECK(!page_is_open_);

#ifndef NDEBUG
  // Check that the context will be torn down properly; if it's not, |pdf_data|
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
                               const CGRect rect,
                               const MacRenderPageParams& params) const {
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
  int pdf_src_rotation = CGPDFPageGetRotationAngle(pdf_page);
  const bool source_is_landscape =
      (source_rect.size.width > source_rect.size.height);
  const bool dest_is_landscape = (rect.size.width > rect.size.height);
  const bool rotate =
      params.autorotate ? (source_is_landscape != dest_is_landscape) : false;
  const float source_width =
      rotate ? source_rect.size.height : source_rect.size.width;
  const float source_height =
      rotate ? source_rect.size.width : source_rect.size.height;

  // See if we need to scale the output.
  float scaling_factor = 1.0;
  const bool scaling_needed =
      (params.shrink_to_fit && ((source_width > rect.size.width) ||
                                (source_height > rect.size.height))) ||
      (params.stretch_to_fit && ((source_width < rect.size.width) &&
                                 (source_height < rect.size.height)));
  if (scaling_needed) {
    float x_scaling_factor = rect.size.width / source_width;
    float y_scaling_factor = rect.size.height / source_height;
    scaling_factor = std::min(x_scaling_factor, y_scaling_factor);
  }
  // Some PDFs have a non-zero origin. Need to take that into account and align
  // the PDF to the origin.
  const float x_origin_offset = -1 * source_rect.origin.x;
  const float y_origin_offset = -1 * source_rect.origin.y;

  // If the PDF needs to be centered, calculate the offsets here.
  float x_offset =
      params.center_horizontally
          ? ((rect.size.width - (source_width * scaling_factor)) / 2)
          : 0;
  if (rotate)
    x_offset = -x_offset;

  float y_offset =
      params.center_vertically
          ? ((rect.size.height - (source_height * scaling_factor)) / 2)
          : 0;

  CGContextSaveGState(context);

  // The transform operations specified here gets applied in reverse order.
  // i.e. the origin offset translation happens first.
  // Origin is at bottom-left.
  CGContextTranslateCTM(context, x_offset, y_offset);

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
  if (page_number > GetPageCount()) {
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
  return static_cast<uint32_t>(CFDataGetLength(pdf_data_));
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

  CFDataGetBytes(pdf_data_, CFRangeMake(0, dst_buffer_size),
                 static_cast<UInt8*>(dst_buffer));
  return true;
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
        CGDataProviderCreateWithCFData(pdf_data_));
    pdf_doc_.reset(CGPDFDocumentCreateWithProvider(pdf_data_provider));
  }
  return pdf_doc_.get();
}

}  // namespace printing
