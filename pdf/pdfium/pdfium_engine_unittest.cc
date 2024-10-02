// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(UNSAFE_BUFFERS_BUILD)
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "pdf/pdfium/pdfium_engine.h"

#include <stdint.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/hash/md5.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_move_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/mouse_event_builder.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"
#include "pdf/test/test_helpers.h"
#include "pdf/ui/thumbnail.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

MATCHER_P2(LayoutWithSize, width, height, "") {
  return arg.size() == gfx::Size(width, height);
}

MATCHER_P(LayoutWithOptions, options, "") {
  return arg.options() == options;
}

blink::WebMouseEvent CreateLeftClickWebMouseEventAtPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder().CreateLeftClickAtPosition(position).Build();
}

blink::WebMouseEvent CreateLeftClickWebMouseUpEventAtPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder().CreateLeftMouseUpAtPosition(position).Build();
}

blink::WebMouseEvent CreateRightClickWebMouseEventAtPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder()
      .SetType(blink::WebInputEvent::Type::kMouseDown)
      .SetPosition(position)
      .SetButton(blink::WebPointerProperties::Button::kRight)
      .SetClickCount(1)
      .Build();
}

blink::WebMouseEvent CreateMoveWebMouseEventToPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder()
      .SetType(blink::WebInputEvent::Type::kMouseMove)
      .SetPosition(position)
      .Build();
}

base::FilePath GetTextSelectionReferenceFilePath(
    std::string_view test_filename) {
  return base::FilePath(FILE_PATH_LITERAL("text_selection"))
      .AppendASCII(test_filename);
}

class MockTestClient : public TestClient {
 public:
  MockTestClient() {
    ON_CALL(*this, ProposeDocumentLayout)
        .WillByDefault([this](const DocumentLayout& layout) {
          TestClient::ProposeDocumentLayout(layout);
        });
  }

  MOCK_METHOD(void, ProposeDocumentLayout, (const DocumentLayout&), (override));
  MOCK_METHOD(void, ScrollToPage, (int), (override));
  MOCK_METHOD(void,
              NavigateTo,
              (const std::string&, WindowOpenDisposition),
              (override));
  MOCK_METHOD(void,
              FormFieldFocusChange,
              (PDFiumEngineClient::FocusFieldType),
              (override));
  MOCK_METHOD(bool, IsPrintPreview, (), (const override));
  MOCK_METHOD(void, DocumentFocusChanged, (bool), (override));
  MOCK_METHOD(void, SetLinkUnderCursor, (const std::string&), (override));
#if BUILDFLAG(ENABLE_PDF_INK2)
  MOCK_METHOD(bool, IsInAnnotationMode, (), (const override));
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
};

}  // namespace

class PDFiumEngineTest : public PDFiumTestBase {
 protected:
  void ExpectPageRect(const PDFiumEngine& engine,
                      size_t page_index,
                      const gfx::Rect& expected_rect) {
    const PDFiumPage& page = GetPDFiumPageForTest(engine, page_index);
    EXPECT_EQ(expected_rect, page.rect());
  }

  // Tries to load a PDF incrementally, returning `true` if the PDF actually was
  // loaded incrementally. Note that this function will return `false` if
  // incremental loading fails, but also if incremental loading is disabled.
  bool TryLoadIncrementally() {
    NiceMock<MockTestClient> client;
    InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
        &client, FILE_PATH_LITERAL("linearized.pdf"));
    if (!initialize_result.engine) {
      ADD_FAILURE();
      return false;
    }
    PDFiumEngine& engine = *initialize_result.engine;

    // Load enough for the document to become partially available.
    initialize_result.document_loader->SimulateLoadData(8192);

    bool loaded_incrementally;
    if (engine.GetNumberOfPages() == 0) {
      // This is not necessarily a test failure; it just indicates incremental
      // loading is not occurring.
      engine.PluginSizeUpdated({});
      loaded_incrementally = false;
    } else {
      // Note: Plugin size chosen so all pages of the document are visible. The
      // engine only updates availability incrementally for visible pages.
      EXPECT_EQ(0, CountAvailablePages(engine));
      engine.PluginSizeUpdated({1024, 4096});
      int available_pages = CountAvailablePages(engine);
      loaded_incrementally =
          0 < available_pages && available_pages < engine.GetNumberOfPages();
    }

    // Verify that loading can finish.
    initialize_result.FinishLoading();
    EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));

    return loaded_incrementally;
  }

  void FinishWithPluginSizeUpdated(PDFiumEngine& engine) {
    engine.PluginSizeUpdated({});

    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Counts the number of available pages. Returns `int` instead of `size_t` for
  // consistency with `PDFiumEngine::GetNumberOfPages()`.
  int CountAvailablePages(const PDFiumEngine& engine) {
    int available_pages = 0;
    for (int i = 0; i < engine.GetNumberOfPages(); ++i) {
      if (GetPDFiumPageForTest(engine, i).available())
        ++available_pages;
    }
    return available_pages;
  }

  void SetSelection(PDFiumEngine& engine,
                    uint32_t start_page_index,
                    uint32_t start_char_index,
                    uint32_t end_page_index,
                    uint32_t end_char_index) {
    engine.SetSelection({start_page_index, start_char_index},
                        {end_page_index, end_char_index});
  }

  void DrawSelectionAndCompare(PDFiumEngine& engine,
                               int page_index,
                               std::string_view expected_png_filename) {
    return DrawSelectionAndCompareImpl(engine, page_index,
                                       expected_png_filename,
                                       /*use_platform_suffix=*/false);
  }

  void DrawSelectionAndCompareWithPlatformExpectations(
      PDFiumEngine& engine,
      int page_index,
      std::string_view expected_png_filename) {
    return DrawSelectionAndCompareImpl(engine, page_index,
                                       expected_png_filename,
                                       /*use_platform_suffix=*/true);
  }

 private:
  void DrawSelectionAndCompareImpl(PDFiumEngine& engine,
                                   int page_index,
                                   std::string_view expected_png_filename,
                                   bool use_platform_suffix) {
    // Since the GetPageContentsRect() return value may have a non-zero origin,
    // create a rect based solely on its size to draw the selections relative to
    // the origin of the contents rect.
    const auto rect = gfx::Rect(engine.GetPageContentsRect(page_index).size());
    ASSERT_TRUE(!rect.IsEmpty());

    SkBitmap bitmap;
    bitmap.allocPixels(
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(rect.size())));
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorWHITE);

    const size_t progressive_index = engine.StartPaint(page_index, rect);
    CHECK_EQ(0u, progressive_index);
    engine.DrawSelections(progressive_index, bitmap);
    // Effectively the same as how PDFiumEngine::FinishPaint() cleans up
    // `progressive_paints_`.
    engine.progressive_paints_.clear();

    base::FilePath expectation_path =
        GetTextSelectionReferenceFilePath(expected_png_filename);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    // Note that the expectation files without a suffix is typically generated
    // on Linux, so there is no code here to add a suffix for Linux.
    if (use_platform_suffix) {
#if BUILDFLAG(IS_WIN)
      constexpr std::wstring_view kSuffix = L"_win";
#else
      constexpr std::string_view kSuffix = "_mac";
#endif  // BUILDFLAG(IS_WIN)
      expectation_path = expectation_path.InsertBeforeExtension(kSuffix);
    }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

    EXPECT_TRUE(MatchesPngFile(bitmap.asImage().get(), expectation_path));
  }
};

TEST_P(PDFiumEngineTest, InitializeWithRectanglesMultiPagesPdf) {
  NiceMock<MockTestClient> client;

  // ProposeDocumentLayout() gets called twice during loading because
  // PDFiumEngine::ContinueLoadingDocument() calls LoadBody() (which eventually
  // triggers a layout proposal), and then calls FinishLoadingDocument() (since
  // the document is complete), which calls LoadBody() again. Coalescing these
  // proposals is not correct unless we address the issue covered by
  // PDFiumEngineTest.ProposeDocumentLayoutWithOverlap.
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
      .Times(2);

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(5, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {38, 3, 266, 333});
  ExpectPageRect(*engine, 1, {5, 350, 333, 266});
  ExpectPageRect(*engine, 2, {38, 630, 266, 333});
  ExpectPageRect(*engine, 3, {38, 977, 266, 333});
  ExpectPageRect(*engine, 4, {38, 1324, 266, 333});
}

TEST_P(PDFiumEngineTest, InitializeWithRectanglesMultiPagesPdfInTwoUpView) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  DocumentLayout::Options options;
  options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithOptions(options)))
      .WillOnce(Return());
  engine->SetDocumentLayout(DocumentLayout::PageSpread::kTwoUpOdd);

  engine->ApplyDocumentLayout(options);

  ASSERT_EQ(5, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {72, 3, 266, 333});
  ExpectPageRect(*engine, 1, {340, 3, 333, 266});
  ExpectPageRect(*engine, 2, {72, 346, 266, 333});
  ExpectPageRect(*engine, 3, {340, 346, 266, 333});
  ExpectPageRect(*engine, 4, {68, 689, 266, 333});
}

TEST_P(PDFiumEngineTest, AppendBlankPagesWithFewerPages) {
  NiceMock<MockTestClient> client;
  {
    InSequence normal_then_append;
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
        .Times(2);
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 1037)));
  }

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  engine->AppendBlankPages(3);
  ASSERT_EQ(3, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {5, 3, 266, 333});
  ExpectPageRect(*engine, 1, {5, 350, 266, 333});
  ExpectPageRect(*engine, 2, {5, 697, 266, 333});
}

TEST_P(PDFiumEngineTest, AppendBlankPagesWithMorePages) {
  NiceMock<MockTestClient> client;
  {
    InSequence normal_then_append;
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
        .Times(2);
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 2425)));
  }

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  engine->AppendBlankPages(7);
  ASSERT_EQ(7, engine->GetNumberOfPages());

  ExpectPageRect(*engine, 0, {5, 3, 266, 333});
  ExpectPageRect(*engine, 1, {5, 350, 266, 333});
  ExpectPageRect(*engine, 2, {5, 697, 266, 333});
  ExpectPageRect(*engine, 3, {5, 1044, 266, 333});
  ExpectPageRect(*engine, 4, {5, 1391, 266, 333});
  ExpectPageRect(*engine, 5, {5, 1738, 266, 333});
  ExpectPageRect(*engine, 6, {5, 2085, 266, 333});
}

TEST_P(PDFiumEngineTest, ProposeDocumentLayoutWithOverlap) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1463)))
      .WillOnce(Return());
  engine->RotateClockwise();

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
      .WillOnce(Return());
  engine->RotateCounterclockwise();
}

TEST_P(PDFiumEngineTest, ApplyDocumentLayoutBeforePluginSizeUpdated) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  initialize_result.FinishLoading();
  PDFiumEngine& engine = *initialize_result.engine;

  DocumentLayout::Options options;
  options.RotatePagesClockwise();
  EXPECT_CALL(client, ScrollToPage(-1)).Times(0);
  EXPECT_EQ(gfx::Size(343, 1664), engine.ApplyDocumentLayout(options));

  EXPECT_CALL(client, ScrollToPage(-1)).Times(1);
  FinishWithPluginSizeUpdated(engine);
}

TEST_P(PDFiumEngineTest, ApplyDocumentLayoutAvoidsInfiniteLoop) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  DocumentLayout::Options options;
  EXPECT_CALL(client, ScrollToPage(-1)).Times(0);
  EXPECT_EQ(gfx::Size(343, 1664), engine->ApplyDocumentLayout(options));

  options.RotatePagesClockwise();
  EXPECT_CALL(client, ScrollToPage(-1)).Times(1);
  EXPECT_EQ(gfx::Size(343, 1463), engine->ApplyDocumentLayout(options));
  EXPECT_EQ(gfx::Size(343, 1463), engine->ApplyDocumentLayout(options));
}

TEST_P(PDFiumEngineTest, GetDocumentAttachments) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("embedded_attachments.pdf"));
  ASSERT_TRUE(engine);

  const std::vector<DocumentAttachmentInfo>& attachments =
      engine->GetDocumentAttachmentInfoList();
  ASSERT_EQ(3u, attachments.size());

  {
    const DocumentAttachmentInfo& attachment = attachments[0];
    EXPECT_EQ("1.txt", base::UTF16ToUTF8(attachment.name));
    EXPECT_TRUE(attachment.is_readable);
    EXPECT_EQ(4u, attachment.size_bytes);
    EXPECT_EQ("D:20170712214438-07'00'",
              base::UTF16ToUTF8(attachment.creation_date));
    EXPECT_EQ("D:20160115091400", base::UTF16ToUTF8(attachment.modified_date));

    std::vector<uint8_t> content = engine->GetAttachmentData(0);
    ASSERT_EQ(attachment.size_bytes, content.size());
    std::string content_str(content.begin(), content.end());
    EXPECT_EQ("test", content_str);
  }

  {
    static constexpr char kCheckSum[] = "72afcddedf554dda63c0c88e06f1ce18";
    const DocumentAttachmentInfo& attachment = attachments[1];
    EXPECT_EQ("attached.pdf", base::UTF16ToUTF8(attachment.name));
    EXPECT_TRUE(attachment.is_readable);
    EXPECT_EQ(5869u, attachment.size_bytes);
    EXPECT_EQ("D:20170712214443-07'00'",
              base::UTF16ToUTF8(attachment.creation_date));
    EXPECT_EQ("D:20170712214410", base::UTF16ToUTF8(attachment.modified_date));

    std::vector<uint8_t> content = engine->GetAttachmentData(1);
    ASSERT_EQ(attachment.size_bytes, content.size());
    // The whole attachment content is too long to do string comparison.
    // Instead, we only verify the checksum value here.
    base::MD5Digest hash;
    base::MD5Sum(content, &hash);
    EXPECT_EQ(kCheckSum, base::MD5DigestToBase16(hash));
  }

  {
    // Test attachments with no creation date or last modified date.
    const DocumentAttachmentInfo& attachment = attachments[2];
    EXPECT_EQ("附錄.txt", base::UTF16ToUTF8(attachment.name));
    EXPECT_TRUE(attachment.is_readable);
    EXPECT_EQ(5u, attachment.size_bytes);
    EXPECT_THAT(attachment.creation_date, IsEmpty());
    EXPECT_THAT(attachment.modified_date, IsEmpty());

    std::vector<uint8_t> content = engine->GetAttachmentData(2);
    ASSERT_EQ(attachment.size_bytes, content.size());
    std::string content_str(content.begin(), content.end());
    EXPECT_EQ("test\n", content_str);
  }
}

TEST_P(PDFiumEngineTest, GetInvalidDocumentAttachment) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("invalid_attachment.pdf"));
  ASSERT_TRUE(engine);

  // Test on a document with one invalid attachment, which can make
  // FPDFDoc_GetAttachment() fail. This particular attachment is invalid due
  // to its key value violating the `Limits` entry.
  const std::vector<DocumentAttachmentInfo>& attachments =
      engine->GetDocumentAttachmentInfoList();
  ASSERT_EQ(1u, attachments.size());

  const DocumentAttachmentInfo& attachment = attachments[0];
  EXPECT_THAT(attachment.name, IsEmpty());
  EXPECT_FALSE(attachment.is_readable);
  EXPECT_EQ(0u, attachment.size_bytes);
  EXPECT_THAT(attachment.creation_date, IsEmpty());
  EXPECT_THAT(attachment.modified_date, IsEmpty());
}

TEST_P(PDFiumEngineTest, GetDocumentAttachmentWithInvalidData) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("embedded_attachments_invalid_data.pdf"));
  ASSERT_TRUE(engine);

  const std::vector<DocumentAttachmentInfo>& attachments =
      engine->GetDocumentAttachmentInfoList();
  ASSERT_EQ(1u, attachments.size());

  // Test on an attachment which FPDFAttachment_GetFile() fails to retrieve data
  // from.
  const DocumentAttachmentInfo& attachment = attachments[0];
  EXPECT_EQ("1.txt", base::UTF16ToUTF8(attachment.name));
  EXPECT_FALSE(attachment.is_readable);
  EXPECT_EQ(0u, attachment.size_bytes);
  EXPECT_THAT(attachment.creation_date, IsEmpty());
  EXPECT_THAT(attachment.modified_date, IsEmpty());
}

TEST_P(PDFiumEngineTest, NoDocumentAttachmentInfo) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_EQ(0u, engine->GetDocumentAttachmentInfoList().size());
}

TEST_P(PDFiumEngineTest, GetDocumentMetadata) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("document_info.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();

  EXPECT_EQ(PdfVersion::k1_7, doc_metadata.version);
  EXPECT_EQ(714u, doc_metadata.size_bytes);
  EXPECT_FALSE(doc_metadata.linearized);
  EXPECT_EQ("Sample PDF Document Info", doc_metadata.title);
  EXPECT_EQ("Chromium Authors", doc_metadata.author);
  EXPECT_EQ("Testing", doc_metadata.subject);
  EXPECT_EQ("testing,chromium,pdfium,document,info", doc_metadata.keywords);
  EXPECT_EQ("Your Preferred Text Editor", doc_metadata.creator);
  EXPECT_EQ("fixup_pdf_template.py", doc_metadata.producer);

  base::Time expected_creation_date;
  ASSERT_TRUE(base::Time::FromUTCString("2020-02-05 15:39:12",
                                        &expected_creation_date));
  EXPECT_EQ(expected_creation_date, doc_metadata.creation_date);

  base::Time expected_mod_date;
  ASSERT_TRUE(
      base::Time::FromUTCString("2020-02-06 09:42:34", &expected_mod_date));
  EXPECT_EQ(expected_mod_date, doc_metadata.mod_date);
}

TEST_P(PDFiumEngineTest, GetEmptyDocumentMetadata) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();

  EXPECT_EQ(PdfVersion::k1_7, doc_metadata.version);
  EXPECT_EQ(786u, doc_metadata.size_bytes);
  EXPECT_FALSE(doc_metadata.linearized);
  EXPECT_THAT(doc_metadata.title, IsEmpty());
  EXPECT_THAT(doc_metadata.author, IsEmpty());
  EXPECT_THAT(doc_metadata.subject, IsEmpty());
  EXPECT_THAT(doc_metadata.keywords, IsEmpty());
  EXPECT_THAT(doc_metadata.creator, IsEmpty());
  EXPECT_THAT(doc_metadata.producer, IsEmpty());
  EXPECT_TRUE(doc_metadata.creation_date.is_null());
  EXPECT_TRUE(doc_metadata.mod_date.is_null());
}

TEST_P(PDFiumEngineTest, GetLinearizedDocumentMetadata) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_TRUE(engine->GetDocumentMetadata().linearized);
}

TEST_P(PDFiumEngineTest, GetBadPdfVersion) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bad_version.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();
  EXPECT_EQ(PdfVersion::kUnknown, doc_metadata.version);
}

TEST_P(PDFiumEngineTest, GetNamedDestination) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("named_destinations.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());

  // A destination with a valid page object
  std::optional<PDFiumEngine::NamedDestination> valid_page_obj =
      engine->GetNamedDestination("ValidPageObj");
  ASSERT_TRUE(valid_page_obj.has_value());
  EXPECT_EQ(0u, valid_page_obj->page);
  EXPECT_EQ("XYZ", valid_page_obj->view);
  ASSERT_EQ(3u, valid_page_obj->num_params);
  EXPECT_EQ(1.2f, valid_page_obj->params[2]);

  // A destination with an invalid page object
  std::optional<PDFiumEngine::NamedDestination> invalid_page_obj =
      engine->GetNamedDestination("InvalidPageObj");
  ASSERT_FALSE(invalid_page_obj.has_value());

  // A destination with a valid page number
  std::optional<PDFiumEngine::NamedDestination> valid_page_number =
      engine->GetNamedDestination("ValidPageNumber");
  ASSERT_TRUE(valid_page_number.has_value());
  EXPECT_EQ(1u, valid_page_number->page);

  // A destination with an out-of-range page number
  std::optional<PDFiumEngine::NamedDestination> invalid_page_number =
      engine->GetNamedDestination("OutOfRangePageNumber");
  EXPECT_FALSE(invalid_page_number.has_value());
}

TEST_P(PDFiumEngineTest, PluginSizeUpdatedBeforeLoad) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  engine.PluginSizeUpdated({});
  initialize_result.FinishLoading();

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_P(PDFiumEngineTest, PluginSizeUpdatedDuringLoad) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  EXPECT_TRUE(initialize_result.document_loader->SimulateLoadData(1024));
  engine.PluginSizeUpdated({});
  initialize_result.FinishLoading();

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_P(PDFiumEngineTest, PluginSizeUpdatedAfterLoad) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  initialize_result.FinishLoading();
  FinishWithPluginSizeUpdated(engine);

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_P(PDFiumEngineTest, OnLeftMouseDownBeforePluginSizeUpdated) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  initialize_result.FinishLoading();
  PDFiumEngine& engine = *initialize_result.engine;

  EXPECT_TRUE(engine.HandleInputEvent(blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseDown, {0, 0}, {100, 200},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::Modifiers::kLeftButtonDown,
      blink::WebInputEvent::GetStaticTimeStampForTests())));
}

TEST_P(PDFiumEngineTest, OnLeftMouseDownAfterPluginSizeUpdated) {
  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  initialize_result.FinishLoading();
  PDFiumEngine& engine = *initialize_result.engine;

  engine.PluginSizeUpdated({300, 400});
  EXPECT_TRUE(engine.HandleInputEvent(blink::WebMouseEvent(
      blink::WebInputEvent::Type::kMouseDown, {0, 0}, {100, 200},
      blink::WebPointerProperties::Button::kLeft, /*click_count_param=*/1,
      blink::WebInputEvent::Modifiers::kLeftButtonDown,
      blink::WebInputEvent::GetStaticTimeStampForTests())));
}

TEST_P(PDFiumEngineTest, IncrementalLoadingFeatureDefault) {
  EXPECT_FALSE(TryLoadIncrementally());
}

TEST_P(PDFiumEngineTest, IncrementalLoadingFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);
  EXPECT_TRUE(TryLoadIncrementally());
}

TEST_P(PDFiumEngineTest, IncrementalLoadingFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPdfIncrementalLoading);
  EXPECT_FALSE(TryLoadIncrementally());
}

TEST_P(PDFiumEngineTest, RequestThumbnail) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  const int num_pages = engine->GetNumberOfPages();
  ASSERT_EQ(5, num_pages);
  ASSERT_EQ(num_pages, CountAvailablePages(*engine));

  // Each page should immediately return a thumbnail.
  for (int i = 0; i < num_pages; ++i) {
    base::MockCallback<SendThumbnailCallback> send_callback;
    EXPECT_CALL(send_callback, Run);
    engine->RequestThumbnail(/*page_index=*/i, /*device_pixel_ratio=*/1,
                             send_callback.Get());
  }
}

TEST_P(PDFiumEngineTest, RequestThumbnailLinearized) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);

  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  // Load only some pages.
  initialize_result.document_loader->SimulateLoadData(8192);

  // Note: Plugin size chosen so all pages of the document are visible. The
  // engine only updates availability incrementally for visible pages.
  engine.PluginSizeUpdated({1024, 4096});

  const int num_pages = engine.GetNumberOfPages();
  ASSERT_EQ(3, num_pages);
  const int available_pages = CountAvailablePages(engine);
  ASSERT_LT(0, available_pages);
  ASSERT_GT(num_pages, available_pages);

  // Initialize callbacks for first and last pages.
  base::MockCallback<SendThumbnailCallback> first_loaded;
  base::MockCallback<SendThumbnailCallback> last_loaded;

  // When the document is partially loaded, `SendThumbnailCallback` is only run
  // for the loaded page even though `RequestThumbnail()` gets called for both
  // pages.
  EXPECT_CALL(first_loaded, Run);
  engine.RequestThumbnail(/*page_index=*/0, /*device_pixel_ratio=*/1,
                          first_loaded.Get());
  engine.RequestThumbnail(/*page_index=*/num_pages - 1,
                          /*device_pixel_ratio=*/1, last_loaded.Get());

  // Finish loading the document. `SendThumbnailCallback` should be run for the
  // last page.
  EXPECT_CALL(last_loaded, Run);
  initialize_result.FinishLoading();
}

TEST_P(PDFiumEngineTest, HandleInputEventKeyDown) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_CALL(client, DocumentFocusChanged(true));

  blink::WebKeyboardEvent key_down_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_down_event.windows_key_code = ui::VKEY_TAB;
  EXPECT_TRUE(engine->HandleInputEvent(key_down_event));
}

TEST_P(PDFiumEngineTest, HandleInputEventRawKeyDown) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_CALL(client, DocumentFocusChanged(true));

  blink::WebKeyboardEvent raw_key_down_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  raw_key_down_event.windows_key_code = ui::VKEY_TAB;
  EXPECT_TRUE(engine->HandleInputEvent(raw_key_down_event));
}

namespace {
#if BUILDFLAG(IS_WIN)
constexpr char kSelectTextExpectedText[] =
    "Hello, world!\r\nGoodbye, world!\r\nHello, world!\r\nGoodbye, world!";
#else
constexpr char kSelectTextExpectedText[] =
    "Hello, world!\nGoodbye, world!\nHello, world!\nGoodbye, world!";
#endif
}  // namespace

TEST_P(PDFiumEngineTest, SelectText) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_TRUE(engine->HasPermission(DocumentPermission::kCopy));

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  engine->SelectAll();
  EXPECT_EQ(kSelectTextExpectedText, engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SelectTextBackwards) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kSecondPageBeginPosition(100, 420);
  constexpr gfx::PointF kFirstPageEndPosition(100, 120);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kSecondPageBeginPosition)));
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kFirstPageEndPosition)));

#if BUILDFLAG(IS_WIN)
  constexpr char kExpectedText[] = "bye, world!\r\nHello, world!\r\nGoodby";
#else
  constexpr char kExpectedText[] = "bye, world!\nHello, world!\nGoodby";
#endif
  EXPECT_EQ(kExpectedText, engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SelectTextWithCopyRestriction) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("hello_world2_with_copy_restriction.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_FALSE(engine->HasPermission(DocumentPermission::kCopy));

  // The copy restriction should not affect the text selection hehavior.
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  engine->SelectAll();
  EXPECT_EQ(kSelectTextExpectedText, engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SelectCroppedText) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  engine->SelectAll();
#if BUILDFLAG(IS_WIN)
  constexpr char kExpectedText[] = "world!\r\n";
#else
  constexpr char kExpectedText[] = "world!\n";
#endif
  EXPECT_EQ(kExpectedText, engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SelectTextWithDoubleClick) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kPosition(100, 120);
  EXPECT_TRUE(engine->HandleInputEvent(MouseEventBuilder()
                                           .CreateLeftClickAtPosition(kPosition)
                                           .SetClickCount(2)
                                           .Build()));
  EXPECT_EQ("Goodbye", engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SelectTextWithTripleClick) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kPosition(100, 120);
  EXPECT_TRUE(engine->HandleInputEvent(MouseEventBuilder()
                                           .CreateLeftClickAtPosition(kPosition)
                                           .SetClickCount(3)
                                           .Build()));
  EXPECT_EQ("Goodbye, world!", engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SelectTextWithMouse) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kStartPosition(50, 110);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kStartPosition)));

  constexpr gfx::PointF kEndPosition(100, 110);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kEndPosition)));

  EXPECT_EQ("Goodb", engine->GetSelectedText());
}

#if BUILDFLAG(IS_MAC)
TEST_P(PDFiumEngineTest, CtrlLeftClickShouldNotSelectTextOnMac) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  // In https://crbug.com/339681892, these are the events PDFiumEngine sees.
  constexpr gfx::PointF kStartPosition(50, 110);
  MouseEventBuilder builder;
  builder.CreateLeftClickAtPosition(kStartPosition)
      .SetModifiers(blink::WebInputEvent::Modifiers::kControlKey);
  EXPECT_FALSE(engine->HandleInputEvent(builder.Build()));

  constexpr gfx::PointF kEndPosition(100, 110);
  EXPECT_FALSE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kEndPosition)));

  EXPECT_EQ("", engine->GetSelectedText());
}
#else
TEST_P(PDFiumEngineTest, CtrlLeftClickSelectTextOnNonMac) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kStartPosition(50, 110);
  MouseEventBuilder builder;
  builder.CreateLeftClickAtPosition(kStartPosition)
      .SetModifiers(blink::WebInputEvent::Modifiers::kControlKey);
  EXPECT_TRUE(engine->HandleInputEvent(builder.Build()));

  constexpr gfx::PointF kEndPosition(100, 110);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kEndPosition)));

  EXPECT_EQ("Goodb", engine->GetSelectedText());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_P(PDFiumEngineTest, SelectLinkAreaWithNoText) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kStartPosition(90, 120);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kStartPosition)));

  constexpr gfx::PointF kMiddlePosition(100, 230);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kMiddlePosition)));

#if BUILDFLAG(IS_WIN)
  constexpr char kExpectedText[] = "Link Annotations - Page 1\r\nL";
#else
  constexpr char kExpectedText[] = "Link Annotations - Page 1\nL";
#endif
  EXPECT_EQ(kExpectedText, engine->GetSelectedText());

  constexpr gfx::PointF kEndPosition(430, 230);
  EXPECT_FALSE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kEndPosition)));

  // This is still `kExpectedText` because of the unit test's uncanny ability to
  // move the mouse to `kEndPosition` in one move.
  EXPECT_EQ(kExpectedText, engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, DrawTextSelectionsHelloWorld) {
  constexpr int kPageIndex = 0;
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
  DrawSelectionAndCompare(*engine, kPageIndex, "hello_world_blank.png");

  SetSelection(*engine, /*start_page_index=*/kPageIndex, /*start_char_index=*/1,
               /*end_page_index=*/kPageIndex, /*end_char_index=*/2);
  EXPECT_EQ("e", engine->GetSelectedText());
  DrawSelectionAndCompare(*engine, kPageIndex, "hello_world_selection_1.png");

  SetSelection(*engine, /*start_page_index=*/kPageIndex, /*start_char_index=*/0,
               /*end_page_index=*/kPageIndex, /*end_char_index=*/3);
  EXPECT_EQ("Hel", engine->GetSelectedText());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_selection_2.png");

  SetSelection(*engine, /*start_page_index=*/kPageIndex, /*start_char_index=*/0,
               /*end_page_index=*/kPageIndex, /*end_char_index=*/6);
  EXPECT_EQ("Hello,", engine->GetSelectedText());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_selection_3.png");
}

TEST_P(PDFiumEngineTest, DrawTextSelectionsBigtableMicro) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bigtable_micro.pdf"));
  ASSERT_TRUE(engine);

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  engine->SelectAll();
  EXPECT_EQ("{fay,jeff,sanjay,wilsonh,kerr,m3b,tushar,k es,gruber}@google.com",
            engine->GetSelectedText());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "bigtable_micro_selection.png");
}

TEST_P(PDFiumEngineTest, LinkNavigates) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_CALL(client, NavigateTo("", WindowOpenDisposition::CURRENT_TAB));
  constexpr gfx::PointF kMiddlePosition(100, 230);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kMiddlePosition)));
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseUpEventAtPosition(kMiddlePosition)));
}

// Test case for crbug.com/699000
TEST_P(PDFiumEngineTest, LinkDisabledInPrintPreview) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_CALL(client, IsPrintPreview()).WillRepeatedly(Return(true));

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_CALL(client, NavigateTo(_, _)).Times(0);
  constexpr gfx::PointF kMiddlePosition(100, 230);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kMiddlePosition)));
  EXPECT_FALSE(engine->HandleInputEvent(
      CreateLeftClickWebMouseUpEventAtPosition(kMiddlePosition)));
}

TEST_P(PDFiumEngineTest, SelectTextWithNonPrintableCharacter) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_1357385.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  engine->SelectAll();
  EXPECT_EQ("Hello, world!", engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, RotateAfterSelectedText) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kPosition(100, 120);
  EXPECT_TRUE(engine->HandleInputEvent(MouseEventBuilder()
                                           .CreateLeftClickAtPosition(kPosition)
                                           .SetClickCount(2)
                                           .Build()));
  EXPECT_EQ("Goodbye", engine->GetSelectedText());

  DocumentLayout::Options options;
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 556)))
      .WillOnce(Return());
  engine->RotateClockwise();
  options.RotatePagesClockwise();
  engine->ApplyDocumentLayout(options);
  EXPECT_EQ("Goodbye", engine->GetSelectedText());

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 556)))
      .WillOnce(Return());
  engine->RotateCounterclockwise();
  options.RotatePagesCounterclockwise();
  engine->ApplyDocumentLayout(options);
  EXPECT_EQ("Goodbye", engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, MultiPagesPdfInTwoUpViewAfterSelectedText) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  constexpr gfx::PointF kPosition(100, 120);
  EXPECT_TRUE(engine->HandleInputEvent(MouseEventBuilder()
                                           .CreateLeftClickAtPosition(kPosition)
                                           .SetClickCount(2)
                                           .Build()));
  EXPECT_EQ("Goodbye", engine->GetSelectedText());

  DocumentLayout::Options options;
  options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithOptions(options)))
      .WillOnce(Return());
  engine->SetDocumentLayout(DocumentLayout::PageSpread::kTwoUpOdd);
  engine->ApplyDocumentLayout(options);
  EXPECT_EQ("Goodbye", engine->GetSelectedText());

  options.set_page_spread(DocumentLayout::PageSpread::kOneUp);
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithOptions(options)))
      .WillOnce(Return());
  engine->SetDocumentLayout(DocumentLayout::PageSpread::kOneUp);
  engine->ApplyDocumentLayout(options);
  EXPECT_EQ("Goodbye", engine->GetSelectedText());
}

TEST_P(PDFiumEngineTest, SetFormHighlight) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  // Removing form highlights should remove focus.
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->SetFormHighlight(false);
}

TEST_P(PDFiumEngineTest, ClearTextSelection) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  // Select text.
  engine->SelectAll();
  EXPECT_EQ(kSelectTextExpectedText, engine->GetSelectedText());

  // Clear selected text.
  engine->ClearTextSelection();
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineTest, testing::Bool());

using PDFiumEngineDeathTest = PDFiumEngineTest;

TEST_P(PDFiumEngineDeathTest, RequestThumbnailRedundant) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);

  NiceMock<MockTestClient> client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  // Load only some pages.
  initialize_result.document_loader->SimulateLoadData(8192);

  // Twice request a thumbnail for the second page, which is not loaded. The
  // second call should crash.
  base::MockCallback<SendThumbnailCallback> mock_callback;
  engine.RequestThumbnail(/*page_index=*/1, /*device_pixel_ratio=*/1,
                          mock_callback.Get());
  EXPECT_DCHECK_DEATH(engine.RequestThumbnail(
      /*page_index=*/1, /*device_pixel_ratio=*/1, mock_callback.Get()));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineDeathTest, testing::Bool());

class PDFiumEngineTabbingTest : public PDFiumTestBase {
 public:
  PDFiumEngineTabbingTest() = default;
  ~PDFiumEngineTabbingTest() override = default;
  PDFiumEngineTabbingTest(const PDFiumEngineTabbingTest&) = delete;
  PDFiumEngineTabbingTest& operator=(const PDFiumEngineTabbingTest&) = delete;

  bool HandleTabEvent(PDFiumEngine* engine, int modifiers) {
    return engine->HandleTabEvent(modifiers);
  }

  PDFiumEngine::FocusElementType GetFocusedElementType(PDFiumEngine* engine) {
    return engine->focus_element_type_;
  }

  int GetLastFocusedPage(PDFiumEngine* engine) {
    return engine->last_focused_page_;
  }

  PDFiumEngine::FocusElementType GetLastFocusedElementType(
      PDFiumEngine* engine) {
    return engine->last_focused_element_type_;
  }

  int GetLastFocusedAnnotationIndex(PDFiumEngine* engine) {
    return engine->last_focused_annot_index_;
  }

  PDFiumEngineClient::FocusFieldType FormFocusFieldType(PDFiumEngine* engine) {
    return engine->focus_field_type_;
  }

  size_t GetSelectionSize(PDFiumEngine* engine) {
    return engine->selection_.size();
  }

  void ScrollFocusedAnnotationIntoView(PDFiumEngine* engine) {
    engine->ScrollFocusedAnnotationIntoView();
  }
};

TEST_P(PDFiumEngineTabbingTest, LinkUnderCursor) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Widget annotation
   * ++++ Widget annotation
   * ++++ Highlight annotation
   * ++++ Link annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);

  // Tab to right before the first non-link annotation.
  EXPECT_CALL(client, DocumentFocusChanged(true));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab through non-link annotations and validate link under cursor.
  {
    InSequence sequence;
    EXPECT_CALL(client, SetLinkUnderCursor(""));
    EXPECT_CALL(client, DocumentFocusChanged(false));
    EXPECT_CALL(client, SetLinkUnderCursor("")).Times(2);
  }

  for (int i = 0; i < 3; i++)
    ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab to Link annotation.
  EXPECT_CALL(client, SetLinkUnderCursor("https://www.google.com/"));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab to previous annotation.
  EXPECT_CALL(client, SetLinkUnderCursor(""));
  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
}

// Test case for crbug.com/1088296
TEST_P(PDFiumEngineTabbingTest, LinkUnderCursorAfterTabAndRightClick) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);

  // Ensure the plugin has a pre-determined size, to enable the hit tests below.
  engine->PluginSizeUpdated({612, 792});

  // Tab to right before the first non-link annotation.
  EXPECT_CALL(client, DocumentFocusChanged(true));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab through non-link annotations and validate link under cursor.
  {
    InSequence sequence;
    EXPECT_CALL(client, SetLinkUnderCursor(""));
    EXPECT_CALL(client, DocumentFocusChanged(false));
  }

  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));
  EXPECT_CALL(client, SetLinkUnderCursor(""));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));
  EXPECT_CALL(client, SetLinkUnderCursor(""));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Tab to Link annotation.
  EXPECT_CALL(client, SetLinkUnderCursor("https://www.google.com/"));
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Right click somewhere far away should reset the link.
  constexpr gfx::PointF kOffScreenPosition(0, 0);
  EXPECT_CALL(client, SetLinkUnderCursor(""));
  EXPECT_FALSE(engine->HandleInputEvent(
      CreateRightClickWebMouseEventAtPosition(kOffScreenPosition)));

  // Right click on the link should set it again.
  constexpr gfx::PointF kLinkPosition(170, 595);
  EXPECT_CALL(client, SetLinkUnderCursor("https://www.google.com/"));
  EXPECT_FALSE(engine->HandleInputEvent(
      CreateRightClickWebMouseEventAtPosition(kLinkPosition)));
}

TEST_P(PDFiumEngineTabbingTest, TabbingSupportedAnnots) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Widget annotation
   * ++++ Widget annotation
   * ++++ Highlight annotation
   * ++++ Link annotation
   */
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(1, engine->GetNumberOfPages());

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_FALSE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, TabbingForward) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedPage(engine.get()));

  ASSERT_FALSE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, TabbingBackward) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, TabbingWithModifiers) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing with ctrl modifier.
  ASSERT_FALSE(HandleTabEvent(engine.get(),
                              blink::WebInputEvent::Modifiers::kControlKey));
  // Tabbing with alt modifier.
  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kAltKey));

  // Tab to bring document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Tabbing with ctrl modifier.
  ASSERT_FALSE(HandleTabEvent(engine.get(),
                              blink::WebInputEvent::Modifiers::kControlKey));
  // Tabbing with alt modifier.
  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kAltKey));

  // Tab to bring first page into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));

  // Tabbing with ctrl modifier.
  ASSERT_FALSE(HandleTabEvent(engine.get(),
                              blink::WebInputEvent::Modifiers::kControlKey));
  // Tabbing with alt modifier.
  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kAltKey));
}

TEST_P(PDFiumEngineTabbingTest, NoFocusableElementTabbing) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++ Page 2
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false, true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  ASSERT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing forward.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_FALSE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));

  // Tabbing backward.
  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  ASSERT_FALSE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, RestoringDocumentFocus) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false, true};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring the document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  engine->UpdateFocus(/*has_focus=*/false);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetLastFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedAnnotationIndex(engine.get()));

  engine->UpdateFocus(/*has_focus=*/true);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, RestoringAnnotFocus) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation
   * ++++ Annotation
   * ++ Page 2
   * ++++ Annotation
   */
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr bool kExpectedFocusState[] = {true, false};
  {
    InSequence sequence;
    for (auto focused : kExpectedFocusState)
      EXPECT_CALL(client, DocumentFocusChanged(focused));
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring last annotation of page 0 into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));

  engine->UpdateFocus(/*has_focus=*/false);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetLastFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedAnnotationIndex(engine.get()));

  engine->UpdateFocus(/*has_focus=*/true);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));

  // Tabbing now should bring the second page's annotation to focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedPage(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, VerifyFormFieldStatesOnTabbing) {
  /*
   * Document structure
   * Document
   * ++ Page 1
   * ++++ Annotation (Text Field)
   * ++++ Annotation (Radio Button)
   */
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  // Bring focus to the document.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(PDFiumEngineClient::FocusFieldType::kNoFocus,
            FormFocusFieldType(engine.get()));
  EXPECT_FALSE(engine->CanEditText());

  // Bring focus to the text field on the page.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(PDFiumEngineClient::FocusFieldType::kText,
            FormFocusFieldType(engine.get()));
  EXPECT_TRUE(engine->CanEditText());

  // Bring focus to the button on the page.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(PDFiumEngineClient::FocusFieldType::kNonText,
            FormFocusFieldType(engine.get()));
  EXPECT_FALSE(engine->CanEditText());
}

TEST_P(PDFiumEngineTabbingTest, ClearSelectionOnFocusInFormTextArea) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("form_text_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Select all text.
  engine->SelectAll();
  EXPECT_EQ(1u, GetSelectionSize(engine.get()));

  // Tab to bring focus to a form text area annotation.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(0u, GetSelectionSize(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, RetainSelectionOnFocusNotInFormTextArea) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("annots.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Select all text.
  engine->SelectAll();
  EXPECT_EQ(1u, GetSelectionSize(engine.get()));

  // Tab to bring focus to a non form text area annotation (Button).
  ASSERT_TRUE(
      HandleTabEvent(engine.get(), blink::WebInputEvent::Modifiers::kShiftKey));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(1u, GetSelectionSize(engine.get()));
}

class ScrollingTestClient : public TestClient {
 public:
  ScrollingTestClient() = default;
  ~ScrollingTestClient() override = default;
  ScrollingTestClient(const ScrollingTestClient&) = delete;
  ScrollingTestClient& operator=(const ScrollingTestClient&) = delete;

  // Mock PDFiumEngineClient methods.
  MOCK_METHOD(void, ScrollToX, (int), (override));
  MOCK_METHOD(void, ScrollToY, (int), (override));
};

TEST_P(PDFiumEngineTabbingTest, MaintainViewportWhenFocusIsUpdated) {
  StrictMock<ScrollingTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  engine->PluginSizeUpdated(gfx::Size(60, 40));

  {
    InSequence sequence;
    static constexpr gfx::Point kScrollValue = {510, 478};
    EXPECT_CALL(client, ScrollToY(kScrollValue.y()))
        .WillOnce(Invoke(
            [&engine]() { engine->ScrolledToYPosition(kScrollValue.y()); }));
    EXPECT_CALL(client, ScrollToX(kScrollValue.x()))
        .WillOnce(Invoke(
            [&engine]() { engine->ScrolledToXPosition(kScrollValue.x()); }));
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring the document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Tab to an annotation.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));

  // Scroll focused annotation out of viewport.
  static constexpr gfx::Point kScrollPosition = {242, 746};
  engine->ScrolledToXPosition(kScrollPosition.x());
  engine->ScrolledToYPosition(kScrollPosition.y());

  engine->UpdateFocus(/*has_focus=*/false);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetLastFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(1, GetLastFocusedAnnotationIndex(engine.get()));

  // Restore focus, we shouldn't have any calls to scroll viewport.
  engine->UpdateFocus(/*has_focus=*/true);
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(0, GetLastFocusedPage(engine.get()));
}

TEST_P(PDFiumEngineTabbingTest, ScrollFocusedAnnotationIntoView) {
  StrictMock<ScrollingTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  engine->PluginSizeUpdated(gfx::Size(60, 40));

  {
    InSequence sequence;
    static constexpr gfx::Point kScrollValues[] = {{510, 478}, {510, 478}};

    for (const auto& scroll_value : kScrollValues) {
      EXPECT_CALL(client, ScrollToY(scroll_value.y()))
          .WillOnce(Invoke([&engine, &scroll_value]() {
            engine->ScrolledToYPosition(scroll_value.y());
          }));
      EXPECT_CALL(client, ScrollToX(scroll_value.x()))
          .WillOnce(Invoke([&engine, &scroll_value]() {
            engine->ScrolledToXPosition(scroll_value.x());
          }));
    }
  }

  EXPECT_EQ(PDFiumEngine::FocusElementType::kNone,
            GetFocusedElementType(engine.get()));
  EXPECT_EQ(-1, GetLastFocusedPage(engine.get()));

  // Tabbing to bring the document into focus.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kDocument,
            GetFocusedElementType(engine.get()));

  // Tab to an annotation.
  ASSERT_TRUE(HandleTabEvent(engine.get(), 0));
  EXPECT_EQ(PDFiumEngine::FocusElementType::kPage,
            GetFocusedElementType(engine.get()));

  // Scroll focused annotation out of viewport.
  static constexpr gfx::Point kScrollPosition = {242, 746};
  engine->ScrolledToXPosition(kScrollPosition.x());
  engine->ScrolledToYPosition(kScrollPosition.y());

  // Scroll the focused annotation into view.
  ScrollFocusedAnnotationIntoView(engine.get());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineTabbingTest, testing::Bool());

using PDFiumEngineReadOnlyTest = PDFiumTestBase;

TEST_P(PDFiumEngineReadOnlyTest, KillFormFocus) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  // Setting read-only mode should kill form focus.
  EXPECT_FALSE(engine->IsReadOnly());
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->SetReadOnly(true);

  // Attempting to focus during read-only mode should once more trigger a
  // killing of form focus.
  EXPECT_TRUE(engine->IsReadOnly());
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->UpdateFocus(true);
}

TEST_P(PDFiumEngineReadOnlyTest, UnselectText) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  // Select text before going into read-only mode.
  EXPECT_FALSE(engine->IsReadOnly());
  engine->SelectAll();
  EXPECT_EQ(kSelectTextExpectedText, engine->GetSelectedText());

  // Setting read-only mode should unselect the text.
  engine->SetReadOnly(true);
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineReadOnlyTest, testing::Bool());

#if BUILDFLAG(ENABLE_PDF_INK2)
class AnnotationModeTestClient : public MockTestClient {
 public:
  AnnotationModeTestClient() = default;
  AnnotationModeTestClient(const AnnotationModeTestClient&) = delete;
  AnnotationModeTestClient& operator=(const AnnotationModeTestClient&) = delete;
  ~AnnotationModeTestClient() override = default;

  // PDFiumEngineClient overrides:
  bool IsInAnnotationMode() const override { return annotation_mode_; }

  void set_annotation_mode(bool annotation_mode) {
    annotation_mode_ = annotation_mode;
  }

 private:
  bool annotation_mode_ = false;
};

using PDFiumEngineAnnotationModeTest = PDFiumTestBase;

TEST_P(PDFiumEngineAnnotationModeTest, KillFormFocus) {
  NiceMock<AnnotationModeTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  client.set_annotation_mode(true);

  // Attempting to focus in annotation mode should once more trigger a killing
  // of form focus.
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->UpdateFocus(true);
}

TEST_P(PDFiumEngineAnnotationModeTest, CannotSelectText) {
  NiceMock<AnnotationModeTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  client.set_annotation_mode(true);

  // Attempting to select text should do nothing in annotation mode.
  engine->SelectAll();
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineAnnotationModeTest, testing::Bool());

#endif  // BUILDFLAG(ENABLE_PDF_INK2)

}  // namespace chrome_pdf
