// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_PDFIUM_ENGINE_H_
#define PDF_TEST_TEST_PDFIUM_ENGINE_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_metadata.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

class TestPDFiumEngine : public PDFiumEngine {
 public:
  // Page number.
  static constexpr uint32_t kPageNumber = 13u;

  // Dummy loaded data.
  static constexpr uint8_t kLoadedData[] = {'l', 'o', 'a', 'd', 'e', 'd'};

  // Dummy save data.
  static constexpr uint8_t kSaveData[] = {'s', 'a', 'v', 'e'};

  explicit TestPDFiumEngine(PDFiumEngineClient* client);

  TestPDFiumEngine(const TestPDFiumEngine&) = delete;

  TestPDFiumEngine& operator=(const TestPDFiumEngine&) = delete;

  ~TestPDFiumEngine() override;

  MOCK_METHOD(void, PageOffsetUpdated, (const gfx::Vector2d&), (override));

  MOCK_METHOD(void, PluginSizeUpdated, (const gfx::Size&), (override));

  MOCK_METHOD(void, ScrolledToXPosition, (int), (override));
  MOCK_METHOD(void, ScrolledToYPosition, (int), (override));

  MOCK_METHOD(void,
              Paint,
              (const gfx::Rect&,
               SkBitmap&,
               std::vector<gfx::Rect>&,
               std::vector<gfx::Rect>&),
              (override));

  MOCK_METHOD(bool,
              HandleInputEvent,
              (const blink::WebInputEvent&),
              (override));

  MOCK_METHOD(std::vector<uint8_t>,
              PrintPages,
              (base::span<const int>, const blink::WebPrintParams&),
              (override));

  MOCK_METHOD(void, ZoomUpdated, (double), (override));

  MOCK_METHOD(gfx::Size,
              ApplyDocumentLayout,
              (const DocumentLayout::Options&),
              (override));

  MOCK_METHOD(bool, CanEditText, (), (const override));

  MOCK_METHOD(bool, HasPermission, (DocumentPermission), (const override));

  MOCK_METHOD(void, SelectAll, (), (override));

  const std::vector<DocumentAttachmentInfo>& GetDocumentAttachmentInfoList()
      const override;

  const DocumentMetadata& GetDocumentMetadata() const override;

  int GetNumberOfPages() const override;

  MOCK_METHOD(bool, IsPageVisible, (int), (const override));

  MOCK_METHOD(gfx::Rect, GetPageContentsRect, (int), (override));

  MOCK_METHOD(gfx::Rect, GetPageScreenRect, (int), (const override));

  MOCK_METHOD(std::optional<gfx::SizeF>,
              GetPageSizeInPoints,
              (int),
              (const override));

  // Returns an empty bookmark list.
  base::Value::List GetBookmarks() override;

  MOCK_METHOD(void, SetGrayscale, (bool), (override));

  MOCK_METHOD(bool, IsPDFDocTagged, (), (const override));

  MOCK_METHOD(uint32_t, GetLoadedByteSize, (), (override));

  MOCK_METHOD(bool,
              ReadLoadedBytes,
              (uint32_t, base::span<uint8_t>),
              (override));

  MOCK_METHOD(void,
              RequestThumbnail,
              (int, float, SendThumbnailCallback),
              (override));

#if BUILDFLAG(ENABLE_PDF_INK2)
  MOCK_METHOD(gfx::Size, GetThumbnailSize, (int, float), (override));

  MOCK_METHOD(void,
              ApplyStroke,
              (int, InkStrokeId, const ink::Stroke&),
              (override));

  MOCK_METHOD(void, UpdateStrokeActive, (int, InkStrokeId, bool), (override));

  MOCK_METHOD(void, DiscardStroke, (int, InkStrokeId), (override));

  MOCK_METHOD(PDFLoadedWithV2InkAnnotations,
              ContainsV2InkPath,
              (base::TimeDelta),
              (const override));

  MOCK_METHOD((std::map<InkModeledShapeId, ink::PartitionedMesh>),
              LoadV2InkPathsForPage,
              (int),
              (override));

  MOCK_METHOD(void,
              UpdateShapeActive,
              (int, InkModeledShapeId, bool),
              (override));

  MOCK_METHOD(bool, ExtendSelectionByPoint, (const gfx::PointF&), (override));

  MOCK_METHOD(gfx::Transform, GetCanonicalToPdfTransform, (int), (override));

  MOCK_METHOD((std::map<int, std::vector<PdfRect>>),
              GetSelectionRectMap,
              (),
              (override));

  MOCK_METHOD(bool,
              IsSelectableTextOrLinkArea,
              (const gfx::PointF&),
              (override));

  MOCK_METHOD(void,
              OnTextOrLinkAreaClick,
              (const gfx::PointF&, int),
              (override));
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

  MOCK_METHOD(std::vector<uint8_t>, GetSaveData, (), (override));

  MOCK_METHOD(void, SetCaretPosition, (const gfx::Point&), (override));

  MOCK_METHOD(void, OnDocumentCanceled, (), (override));

  MOCK_METHOD(void, SetFormHighlight, (bool), (override));

  MOCK_METHOD(bool,
              FindAndHighlightTextFragments,
              (base::span<const std::string>),
              (override));

  MOCK_METHOD(void, ScrollToFirstTextFragment, (bool), (override));

  MOCK_METHOD(void, RemoveTextFragments, (), (override));

  MOCK_METHOD(void, ClearTextSelection, (), (override));

  MOCK_METHOD(void, SetCaretBrowsingEnabled, (bool), (override));

  MOCK_METHOD(void, SetCaretBlinkInterval, (base::TimeDelta), (override));

 protected:
  std::vector<DocumentAttachmentInfo>& doc_attachment_info_list() {
    return doc_attachment_info_list_;
  }

  DocumentMetadata& metadata() { return metadata_; }

 private:
  std::vector<DocumentAttachmentInfo> doc_attachment_info_list_;

  DocumentMetadata metadata_;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_PDFIUM_ENGINE_H_
