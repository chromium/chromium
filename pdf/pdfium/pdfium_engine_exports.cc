// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine_exports.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_span.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "pdf/document_metadata.h"
#include "pdf/loader/document_loader.h"
#include "pdf/loader/url_loader_wrapper.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_document.h"
#include "pdf/pdfium/pdfium_document_metadata.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_print.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "printing/nup_parameters.h"
#include "printing/units.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_attachment.h"
#include "third_party/pdfium/public/fpdf_catalog.h"
#include "third_party/pdfium/public/fpdf_doc.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_structtree.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory>

#include "base/functional/callback.h"
#include "pdf/pdf_progressive_searchifier.h"
#include "pdf/pdfium/pdfium_progressive_searchifier.h"
#include "pdf/pdfium/pdfium_searchify.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

using printing::ConvertUnitFloat;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

class DataDocumentLoader : public DocumentLoader {
 public:
  explicit DataDocumentLoader(base::span<const uint8_t> pdf_data)
      : pdf_data_(pdf_data) {}
  ~DataDocumentLoader() override = default;

  // DocumentLoader:
  bool Init(std::unique_ptr<URLLoaderWrapper> loader,
            const std::string& url) override {
    NOTREACHED() << "PDFiumDocument doesn't call this";
  }
  bool GetBlock(uint32_t position, uint32_t size, void* buf) const override {
    if (!IsDataAvailable(position, size)) {
      return false;
    }
    auto copy_span = pdf_data_.subspan(position, size);
    memcpy(buf, copy_span.data(), copy_span.size());
    return true;
  }
  bool IsDataAvailable(uint32_t position, uint32_t size) const override {
    CHECK_LE(position, GetDocumentSize());
    CHECK_LE(size, GetDocumentSize() - position);
    return true;
  }
  void RequestData(uint32_t position, uint32_t size) override {}
  bool IsDocumentComplete() const override { return true; }
  uint32_t GetDocumentSize() const override { return pdf_data_.size(); }
  uint32_t BytesReceived() const override { return pdf_data_.size(); }
  void ClearPendingRequests() override {}

 private:
  const base::raw_span<const uint8_t> pdf_data_;
};

int CalculatePosition(FPDF_PAGE page,
                      const PDFiumEngineExports::RenderingSettings& settings,
                      gfx::Rect* dest) {
  // settings.bounds is in terms of the max DPI. Convert page sizes to match.
  const int dpi_x = settings.dpi.width();
  const int dpi_y = settings.dpi.height();
  const int dpi = std::max(dpi_x, dpi_y);
  int page_width = static_cast<int>(
      ConvertUnitFloat(FPDF_GetPageWidthF(page), kPointsPerInch, dpi));
  int page_height = static_cast<int>(
      ConvertUnitFloat(FPDF_GetPageHeightF(page), kPointsPerInch, dpi));

  // Start by assuming that we will draw exactly to the bounds rect
  // specified.
  *dest = settings.bounds;

  int rotate = 0;  // normal orientation.

  // Auto-rotate landscape pages to print correctly.
  if (settings.autorotate &&
      (dest->width() > dest->height()) != (page_width > page_height)) {
    rotate = 3;  // 90 degrees counter-clockwise.
    std::swap(page_width, page_height);
  }

  // See if we need to scale the output
  bool scale_to_bounds = false;
  if (settings.fit_to_bounds &&
      ((page_width > dest->width()) || (page_height > dest->height()))) {
    scale_to_bounds = true;
  } else if (settings.stretch_to_bounds &&
             ((page_width < dest->width()) || (page_height < dest->height()))) {
    scale_to_bounds = true;
  }

  if (scale_to_bounds) {
    // If we need to maintain aspect ratio, calculate the actual width and
    // height.
    if (settings.keep_aspect_ratio) {
      double scale_factor_x = page_width;
      scale_factor_x /= dest->width();
      double scale_factor_y = page_height;
      scale_factor_y /= dest->height();
      if (scale_factor_x > scale_factor_y) {
        dest->set_height(page_height / scale_factor_x);
      } else {
        dest->set_width(page_width / scale_factor_y);
      }
    }
  } else {
    // We are not scaling to bounds. Draw in the actual page size. If the
    // actual page size is larger than the bounds, the output will be
    // clipped.
    dest->set_width(page_width);
    dest->set_height(page_height);
  }

  // Scale the bounds to device units if DPI is rectangular.
  if (dpi_x != dpi_y) {
    dest->set_width(dest->width() * dpi_x / dpi);
    dest->set_height(dest->height() * dpi_y / dpi);
  }

  if (settings.center_in_bounds) {
    gfx::Vector2d offset(
        (settings.bounds.width() * dpi_x / dpi - dest->width()) / 2,
        (settings.bounds.height() * dpi_y / dpi - dest->height()) / 2);
    dest->Offset(offset);
  }
  return rotate;
}

ScopedFPDFDocument LoadPdfData(base::span<const uint8_t> pdf_buffer) {
  return ScopedFPDFDocument(FPDF_LoadMemDocument64(
      pdf_buffer.data(), pdf_buffer.size(), /*password=*/nullptr));
}

ScopedFPDFDocument CreatePdfDoc(
    std::vector<base::span<const uint8_t>> input_buffers) {
  if (input_buffers.empty())
    return nullptr;

  ScopedFPDFDocument doc(FPDF_CreateNewDocument());
  size_t index = 0;
  for (auto input_buffer : input_buffers) {
    ScopedFPDFDocument single_page_doc = LoadPdfData(input_buffer);
    if (!FPDF_ImportPages(doc.get(), single_page_doc.get(), "1", index++)) {
      return nullptr;
    }
  }

  return doc;
}

bool IsValidPrintableArea(const gfx::Size& page_size,
                          const gfx::Rect& printable_area) {
  return !printable_area.IsEmpty() && printable_area.x() >= 0 &&
         printable_area.y() >= 0 &&
         printable_area.right() <= page_size.width() &&
         printable_area.bottom() <= page_size.height();
}

int GetRenderFlagsFromSettings(
    const PDFiumEngineExports::RenderingSettings& settings) {
  int flags = FPDF_ANNOT;
  if (!settings.use_color)
    flags |= FPDF_GRAYSCALE;
  if (settings.render_for_printing)
    flags |= FPDF_PRINTING;
  return flags;
}

base::Value RecursiveGetStructTree(FPDF_STRUCTELEMENT struct_elem) {
  int children_count = FPDF_StructElement_CountChildren(struct_elem);
  if (children_count <= 0)
    return base::Value();

  std::optional<std::u16string> opt_type =
      CallPDFiumWideStringBufferApiAndReturnOptional(
          base::BindRepeating(FPDF_StructElement_GetType, struct_elem), true);
  if (!opt_type)
    return base::Value();

  base::Value::Dict result;
  result.Set("type", *opt_type);

  std::optional<std::u16string> opt_alt =
      CallPDFiumWideStringBufferApiAndReturnOptional(
          base::BindRepeating(FPDF_StructElement_GetAltText, struct_elem),
          true);
  if (opt_alt)
    result.Set("alt", *opt_alt);

  std::optional<std::u16string> opt_lang =
      CallPDFiumWideStringBufferApiAndReturnOptional(
          base::BindRepeating(FPDF_StructElement_GetLang, struct_elem), true);
  if (opt_lang)
    result.Set("lang", *opt_lang);

  base::Value::List children;
  for (int i = 0; i < children_count; i++) {
    FPDF_STRUCTELEMENT child_elem =
        FPDF_StructElement_GetChildAtIndex(struct_elem, i);

    base::Value child = RecursiveGetStructTree(child_elem);
    if (child.is_dict())
      children.Append(std::move(child));
  }

  // use "~children" instead of "children" because we pretty-print the
  // result of this as JSON and the keys are sorted; it's much easier to
  // understand when the children are the last key.
  if (!children.empty())
    result.Set("~children", std::move(children));

  return base::Value(std::move(result));
}

}  // namespace

PDFiumEngineExports::RenderingSettings::RenderingSettings(
    const gfx::Size& dpi,
    const gfx::Rect& bounds,
    bool fit_to_bounds,
    bool stretch_to_bounds,
    bool keep_aspect_ratio,
    bool center_in_bounds,
    bool autorotate,
    bool use_color,
    bool render_for_printing)
    : dpi(dpi),
      bounds(bounds),
      fit_to_bounds(fit_to_bounds),
      stretch_to_bounds(stretch_to_bounds),
      keep_aspect_ratio(keep_aspect_ratio),
      center_in_bounds(center_in_bounds),
      autorotate(autorotate),
      use_color(use_color),
      render_for_printing(render_for_printing) {}

PDFiumEngineExports::RenderingSettings::RenderingSettings(
    const RenderingSettings& that) = default;

PDFiumEngineExports* PDFiumEngineExports::Get() {
  static base::NoDestructor<PDFiumEngineExports> exports;
  return exports.get();
}

PDFiumEngineExports::PDFiumEngineExports() = default;

PDFiumEngineExports::~PDFiumEngineExports() = default;

#if BUILDFLAG(IS_CHROMEOS)
std::optional<FlattenPdfResult> PDFiumEngineExports::CreateFlattenedPdf(
    base::span<const uint8_t> input_buffer) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(input_buffer);
  return doc ? PDFiumPrint::CreateFlattenedPdf(std::move(doc)) : std::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
bool PDFiumEngineExports::RenderPDFPageToDC(
    base::span<const uint8_t> pdf_buffer,
    int page_index,
    const RenderingSettings& settings,
    HDC dc) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;
  ScopedFPDFPage page(FPDF_LoadPage(doc.get(), page_index));
  if (!page)
    return false;

  RenderingSettings new_settings = settings;
  // calculate the page size
  if (new_settings.dpi.width() == -1)
    new_settings.dpi.set_width(GetDeviceCaps(dc, LOGPIXELSX));
  if (new_settings.dpi.height() == -1)
    new_settings.dpi.set_height(GetDeviceCaps(dc, LOGPIXELSY));

  gfx::Rect dest;
  int rotate = CalculatePosition(page.get(), new_settings, &dest);

  int save_state = SaveDC(dc);
  // The caller wanted all drawing to happen within the bounds specified.
  // Based on scale calculations, our destination rect might be larger
  // than the bounds. Set the clip rect to the bounds.
  IntersectClipRect(dc, settings.bounds.x(), settings.bounds.y(),
                    settings.bounds.x() + settings.bounds.width(),
                    settings.bounds.y() + settings.bounds.height());

  int flags = GetRenderFlagsFromSettings(settings);

  // A "temporary" hack. Some PDFs seems to render very slowly if
  // FPDF_RenderPage() is directly used on a printer DC. I suspect it is
  // because of the code to talk Postscript directly to the printer if
  // the printer supports this. Need to discuss this with PDFium. For now,
  // render to a bitmap and then blit the bitmap to the DC if we have been
  // supplied a printer DC.
  int device_type = GetDeviceCaps(dc, TECHNOLOGY);
  if (device_type == DT_RASPRINTER || device_type == DT_PLOTTER) {
    ScopedFPDFBitmap bitmap(
        FPDFBitmap_Create(dest.width(), dest.height(), FPDFBitmap_BGRx));
    // Clear the bitmap
    FPDFBitmap_FillRect(bitmap.get(), 0, 0, dest.width(), dest.height(),
                        0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap.get(), page.get(), 0, 0, dest.width(),
                          dest.height(), rotate, flags);
    int stride = FPDFBitmap_GetStride(bitmap.get());
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dest.width();
    bmi.bmiHeader.biHeight = -dest.height();  // top-down image
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = stride * dest.height();
    StretchDIBits(dc, dest.x(), dest.y(), dest.width(), dest.height(), 0, 0,
                  dest.width(), dest.height(),
                  FPDFBitmap_GetBuffer(bitmap.get()), &bmi, DIB_RGB_COLORS,
                  SRCCOPY);
  } else {
    FPDF_RenderPage(dc, page.get(), dest.x(), dest.y(), dest.width(),
                    dest.height(), rotate, flags);
  }
  RestoreDC(dc, save_state);
  return true;
}

void PDFiumEngineExports::SetPDFUsePrintMode(int mode) {
  FPDF_SetPrintMode(mode);
}
#endif  // BUILDFLAG(IS_WIN)

bool PDFiumEngineExports::RenderPDFPageToBitmap(
    base::span<const uint8_t> pdf_buffer,
    int page_index,
    const RenderingSettings& settings,
    void* bitmap_buffer) {
  constexpr int kBgraImageColorChannels = 4;
  base::CheckedNumeric<int> stride = kBgraImageColorChannels;
  stride *= settings.bounds.width();
  if (!stride.IsValid()) {
    return false;
  }

  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;
  ScopedFPDFPage page(FPDF_LoadPage(doc.get(), page_index));
  if (!page)
    return false;

  gfx::Rect dest;
  int rotate = CalculatePosition(page.get(), settings, &dest);

  ScopedFPDFBitmap bitmap(
      FPDFBitmap_CreateEx(settings.bounds.width(), settings.bounds.height(),
                          FPDFBitmap_BGRA, bitmap_buffer, stride.ValueOrDie()));
  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap.get(), 0, 0, settings.bounds.width(),
                      settings.bounds.height(), 0xFFFFFFFF);
  // Shift top-left corner of bounds to (0, 0) if it's not there.
  dest.set_origin(dest.origin() - settings.bounds.OffsetFromOrigin());

  FPDF_RenderPageBitmap(bitmap.get(), page.get(), dest.x(), dest.y(),
                        dest.width(), dest.height(), rotate,
                        GetRenderFlagsFromSettings(settings));
  return true;
}

std::vector<uint8_t> PDFiumEngineExports::ConvertPdfPagesToNupPdf(
    std::vector<base::span<const uint8_t>> input_buffers,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  if (!IsValidPrintableArea(page_size, printable_area))
    return std::vector<uint8_t>();

  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = CreatePdfDoc(std::move(input_buffers));
  if (!doc)
    return std::vector<uint8_t>();

  return PDFiumPrint::CreateNupPdf(std::move(doc), pages_per_sheet, page_size,
                                   printable_area);
}

std::vector<uint8_t> PDFiumEngineExports::ConvertPdfDocumentToNupPdf(
    base::span<const uint8_t> input_buffer,
    size_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area) {
  if (!IsValidPrintableArea(page_size, printable_area))
    return std::vector<uint8_t>();

  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(input_buffer);
  if (!doc)
    return std::vector<uint8_t>();

  return PDFiumPrint::CreateNupPdf(std::move(doc), pages_per_sheet, page_size,
                                   printable_area);
}

bool PDFiumEngineExports::GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                                        int* page_count,
                                        float* max_page_width) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return false;

  if (!page_count && !max_page_width)
    return true;

  int page_count_local = FPDF_GetPageCount(doc.get());
  if (page_count)
    *page_count = page_count_local;

  if (max_page_width) {
    *max_page_width = 0;
    for (int page_index = 0; page_index < page_count_local; page_index++) {
      FS_SIZEF page_size;
      if (FPDF_GetPageSizeByIndexF(doc.get(), page_index, &page_size) &&
          page_size.width > *max_page_width) {
        *max_page_width = page_size.width;
      }
    }
  }
  return true;
}

std::optional<DocumentMetadata> PDFiumEngineExports::GetPDFDocMetadata(
    base::span<const uint8_t> pdf_buffer) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);

  DataDocumentLoader loader(pdf_buffer);
  PDFiumDocument pdfium_doc(&loader);
  pdfium_doc.file_access().m_FileLen = pdf_buffer.size();

  pdfium_doc.CreateFPDFAvailability();
  FPDF_AVAIL pdf_avail = pdfium_doc.fpdf_availability();
  if (!pdf_avail) {
    return std::nullopt;
  }

  pdfium_doc.LoadDocument("");
  if (!pdfium_doc.doc()) {
    return std::nullopt;
  }

  return GetPDFiumDocumentMetadata(
      pdfium_doc.doc(), pdf_buffer.size(), FPDF_GetPageCount(pdfium_doc.doc()),
      FPDFAvail_IsLinearized(pdf_avail) == PDF_LINEARIZED,
      FPDFDoc_GetAttachmentCount(pdfium_doc.doc()) > 0);
}

std::optional<bool> PDFiumEngineExports::IsPDFDocTagged(
    base::span<const uint8_t> pdf_buffer) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return std::nullopt;

  return FPDFCatalog_IsTagged(doc.get());
}

base::Value PDFiumEngineExports::GetPDFStructTreeForPage(
    base::span<const uint8_t> pdf_buffer,
    int page_index) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return base::Value();

  ScopedFPDFPage page(FPDF_LoadPage(doc.get(), page_index));
  if (!page)
    return base::Value();

  ScopedFPDFStructTree struct_tree(FPDF_StructTree_GetForPage(page.get()));
  if (!struct_tree)
    return base::Value();

  // We only expect one child of the struct tree - i.e. a single root node.
  int children = FPDF_StructTree_CountChildren(struct_tree.get());
  if (children != 1)
    return base::Value();

  FPDF_STRUCTELEMENT struct_root_elem =
      FPDF_StructTree_GetChildAtIndex(struct_tree.get(), 0);
  if (!struct_root_elem)
    return base::Value();

  return RecursiveGetStructTree(struct_root_elem);
}

std::optional<bool> PDFiumEngineExports::PDFDocHasOutline(
    base::span<const uint8_t> pdf_buffer) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc) {
    return std::nullopt;
  }

  return FPDFBookmark_GetFirstChild(doc.get(), nullptr);
}

std::optional<gfx::SizeF> PDFiumEngineExports::GetPDFPageSizeByIndex(
    base::span<const uint8_t> pdf_buffer,
    int page_index) {
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc)
    return std::nullopt;

  FS_SIZEF size;
  if (!FPDF_GetPageSizeByIndexF(doc.get(), page_index, &size))
    return std::nullopt;

  return gfx::SizeF(size.width, size.height);
}

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
std::vector<uint8_t> PDFiumEngineExports::Searchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback) {
  return PDFiumSearchify(pdf_buffer, std::move(perform_ocr_callback));
}

std::unique_ptr<PdfProgressiveSearchifier>
PDFiumEngineExports::CreateProgressiveSearchifier() {
  return std::make_unique<PdfiumProgressiveSearchifier>();
}
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace chrome_pdf
