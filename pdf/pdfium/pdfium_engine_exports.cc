// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine_exports.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_span.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "pdf/document_metadata.h"
#include "pdf/loader/document_loader.h"
#include "pdf/loader/url_loader_wrapper.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_document.h"
#include "pdf/pdfium/pdfium_document_metadata.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_print.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "printing/nup_parameters.h"
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

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory>

#include "base/functional/callback.h"
#include "pdf/pdf_progressive_searchifier.h"
#include "pdf/pdfium/pdfium_progressive_searchifier.h"
#include "pdf/pdfium/pdfium_searchify.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

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
  return RenderPageToDC(page.get(), settings, dc);
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
  ScopedUnsupportedFeature scoped_unsupported_feature(
      ScopedUnsupportedFeature::kNoEngine);
  ScopedFPDFDocument doc = LoadPdfData(pdf_buffer);
  if (!doc) {
    return false;
  }
  ScopedFPDFPage page(FPDF_LoadPage(doc.get(), page_index));
  return RenderPageToBitmap(page.get(), settings, bitmap_buffer);
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
