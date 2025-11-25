// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_move_support.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/page_character_index.h"
#include "pdf/pdf_features.h"
#include "pdf/pdfium/pdfium_draw_selection_test_base.h"
#include "pdf/pdfium/pdfium_engine_client.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/input_event_util.h"
#include "pdf/test/mouse_event_builder.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"
#include "pdf/test/test_helpers.h"
#include "pdf/text_search.h"
#include "pdf/ui/thumbnail.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_PDF_INK2)
#include <array>

#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_constants.h"
#include "pdf/pdf_ink_metrics_handler.h"
#include "pdf/pdfium/pdfium_test_helpers.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#endif

namespace chrome_pdf {

namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kSelectTextExpectedText[] =
    "Hello, world!\nGoodbye, world!\nHello, world!\nGoodbye, world!";

constexpr gfx::PointF kHelloWorldStartPosition{35.0f, 110.0f};
constexpr gfx::PointF kHelloWorldEndPosition{100.0f, 110.0f};

MATCHER_P2(LayoutWithSize, width, height, "") {
  return arg.size() == gfx::Size(width, height);
}

MATCHER_P(LayoutWithOptions, options, "") {
  return arg.options() == options;
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

std::string GetPlatformTextExpectation(std::string expectation) {
#if BUILDFLAG(IS_WIN)
  base::ReplaceSubstringsAfterOffset(&expectation, /*start_offset=*/0, "\n",
                                     "\r\n");
#endif
  return expectation;
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
  MOCK_METHOD(void, Invalidate, (const gfx::Rect&), (override));
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
  MOCK_METHOD(void, ScrollToX, (int, bool), (override));
  MOCK_METHOD(void, ScrollToY, (int, bool), (override));
#if BUILDFLAG(ENABLE_PDF_INK2)
  MOCK_METHOD(bool, IsInAnnotationMode, (), (const override));
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
};

void SimulateMultiClick(PDFiumEngine& engine,
                        const gfx::PointF& position,
                        int click_count) {
  for (int i = 0, click = 1; i < click_count; ++i, ++click) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // On both Linux and ChromeOS `click_count` is only 1, 2 or 3. On MacOS and
    // Windows `click_count` just keeps increasing as the user keeps clicking.
    if (click > 3) {
      click = 1;
    }
#endif
    EXPECT_TRUE(engine.HandleInputEvent(MouseEventBuilder()
                                            .CreateLeftClickAtPosition(position)
                                            .SetClickCount(click)
                                            .Build()));
  }
}

}  // namespace

class PDFiumEngineTest : public PDFiumTestBase {
 protected:
  void ExpectPageRect(const PDFiumEngine& engine,
                      size_t page_index,
                      const gfx::Rect& expected_rect) {
    const PDFiumPage& page = GetPDFiumPage(engine, page_index);
    EXPECT_EQ(expected_rect, page.rect());
  }

  // Tries to load a PDF incrementally, returning `true` if the PDF actually was
  // loaded incrementally. Note that this function will return `false` if
  // incremental loading fails, but also if incremental loading is disabled.
  bool TryLoadIncrementally() {
    TestClient client;
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
      if (GetPDFiumPage(engine, i).available()) {
        ++available_pages;
      }
    }
    return available_pages;
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

  EXPECT_CALL(client, ScrollToPage(-1));
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
  EXPECT_CALL(client, ScrollToPage(-1));
  EXPECT_EQ(gfx::Size(343, 1463), engine->ApplyDocumentLayout(options));
  EXPECT_EQ(gfx::Size(343, 1463), engine->ApplyDocumentLayout(options));
}

TEST_P(PDFiumEngineTest, GetDocumentAttachments) {
  TestClient client;
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
    static constexpr char kCheckSum[] =
        "137F774765ABC1E8D6E650DB560F5EBBBC1603664BF34D21A6AD846BB26E2165";
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
    EXPECT_EQ(kCheckSum, base::HexEncode(crypto::hash::Sha256(content)));
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
  TestClient client;
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
  TestClient client;
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
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_EQ(0u, engine->GetDocumentAttachmentInfoList().size());
}

TEST_P(PDFiumEngineTest, GetDocumentMetadata) {
  TestClient client;
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
  TestClient client;
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
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("linearized.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_TRUE(engine->GetDocumentMetadata().linearized);
}

TEST_P(PDFiumEngineTest, GetBadPdfVersion) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bad_version.pdf"));
  ASSERT_TRUE(engine);

  const DocumentMetadata& doc_metadata = engine->GetDocumentMetadata();
  EXPECT_EQ(PdfVersion::kUnknown, doc_metadata.version);
}

TEST_P(PDFiumEngineTest, GetNamedDestination) {
  TestClient client;
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
  UNSAFE_TODO({ EXPECT_EQ(1.2f, valid_page_obj->params[2]); });

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
  TestClient client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  engine.PluginSizeUpdated({});
  initialize_result.FinishLoading();

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_P(PDFiumEngineTest, PluginSizeUpdatedDuringLoad) {
  TestClient client;
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
  TestClient client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  initialize_result.FinishLoading();
  FinishWithPluginSizeUpdated(engine);

  EXPECT_EQ(engine.GetNumberOfPages(), CountAvailablePages(engine));
}

TEST_P(PDFiumEngineTest, OnLeftMouseDownBeforePluginSizeUpdated) {
  TestClient client;
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
  TestClient client;
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

TEST_P(PDFiumEngineTest, GetPageSizeInPoints) {
  TestClient client;
  InitializeEngineResult initialize_result = InitializeEngineWithoutLoading(
      &client, FILE_PATH_LITERAL("variable_page_sizes.pdf"));
  ASSERT_TRUE(initialize_result.engine);
  PDFiumEngine& engine = *initialize_result.engine;

  engine.PluginSizeUpdated({});
  initialize_result.FinishLoading();

  ASSERT_EQ(engine.GetNumberOfPages(), 7);
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/0),
              testing::Optional(gfx::SizeF(612.0f, 792.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/1),
              testing::Optional(gfx::SizeF(595.0f, 842.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/2),
              testing::Optional(gfx::SizeF(200.0f, 200.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/3),
              testing::Optional(gfx::SizeF(1000.0f, 200.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/4),
              testing::Optional(gfx::SizeF(200.0f, 1000.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/5),
              testing::Optional(gfx::SizeF(1500.0f, 50.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/6),
              testing::Optional(gfx::SizeF(50.0f, 1500.0f)));
  EXPECT_THAT(engine.GetPageSizeInPoints(/*page_index=*/7), std::nullopt);
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

  TestClient client;
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

TEST_P(PDFiumEngineTest, GetPageText) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  static constexpr char16_t kExpectedPageText[] =
      u"Hello, world!\r\nGoodbye, world!";

  EXPECT_EQ(kExpectedPageText, engine->GetPageText(/*page_index=*/0));
  EXPECT_EQ(kExpectedPageText, engine->GetPageText(/*page_index=*/1));
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

TEST_P(PDFiumEngineTest, GetScreenRectsForCaret) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  ASSERT_EQ(30u, engine->GetCharCount(0));
  ASSERT_EQ(30u, engine->GetCharCount(1));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  constexpr gfx::Rect kExpectedRect1{32, 186, 12, 22};
  constexpr gfx::Rect kExpectedRect2{67, 186, 5, 22};
  constexpr gfx::Rect kExpectedRect3{43, 466, 8, 22};
#else
  constexpr gfx::Rect kExpectedRect1{32, 188, 12, 19};
  constexpr gfx::Rect kExpectedRect2{67, 188, 5, 19};
  constexpr gfx::Rect kExpectedRect3{43, 468, 8, 19};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_THAT(engine->GetScreenRectsForCaret({0, 0}),
              ElementsAre(kExpectedRect1));
  EXPECT_THAT(engine->GetScreenRectsForCaret({0, 5}),
              ElementsAre(kExpectedRect2));
  EXPECT_THAT(engine->GetScreenRectsForCaret({1, 1}),
              ElementsAre(kExpectedRect3));
}

TEST_P(PDFiumEngineTest, GetScreenRectsForCaretBlankPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  ASSERT_EQ(0u, engine->GetCharCount(0));

  EXPECT_THAT(engine->GetScreenRectsForCaret({0, 0}),
              ElementsAre(gfx::Rect(18, 16, 17, 17)));
}

TEST_P(PDFiumEngineTest, GetScreenRectsForCaretMiniBlankPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank_mini.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  ASSERT_EQ(0u, engine->GetCharCount(0));

  // Page is too small to fit a caret.
  EXPECT_THAT(engine->GetScreenRectsForCaret({0, 0}), IsEmpty());
}

TEST_P(PDFiumEngineTest, GetTextRunInfoAt) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  ASSERT_EQ(30u, engine->GetCharCount(0));

  EXPECT_FALSE(engine->GetTextRunInfoAt({0, 31}).has_value());
  EXPECT_TRUE(engine->GetTextRunInfoAt({0, 10}).has_value());
}

TEST_P(PDFiumEngineTest, GetTextRunInfoAtBlankPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  ASSERT_EQ(0u, engine->GetCharCount(0));

  EXPECT_FALSE(engine->GetTextRunInfoAt({0, 0}).has_value());
}

TEST_P(PDFiumEngineTest, InvalidateRect) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_CALL(client, Invalidate(gfx::Rect(1, 2, 3, 4)));
  engine->InvalidateRect(gfx::Rect(1, 2, 3, 4));
}

TEST_P(PDFiumEngineTest, IsSynthesizedNewline) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("text_newlines.pdf"));
  ASSERT_TRUE(engine);

  // 'L'.
  EXPECT_FALSE(engine->IsSynthesizedNewline({0, 0}));

  // '\n' non-synthesized.
  EXPECT_FALSE(engine->IsSynthesizedNewline({0, 6}));

  // '\r' non-synthesized.
  EXPECT_FALSE(engine->IsSynthesizedNewline({0, 13}));

  // '\r' synthesized.
  EXPECT_TRUE(engine->IsSynthesizedNewline({0, 21}));

  // '\n' synthesized.
  EXPECT_TRUE(engine->IsSynthesizedNewline({0, 22}));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineTest, testing::Bool());

class PDFiumEngineSelectionTest : public PDFiumEngineTest {
 public:
  void TearDown() override {
    // Reset `engine_` before PDFium gets uninitialized.
    engine_.reset();
    PDFiumEngineTest::TearDown();
  }

  [[nodiscard]] PDFiumEngine* CreateEngine(
      const base::FilePath::CharType* test_filename) {
    engine_ = InitializeEngine(&client_, test_filename);
    if (engine_) {
      // Plugin size chosen so all pages of the document are visible.
      engine_->PluginSizeUpdated({1024, 4096});

      EXPECT_THAT(engine_->GetSelectedText(), IsEmpty());
    }
    return engine_.get();
  }

  // Select from `point1` to `point2` and then select in the reverse order.
  // Only returns one of the two selection results since they should be the
  // same.
  std::string DoForwardBackwardSelections(const gfx::PointF& point1,
                                          const gfx::PointF& point2) {
    EXPECT_TRUE(engine_->HandleInputEvent(
        CreateLeftClickWebMouseEventAtPosition(point1)));
    EXPECT_TRUE(
        engine_->HandleInputEvent(CreateMoveWebMouseEventToPosition(point2)));
    std::string direction1_text = engine_->GetSelectedText();

    EXPECT_TRUE(engine_->HandleInputEvent(
        CreateLeftClickWebMouseEventAtPosition(point2)));
    EXPECT_TRUE(
        engine_->HandleInputEvent(CreateMoveWebMouseEventToPosition(point1)));
    std::string direction2_text = engine_->GetSelectedText();
    EXPECT_EQ(direction1_text, direction2_text);
    return direction1_text;
  }

 private:
  std::unique_ptr<PDFiumEngine> engine_;
  TestClient client_;
};

TEST_P(PDFiumEngineSelectionTest, SelectText) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_TRUE(engine->HasPermission(DocumentPermission::kCopy));

  engine->SelectAll();
  EXPECT_EQ(GetPlatformTextExpectation(kSelectTextExpectedText),
            engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextBackwards) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kSecondPageBeginPosition(105, 420);
  constexpr gfx::PointF kFirstPageEndPosition(85, 120);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kSecondPageBeginPosition)));
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kFirstPageEndPosition)));

  constexpr char kExpectedText[] = "bye, world!\nHello, world!\nGoodby";
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithCopyRestriction) {
  PDFiumEngine* engine =
      CreateEngine(FILE_PATH_LITERAL("hello_world2_with_copy_restriction.pdf"));
  ASSERT_TRUE(engine);

  // The copy restriction should not affect the text selection hehavior.
  EXPECT_FALSE(engine->HasPermission(DocumentPermission::kCopy));

  engine->SelectAll();
  EXPECT_EQ(GetPlatformTextExpectation(kSelectTextExpectedText),
            engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectCroppedText) {
  PDFiumEngine* engine =
      CreateEngine(FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  engine->SelectAll();
  constexpr char kExpectedText[] = "world!\n";
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithDoubleClick) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(100, 120);
  SimulateMultiClick(*engine, kPosition, 2);
  EXPECT_EQ("Goodbye", engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithTripleClick) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(100, 120);
  SimulateMultiClick(*engine, kPosition, 3);
  EXPECT_EQ("Goodbye, world!", engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithFourClicks) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(100, 120);
  SimulateMultiClick(*engine, kPosition, 4);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
#else
  EXPECT_EQ("Goodbye, world!", engine->GetSelectedText());
#endif
}

TEST_P(PDFiumEngineSelectionTest, SelectTextFiveClicks) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(100, 120);
  SimulateMultiClick(*engine, kPosition, 5);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ("Goodbye", engine->GetSelectedText());
#else
  EXPECT_EQ("Goodbye, world!", engine->GetSelectedText());
#endif
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithSixClicks) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(100, 120);
  SimulateMultiClick(*engine, kPosition, 6);
  EXPECT_EQ("Goodbye, world!", engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithMouse) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kHelloWorldStartPosition)));

  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kHelloWorldEndPosition)));

  EXPECT_EQ("Goodb", engine->GetSelectedText());
}

#if BUILDFLAG(IS_MAC)
TEST_P(PDFiumEngineSelectionTest, CtrlLeftClickShouldNotSelectTextOnMac) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // In https://crbug.com/339681892, these are the events PDFiumEngine sees.
  MouseEventBuilder builder;
  builder.CreateLeftClickAtPosition(kHelloWorldStartPosition)
      .SetModifiers(blink::WebInputEvent::Modifiers::kControlKey);
  EXPECT_FALSE(engine->HandleInputEvent(builder.Build()));

  EXPECT_FALSE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kHelloWorldEndPosition)));

  EXPECT_EQ("", engine->GetSelectedText());
}
#else
TEST_P(PDFiumEngineSelectionTest, CtrlLeftClickSelectTextOnNonMac) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  MouseEventBuilder builder;
  builder.CreateLeftClickAtPosition(kHelloWorldStartPosition)
      .SetModifiers(blink::WebInputEvent::Modifiers::kControlKey);
  EXPECT_TRUE(engine->HandleInputEvent(builder.Build()));

  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kHelloWorldEndPosition)));

  EXPECT_EQ("Goodb", engine->GetSelectedText());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_P(PDFiumEngineSelectionTest, SelectLinkAreaWithNoText) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kStartPosition(90, 120);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kStartPosition)));

  constexpr gfx::PointF kMiddlePosition(120, 230);
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kMiddlePosition)));

  constexpr char kExpectedText[] = "Link Annotations - Page 1\nL";
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());

  constexpr gfx::PointF kEndPosition(430, 230);
  EXPECT_FALSE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition(kEndPosition)));

  // This is still `kExpectedText` because of the unit test's uncanny ability to
  // move the mouse to `kEndPosition` in one move.
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextOneChar) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  static constexpr gfx::PointF kStartPosition(158, 120);
  static constexpr gfx::PointF kEndPosition(161, 120);
  std::string result =
      DoForwardBackwardSelections(kStartPosition, kEndPosition);
  EXPECT_EQ("r", result);
}

TEST_P(PDFiumEngineSelectionTest, SelectTextTwoChar) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  static constexpr gfx::PointF kStartPosition(158, 120);
  static constexpr gfx::PointF kEndPosition(167, 120);
  std::string result =
      DoForwardBackwardSelections(kStartPosition, kEndPosition);
  EXPECT_EQ("rl", result);
}

TEST_P(PDFiumEngineSelectionTest, SelectTextAcrossLine) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Click and drag to the right.
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition({159, 120})));
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateMoveWebMouseEventToPosition({159.5f, 120.0f})));
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({163, 120})));
  EXPECT_EQ("r", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({167, 120})));
  EXPECT_EQ("rl", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({172, 120})));
  EXPECT_EQ("rl", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({176, 120})));
  EXPECT_EQ("rld", engine->GetSelectedText());

  // Click and drag to the left.
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition({159.5f, 120.0f})));
  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({159, 120})));
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({146, 120})));
  EXPECT_EQ("o", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({145, 120})));
  EXPECT_EQ("o", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({137, 120})));
  EXPECT_EQ("wo", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({130, 120})));
  EXPECT_EQ("wo", engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextAcrossLineRtl) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hebrew_mirrored.pdf"));
  ASSERT_TRUE(engine);

  // Click and drag to the right.
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition({220, 50})));
  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({225, 50})));
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({230, 50})));
  EXPECT_EQ("י", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({235, 50})));
  EXPECT_EQ("ני", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({240, 50})));
  EXPECT_EQ("בני", engine->GetSelectedText());

  // Click and drag to the left.
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition({225, 50})));
  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({220, 50})));
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({215, 50})));
  EXPECT_EQ("מ", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({210, 50})));
  EXPECT_EQ("מי", engine->GetSelectedText());

  EXPECT_TRUE(
      engine->HandleInputEvent(CreateMoveWebMouseEventToPosition({205, 50})));
  EXPECT_EQ("מין", engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextAcrossEmptyPage) {
  PDFiumEngine* engine = CreateEngine(
      FILE_PATH_LITERAL("multi_page_hello_world_with_empty_page.pdf"));
  ASSERT_TRUE(engine);

  static constexpr gfx::PointF kStartPosition(80, 200);
  static constexpr gfx::PointF kEndPosition(95, 765);
  std::string result =
      DoForwardBackwardSelections(kStartPosition, kEndPosition);
  EXPECT_EQ(GetPlatformTextExpectation("world!\nGoodbye,"), result);

  // Select all.
  engine->SelectAll();
  static constexpr char kExpectedAllSelection[] =
      "Hello, world!\nGoodbye, world!";
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedAllSelection),
            engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithDoubleClickOnEmptyPage) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(100, 100);
  EXPECT_TRUE(engine->HandleInputEvent(MouseEventBuilder()
                                           .CreateLeftClickAtPosition(kPosition)
                                           .SetClickCount(2)
                                           .Build()));
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithDoubleClickAtEndOfPage) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  constexpr gfx::PointF kPosition(195, 130);
  EXPECT_TRUE(engine->HandleInputEvent(MouseEventBuilder()
                                           .CreateLeftClickAtPosition(kPosition)
                                           .SetClickCount(2)
                                           .Build()));
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

TEST_P(PDFiumEngineSelectionTest, SelectTextWithNonPrintableCharacter) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("bug_1357385.pdf"));
  ASSERT_TRUE(engine);

  engine->SelectAll();
  EXPECT_EQ("Hello, world!", engine->GetSelectedText());
}

TEST_P(PDFiumEngineSelectionTest, ClearTextSelection) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Select text.
  engine->SelectAll();
  EXPECT_EQ(GetPlatformTextExpectation(kSelectTextExpectedText),
            engine->GetSelectedText());

  // Clear selected text.
  engine->ClearTextSelection();
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

TEST_P(PDFiumEngineSelectionTest, StartExtendAndInvalidateSelectionByChar) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Cannot extend selection without starting a selection first.
  engine->ExtendAndInvalidateSelectionByChar({1, 5});
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
  EXPECT_FALSE(engine->IsSelecting());

  engine->StartSelection({0, 21});
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
  EXPECT_FALSE(engine->IsSelecting());

  engine->ExtendAndInvalidateSelectionByChar({0, 22});
  EXPECT_EQ("e", engine->GetSelectedText());
  EXPECT_TRUE(engine->IsSelecting());

  engine->ExtendAndInvalidateSelectionByChar({1, 5});
  constexpr char kExpectedText[] = "e, world!\nHello";
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());
  EXPECT_TRUE(engine->IsSelecting());

  // Start another selection while one is active. This should be a no-op.
  engine->StartSelection({0, 0});
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());
  EXPECT_TRUE(engine->IsSelecting());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineSelectionTest, testing::Bool());

using PDFiumEngineDrawSelectionTest = PDFiumDrawSelectionTestBase;

TEST_P(PDFiumEngineDrawSelectionTest, DrawTextSelectionsHelloWorld) {
  constexpr int kPageIndex = 0;
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
  DrawAndExpectBlank(*engine, kPageIndex,
                     /*expected_visible_page_size=*/gfx::Size(266, 266));

  SetSelection(*engine, /*start_page_index=*/kPageIndex, /*start_char_index=*/1,
               /*end_page_index=*/kPageIndex, /*end_char_index=*/2);
  EXPECT_EQ("e", engine->GetSelectedText());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_selection_1.png");

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

TEST_P(PDFiumEngineDrawSelectionTest, DrawTextSelectionsBigtableMicro) {
  TestClient client;
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

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineDrawSelectionTest, testing::Bool());

using PDFiumEngineDeathTest = PDFiumEngineTest;

TEST_P(PDFiumEngineDeathTest, RequestThumbnailRedundant) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPdfIncrementalLoading);

  TestClient client;
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

  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));
  }

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
    for (auto focused : kExpectedFocusState) {
      EXPECT_CALL(client, DocumentFocusChanged(focused));
    }
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
    for (auto focused : kExpectedFocusState) {
      EXPECT_CALL(client, DocumentFocusChanged(focused));
    }
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
    for (auto focused : kExpectedFocusState) {
      EXPECT_CALL(client, DocumentFocusChanged(focused));
    }
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
    for (auto focused : kExpectedFocusState) {
      EXPECT_CALL(client, DocumentFocusChanged(focused));
    }
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
    for (auto focused : kExpectedFocusState) {
      EXPECT_CALL(client, DocumentFocusChanged(focused));
    }
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

TEST_P(PDFiumEngineTabbingTest, SetFormHighlight) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  InSequence sequence;
  EXPECT_CALL(client,
              FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText));

  // Tab into the document.
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));
  // Tab into the page.
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Removing form highlights should remove focus.
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->SetFormHighlight(false);
}

class ScrollingTestClient : public TestClient {
 public:
  ScrollingTestClient() = default;
  ~ScrollingTestClient() override = default;
  ScrollingTestClient(const ScrollingTestClient&) = delete;
  ScrollingTestClient& operator=(const ScrollingTestClient&) = delete;

  // Mock PDFiumEngineClient methods.
  MOCK_METHOD(void, ScrollToX, (int, bool), (override));
  MOCK_METHOD(void, ScrollToY, (int, bool), (override));
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
    EXPECT_CALL(client,
                ScrollToY(kScrollValue.y(), /*force_smooth_scroll=*/false))
        .WillOnce(
            [&engine]() { engine->ScrolledToYPosition(kScrollValue.y()); });
    EXPECT_CALL(client,
                ScrollToX(kScrollValue.x(), /*force_smooth_scroll=*/false))
        .WillOnce(
            [&engine]() { engine->ScrolledToXPosition(kScrollValue.x()); });
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
      EXPECT_CALL(client,
                  ScrollToY(scroll_value.y(), /*force_smooth_scroll=*/false))
          .WillOnce([&engine, &scroll_value]() {
            engine->ScrolledToYPosition(scroll_value.y());
          });
      EXPECT_CALL(client,
                  ScrollToX(scroll_value.x(), /*force_smooth_scroll=*/false))
          .WillOnce([&engine, &scroll_value]() {
            engine->ScrolledToXPosition(scroll_value.x());
          });
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

using PDFiumEngineReadOnlyTest = PDFiumEngineTabbingTest;

TEST_P(PDFiumEngineReadOnlyTest, KillFormFocus) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  InSequence sequence;
  EXPECT_CALL(client,
              FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText));

  // Tab into the document.
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));
  // Tab into the page.
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Setting read-only mode should kill form focus.
  EXPECT_FALSE(engine->IsReadOnly());
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->SetReadOnly(true);
  EXPECT_TRUE(engine->IsReadOnly());

  // Attempting to focus during read-only mode should do nothing since there is
  // no form focus change.
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
  EXPECT_EQ(GetPlatformTextExpectation(kSelectTextExpectedText),
            engine->GetSelectedText());

  // Setting read-only mode should unselect the text.
  engine->SetReadOnly(true);
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineReadOnlyTest, testing::Bool());

#if BUILDFLAG(ENABLE_PDF_INK2)
using PDFiumEngineInkTest = PDFiumEngineTabbingTest;

TEST_P(PDFiumEngineInkTest, KillFormFocusInAnnotationMode) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  InSequence sequence;
  EXPECT_CALL(client,
              FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText));

  // Tab into the document.
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));
  // Tab into the page.
  ASSERT_TRUE(HandleTabEvent(engine.get(), /*modifiers=*/0));

  // Attempting to focus the PDF Viewer in annotation mode should kill form
  // focus.
  EXPECT_CALL(client, IsInAnnotationMode()).WillOnce(Return(true));
  EXPECT_CALL(client, FormFieldFocusChange(
                          PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->UpdateFocus(true);
}

TEST_P(PDFiumEngineInkTest, CannotSelectTextInAnnotationMode) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());

  // Update the plugin size so that all the text is visible by
  // `SelectionChangeInvalidator`.
  engine->PluginSizeUpdated({500, 500});

  EXPECT_CALL(client, IsInAnnotationMode()).WillOnce(Return(true));

  // Attempting to select text should do nothing in annotation mode.
  engine->SelectAll();
  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
}

TEST_P(PDFiumEngineInkTest, ContainsV2InkPath) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  constexpr base::TimeDelta kContainsV2InkPathTimeout =
      base::Milliseconds(5000);
  EXPECT_EQ(engine->ContainsV2InkPath(kContainsV2InkPathTimeout),
            PDFLoadedWithV2InkAnnotations::kFalse);

  engine = InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  EXPECT_EQ(engine->ContainsV2InkPath(kContainsV2InkPathTimeout),
            PDFLoadedWithV2InkAnnotations::kTrue);

  // Test timeout.
  engine = InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  EXPECT_EQ(engine->ContainsV2InkPath(base::Milliseconds(0)),
            PDFLoadedWithV2InkAnnotations::kUnknown);
}

TEST_P(PDFiumEngineInkTest, LoadV2InkPathsForPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());
  EXPECT_TRUE(engine->ink_modeled_shape_map_for_testing().empty());

  constexpr int kPageIndex = 0;
  std::map<InkModeledShapeId, ink::PartitionedMesh> ink_shapes =
      engine->LoadV2InkPathsForPage(kPageIndex);
  ASSERT_EQ(1u, ink_shapes.size());
  const auto ink_shapes_it = ink_shapes.begin();

  const std::map<InkModeledShapeId, FPDF_PAGEOBJECT>& pdf_shapes =
      engine->ink_modeled_shape_map_for_testing();
  ASSERT_EQ(1u, pdf_shapes.size());
  const auto pdf_shapes_it = pdf_shapes.begin();

  EXPECT_EQ(ink_shapes_it->first, pdf_shapes_it->first);
  EXPECT_EQ(1u, ink_shapes_it->second.Meshes().size());
  EXPECT_TRUE(pdf_shapes_it->second);

  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));
}

TEST_P(PDFiumEngineInkTest, GetCanonicalToPdfTransform) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());

  static constexpr gfx::PointF kCanonicalTopLeftPoint(0.0f, 0.0f);
  static constexpr gfx::PointF kCanonicalMiddlePoint(100.0f, 50.0f);
  const gfx::Transform transform =
      engine->GetCanonicalToPdfTransform(/*page_index=*/0);
  EXPECT_EQ(gfx::PointF(0.0f, 200.0f),
            transform.MapPoint(kCanonicalTopLeftPoint));
  EXPECT_EQ(gfx::PointF(75.0f, 162.5f),
            transform.MapPoint(kCanonicalMiddlePoint));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineInkTest, testing::Bool());

class PDFiumEngineInkTextSelectionTest : public PDFiumEngineInkTest {
 public:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  static constexpr PdfRect kGoodbyeWorldExpectedRectPage0{20.0f, 96.656f,
                                                          136.496f, 111.648f};
#else
  static constexpr PdfRect kGoodbyeWorldExpectedRectPage0{20.0f, 96.592f,
                                                          136.496f, 111.792f};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  static constexpr gfx::PointF kNonTextPositionPage0{5.0f, 5.0f};

  void TearDown() override {
    // Reset `engine_` before PDFium gets uninitialized.
    engine_.reset();
    PDFiumEngineInkTest::TearDown();
  }

  [[nodiscard]] PDFiumEngine* CreateEngine(
      const base::FilePath::CharType* test_filename) {
    engine_ = InitializeEngine(&client_, test_filename);
    if (engine_) {
      // Plugin size chosen so all pages of the document are visible.
      engine_->PluginSizeUpdated({1024, 4096});

      EXPECT_THAT(engine_->GetSelectedText(), IsEmpty());
      EXPECT_THAT(engine_->GetSelectionRectMap(), IsEmpty());
    }
    return engine_.get();
  }

 private:
  std::unique_ptr<PDFiumEngine> engine_;
  TestClient client_;
};

TEST_P(PDFiumEngineInkTextSelectionTest, ExtendSelectionByNonTextPoint) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Test point not on a page.
  EXPECT_FALSE(engine->ExtendSelectionByPoint({-30.0f, -30.0f}));

  // Test point not on any text.
  EXPECT_FALSE(engine->ExtendSelectionByPoint(kNonTextPositionPage0));
}

TEST_P(PDFiumEngineInkTextSelectionTest, ExtendSelectionByPoint) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/1);

  EXPECT_TRUE(engine->ExtendSelectionByPoint(kHelloWorldEndPosition));

  EXPECT_EQ("Goodb", engine->GetSelectedText());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  constexpr PdfRect kExpectedRect{20.0f, 99.824f, 68.032f, 111.648f};
#else
  constexpr PdfRect kExpectedRect{20.0f, 99.712f, 68.032f, 111.792f};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_THAT(engine->GetSelectionRectMap(),
              ElementsAre(Pair(0, ElementsAre(kExpectedRect))));
}

TEST_P(PDFiumEngineInkTextSelectionTest, ExtendSelectionByPointMultiPage) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/1);

  constexpr gfx::PointF kEndPosition(75.0f, 480.0f);
  EXPECT_TRUE(engine->ExtendSelectionByPoint(kEndPosition));

  constexpr char kExpectedText[] = "Goodbye, world!\nHello, ";
  EXPECT_EQ(GetPlatformTextExpectation(kExpectedText),
            engine->GetSelectedText());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  constexpr PdfRect kExpectedRectPage1{20.0f, 48.008f, 52.664f, 58.328f};
#else
  constexpr PdfRect kExpectedRectPage1{20.0f, 48.32f, 52.664f, 58.196f};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_THAT(engine->GetSelectionRectMap(),
              ElementsAre(Pair(0, ElementsAre(kGoodbyeWorldExpectedRectPage0)),
                          Pair(1, ElementsAre(kExpectedRectPage1))));
}

TEST_P(PDFiumEngineInkTextSelectionTest, OnTextOrLinkAreaClickWithSingleClick) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/1);

  EXPECT_THAT(engine->GetSelectedText(), IsEmpty());
  EXPECT_THAT(engine->GetSelectionRectMap(), IsEmpty());
}

TEST_P(PDFiumEngineInkTextSelectionTest, OnTextOrLinkAreaClickWithDoubleClick) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/2);

  EXPECT_EQ("Goodbye", engine->GetSelectedText());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  constexpr PdfRect kExpectedRect{20.0f, 96.656f, 84.928f, 111.648f};
#else
  constexpr PdfRect kExpectedRect{20.0f, 96.592f, 84.928f, 111.792f};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_THAT(engine->GetSelectionRectMap(),
              ElementsAre(Pair(0, ElementsAre(kExpectedRect))));
}

TEST_P(PDFiumEngineInkTextSelectionTest, IsSelectableTextOrLinkAreaText) {
  PDFiumEngine* engine =
      CreateEngine(FILE_PATH_LITERAL("form_text_fields.pdf"));
  ASSERT_TRUE(engine);

  // Non-text position.
  EXPECT_FALSE(engine->IsSelectableTextOrLinkArea(kNonTextPositionPage0));

  // Form field position.
  EXPECT_FALSE(engine->IsSelectableTextOrLinkArea({155.0f, 250.0f}));

  // Text position.
  EXPECT_TRUE(engine->IsSelectableTextOrLinkArea({160.0f, 145.0f}));
}

TEST_P(PDFiumEngineInkTextSelectionTest, IsSelectableTextOrLinkAreaLink) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  // Link position.
  EXPECT_TRUE(engine->IsSelectableTextOrLinkArea({155.0f, 230.0f}));
}

TEST_P(PDFiumEngineInkTextSelectionTest, OnTextOrLinkAreaClickWithTripleClick) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/3);

  EXPECT_EQ("Goodbye, world!", engine->GetSelectedText());
  EXPECT_THAT(
      engine->GetSelectionRectMap(),
      ElementsAre(Pair(0, ElementsAre(kGoodbyeWorldExpectedRectPage0))));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFiumEngineInkTextSelectionTest,
                         testing::Bool());

using PDFiumEngineInkDrawTest = PDFiumTestBase;

TEST_P(PDFiumEngineInkDrawTest, NoStrokeData) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            0);
}

TEST_P(PDFiumEngineInkDrawTest, StrokeData) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);
  int page_count = FPDF_GetPageCount(engine->doc());
  ASSERT_EQ(page_count, 1);

  // Original document drawn on has no stroke data.
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            0);

  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  constexpr int kPageIndex = 0;
  constexpr gfx::Size kPageSizeInPoints(200, 200);
  const base::FilePath kBlankPngFilePath(FILE_PATH_LITERAL("blank.png"));
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kBlankPngFilePath);

  // Draw 2 strokes.
  auto pen_brush = std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen,
                                                 SK_ColorRED, /*size=*/4.0f);
  constexpr auto kPenInputs = std::to_array<PdfInkInputData>({
      {{5.0f, 5.0f}, base::Seconds(0.0f)},
      {{50.0f, 5.0f}, base::Seconds(0.1f)},
  });
  auto highlighter_brush = std::make_unique<PdfInkBrush>(
      PdfInkBrush::Type::kHighlighter, SK_ColorCYAN, /*size=*/6.0f);
  constexpr auto kHighlighterInputs = std::to_array<PdfInkInputData>({
      {{75.0f, 5.0f}, base::Seconds(0.0f)},
      {{75.0f, 60.0f}, base::Seconds(0.1f)},
  });
  std::optional<ink::StrokeInputBatch> pen_inputs =
      CreateInkInputBatch(kPenInputs);
  ASSERT_TRUE(pen_inputs.has_value());
  std::optional<ink::StrokeInputBatch> highlighter_inputs =
      CreateInkInputBatch(kHighlighterInputs);
  ASSERT_TRUE(highlighter_inputs.has_value());
  ink::Stroke pen_stroke(pen_brush->ink_brush(), pen_inputs.value());
  ink::Stroke highlighter_stroke(highlighter_brush->ink_brush(),
                                 highlighter_inputs.value());
  constexpr InkStrokeId kPenStrokeId(1);
  constexpr InkStrokeId kHighlighterStrokeId(2);
  engine->ApplyStroke(kPageIndex, kPenStrokeId, pen_stroke);
  engine->ApplyStroke(kPageIndex, kHighlighterStrokeId, highlighter_stroke);

  PDFiumPage& page = GetPDFiumPage(*engine, kPageIndex);

  // Verify the visibility of strokes for in-memory PDF.
  const base::FilePath kAppliedStroke2FilePath(
      GetInkTestDataFilePath(FILE_PATH_LITERAL("applied_stroke2.png")));
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kAppliedStroke2FilePath);
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));

  // Getting the save data should now have the new strokes.
  // Verify visibility of strokes in that copy.  Must call GetSaveData()
  // before checking mark objects count, so that the PDF gets regenerated.
  saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kAppliedStroke2FilePath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            2);

  // Set the highlighter stroke as inactive, to perform the equivalent of an
  // "undo" action. The affected stroke should no longer be included in the
  // saved PDF data.
  engine->UpdateStrokeActive(kPageIndex, kHighlighterStrokeId,
                             /*active=*/false);
  const base::FilePath kAppliedStroke1FilePath(
      GetInkTestDataFilePath(FILE_PATH_LITERAL("applied_stroke1.png")));
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kAppliedStroke1FilePath);
  saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kAppliedStroke1FilePath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            1);
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));

  // Set the highlighter stroke as active again, to perform the equivalent of an
  // "redo" action. The affected stroke should be included in the saved PDF data
  // again.
  engine->UpdateStrokeActive(kPageIndex, kHighlighterStrokeId, /*active=*/true);
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kAppliedStroke2FilePath);
  saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kAppliedStroke2FilePath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            2);
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));
}

TEST_P(PDFiumEngineInkDrawTest, StrokeDiscardStroke) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);
  int page_count = FPDF_GetPageCount(engine->doc());
  ASSERT_EQ(page_count, 1);

  // Original document drawn on has no stroke data.
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            0);

  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  constexpr int kPageIndex = 0;
  constexpr gfx::Size kPageSizeInPoints(200, 200);
  const base::FilePath kBlankPngFilePath(FILE_PATH_LITERAL("blank.png"));
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kBlankPngFilePath);

  // Draw a stroke.
  auto brush = std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen,
                                             SK_ColorRED, /*size=*/4.0f);
  constexpr auto kInputs0 = std::to_array<PdfInkInputData>({
      {{5.0f, 5.0f}, base::Seconds(0.0f)},
      {{50.0f, 5.0f}, base::Seconds(0.1f)},
  });
  std::optional<ink::StrokeInputBatch> batch = CreateInkInputBatch(kInputs0);
  ASSERT_TRUE(batch.has_value());
  ink::Stroke stroke0(brush->ink_brush(), batch.value());
  constexpr InkStrokeId kStrokeId(0);
  engine->ApplyStroke(kPageIndex, kStrokeId, stroke0);

  PDFiumPage& page = GetPDFiumPage(*engine, kPageIndex);

  // Verify the visibility of strokes for in-memory PDF.
  const base::FilePath kAppliedStroke1FilePath(
      GetInkTestDataFilePath(FILE_PATH_LITERAL("applied_stroke1.png")));
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kAppliedStroke1FilePath);
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));

  // Set the stroke as inactive, to perform the equivalent of an "undo" action.
  engine->UpdateStrokeActive(kPageIndex, kStrokeId, /*active=*/false);

  // The document should not have any stroke data.
  saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kBlankPngFilePath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            0);
  EXPECT_EQ(FPDFPage_CountObjects(page.GetPage()), 1);
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));

  // Discard the stroke.
  engine->DiscardStroke(kPageIndex, kStrokeId);

  EXPECT_EQ(FPDFPage_CountObjects(page.GetPage()), 0);
  EXPECT_FALSE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));

  // Draw a new stroke, reusing the same InkStrokeId. This can occur after an
  // undo action.
  constexpr auto kInputs1 = std::to_array<PdfInkInputData>({
      {{75.0f, 5.0f}, base::Seconds(0.0f)},
      {{75.0f, 60.0f}, base::Seconds(0.1f)},
  });
  batch = CreateInkInputBatch(kInputs1);
  ASSERT_TRUE(batch.has_value());
  ink::Stroke stroke1(brush->ink_brush(), batch.value());
  engine->ApplyStroke(kPageIndex, kStrokeId, stroke1);

  // Verify the visibility of strokes for in-memory PDF.
  const base::FilePath kAppliedStroke3FilePath(
      GetInkTestDataFilePath(FILE_PATH_LITERAL("applied_stroke3.png")));
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kAppliedStroke3FilePath);
  EXPECT_EQ(FPDFPage_CountObjects(page.GetPage()), 1);
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));
}

TEST_P(PDFiumEngineInkDrawTest, LoadedV2InkPathsAndUpdateShapeActive) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  // Check the initial loaded PDF.
  constexpr int kPageIndex = 0;
  constexpr gfx::Size kPageSizeInPoints(200, 200);
  const base::FilePath kInkV2PngPath =
      GetInkTestDataFilePath(FILE_PATH_LITERAL("ink_v2.png"));
  PDFiumPage& page = GetPDFiumPage(*engine, kPageIndex);
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kInkV2PngPath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            1);

  // Check the LoadV2InkPathsForPage() call does not change the rendering.
  std::map<InkModeledShapeId, ink::PartitionedMesh> ink_shapes =
      engine->LoadV2InkPathsForPage(kPageIndex);
  ASSERT_EQ(1u, ink_shapes.size());
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kInkV2PngPath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            1);

  // Attempt to unload the page before erasing. This would have caught
  // https://crbug.com/402364794.
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));
  page.Unload();

  // Erase the shape and check the rendering. Also check the save version.
  const auto ink_shapes_it = ink_shapes.begin();
  const InkModeledShapeId& shape_id = ink_shapes_it->first;
  engine->UpdateShapeActive(kPageIndex, shape_id, /*active=*/false);
  const base::FilePath kBlankPngPath(FILE_PATH_LITERAL("blank.png"));
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kBlankPngPath);
  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kBlankPngPath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            0);

  // Attempt to unload the page before undoing. This would have caught
  // https://crbug.com/402454523.
  EXPECT_TRUE(engine->stroked_pages_unload_preventers_for_testing().contains(
      kPageIndex));
  page.Unload();

  // Undo the erasure and check the rendering.
  engine->UpdateShapeActive(kPageIndex, shape_id, /*active=*/true);
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kInkV2PngPath);
  saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kInkV2PngPath);
  EXPECT_EQ(GetPdfMarkObjCountForTesting(engine->doc(),
                                         kInkAnnotationIdentifierKeyV2),
            1);
}

TEST_P(PDFiumEngineInkDrawTest, ThumbnailsDoNotContainStrokes) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  static constexpr int kPageIndex = 0;
  static constexpr float kDevicePixelRatio = 1;
  // Note that this is not the same size as pdf/test/data/blank.png.
  static constexpr gfx::Size kExpectedImageSize(140, 140);
  // Since blank.pdf renders at all white pixels, check by just counting the
  // pixels. The raw image data has 4 components per pixel.
  const size_t kExpectedWhiteComponentCount = kExpectedImageSize.GetArea() * 4;
  {
    base::test::TestFuture<Thumbnail> future;
    engine->RequestThumbnail(kPageIndex, kDevicePixelRatio,
                             future.GetCallback());
    ASSERT_TRUE(future.Wait());

    Thumbnail thumbnail = future.Take();
    ASSERT_EQ(kExpectedImageSize, thumbnail.image_size());
    EXPECT_THAT(thumbnail.GetImageData(),
                Contains(0xFF).Times(kExpectedWhiteComponentCount));
  }

  // Draw 2 strokes.
  auto pen_brush = std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen,
                                                 SK_ColorRED, /*size=*/4.0f);
  static constexpr auto kPenInputs = std::to_array<PdfInkInputData>({
      {{5.0f, 5.0f}, base::Seconds(0.0f)},
      {{50.0f, 5.0f}, base::Seconds(0.1f)},
  });
  auto highlighter_brush = std::make_unique<PdfInkBrush>(
      PdfInkBrush::Type::kHighlighter, SK_ColorCYAN, /*size=*/6.0f);
  static constexpr auto kHighlighterInputs = std::to_array<PdfInkInputData>({
      {{75.0f, 5.0f}, base::Seconds(0.0f)},
      {{75.0f, 60.0f}, base::Seconds(0.1f)},
  });
  std::optional<ink::StrokeInputBatch> pen_inputs =
      CreateInkInputBatch(kPenInputs);
  ASSERT_TRUE(pen_inputs.has_value());
  std::optional<ink::StrokeInputBatch> highlighter_inputs =
      CreateInkInputBatch(kHighlighterInputs);
  ASSERT_TRUE(highlighter_inputs.has_value());
  ink::Stroke pen_stroke(pen_brush->ink_brush(), pen_inputs.value());
  ink::Stroke highlighter_stroke(highlighter_brush->ink_brush(),
                                 highlighter_inputs.value());
  static constexpr InkStrokeId kPenStrokeId(1);
  static constexpr InkStrokeId kHighlighterStrokeId(2);
  engine->ApplyStroke(kPageIndex, kPenStrokeId, pen_stroke);
  engine->ApplyStroke(kPageIndex, kHighlighterStrokeId, highlighter_stroke);

  {
    base::test::TestFuture<Thumbnail> future;
    engine->RequestThumbnail(kPageIndex, kDevicePixelRatio,
                             future.GetCallback());
    ASSERT_TRUE(future.Wait());

    Thumbnail thumbnail = future.Take();
    ASSERT_EQ(kExpectedImageSize, thumbnail.image_size());
    EXPECT_THAT(thumbnail.GetImageData(),
                Contains(0xFF).Times(kExpectedWhiteComponentCount));
  }
}

TEST_P(PDFiumEngineInkDrawTest, RotatedPdf) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rotated_multi_page_cropped.pdf"));
  ASSERT_TRUE(engine);

  // Draw 2 strokes.
  auto pen_brush = std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen,
                                                 SK_ColorRED, /*size=*/4.0f);
  constexpr auto kPenInputs = std::to_array<PdfInkInputData>({
      {{5.0f, 5.0f}, base::Seconds(0.0f)},
      {{50.0f, 5.0f}, base::Seconds(0.1f)},
  });
  auto highlighter_brush = std::make_unique<PdfInkBrush>(
      PdfInkBrush::Type::kHighlighter, SK_ColorCYAN, /*size=*/6.0f);
  constexpr auto kHighlighterInputs = std::to_array<PdfInkInputData>({
      {{75.0f, 5.0f}, base::Seconds(0.0f)},
      {{75.0f, 60.0f}, base::Seconds(0.1f)},
  });
  std::optional<ink::StrokeInputBatch> pen_inputs =
      CreateInkInputBatch(kPenInputs);
  ASSERT_TRUE(pen_inputs.has_value());
  std::optional<ink::StrokeInputBatch> highlighter_inputs =
      CreateInkInputBatch(kHighlighterInputs);
  ASSERT_TRUE(highlighter_inputs.has_value());
  ink::Stroke pen_stroke(pen_brush->ink_brush(), pen_inputs.value());
  ink::Stroke highlighter_stroke(highlighter_brush->ink_brush(),
                                 highlighter_inputs.value());
  constexpr InkStrokeId kPenStrokeId(1);
  constexpr InkStrokeId kHighlighterStrokeId(2);
  constexpr int kPageIndex = 1;
  engine->ApplyStroke(kPageIndex, kPenStrokeId, pen_stroke);
  engine->ApplyStroke(kPageIndex, kHighlighterStrokeId, highlighter_stroke);

  PDFiumPage& page = GetPDFiumPage(*engine, kPageIndex);

  // Verify the visibility of strokes for in-memory PDF.
  constexpr gfx::Size kPageSizeInPoints(500, 350);
  const base::FilePath kExpectedFilePath(GetInkTestDataFilePath(
      FILE_PATH_LITERAL("rotated_multi_page_cropped1.png")));
  CheckPdfRendering(page.GetPage(), kPageSizeInPoints, kExpectedFilePath);

  // Getting the save data should now have the new strokes.
  // Verify visibility of strokes in that copy.  Must call GetSaveData()
  // before checking mark objects count, so that the PDF gets regenerated.
  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_FALSE(saved_pdf_data.empty());
  CheckPdfRendering(saved_pdf_data, kPageIndex, kPageSizeInPoints,
                    kExpectedFilePath);
}

// Don't be concerned about any slight rendering differences in AGG vs. Skia,
// covering one of these is sufficient for checking how data is written out.
INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineInkDrawTest, testing::Values(false));

using PDFiumEngineInkPrintTest = PDFiumTestBase;

TEST_P(PDFiumEngineInkPrintTest, InkStrokes) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  // Draw a stroke.
  static constexpr auto kInputs = std::to_array<PdfInkInputData>({
      {{5.0f, 5.0f}, base::Seconds(0.0f)},
      {{50.0f, 5.0f}, base::Seconds(0.1f)},
  });
  std::optional<ink::StrokeInputBatch> batch = CreateInkInputBatch(kInputs);
  ASSERT_TRUE(batch.has_value());
  auto brush = std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen,
                                             SK_ColorRED, /*size=*/4.0f);
  ink::Stroke stroke(brush->ink_brush(), batch.value());
  static constexpr std::array<int, 1> kPagesToPrint = {0};
  static constexpr InkStrokeId kStrokeId(0);
  engine->ApplyStroke(kPagesToPrint[0], kStrokeId, stroke);

  blink::WebPrintParams print_params = GetDefaultPrintParams();
  print_params.printable_area_in_css_pixels = kPrintableAreaRect;
  print_params.print_scaling_option =
      printing::mojom::PrintScalingOption::kFitToPaper;

  engine->PrintBegin();
  std::vector<uint8_t> pdf_data =
      engine->PrintPages(kPagesToPrint, print_params);
  engine->PrintEnd();

  base::FilePath expected_output = GetInkTestDataFilePath(
      FILE_PATH_LITERAL("applied_stroke_printed_fit_to_page.png"));
  CheckPdfRendering(pdf_data, kPagesToPrint[0], {612, 792}, expected_output);
}

// Don't be concerned about any slight rendering differences in AGG vs. Skia,
// covering one of these is sufficient for checking how data is written out.
INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineInkPrintTest, testing::Values(false));

class PDFiumEngineCaretTest : public PDFiumDrawSelectionTestBase {
 public:
  static constexpr gfx::Size kAnnotationFormFieldsVisiblePageSize{816, 1056};
  static constexpr gfx::Size kHelloWorldExpectedVisiblePageSize{266, 266};
  static constexpr gfx::PointF kHelloWorldGoodbyeWorldCharB{85.0f, 118.0f};
  PDFiumEngineCaretTest() = default;
  PDFiumEngineCaretTest(const PDFiumEngineCaretTest&) = delete;
  PDFiumEngineCaretTest& operator=(const PDFiumEngineCaretTest&) = delete;
  ~PDFiumEngineCaretTest() override = default;

  MockTestClient& client() { return client_; }

  void SetUp() override {
    PDFiumDrawSelectionTestBase::SetUp();

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPdfInk2,
        {{features::kPdfInk2TextHighlighting.name, "true"}});
  }

  void TearDown() override {
    // Reset `engine_` before PDFium gets uninitialized.
    engine_.reset();
    PDFiumDrawSelectionTestBase::TearDown();
  }

  [[nodiscard]] PDFiumEngine* CreateEngine(
      const base::FilePath::CharType* test_filename) {
    engine_ = InitializeEngine(&client_, test_filename);
    return engine_.get();
  }

  [[nodiscard]] PDFiumEngine* CreateEngineWithCaret(
      const base::FilePath::CharType* test_filename) {
    engine_ = InitializeEngine(&client_, test_filename);
    if (engine_) {
      // Plugin size chosen so all pages of the document are visible.
      engine_->PluginSizeUpdated({1024, 4096});
      engine_->UpdateFocus(true);
      engine_->SetCaretBrowsingEnabled(true);
    }
    return engine_.get();
  }

  bool HandleKeyDownEvent(ui::KeyboardCode key) {
    blink::WebKeyboardEvent event(
        blink::WebInputEvent::Type::kKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = key;
    return engine_->HandleInputEvent(event);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PDFiumEngine> engine_;
  NiceMock<MockTestClient> client_;
};

TEST_P(PDFiumEngineCaretTest, InitializeCaretWithoutFocus) {
  PDFiumEngine* engine = CreateEngine(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Plugin size chosen so all pages of the document are visible.
  engine->PluginSizeUpdated({1024, 4096});

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kHelloWorldExpectedVisiblePageSize);

  // Engine does not have focus, so enabling caret browsing still draws blank.
  engine->SetCaretBrowsingEnabled(true);

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kHelloWorldExpectedVisiblePageSize);
}

TEST_P(PDFiumEngineCaretTest, SetCaretBrowsingEnabled) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret.png");

  engine->SetCaretBrowsingEnabled(false);

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kHelloWorldExpectedVisiblePageSize);

  engine->SetCaretBrowsingEnabled(true);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret.png");
}

TEST_P(PDFiumEngineCaretTest, SetCaretBrowsingEnabledNoOp) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret.png");

  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kHelloWorldGoodbyeWorldCharB)));

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_1.png");

  // Already enabled. Caret should not move.
  engine->SetCaretBrowsingEnabled(true);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_1.png");
}

TEST_P(PDFiumEngineCaretTest,
       SetCaretBrowsingEnabledSetsCaretAtFirstVisibleTextRun) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  // Starts at first text run.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "link_annots_visible_text_run_0.png");

  // Scroll so that text on page 0 and page 1 are visible.
  constexpr gfx::Size kPluginSizeWithPage0AndPage1{617, 900};
  engine->PluginSizeUpdated(kPluginSizeWithPage0AndPage1);
  engine->ScrolledToYPosition(400);

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0, {612, 659});
  DrawCaretAndExpectBlank(*engine, /*page_index=*/1, {612, 227});

  engine->SetCaretBrowsingEnabled(false);
  engine->SetCaretBrowsingEnabled(true);

  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "link_annots_visible_text_run_1.png");
  DrawCaretAndExpectBlank(*engine, /*page_index=*/1, {612, 227});

  // Scroll so both pages are visible, but only text on page 1 is visible.
  engine->ScrolledToYPosition(800);

  engine->SetCaretBrowsingEnabled(false);
  engine->SetCaretBrowsingEnabled(true);

  constexpr gfx::Size kPage0ExpectedVisiblePageSize{612, 259};
  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kPage0ExpectedVisiblePageSize);
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/1, "link_annots_visible_text_run_2.png");

  // Shrink the plugin's size so that no text is visible.
  engine->PluginSizeUpdated({617, 300});

  engine->SetCaretBrowsingEnabled(false);
  engine->SetCaretBrowsingEnabled(true);

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kPage0ExpectedVisiblePageSize);
  DrawCaretAndExpectBlank(*engine, /*page_index=*/1, {612, 27});

  // Go back to the previous plugin size. The caret should still be at the
  // previous position.
  engine->PluginSizeUpdated(kPluginSizeWithPage0AndPage1);

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kPage0ExpectedVisiblePageSize);
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/1, "link_annots_visible_text_run_2.png");
}

TEST_P(PDFiumEngineCaretTest, UpdateFocus) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret.png");

  engine->UpdateFocus(false);

  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kHelloWorldExpectedVisiblePageSize);

  engine->UpdateFocus(true);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret.png");
}

TEST_P(PDFiumEngineCaretTest, DrawOnGeometryChange) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->ScrolledToXPosition(20);

  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_on_geometry_change_0.png");

  engine->ScrolledToYPosition(40);

  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_on_geometry_change_1.png");
}

TEST_P(PDFiumEngineCaretTest, TextClick) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // The "b" in "Goodbye, world!".
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kHelloWorldGoodbyeWorldCharB)));

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_1.png");

  // The newline after "Hello, world!".
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(gfx::PointF(130, 190))));

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_newline.png");
}

TEST_P(PDFiumEngineCaretTest, TextMultiClick) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(kHelloWorldGoodbyeWorldCharB)));

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_1.png");

  SimulateMultiClick(*engine, kHelloWorldGoodbyeWorldCharB, /*click_count=*/2);

  // Caret should not be visible.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_text_selection_word.png");

  SimulateMultiClick(*engine, kHelloWorldGoodbyeWorldCharB, /*click_count=*/3);

  // Caret should not be visible.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_text_selection_line.png");

  SimulateMultiClick(*engine, kHelloWorldGoodbyeWorldCharB, /*click_count=*/4);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_1.png");
#else
  // Caret should not be visible.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_text_selection_line.png");
#endif
}

TEST_P(PDFiumEngineCaretTest, TextClickSyntheticWhitespace) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("text_synthetic_whitespace.pdf"));
  ASSERT_TRUE(engine);

  // The synthetic whitespace with an empty screen rect.
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(gfx::PointF(102, 130))));

  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "text_synthetic_whitespace_caret_0.png");
}

TEST_P(PDFiumEngineCaretTest, TextClickMultiPage) {
  PDFiumEngine* engine = CreateEngineWithCaret(
      FILE_PATH_LITERAL("multi_page_hello_world_with_empty_page.pdf"));
  ASSERT_TRUE(engine);

  // First page. The first "l" in "Hello, world!".
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(gfx::PointF(52, 190))));

  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "multi_page_hello_world_caret_0.png");

  // Second page. The "w" in "Goodbye, world!".
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(gfx::PointF(100, 750))));

  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/2, "multi_page_hello_world_caret_1.png");
}

TEST_P(PDFiumEngineCaretTest, TextSelectAndMove) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/1);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_start.png");

  EXPECT_TRUE(engine->ExtendSelectionByPoint(kHelloWorldEndPosition));

  // Caret should not be visible when text selecting.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_text_selection.png");

  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_RIGHT));

  // TODO(crbug.com/446944878): Caret should appear at the end of the text
  // selection.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_text_selection_end.png");
}

TEST_P(PDFiumEngineCaretTest, TextSelectAndBack) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  engine->OnTextOrLinkAreaClick(kHelloWorldStartPosition, /*click_count=*/1);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_start.png");

  EXPECT_TRUE(engine->ExtendSelectionByPoint(kHelloWorldEndPosition));
  EXPECT_TRUE(engine->ExtendSelectionByPoint(kHelloWorldStartPosition));

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret_start.png");
}

TEST_P(PDFiumEngineCaretTest, SelectAll) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  DrawCaretAndCompareWithPlatformExpectations(*engine, /*page_index=*/0,
                                              "hello_world_caret.png");

  engine->SelectAll();

  // Caret should not be visible.
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/0, "hello_world_caret_text_selection_all.png");
  DrawCaretAndCompareWithPlatformExpectations(
      *engine, /*page_index=*/1, "hello_world_caret_text_selection_all.png");
}

TEST_P(PDFiumEngineCaretTest, FormFocus) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  // Focus onto a form field page element.
  EXPECT_CALL(client(),
              FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText));
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_TAB));
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_TAB));

  // Caret should not be visible.
  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kAnnotationFormFieldsVisiblePageSize);

  // Press an arrow key, which is a key event that the caret may handle.
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_DOWN));

  // Caret should not be visible.
  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kAnnotationFormFieldsVisiblePageSize);

  // Press tab, which will update focus to a different form field.
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_TAB));

  // Caret should not be visible.
  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kAnnotationFormFieldsVisiblePageSize);

  // Click outside the form field to kill focus.
  EXPECT_CALL(client(), FormFieldFocusChange(
                            PDFiumEngineClient::FocusFieldType::kNoFocus));
  EXPECT_TRUE(engine->HandleInputEvent(
      CreateLeftClickWebMouseEventAtPosition(gfx::PointF(1.0f, 1.0f))));

  // Caret should be visible.
  DrawCaretAndCompare(*engine, /*page_index=*/0,
                      "annotation_form_fields_caret.png");
}

TEST_P(PDFiumEngineCaretTest, FormFieldLoseFocusGainFocus) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  // Focus onto a form field page element.
  EXPECT_CALL(client(),
              FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText));
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_TAB));
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_TAB));

  // Caret should not be visible.
  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kAnnotationFormFieldsVisiblePageSize);

  EXPECT_CALL(client(), FormFieldFocusChange(
                            PDFiumEngineClient::FocusFieldType::kNoFocus));
  engine->UpdateFocus(false);

  EXPECT_CALL(client(),
              FormFieldFocusChange(PDFiumEngineClient::FocusFieldType::kText));
  engine->UpdateFocus(true);

  // Form still has focus, so caret should not be visible.
  DrawCaretAndExpectBlank(*engine, /*page_index=*/0,
                          kAnnotationFormFieldsVisiblePageSize);
}

TEST_P(PDFiumEngineCaretTest, ScrollToChar) {
  PDFiumEngine* engine =
      CreateEngineWithCaret(FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Already visible.
  EXPECT_CALL(client(), ScrollToX(_, _)).Times(0);
  EXPECT_CALL(client(), ScrollToY(_, _)).Times(0);
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_RIGHT));
  Mock::VerifyAndClearExpectations(&client());

  // Shrink the plugin size so only a portion of the page is visible.
  engine->PluginSizeUpdated({35, 300});

  EXPECT_CALL(client(), ScrollToX(37, /*force_smooth_scroll=*/false));
  EXPECT_CALL(client(), ScrollToY(0, /*force_smooth_scroll=*/false));
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_DOWN));
  Mock::VerifyAndClearExpectations(&client());

  EXPECT_CALL(client(), ScrollToX(36, /*force_smooth_scroll=*/false));
  EXPECT_CALL(client(), ScrollToY(327, /*force_smooth_scroll=*/false));
  EXPECT_TRUE(HandleKeyDownEvent(ui::KeyboardCode::VKEY_DOWN));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumEngineCaretTest, testing::Bool());

#endif  // BUILDFLAG(ENABLE_PDF_INK2)

class SearchStringTestClient : public TestClient {
 public:
  std::vector<SearchStringResult> SearchString(const std::u16string& needle,
                                               const std::u16string& haystack,
                                               bool case_sensitive) override {
    EXPECT_FALSE(needle.empty());
    EXPECT_FALSE(haystack.empty());
    return TextSearch(/*needle=*/needle, /*haystack=*/haystack, case_sensitive);
  }

  MOCK_METHOD(void, ScrollToX, (int, bool), (override));
  MOCK_METHOD(void, ScrollToY, (int, bool), (override));
  MOCK_METHOD(void, OnNewTextFragmentsSearchStarted, (), (override));
};

class PDFiumEngineHighlightTextFragmentTest
    : public PDFiumEngineDrawSelectionTest {
 public:
  static constexpr gfx::Size kSpannerExpectedVisiblePageSize{816, 1056};

  std::unique_ptr<PDFiumEngine> InitializePdfEngine(TestClient& client) {
    std::unique_ptr<PDFiumEngine> engine =
        InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
    // Update the plugin size so that all the text is visible by
    // `HighlightChangeInvalidator`.
    engine->PluginSizeUpdated({821, 1059});
    return engine;
  }
};

TEST_P(PDFiumEngineHighlightTextFragmentTest, OnlyTextStart) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);
  engine->FindAndHighlightTextFragments({"Spanner"});

  DrawHighlightsAndCompare(*engine, 0, "spanner_text_start_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, TextStartAndEnd) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"spanner,database"});

  DrawHighlightsAndCompare(*engine, 0, "spanner_text_start_end_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, TextStartAndTextSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"how,-many"});

  DrawHighlightsAndCompare(*engine, 0,
                           "spanner_text_start_suffix_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, TextStartEndAndSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"this,api,-and"});

  DrawHighlightsAndCompare(*engine, 0,
                           "spanner_text_start_end_suffix_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, TextPrefixAndTextStart) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"is-,Google"});

  DrawHighlightsAndCompare(*engine, 0,
                           "spanner_text_prefix_start_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, TextPrefixStartAndSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"of-,Google,-'s"});

  DrawHighlightsAndCompare(*engine, 0,
                           "spanner_text_prefix_start_suffix_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, TextPrefixStartEndAndSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"and-,applications,old,-timestamps"});

  DrawHighlightsAndCompare(
      *engine, 0, "spanner_text_prefix_start_end_suffix_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, MultipleTextFragments) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"Google", "is-,Google",
                                         "of-,Google,-'s",
                                         "and-,applications,old,-timestamps"});

  DrawHighlightsAndCompare(*engine, 0,
                           "spanner_multiple_fragments_highlight.png");
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, FragmentNotInPDF) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"apples"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"of-,Google,-random"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"of-,Google,random"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"and-,applications,old,-random"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"apples-,Google"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"Google,-random"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"applications,random"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);

  engine->FindAndHighlightTextFragments({"applications,old,-random"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);
}

// Assert that the second highlight should clear the existing highlight.
TEST_P(PDFiumEngineHighlightTextFragmentTest, ConsecutiveHighlights) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"Spanner"});
  DrawHighlightsAndCompare(*engine, 0, "spanner_text_start_highlight.png");

  engine->FindAndHighlightTextFragments({"spanner,database"});
  DrawHighlightsAndCompare(*engine, 0, "spanner_text_start_end_highlight.png");
}

// Assert that a failed text fragment search should also clear the existing
// highlight.
TEST_P(PDFiumEngineHighlightTextFragmentTest,
       ClearExistingHighlightOnFailedFind) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"Spanner"});
  DrawHighlightsAndCompare(*engine, 0, "spanner_text_start_highlight.png");

  engine->FindAndHighlightTextFragments({"does_not_exist"});
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, RemoveTextFragments) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  engine->FindAndHighlightTextFragments({"Spanner"});
  DrawHighlightsAndCompare(*engine, 0, "spanner_text_start_highlight.png");

  engine->RemoveTextFragments();
  DrawAndExpectBlank(*engine, 0, kSpannerExpectedVisiblePageSize);
}

TEST_P(PDFiumEngineHighlightTextFragmentTest, ScrollToFirstTextFragment) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);
  engine->PluginSizeUpdated({200, 400});

  EXPECT_CALL(client, ScrollToX(424, /*force_smooth_scroll=*/true));
  EXPECT_CALL(client, ScrollToY(749, /*force_smooth_scroll=*/true));

  engine->FindAndHighlightTextFragments({"difficult to implement"});
  engine->ScrollToFirstTextFragment(/*force_smooth_scroll=*/true);
}

// Assert that OnNewTextFragmentsSearchStarted() is called for any text
// fragment search.
TEST_P(PDFiumEngineHighlightTextFragmentTest, OnNewTextFragmentsSearchStarted) {
  SearchStringTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializePdfEngine(client);
  ASSERT_TRUE(engine);

  {
    EXPECT_CALL(client, OnNewTextFragmentsSearchStarted);
    engine->FindAndHighlightTextFragments({});
  }
  {
    EXPECT_CALL(client, OnNewTextFragmentsSearchStarted);
    engine->FindAndHighlightTextFragments({"not_found"});
  }
  {
    EXPECT_CALL(client, OnNewTextFragmentsSearchStarted);
    engine->FindAndHighlightTextFragments({"Google"});
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFiumEngineHighlightTextFragmentTest,
                         testing::Bool());

}  // namespace chrome_pdf
