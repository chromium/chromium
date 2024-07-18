// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_form_filler.h"

#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gin/public/isolate_holder.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_formfill.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "v8/include/v8-isolate.h"

namespace chrome_pdf {

namespace {

using ::testing::Contains;
using ::testing::InSequence;
using ::testing::Return;

class FormFillerTestClient : public TestClient {
 public:
  FormFillerTestClient() = default;
  ~FormFillerTestClient() override = default;
  FormFillerTestClient(const FormFillerTestClient&) = delete;
  FormFillerTestClient& operator=(const FormFillerTestClient&) = delete;

  // Mock PDFiumEngineClient methods.
  MOCK_METHOD(void, Beep, (), (override));
  MOCK_METHOD(std::string, GetURL, (), (override));
  MOCK_METHOD(void, ScrollToX, (int), (override));
  MOCK_METHOD(void, ScrollToY, (int), (override));
  MOCK_METHOD(void,
              NavigateTo,
              (const std::string&, WindowOpenDisposition),
              (override));
};

}  // namespace

class FormFillerTest : public PDFiumTestBase {
 public:
  FormFillerTest() = default;
  ~FormFillerTest() override = default;
  FormFillerTest(const FormFillerTest&) = delete;
  FormFillerTest& operator=(const FormFillerTest&) = delete;

  void TriggerFormFocusChange(PDFiumEngine* engine,
                              FPDF_ANNOTATION annot,
                              int page_index) {
    ASSERT_TRUE(engine);
    engine->form_filler_.Form_OnFocusChange(&engine->form_filler_, annot,
                                            page_index);
  }

  void TriggerDoURIActionWithKeyboardModifier(PDFiumEngine* engine,
                                              FPDF_BYTESTRING uri,
                                              int modifiers) {
    ASSERT_TRUE(engine);
    engine->form_filler_.Form_DoURIActionWithKeyboardModifier(
        &engine->form_filler_, uri, modifiers);
  }

#if defined(PDF_ENABLE_V8)
  void TriggerBeep(PDFiumEngine* engine) {
    ASSERT_TRUE(engine);
    engine->form_filler_.Form_Beep(&engine->form_filler_,
                                   JSPLATFORM_BEEP_DEFAULT);
  }

  int TriggerGetFilePath(PDFiumEngine& engine, void* file_path, int length) {
    return engine.form_filler_.Form_GetFilePath(&engine.form_filler_, file_path,
                                                length);
  }
#endif  // defined(PDF_ENABLE_V8)
};

TEST_P(FormFillerTest, DoURIActionWithKeyboardModifier) {
  FormFillerTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);

  const char kUri[] = "https://www.google.com/";
  {
    InSequence sequence;
    EXPECT_CALL(client, NavigateTo(kUri, WindowOpenDisposition::CURRENT_TAB))
        .Times(1);
    EXPECT_CALL(client, NavigateTo(kUri, WindowOpenDisposition::SAVE_TO_DISK))
        .Times(1);
    EXPECT_CALL(client,
                NavigateTo(kUri, WindowOpenDisposition::NEW_BACKGROUND_TAB))
        .Times(1);
    EXPECT_CALL(client, NavigateTo(kUri, WindowOpenDisposition::NEW_WINDOW))
        .Times(1);
    EXPECT_CALL(client,
                NavigateTo(kUri, WindowOpenDisposition::NEW_FOREGROUND_TAB))
        .Times(1);
    EXPECT_CALL(client,
                NavigateTo(kUri, WindowOpenDisposition::NEW_BACKGROUND_TAB))
        .Times(1);
    EXPECT_CALL(client,
                NavigateTo(kUri, WindowOpenDisposition::NEW_FOREGROUND_TAB))
        .Times(1);
  }

  constexpr blink::WebInputEvent::Modifiers kModifierKey =
#if BUILDFLAG(IS_MAC)
      blink::WebInputEvent::Modifiers::kMetaKey;
#else
      blink::WebInputEvent::Modifiers::kControlKey;
#endif

  int modifiers = 0;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = blink::WebInputEvent::Modifiers::kAltKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = kModifierKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = blink::WebInputEvent::Modifiers::kShiftKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers |= kModifierKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = blink::WebInputEvent::Modifiers::kMiddleButtonDown;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers |= blink::WebInputEvent::Modifiers::kShiftKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
}

TEST_P(FormFillerTest, FormOnFocusChange) {
  struct {
    // Initial scroll position of the document.
    gfx::Point initial_position;
    // Page number on which the annotation is present.
    int page_index;
    // The index of test annotation on page_index.
    int annot_index;
    // The scroll position to bring the annotation into view. (0,0) if the
    // annotation is already in view.
    gfx::Point final_scroll_position;
  } static constexpr test_cases[] = {
      {{0, 0}, 0, 0, {242, 746}},   {{0, 0}, 0, 1, {510, 478}},
      {{242, 40}, 0, 0, {0, 746}},  {{60, 758}, 0, 0, {242, 0}},
      {{242, 758}, 0, 0, {0, 0}},   {{242, 768}, 0, 0, {0, 746}},
      {{274, 758}, 0, 0, {242, 0}}, {{60, 40}, 1, 0, {242, 1816}}};

  FormFillerTestClient client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("annotation_form_fields.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(2, engine->GetNumberOfPages());
  engine->PluginSizeUpdated(gfx::Size(60, 40));

  {
    InSequence sequence;

    for (const auto& test_case : test_cases) {
      if (test_case.final_scroll_position.y() != 0) {
        EXPECT_CALL(client, ScrollToY(test_case.final_scroll_position.y()));
      }
      if (test_case.final_scroll_position.x() != 0)
        EXPECT_CALL(client, ScrollToX(test_case.final_scroll_position.x()));
    }
  }

  for (const auto& test_case : test_cases) {
    // Setting up the initial scroll positions.
    engine->ScrolledToXPosition(test_case.initial_position.x());
    engine->ScrolledToYPosition(test_case.initial_position.y());

    PDFiumPage& page = GetPDFiumPageForTest(*engine, test_case.page_index);
    ScopedFPDFAnnotation annot(
        FPDFPage_GetAnnot(page.GetPage(), test_case.annot_index));
    ASSERT_TRUE(annot);
    TriggerFormFocusChange(engine.get(), annot.get(), test_case.page_index);
  }
}

INSTANTIATE_TEST_SUITE_P(All, FormFillerTest, testing::Bool());

#if defined(PDF_ENABLE_V8)
class FormFillerJavaScriptTest : public FormFillerTest {
 public:
  void SetUp() override {
    // Needed for setting up V8.
    //
    // Note that this does not call FormFillerTest::SetUp() to avoid double SDK
    // initialization.
    InitializeSDK(/*enable_v8=*/true, /*use_skia=*/GetParam(),
                  FontMappingMode::kNoMapping);
  }

  void TearDown() override {
    // Note that this does not call FormFillerTest::TearDown() to avoid double
    // SDK destruction.
    ShutdownSDK();
  }
};

TEST_P(FormFillerJavaScriptTest, IsolateScoping) {
  // Enter the embedder's isolate so it can be captured when the
  // `PDFiumFormFiller` is created.
  v8::Isolate* embedder_isolate = GetBlinkIsolate();
  v8::Isolate::Scope embedder_isolate_scope(embedder_isolate);

  FormFillerTestClient client;
  PDFiumEngine engine(&client, PDFiumFormFiller::ScriptOption::kJavaScript);

  gin::IsolateHolder pdfium_test_isolate_holder(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      gin::IsolateHolder::IsolateType::kTest);
  v8::Isolate* pdfium_test_isolate = pdfium_test_isolate_holder.isolate();

  // Enter PDFium's isolate and then trigger a beep callback. The embedder's
  // isolate should be entered during the callback's execution.
  v8::Isolate::Scope pdfium_test_isolate_scope(pdfium_test_isolate);
  EXPECT_CALL(client, Beep).WillOnce([&embedder_isolate]() {
    EXPECT_EQ(v8::Isolate::TryGetCurrent(), embedder_isolate);
  });
  TriggerBeep(&engine);

  // PDFium's isolate should be entered again after the callback completes.
  EXPECT_EQ(v8::Isolate::TryGetCurrent(), pdfium_test_isolate);
}

TEST_P(FormFillerJavaScriptTest, GetFilePath) {
  constexpr char kTestPath[] = "https://www.example.com/path/to/the.pdf";
  constexpr int kTestPathSize = static_cast<int>(std::size(kTestPath));

  FormFillerTestClient client;
  EXPECT_CALL(client, GetURL).Times(2).WillRepeatedly(Return(kTestPath));
  PDFiumEngine engine(&client, PDFiumFormFiller::ScriptOption::kJavaScript);

  EXPECT_EQ(TriggerGetFilePath(engine, /*file_path=*/nullptr, /*length=*/0),
            kTestPathSize);

  std::vector<char> buffer(kTestPathSize, 'X');
  EXPECT_EQ(TriggerGetFilePath(engine, buffer.data(), buffer.size()),
            kTestPathSize);
  EXPECT_STREQ(buffer.data(), kTestPath);
}

TEST_P(FormFillerJavaScriptTest, GetFilePathEmpty) {
  FormFillerTestClient client;
  EXPECT_CALL(client, GetURL).Times(2).WillRepeatedly(Return(std::string()));
  PDFiumEngine engine(&client, PDFiumFormFiller::ScriptOption::kJavaScript);

  EXPECT_EQ(TriggerGetFilePath(engine, /*file_path=*/nullptr, /*length=*/0), 1);

  char buffer[] = "buffer";
  EXPECT_EQ(TriggerGetFilePath(engine, buffer, /*length=*/1), 1);

  // The trailing null should be copied over.
  EXPECT_STREQ(buffer, "");
}

TEST_P(FormFillerJavaScriptTest, GetFilePathShortBuffer) {
  constexpr char kTestPath[] = "https://www.example.com/path/to/the.pdf";
  constexpr int kTestPathSize = static_cast<int>(std::size(kTestPath));

  FormFillerTestClient client;
  EXPECT_CALL(client, GetURL).WillRepeatedly(Return(kTestPath));
  PDFiumEngine engine(&client, PDFiumFormFiller::ScriptOption::kJavaScript);

  std::vector<char> buffer(kTestPathSize - 1, 'X');
  EXPECT_EQ(TriggerGetFilePath(engine, buffer.data(), buffer.size()),
            kTestPathSize);

  // Nothing should be copied over. The buffer size is too small to contain a
  // trailing null.
  EXPECT_THAT(buffer, Contains('X').Times(buffer.size()));
}

INSTANTIATE_TEST_SUITE_P(All, FormFillerJavaScriptTest, testing::Bool());
#endif  // defined(PDF_ENABLE_V8)

}  // namespace chrome_pdf
