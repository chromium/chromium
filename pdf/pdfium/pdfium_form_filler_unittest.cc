// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/ppapi_migration/input_event_conversions.h"
#include "pdf/test/test_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

using testing::InSequence;

namespace chrome_pdf {

namespace {

class FormFillerTestClient : public TestClient {
 public:
  FormFillerTestClient() = default;
  ~FormFillerTestClient() override = default;
  FormFillerTestClient(const FormFillerTestClient&) = delete;
  FormFillerTestClient& operator=(const FormFillerTestClient&) = delete;

  // Mock PDFEngine::Client methods.
  MOCK_METHOD(void, ScrollToX, (int), (override));
  MOCK_METHOD(void, ScrollToY, (int, bool), (override));
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
};

TEST_F(FormFillerTest, DoURIActionWithKeyboardModifier) {
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

#if defined(OS_MAC)
#define modifier_key kInputEventModifierMetaKey;
#else
#define modifier_key kInputEventModifierControlKey
#endif

  int modifiers = 0;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = kInputEventModifierAltKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = modifier_key;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = kInputEventModifierShiftKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers |= modifier_key;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers = kInputEventModifierMiddleButtonDown;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
  modifiers |= kInputEventModifierShiftKey;
  TriggerDoURIActionWithKeyboardModifier(engine.get(), kUri, modifiers);
}

TEST_F(FormFillerTest, FormOnFocusChange) {
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
        EXPECT_CALL(client,
                    ScrollToY(test_case.final_scroll_position.y(), false));
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

}  // namespace chrome_pdf
