// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/metafile_skia.h"

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "cc/paint/skia_paint_canvas.h"
#include "printing/metafile_agent.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/docs/SkMultiPictureDocument.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

#if BUILDFLAG(IS_APPLE)
#include "printing/pdf_metafile_cg_mac.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/files/file_util.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// `InitFromData()` should make a copy of data for the safety of all operations
// which would then operate upon that.
constexpr bool kInitFromDataCopyData = true;

bool WriteAssetToBuffer(const SkStreamAsset* asset, void* buffer, size_t size) {
  // Calling duplicate() keeps original asset state unchanged.
  std::unique_ptr<SkStreamAsset> assetCopy(asset->duplicate());
  size_t length = assetCopy->getLength();
  return length <= size && length == assetCopy->read(buffer, length);
}

}  // namespace

namespace printing {

struct Page {
  Page(const SkSize& s, cc::PaintRecord c) : size(s), content(std::move(c)) {}
  SkSize size;
  cc::PaintRecord content;
};

struct MetafileSkiaData {
  cc::InspectablePaintRecorder recorder;  // Current recording

  std::vector<Page> pages;
  std::unique_ptr<SkStreamAsset> data_stream;
  ContentToProxyTokenMap subframe_content_info;
  std::map<uint32_t, sk_sp<SkPicture>> subframe_pics;
  int document_cookie = 0;
  raw_ptr<ContentProxySet> typeface_content_info = nullptr;

  // The scale factor is used because Blink occasionally calls
  // PaintCanvas::getTotalMatrix() even though the total matrix is not as
  // meaningful for a vector canvas as for a raster canvas.
  float scale_factor;
  SkSize size;
  mojom::SkiaDocumentType type;

#if BUILDFLAG(IS_APPLE)
  PdfMetafileCg pdf_cg;
#endif
};

MetafileSkia::MetafileSkia() : data_(std::make_unique<MetafileSkiaData>()) {
  data_->type = mojom::SkiaDocumentType::kPDF;
}

MetafileSkia::MetafileSkia(mojom::SkiaDocumentType type, int document_cookie)
    : data_(std::make_unique<MetafileSkiaData>()) {
  data_->type = type;
  data_->document_cookie = document_cookie;
}

MetafileSkia::~MetafileSkia() = default;

bool MetafileSkia::Init() {
  return true;
}

void MetafileSkia::UtilizeTypefaceContext(
    ContentProxySet* typeface_content_info) {
  data_->typeface_content_info = typeface_content_info;
}

// TODO(halcanary): Create a Metafile class that only stores data.
// Metafile::InitFromData is orthogonal to what the rest of
// MetafileSkia does.
bool MetafileSkia::InitFromData(base::span<const uint8_t> data) {
  data_->data_stream = std::make_unique<SkMemoryStream>(
      data.data(), data.size(), kInitFromDataCopyData);
  return true;
}

void MetafileSkia::StartPage(const gfx::Size& page_size,
                             const gfx::Rect& content_area,
                             float scale_factor,
                             mojom::PageOrientation page_orientation) {
  gfx::Size physical_page_size = page_size;
  if (page_orientation != mojom::PageOrientation::kUpright)
    physical_page_size.SetSize(page_size.height(), page_size.width());

  DCHECK_GT(page_size.width(), 0);
  DCHECK_GT(page_size.height(), 0);
  DCHECK_GT(scale_factor, 0.0f);
  if (data_->recorder.getRecordingCanvas())
    FinishPage();
  DCHECK(!data_->recorder.getRecordingCanvas());

  float inverse_scale = 1.0 / scale_factor;
  cc::PaintCanvas* canvas = data_->recorder.beginRecording(
      gfx::ScaleToCeiledSize(physical_page_size, inverse_scale));
  // Recording canvas is owned by the `data_->recorder`.  No ref() necessary.
  if (content_area != gfx::Rect(page_size) ||
      page_orientation != mojom::PageOrientation::kUpright) {
    canvas->scale(inverse_scale, inverse_scale);
    if (page_orientation == mojom::PageOrientation::kRotateLeft) {
      canvas->translate(0, physical_page_size.height());
      canvas->rotate(-90);
    } else if (page_orientation == mojom::PageOrientation::kRotateRight) {
      canvas->translate(physical_page_size.width(), 0);
      canvas->rotate(90);
    }
    SkRect sk_content_area = gfx::RectToSkRect(content_area);
    canvas->clipRect(sk_content_area);
    canvas->translate(sk_content_area.x(), sk_content_area.y());
    canvas->scale(scale_factor, scale_factor);
  }

  data_->size = gfx::SizeFToSkSize(gfx::SizeF(physical_page_size));
  data_->scale_factor = scale_factor;
  // We scale the recording canvas's size so that
  // canvas->getTotalMatrix() returns a value that ignores the scale
  // factor.  We store the scale factor and re-apply it later.
  // http://crbug.com/469656
}

cc::PaintCanvas* MetafileSkia::GetVectorCanvasForNewPage(
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    float scale_factor,
    mojom::PageOrientation page_orientation) {
  StartPage(page_size, content_area, scale_factor, page_orientation);
  return data_->recorder.getRecordingCanvas();
}

bool MetafileSkia::FinishPage() {
  if (!data_->recorder.getRecordingCanvas())
    return false;

  cc::PaintRecord pic = data_->recorder.finishRecordingAsPicture();
  if (data_->scale_factor != 1.0f) {
    cc::PaintRecorder recorder;
    cc::PaintCanvas* canvas = recorder.beginRecording();
    canvas->scale(data_->scale_factor, data_->scale_factor);
    canvas->drawPicture(std::move(pic));
    pic = recorder.finishRecordingAsPicture();
  }
  AppendPage(data_->size, std::move(pic));
  return true;
}

bool MetafileSkia::FinishDocument() {
  // If we've already set the data in InitFromData, leave it be.
  if (data_->data_stream)
    return false;

  if (data_->recorder.getRecordingCanvas())
    FinishPage();

  SkDynamicMemoryWStream stream;
  sk_sp<SkDocument> doc;
  cc::PlaybackCallbacks::CustomDataRasterCallback custom_callback;
  switch (data_->type) {
    case mojom::SkiaDocumentType::kPDF:
      doc = MakePdfDocument(printing::GetAgent(), title_, accessibility_tree_,
                            generate_document_outline_, &stream);
      break;
#if BUILDFLAG(IS_WIN)
    case mojom::SkiaDocumentType::kXPS:
      doc = MakeXpsDocument(&stream);
      break;
#endif
    case mojom::SkiaDocumentType::kMSKP:
      SkSerialProcs procs = SerializationProcs(&data_->subframe_content_info,
                                               data_->typeface_content_info);
      doc = SkMultiPictureDocument::Make(&stream, &procs);
      // It is safe to use base::Unretained(this) because the callback
      // is only used by `canvas` in the following loop which has shorter
      // lifetime than `this`.
      custom_callback = base::BindRepeating(
          &MetafileSkia::CustomDataToSkPictureCallback, base::Unretained(this));
      break;
  }

  for (const Page& page : data_->pages) {
    cc::SkiaPaintCanvas canvas(
        doc->beginPage(page.size.width(), page.size.height()));
    canvas.drawPicture(page.content, custom_callback);
    doc->endPage();
  }
  doc->close();

  data_->data_stream = stream.detachAsStream();
  return true;
}

void MetafileSkia::FinishFrameContent() {
  // Sanity check to make sure we print the entire frame as a single page
  // content.
  DCHECK_EQ(data_->pages.size(), 1u);
  // Also make sure it is in skia multi-picture document format.
  DCHECK_EQ(data_->type, mojom::SkiaDocumentType::kMSKP);
  DCHECK(!data_->data_stream);

  cc::PlaybackCallbacks callbacks;
  callbacks.custom_callback = base::BindRepeating(
      &MetafileSkia::CustomDataToSkPictureCallback, base::Unretained(this));
  sk_sp<SkPicture> pic = data_->pages[0].content.ToSkPicture(
      SkRect::MakeSize(data_->pages[0].size), nullptr, callbacks);
  SkSerialProcs procs = SerializationProcs(&data_->subframe_content_info,
                                           data_->typeface_content_info);
  SkDynamicMemoryWStream stream;
  pic->serialize(&stream, &procs);
  data_->data_stream = stream.detachAsStream();
}

uint32_t MetafileSkia::GetDataSize() const {
  if (!data_->data_stream)
    return 0;
  return base::checked_cast<uint32_t>(data_->data_stream->getLength());
}

bool MetafileSkia::GetData(void* dst_buffer, uint32_t dst_buffer_size) const {
  if (!data_->data_stream)
    return false;
  return WriteAssetToBuffer(data_->data_stream.get(), dst_buffer,
                            base::checked_cast<size_t>(dst_buffer_size));
}

bool MetafileSkia::ShouldCopySharedMemoryRegionData() const {
  // When `InitFromData()` copies the data, the caller doesn't have to.
  return !kInitFromDataCopyData;
}

mojom::MetafileDataType MetafileSkia::GetDataType() const {
  return mojom::MetafileDataType::kPDF;
}

gfx::Rect MetafileSkia::GetPageBounds(unsigned int page_number) const {
  if (page_number > 0 && page_number - 1 < data_->pages.size()) {
    SkSize size = data_->pages[page_number - 1].size;
    return gfx::Rect(base::ClampRound(size.width()),
                     base::ClampRound(size.height()));
  }
  return gfx::Rect();
}

unsigned int MetafileSkia::GetPageCount() const {
  return base::checked_cast<unsigned int>(data_->pages.size());
}

printing::NativeDrawingContext MetafileSkia::context() const {
  NOTREACHED();
}

#if BUILDFLAG(IS_WIN)
bool MetafileSkia::Playback(printing::NativeDrawingContext hdc,
                            const RECT* rect) const {
  NOTREACHED();
}

bool MetafileSkia::SafePlayback(printing::NativeDrawingContext hdc) const {
  NOTREACHED();
}

#elif BUILDFLAG(IS_APPLE)
/* TODO(caryclark): The set up of PluginInstance::PrintPDFOutput may result in
   rasterized output.  Even if that flow uses PdfMetafileCg::RenderPage,
   the drawing of the PDF into the canvas may result in a rasterized output.
   PDFMetafileSkia::RenderPage should be not implemented as shown and instead
   should do something like the following CL in PluginInstance::PrintPDFOutput:
http://codereview.chromium.org/7200040/diff/1/webkit/plugins/ppapi/ppapi_plugin_instance.cc
*/
bool MetafileSkia::RenderPage(unsigned int page_number,
                              CGContextRef context,
                              const CGRect& rect,
                              bool autorotate,
                              bool fit_to_page) const {
  DCHECK_GT(GetDataSize(), 0U);
  if (data_->pdf_cg.GetDataSize() == 0) {
    if (GetDataSize() == 0)
      return false;
    size_t length = data_->data_stream->getLength();
    std::vector<uint8_t> buffer(length);
    std::ignore =
        WriteAssetToBuffer(data_->data_stream.get(), &buffer[0], length);
    data_->pdf_cg.InitFromData(buffer);
  }
  return data_->pdf_cg.RenderPage(page_number, context, rect, autorotate,
                                  fit_to_page);
}
#endif

#if BUILDFLAG(IS_ANDROID)
bool MetafileSkia::SaveToFileDescriptor(int fd) const {
  if (GetDataSize() == 0u)
    return false;

  std::unique_ptr<SkStreamAsset> asset(data_->data_stream->duplicate());

  static constexpr size_t kMaximumBufferSize = 1024 * 1024;
  std::vector<uint8_t> buffer(std::min(kMaximumBufferSize, asset->getLength()));
  do {
    size_t read_size = asset->read(&buffer[0], buffer.size());
    bool is_at_end = read_size < buffer.size();
    if (read_size == 0u) {
      break;
    }
    DCHECK_GE(buffer.size(), read_size);
    buffer.resize(read_size);
    if (!base::WriteFileDescriptor(fd, buffer)) {
      return false;
    } else if (is_at_end) {
      break;
    }
  } while (true);

  return true;
}
#else
bool MetafileSkia::SaveTo(base::File* file) const {
  if (GetDataSize() == 0U)
    return false;

  // Calling duplicate() keeps original asset state unchanged.
  std::unique_ptr<SkStreamAsset> asset(data_->data_stream->duplicate());

  static constexpr size_t kMaximumBufferSize = 1024 * 1024;
  std::vector<uint8_t> buffer(std::min(kMaximumBufferSize, asset->getLength()));
  do {
    size_t read_size = asset->read(&buffer[0], buffer.size());
    bool is_at_end = read_size < buffer.size();
    if (read_size == 0) {
      break;
    }
    DCHECK_GE(buffer.size(), read_size);
    if (!file->WriteAtCurrentPosAndCheck(
            base::make_span(&buffer[0], read_size))) {
      return false;
    } else if (is_at_end) {
      break;
    }
  } while (true);

  return true;
}
#endif  // BUILDFLAG(IS_ANDROID)

std::unique_ptr<MetafileSkia> MetafileSkia::GetMetafileForCurrentPage(
    mojom::SkiaDocumentType type) {
  // If we only ever need the metafile for the last page, should we
  // only keep a handle on one PaintRecord?
  auto metafile = std::make_unique<MetafileSkia>(type, data_->document_cookie);
  if (data_->pages.size() == 0)
    return metafile;

  if (data_->recorder.getRecordingCanvas())  // page outstanding
    return metafile;

  metafile->data_->pages.push_back(data_->pages.back());
  metafile->data_->subframe_content_info = data_->subframe_content_info;
  metafile->data_->subframe_pics = data_->subframe_pics;
  metafile->data_->typeface_content_info = data_->typeface_content_info;

  if (!metafile->FinishDocument())  // Generate PDF.
    metafile.reset();

  return metafile;
}

uint32_t MetafileSkia::CreateContentForRemoteFrame(
    const gfx::Rect& rect,
    const base::UnguessableToken& render_proxy_token) {
  // Create a place holder picture.
  sk_sp<SkPicture> pic = SkPicture::MakePlaceholder(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()));

  // Store the map between content id and the proxy id and store the picture
  // content.
  const uint32_t content_id = pic->uniqueID();
  DCHECK(!base::Contains(data_->subframe_content_info, content_id));
  AppendSubframeInfo(content_id, render_proxy_token, std::move(pic));
  return content_id;
}

int MetafileSkia::GetDocumentCookie() const {
  return data_->document_cookie;
}

const ContentToProxyTokenMap& MetafileSkia::GetSubframeContentInfo() const {
  return data_->subframe_content_info;
}

void MetafileSkia::AppendPage(const SkSize& page_size, cc::PaintRecord record) {
  data_->pages.emplace_back(page_size, std::move(record));
}

void MetafileSkia::AppendSubframeInfo(uint32_t content_id,
                                      const base::UnguessableToken& proxy_token,
                                      sk_sp<SkPicture> pic_holder) {
  data_->subframe_content_info[content_id] = proxy_token;
  data_->subframe_pics[content_id] = pic_holder;
}

SkStreamAsset* MetafileSkia::GetPdfData() const {
  return data_->data_stream.get();
}

void MetafileSkia::CustomDataToSkPictureCallback(SkCanvas* canvas,
                                                 uint32_t content_id) {
  // Check whether this is the one we need to handle.
  if (!base::Contains(data_->subframe_content_info, content_id))
    return;

  auto it = data_->subframe_pics.find(content_id);
  CHECK(it != data_->subframe_pics.end(), base::NotFatalUntil::M130);

  // Found the picture, draw it on canvas.
  sk_sp<SkPicture> pic = it->second;
  SkRect rect = pic->cullRect();
  SkMatrix matrix = SkMatrix::Translate(rect.x(), rect.y());
  canvas->drawPicture(it->second, &matrix, nullptr);
}

}  // namespace printing
