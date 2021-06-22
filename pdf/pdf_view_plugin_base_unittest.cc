// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/content_restriction.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "ppapi/cpp/instance.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

// Keep it in-sync with the `kFinalFallbackName` returned by
// net::GetSuggestedFilename().
constexpr char kDefaultDownloadFileName[] = "download";

// Dummy data to save.
constexpr uint8_t kSaveData[] = {'1', '2', '3'};

class TestPDFiumEngine : public PDFiumEngine {
 public:
  explicit TestPDFiumEngine(PDFEngine::Client* client)
      : PDFiumEngine(client, PDFiumFormFiller::ScriptOption::kNoJavaScript) {}

  TestPDFiumEngine(const TestPDFiumEngine&) = delete;

  TestPDFiumEngine& operator=(const TestPDFiumEngine&) = delete;

  ~TestPDFiumEngine() override = default;

  uint32_t GetLoadedByteSize() override { return sizeof(kSaveData); }

  bool ReadLoadedBytes(uint32_t length, void* buffer) override {
    DCHECK_LE(length, GetLoadedByteSize());
    memcpy(buffer, kSaveData, length);
    return true;
  }

  std::vector<uint8_t> GetSaveData() override {
    return std::vector<uint8_t>(std::begin(kSaveData), std::end(kSaveData));
  }
};

// This test approach relies on PdfViewPluginBase continuing to exist.
// PdfViewPluginBase and PdfViewWebPlugin are going to merge once
// OutOfProcessInstance is deprecated.
class FakePdfViewPluginBase : public PdfViewPluginBase {
 public:
  // Public for testing.
  using PdfViewPluginBase::edit_mode;
  using PdfViewPluginBase::full_frame;
  using PdfViewPluginBase::HandleMessage;
  using PdfViewPluginBase::InitializeEngine;
  using PdfViewPluginBase::set_full_frame;

  MOCK_METHOD(bool, Confirm, (const std::string&), (override));

  MOCK_METHOD(std::string,
              Prompt,
              (const std::string&, const std::string&),
              (override));

  MOCK_METHOD(std::vector<PDFEngine::Client::SearchStringResult>,
              SearchString,
              (const char16_t*, const char16_t*, bool),
              (override));

  MOCK_METHOD(bool, IsPrintPreview, (), (override));

  MOCK_METHOD(void, SetSelectedText, (const std::string&), (override));

  MOCK_METHOD(void, SetLinkUnderCursor, (const std::string&), (override));

  MOCK_METHOD(bool, IsValidLink, (const std::string&), (override));

  MOCK_METHOD(std::unique_ptr<Graphics>,
              CreatePaintGraphics,
              (const gfx::Size&),
              (override));

  MOCK_METHOD(bool, BindPaintGraphics, (Graphics&), (override));

  MOCK_METHOD(void,
              ScheduleTaskOnMainThread,
              (const base::Location&, ResultCallback, int32_t, base::TimeDelta),
              (override));

  MOCK_METHOD(base::WeakPtr<PdfViewPluginBase>, GetWeakPtr, (), (override));

  MOCK_METHOD(std::unique_ptr<UrlLoader>,
              CreateUrlLoaderInternal,
              (),
              (override));

  MOCK_METHOD(void, DidOpen, (std::unique_ptr<UrlLoader>, int32_t), (override));

  void SendMessage(base::Value message) override {
    sent_message_ = std::move(message);
  }

  MOCK_METHOD(void, SaveAs, (), (override));

  MOCK_METHOD(void, InitImageData, (const gfx::Size&), (override));

  MOCK_METHOD(void, SetFormFieldInFocus, (bool in_focus), (override));

  MOCK_METHOD(void,
              SetAccessibilityDocInfo,
              (const AccessibilityDocInfo&),
              (override));

  MOCK_METHOD(void,
              SetAccessibilityPageInfo,
              (AccessibilityPageInfo,
               std::vector<AccessibilityTextRunInfo>,
               std::vector<AccessibilityCharInfo>,
               AccessibilityPageObjects),
              (override));

  MOCK_METHOD(void,
              SetAccessibilityViewportInfo,
              (const AccessibilityViewportInfo&),
              (override));

  MOCK_METHOD(void, SetContentRestrictions, (int), (override));

  MOCK_METHOD(void, SetPluginCanSave, (bool), (override));

  MOCK_METHOD(void, PluginDidStartLoading, (), (override));

  MOCK_METHOD(void, PluginDidStopLoading, (), (override));

  MOCK_METHOD(void, InvokePrintDialog, (), (override));

  MOCK_METHOD(void,
              NotifySelectionChanged,
              (const gfx::PointF&, int, const gfx::PointF&, int),
              (override));

  MOCK_METHOD(void, NotifyUnsupportedFeature, (), (override));

  MOCK_METHOD(void, UserMetricsRecordAction, (const std::string&), (override));

  const base::Value& sent_message() const { return sent_message_; }

 private:
  base::Value sent_message_;
};

base::Value CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType type,
                                     const std::string& token) {
  base::Value message(base::Value::Type::DICTIONARY);
  message.SetStringKey("type", "save");
  message.SetIntKey("saveRequestType", static_cast<int>(type));
  message.SetStringKey("token", token);
  return message;
}

base::Value CreateExpectedSaveToBufferResponse(const std::string& token) {
  base::Value expected_response(base::Value::Type::DICTIONARY);
  expected_response.SetStringKey("type", "saveData");
  expected_response.SetStringKey("token", token);
  expected_response.SetStringKey("fileName", kDefaultDownloadFileName);
  expected_response.SetKey("dataToSave",
                           base::Value(base::make_span(kSaveData)));
  return expected_response;
}

base::Value CreateExpectedSaveToFileResponse(const std::string& token) {
  base::Value expected_response(base::Value::Type::DICTIONARY);
  expected_response.SetStringKey("type", "consumeSaveToken");
  expected_response.SetStringKey("token", token);
  return expected_response;
}

}  // namespace

class PdfViewPluginBaseTest : public testing::Test {
 protected:
  FakePdfViewPluginBase fake_plugin_;
};

class PdfViewPluginBaseSaveTest : public PdfViewPluginBaseTest {
 public:
  void SetUp() override {
    std::unique_ptr<TestPDFiumEngine> engine =
        std::make_unique<TestPDFiumEngine>(&fake_plugin_);
    fake_plugin_.InitializeEngine(std::move(engine));
  }
};

TEST_F(PdfViewPluginBaseTest, CreateUrlLoaderInFullFrame) {
  fake_plugin_.set_full_frame(true);
  ASSERT_TRUE(fake_plugin_.full_frame());

  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
  EXPECT_CALL(fake_plugin_, SetContentRestrictions(kContentRestrictionSave |
                                                   kContentRestrictionPrint));
  EXPECT_CALL(fake_plugin_, PluginDidStartLoading());
  EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal());
  fake_plugin_.CreateUrlLoader();
  EXPECT_TRUE(fake_plugin_.GetDidCallStartLoadingForTesting());
}

TEST_F(PdfViewPluginBaseTest, CreateUrlLoaderWithoutFullFrame) {
  ASSERT_FALSE(fake_plugin_.full_frame());

  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
  EXPECT_CALL(fake_plugin_, SetContentRestrictions(kContentRestrictionSave |
                                                   kContentRestrictionPrint))
      .Times(0);
  EXPECT_CALL(fake_plugin_, PluginDidStartLoading()).Times(0);
  EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal());
  fake_plugin_.CreateUrlLoader();
  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
}

TEST_F(PdfViewPluginBaseTest, DocumentHasUnsupportedFeatureInFullFrame) {
  fake_plugin_.set_full_frame(true);
  ASSERT_TRUE(fake_plugin_.full_frame());

  // Arbitrary feature names and their matching metric names.
  static constexpr char kFeature1[] = "feature1";
  static constexpr char kMetric1[] = "PDF_Unsupported_feature1";
  static constexpr char kFeature2[] = "feature2";
  static constexpr char kMetric2[] = "PDF_Unsupported_feature2";

  // Find unsupported `kFeature1` for the first time.
  EXPECT_FALSE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric1));
  EXPECT_FALSE(
      fake_plugin_.GetNotifiedBrowserAboutUnsupportedFeatureForTesting());
  EXPECT_CALL(fake_plugin_, NotifyUnsupportedFeature());
  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction(kMetric1));

  fake_plugin_.DocumentHasUnsupportedFeature(kFeature1);
  EXPECT_TRUE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric1));
  EXPECT_TRUE(
      fake_plugin_.GetNotifiedBrowserAboutUnsupportedFeatureForTesting());

  // Find unsupported `kFeature2` for the first time.
  EXPECT_FALSE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric2));
  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction(kMetric2));

  fake_plugin_.DocumentHasUnsupportedFeature(kFeature2);
  EXPECT_TRUE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric2));
  EXPECT_TRUE(
      fake_plugin_.GetNotifiedBrowserAboutUnsupportedFeatureForTesting());

  // Find unsupported `kFeature1` for the second time.
  fake_plugin_.DocumentHasUnsupportedFeature(kFeature1);
  EXPECT_TRUE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric1));
  EXPECT_TRUE(
      fake_plugin_.GetNotifiedBrowserAboutUnsupportedFeatureForTesting());
}

TEST_F(PdfViewPluginBaseTest, DocumentHasUnsupportedFeatureWithoutFullFrame) {
  ASSERT_FALSE(fake_plugin_.full_frame());

  // An arbitrary feature name and its matching metric name.
  static constexpr char kFeature[] = "feature";
  static constexpr char kMetric[] = "PDF_Unsupported_feature";

  EXPECT_FALSE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric));
  EXPECT_FALSE(
      fake_plugin_.GetNotifiedBrowserAboutUnsupportedFeatureForTesting());

  // NotifyUnsupportedFeature() should never be called if the viewer doesn't
  // occupy the whole frame, but the metrics should still be recorded.
  EXPECT_CALL(fake_plugin_, NotifyUnsupportedFeature()).Times(0);
  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction(kMetric));

  fake_plugin_.DocumentHasUnsupportedFeature(kFeature);
  EXPECT_TRUE(fake_plugin_.UnsupportedFeatureIsReportedForTesting(kMetric));
  EXPECT_FALSE(
      fake_plugin_.GetNotifiedBrowserAboutUnsupportedFeatureForTesting());
}

TEST_F(PdfViewPluginBaseTest, EnteredEditMode) {
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));
  fake_plugin_.EnteredEditMode();

  base::Value expected_response(base::Value::Type::DICTIONARY);
  expected_response.SetStringKey("type", "setIsEditing");

  EXPECT_TRUE(fake_plugin_.edit_mode());
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}

#if BUILDFLAG(ENABLE_INK)
TEST_F(PdfViewPluginBaseSaveTest, SaveAnnotationInNonEditMode) {
  ASSERT_FALSE(fake_plugin_.edit_mode());

  static constexpr char kSaveAnnotInNonEditModeToken[] =
      "save-annot-in-non-edit-mode-token";
  base::Value message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kAnnotation,
                               kSaveAnnotInNonEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveAnnotInNonEditModeToken);

  EXPECT_CALL(fake_plugin_, SetFormFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));
  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}

TEST_F(PdfViewPluginBaseSaveTest, SaveAnnotationInEditMode) {
  fake_plugin_.EnteredEditMode();
  ASSERT_TRUE(fake_plugin_.edit_mode());

  static constexpr char kSaveAnnotInEditModeToken[] =
      "save-annot-in-edit-mode-token";
  base::Value message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kAnnotation,
                               kSaveAnnotInEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveAnnotInEditModeToken);

  EXPECT_CALL(fake_plugin_, SetFormFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));
  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}
#endif  // BUILDFLAG(ENABLE_INK)

TEST_F(PdfViewPluginBaseSaveTest, SaveOriginalInNonEditMode) {
  ASSERT_FALSE(fake_plugin_.edit_mode());

  static constexpr char kSaveOriginalInNonEditModeToken[] =
      "save-original-in-non-edit-mode-token";
  base::Value message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kOriginal,
                               kSaveOriginalInNonEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToFileResponse(kSaveOriginalInNonEditModeToken);

  EXPECT_CALL(fake_plugin_, SaveAs());
  EXPECT_CALL(fake_plugin_, SetFormFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(false)).Times(2);

  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}

TEST_F(PdfViewPluginBaseSaveTest, SaveOriginalInEditMode) {
  fake_plugin_.EnteredEditMode();
  ASSERT_TRUE(fake_plugin_.edit_mode());

  static constexpr char kSaveOriginalInEditModeToken[] =
      "save-original-in-edit-mode-token";
  base::Value message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kOriginal,
                               kSaveOriginalInEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToFileResponse(kSaveOriginalInEditModeToken);

  EXPECT_CALL(fake_plugin_, SaveAs());
  EXPECT_CALL(fake_plugin_, SetFormFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));

  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}

#if BUILDFLAG(ENABLE_INK)
TEST_F(PdfViewPluginBaseSaveTest, SaveEditedInNonEditMode) {
  ASSERT_FALSE(fake_plugin_.edit_mode());

  static constexpr char kSaveEditedInNonEditModeToken[] =
      "save-edited-in-non-edit-mode";
  base::Value message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kEdited,
                               kSaveEditedInNonEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveEditedInNonEditModeToken);

  EXPECT_CALL(fake_plugin_, SetFormFieldInFocus(false));
  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}
#endif  // BUILDFLAG(ENABLE_INK)

TEST_F(PdfViewPluginBaseSaveTest, SaveEditedInEditMode) {
  fake_plugin_.EnteredEditMode();
  ASSERT_TRUE(fake_plugin_.edit_mode());

  static constexpr char kSaveEditedInEditModeToken[] =
      "save-edited-in-edit-mode-token";
  base::Value message = CreateSaveRequestMessage(
      PdfViewPluginBase::SaveRequestType::kEdited, kSaveEditedInEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveEditedInEditModeToken);

  EXPECT_CALL(fake_plugin_, SetFormFieldInFocus(false));
  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(expected_response, fake_plugin_.sent_message());
}

TEST_F(PdfViewPluginBaseTest, HandleSetBackgroundColorMessage) {
  const SkColor kNewBackgroundColor = SK_ColorGREEN;
  ASSERT_NE(kNewBackgroundColor, fake_plugin_.GetBackgroundColor());

  base::Value message(base::Value::Type::DICTIONARY);
  message.SetStringKey("type", "setBackgroundColor");
  message.SetDoubleKey("color", kNewBackgroundColor);

  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(kNewBackgroundColor, fake_plugin_.GetBackgroundColor());
}

}  // namespace chrome_pdf
