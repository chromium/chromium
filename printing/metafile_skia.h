// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_SKIA_H_
#define PRINTING_METAFILE_SKIA_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "printing/common/metafile_utils.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/accessibility/ax_tree_update.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

class SkCanvas;
class SkPicture;
class SkStreamAsset;

namespace base {
class UnguessableToken;
}  // namespace base

namespace printing {

struct MetafileSkiaData;

// This class uses Skia graphics library to generate a PDF or MSKP document.
class COMPONENT_EXPORT(PRINTING_METAFILE) MetafileSkia : public Metafile {
 public:
  // Default constructor, for mojom::SkiaDocumentType::kPDF type only.
  // TODO(weili): we should split up this use case into a different class, see
  //              comments before InitFromData()'s implementation.
  MetafileSkia();
  MetafileSkia(mojom::SkiaDocumentType type, int document_cookie);
  MetafileSkia(const MetafileSkia&) = delete;
  MetafileSkia& operator=(const MetafileSkia&) = delete;
  ~MetafileSkia() override;

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

  printing::NativeDrawingContext context() const override;

#if BUILDFLAG(IS_WIN)
  bool Playback(printing::NativeDrawingContext hdc,
                const RECT* rect) const override;
  bool SafePlayback(printing::NativeDrawingContext hdc) const override;
#elif BUILDFLAG(IS_APPLE)
  bool RenderPage(unsigned int page_number,
                  printing::NativeDrawingContext context,
                  const CGRect& rect,
                  bool autorotate,
                  bool fit_to_page) const override;
#endif

#if BUILDFLAG(IS_ANDROID)
  bool SaveToFileDescriptor(int fd) const override;
#else
  bool SaveTo(base::File* file) const override;
#endif  // BUILDFLAG(IS_ANDROID)

  // Unlike FinishPage() or FinishDocument(), this is for out-of-process
  // subframe printing. It will just serialize the content into SkPicture
  // format and store it as final data.
  void FinishFrameContent();

  // Return a new metafile containing just the current page in draft mode.
  std::unique_ptr<MetafileSkia> GetMetafileForCurrentPage(
      mojom::SkiaDocumentType type);

  // This method calls StartPage and then returns an appropriate
  // PlatformCanvas implementation bound to the context created by
  // StartPage or NULL on error.  The PaintCanvas pointer that
  // is returned is owned by this MetafileSkia object and does not
  // need to be ref()ed or unref()ed.  The canvas will remain valid
  // until FinishPage() or FinishDocument() is called.
  cc::PaintCanvas* GetVectorCanvasForNewPage(
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      float scale_factor,
      mojom::PageOrientation page_orientation);

  // This is used for painting content of out-of-process subframes.
  // For such a subframe, since the content is in another process, we create a
  // place holder picture now, and replace it with actual content by pdf
  // compositor service later.
  uint32_t CreateContentForRemoteFrame(
      const gfx::Rect& rect,
      const base::UnguessableToken& render_proxy_token);

  int GetDocumentCookie() const;
  const ContentToProxyTokenMap& GetSubframeContentInfo() const;

  void UtilizeTypefaceContext(ContentProxySet* typeface_content_info);

  const ui::AXTreeUpdate& accessibility_tree() const {
    return accessibility_tree_;
  }
  ui::AXTreeUpdate& accessibility_tree() { return accessibility_tree_; }

  void set_generate_document_outline(
      mojom::GenerateDocumentOutline generate_document_outline) {
    generate_document_outline_ = generate_document_outline;
  }

  void set_title(std::string title) { title_ = std::move(title); }

 private:
  FRIEND_TEST_ALL_PREFIXES(MetafileSkiaTest, FrameContent);
  FRIEND_TEST_ALL_PREFIXES(MetafileSkiaTest, GetPageBounds);
  FRIEND_TEST_ALL_PREFIXES(MetafileSkiaTest, MultiPictureDocumentTypefaces);

  void AppendPage(const SkSize& page_size, cc::PaintRecord record);
  void AppendSubframeInfo(uint32_t content_id,
                          const base::UnguessableToken& proxy_token,
                          sk_sp<SkPicture> subframe_pic_holder);

  // This is used for tests only.
  SkStreamAsset* GetPdfData() const;

  // Callback function used during page content drawing to replace a custom
  // data holder with corresponding place holder SkPicture.
  void CustomDataToSkPictureCallback(SkCanvas* canvas, uint32_t content_id);

  std::unique_ptr<MetafileSkiaData> data_;

  ui::AXTreeUpdate accessibility_tree_;
  mojom::GenerateDocumentOutline generate_document_outline_ =
      mojom::GenerateDocumentOutline::kNone;
  std::string title_;
};

}  // namespace printing

#endif  // PRINTING_METAFILE_SKIA_H_
