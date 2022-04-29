// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/icu_test_util.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/accessibility_structs.h"
#include "pdf/buildflags.h"
#include "pdf/content_restriction.h"
#include "pdf/document_attachment_info.h"
#include "pdf/document_layout.h"
#include "pdf/document_metadata.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/test/test_pdfium_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

using blink::WebPrintParams;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

// Keep it in-sync with the `kFinalFallbackName` returned by
// net::GetSuggestedFilename().
constexpr char kDefaultDownloadFileName[] = "download";

class MockUrlLoader : public UrlLoader {
 public:
  MOCK_METHOD(void, GrantUniversalAccess, (), (override));
  MOCK_METHOD(void,
              Open,
              (const UrlRequest&, base::OnceCallback<void(int)>),
              (override));
  MOCK_METHOD(void,
              ReadResponseBody,
              (base::span<char>, base::OnceCallback<void(int)>),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

// TODO(crbug.com/1302059): Overhaul this when PdfViewPluginBase merges with
// PdfViewWebPlugin.
class FakePdfViewPluginBase : public PdfViewPluginBase {
 public:
  FakePdfViewPluginBase() {
    ON_CALL(*this, CreateUrlLoaderInternal).WillByDefault([]() {
      return std::make_unique<NiceMock<MockUrlLoader>>();
    });
  }

  // Public for testing.
  using PdfViewPluginBase::accessibility_state;
  using PdfViewPluginBase::engine;
  using PdfViewPluginBase::full_frame;
  using PdfViewPluginBase::HandleInputEvent;
  using PdfViewPluginBase::HandleMessage;
  using PdfViewPluginBase::LoadUrl;
  using PdfViewPluginBase::PrintBegin;
  using PdfViewPluginBase::PrintEnd;
  using PdfViewPluginBase::PrintPages;
  using PdfViewPluginBase::SetCaretPosition;
  using PdfViewPluginBase::SetZoom;
  using PdfViewPluginBase::UpdateGeometryOnPluginRectChanged;
  using PdfViewPluginBase::UpdateScroll;

  MOCK_METHOD(std::string, GetURL, (), (override));

  MOCK_METHOD(bool, Confirm, (const std::string&), (override));

  MOCK_METHOD(std::string,
              Prompt,
              (const std::string&, const std::string&),
              (override));

  MOCK_METHOD(std::vector<PDFEngine::Client::SearchStringResult>,
              SearchString,
              (const char16_t*, const char16_t*, bool),
              (override));

  MOCK_METHOD(bool, IsPrintPreview, (), (const override));

  MOCK_METHOD(void, SetSelectedText, (const std::string&), (override));

  MOCK_METHOD(void, SetLinkUnderCursor, (const std::string&), (override));

  MOCK_METHOD(bool, IsValidLink, (const std::string&), (override));

  MOCK_METHOD(void, InvalidatePluginContainer, (), (override));
  MOCK_METHOD(void, UpdateSnapshot, (sk_sp<SkImage>), (override));
  MOCK_METHOD(void, UpdateScale, (float), (override));
  MOCK_METHOD(void,
              UpdateLayerTransform,
              (float, const gfx::Vector2dF&),
              (override));

  MOCK_METHOD(std::unique_ptr<PDFiumEngine>,
              CreateEngine,
              (PDFEngine::Client*, PDFiumFormFiller::ScriptOption),
              (override));

  base::WeakPtr<PdfViewPluginBase> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(std::unique_ptr<UrlLoader>,
              CreateUrlLoaderInternal,
              (),
              (override));

  MOCK_METHOD(void, OnDocumentLoadComplete, (), (override));

  void SendMessage(base::Value::Dict message) override {
    sent_messages_.push_back(base::Value(std::move(message)));
  }

  MOCK_METHOD(void, SaveAs, (), (override));

  MOCK_METHOD(void, SetFormTextFieldInFocus, (bool), (override));

  MOCK_METHOD(void,
              SetAccessibilityDocInfo,
              (AccessibilityDocInfo),
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
              (AccessibilityViewportInfo),
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

  void clear_sent_messages() { sent_messages_.clear(); }

  const std::vector<base::Value>& sent_messages() const {
    return sent_messages_;
  }

 private:
  std::vector<base::Value> sent_messages_;

  base::WeakPtrFactory<FakePdfViewPluginBase> weak_factory_{this};
};

base::Value CreateExpectedFormTextFieldFocusChangeResponse() {
  base::Value::Dict message;
  message.Set("type", "formFocusChange");
  message.Set("focused", false);
  return base::Value(std::move(message));
}

base::Value::Dict CreateSaveRequestMessage(
    PdfViewPluginBase::SaveRequestType type,
    const std::string& token) {
  base::Value::Dict message;
  message.Set("type", "save");
  message.Set("saveRequestType", static_cast<int>(type));
  message.Set("token", token);
  return message;
}

base::Value CreateExpectedSaveToBufferResponse(const std::string& token,
                                               bool edit_mode) {
  base::Value::Dict expected_response;
  expected_response.Set("type", "saveData");
  expected_response.Set("token", token);
  expected_response.Set("fileName", kDefaultDownloadFileName);
  expected_response.Set("editModeForTesting", edit_mode);
  expected_response.Set(
      "dataToSave", base::Value(base::make_span(TestPDFiumEngine::kSaveData)));
  return base::Value(std::move(expected_response));
}

base::Value CreateExpectedSaveToFileResponse(const std::string& token) {
  base::Value::Dict expected_response;
  expected_response.Set("type", "consumeSaveToken");
  expected_response.Set("token", token);
  return base::Value(std::move(expected_response));
}

}  // namespace

class PdfViewPluginBaseTest : public testing::Test {
 protected:
  NiceMock<FakePdfViewPluginBase> fake_plugin_;
};

class PdfViewPluginBaseWithEngineTest : public PdfViewPluginBaseTest {
 public:
  void SetUp() override {
    auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(&fake_plugin_);
    fake_plugin_.InitializeEngineForTesting(std::move(engine));
  }

 protected:
  void SendDefaultViewportMessage() {
    base::Value message = base::test::ParseJson(R"({
      "type": "viewport",
      "userInitiated": false,
      "zoom": 1,
      "layoutOptions": {
        "direction": 2,
        "defaultPageOrientation": 0,
        "twoUpViewEnabled": false,
      },
      "xOffset": 0,
      "yOffset": 0,
      "pinchPhase": 0,
    })");
    fake_plugin_.HandleMessage(message.GetDict());
  }
};

class PdfViewPluginBaseWithScopedLocaleTest
    : public PdfViewPluginBaseWithEngineTest {
 protected:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_{"en_US"};
  base::test::ScopedRestoreDefaultTimezone la_time_{"America/Los_Angeles"};
};

using PdfViewPluginBaseWithoutDocInfoTest =
    PdfViewPluginBaseWithScopedLocaleTest;

TEST_F(PdfViewPluginBaseTest, LoadUrl) {
  UrlRequest saved_request;
  EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal)
      .WillOnce([&saved_request]() {
        auto mock_loader = std::make_unique<NiceMock<MockUrlLoader>>();
        EXPECT_CALL(*mock_loader, Open)
            .WillOnce(testing::SaveArg<0>(&saved_request));
        return mock_loader;
      });

  // Note that `is_print_preview` only controls the load callback. */
  fake_plugin_.LoadUrl("fake-url", /*is_print_preview=*/false);

  EXPECT_EQ("fake-url", saved_request.url);
  EXPECT_EQ("GET", saved_request.method);
  EXPECT_TRUE(saved_request.ignore_redirects);
  EXPECT_EQ("", saved_request.custom_referrer_url);
  EXPECT_EQ("", saved_request.headers);
  EXPECT_EQ("", saved_request.body);
  EXPECT_LE(saved_request.buffer_lower_threshold,
            saved_request.buffer_upper_threshold);
}

TEST_F(PdfViewPluginBaseTest, DocumentLoadProgress) {
  fake_plugin_.DocumentLoadProgress(10, 200);

  EXPECT_THAT(fake_plugin_.sent_messages(), ElementsAre(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 5.0,
  })")));
}

TEST_F(PdfViewPluginBaseTest, DocumentLoadProgressIgnoreSmall) {
  fake_plugin_.DocumentLoadProgress(2, 100);
  fake_plugin_.clear_sent_messages();

  fake_plugin_.DocumentLoadProgress(3, 100);

  EXPECT_THAT(fake_plugin_.sent_messages(), IsEmpty());
}

TEST_F(PdfViewPluginBaseTest, DocumentLoadProgressMultipleSmall) {
  fake_plugin_.DocumentLoadProgress(2, 100);
  fake_plugin_.clear_sent_messages();

  fake_plugin_.DocumentLoadProgress(3, 100);
  fake_plugin_.DocumentLoadProgress(4, 100);

  EXPECT_THAT(fake_plugin_.sent_messages(), ElementsAre(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 4.0,
  })")));
}

TEST_F(PdfViewPluginBaseTest, DocumentLoadProgressResetByLoadUrl) {
  fake_plugin_.DocumentLoadProgress(2, 100);
  fake_plugin_.clear_sent_messages();

  fake_plugin_.LoadUrl("fake-url", /*is_print_preview=*/false);
  fake_plugin_.DocumentLoadProgress(3, 100);

  EXPECT_THAT(fake_plugin_.sent_messages(), ElementsAre(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 3.0,
  })")));
}

TEST_F(PdfViewPluginBaseTest,
       DocumentLoadProgressNotResetByLoadUrlWithPrintPreview) {
  fake_plugin_.DocumentLoadProgress(2, 100);
  fake_plugin_.clear_sent_messages();

  fake_plugin_.LoadUrl("fake-url", /*is_print_preview=*/true);
  fake_plugin_.DocumentLoadProgress(3, 100);

  EXPECT_THAT(fake_plugin_.sent_messages(), IsEmpty());
}

TEST_F(PdfViewPluginBaseTest, CreateUrlLoaderInFullFrame) {
  fake_plugin_.set_full_frame_for_testing(true);
  ASSERT_TRUE(fake_plugin_.full_frame());

  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
  EXPECT_CALL(fake_plugin_, SetContentRestrictions(kContentRestrictionSave |
                                                   kContentRestrictionPrint));
  EXPECT_CALL(fake_plugin_, PluginDidStartLoading);
  EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal);
  fake_plugin_.CreateUrlLoader();
  EXPECT_TRUE(fake_plugin_.GetDidCallStartLoadingForTesting());
}

TEST_F(PdfViewPluginBaseTest, CreateUrlLoaderWithoutFullFrame) {
  ASSERT_FALSE(fake_plugin_.full_frame());

  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
  EXPECT_CALL(fake_plugin_, SetContentRestrictions(kContentRestrictionSave |
                                                   kContentRestrictionPrint))
      .Times(0);
  EXPECT_CALL(fake_plugin_, PluginDidStartLoading).Times(0);
  EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal);
  fake_plugin_.CreateUrlLoader();
  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
}

TEST_F(PdfViewPluginBaseWithoutDocInfoTest,
       DocumentLoadCompleteInFullFramePdfViewerWithAccessibilityEnabled) {
  // Notify the render frame about document loading.
  fake_plugin_.set_full_frame_for_testing(true);
  ASSERT_TRUE(fake_plugin_.full_frame());
  fake_plugin_.CreateUrlLoader();

  ASSERT_FALSE(fake_plugin_.IsPrintPreview());
  ASSERT_EQ(PdfViewPluginBase::DocumentLoadState::kLoading,
            fake_plugin_.document_load_state_for_testing());

  // Change the accessibility state to pending so that accessibility can be
  // loaded later.
  fake_plugin_.EnableAccessibility();
  EXPECT_EQ(PdfViewPluginBase::AccessibilityState::kPending,
            fake_plugin_.accessibility_state());

  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction("PDF.LoadSuccess"));
  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, PluginDidStopLoading());
  EXPECT_CALL(fake_plugin_,
              SetContentRestrictions(fake_plugin_.GetContentRestrictions()));
  EXPECT_CALL(fake_plugin_,
              SetAccessibilityDocInfo(fake_plugin_.GetAccessibilityDocInfo()));

  fake_plugin_.DocumentLoadComplete();
  EXPECT_EQ(PdfViewPluginBase::DocumentLoadState::kComplete,
            fake_plugin_.document_load_state_for_testing());
  EXPECT_EQ(PdfViewPluginBase::AccessibilityState::kLoaded,
            fake_plugin_.accessibility_state());

  // Check all the sent messages.
  ASSERT_EQ(1u, fake_plugin_.sent_messages().size());
  EXPECT_EQ(CreateExpectedFormTextFieldFocusChangeResponse(),
            fake_plugin_.sent_messages()[0]);
}

TEST_F(PdfViewPluginBaseWithoutDocInfoTest,
       DocumentLoadCompleteInFullFramePdfViewerWithAccessibilityDisabled) {
  // Notify the render frame about document loading.
  fake_plugin_.set_full_frame_for_testing(true);
  ASSERT_TRUE(fake_plugin_.full_frame());
  fake_plugin_.CreateUrlLoader();

  ASSERT_FALSE(fake_plugin_.IsPrintPreview());
  ASSERT_EQ(PdfViewPluginBase::DocumentLoadState::kLoading,
            fake_plugin_.document_load_state_for_testing());
  ASSERT_EQ(PdfViewPluginBase::AccessibilityState::kOff,
            fake_plugin_.accessibility_state());

  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction("PDF.LoadSuccess"));
  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, PluginDidStopLoading());
  EXPECT_CALL(fake_plugin_,
              SetContentRestrictions(fake_plugin_.GetContentRestrictions()));
  EXPECT_CALL(fake_plugin_,
              SetAccessibilityDocInfo(fake_plugin_.GetAccessibilityDocInfo()))
      .Times(0);

  fake_plugin_.DocumentLoadComplete();
  EXPECT_EQ(PdfViewPluginBase::DocumentLoadState::kComplete,
            fake_plugin_.document_load_state_for_testing());
  EXPECT_EQ(PdfViewPluginBase::AccessibilityState::kOff,
            fake_plugin_.accessibility_state());

  // Check all the sent messages.
  ASSERT_EQ(1u, fake_plugin_.sent_messages().size());
  EXPECT_EQ(CreateExpectedFormTextFieldFocusChangeResponse(),
            fake_plugin_.sent_messages()[0]);
}

TEST_F(PdfViewPluginBaseWithoutDocInfoTest,
       DocumentLoadCompleteInNonFullFramePdfViewer) {
  ASSERT_FALSE(fake_plugin_.full_frame());
  fake_plugin_.CreateUrlLoader();

  ASSERT_FALSE(fake_plugin_.IsPrintPreview());
  ASSERT_EQ(PdfViewPluginBase::DocumentLoadState::kLoading,
            fake_plugin_.document_load_state_for_testing());

  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction("PDF.LoadSuccess"));
  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, PluginDidStopLoading()).Times(0);
  EXPECT_CALL(fake_plugin_,
              SetContentRestrictions(fake_plugin_.GetContentRestrictions()))
      .Times(0);

  fake_plugin_.DocumentLoadComplete();
  EXPECT_EQ(PdfViewPluginBase::DocumentLoadState::kComplete,
            fake_plugin_.document_load_state_for_testing());

  // Check all the sent messages.
  ASSERT_EQ(1u, fake_plugin_.sent_messages().size());
  EXPECT_EQ(CreateExpectedFormTextFieldFocusChangeResponse(),
            fake_plugin_.sent_messages()[0]);
}

TEST_F(PdfViewPluginBaseTest, DocumentLoadFailedWithNotifiedRenderFrame) {
  // Notify the render frame about document loading.
  fake_plugin_.set_full_frame_for_testing(true);
  ASSERT_TRUE(fake_plugin_.full_frame());
  fake_plugin_.CreateUrlLoader();

  ASSERT_EQ(PdfViewPluginBase::DocumentLoadState::kLoading,
            fake_plugin_.document_load_state_for_testing());
  EXPECT_TRUE(fake_plugin_.GetDidCallStartLoadingForTesting());

  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction("PDF.LoadFailure"));
  EXPECT_CALL(fake_plugin_, PluginDidStopLoading());

  fake_plugin_.DocumentLoadFailed();
  EXPECT_EQ(PdfViewPluginBase::DocumentLoadState::kFailed,
            fake_plugin_.document_load_state_for_testing());
  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
}

TEST_F(PdfViewPluginBaseTest, DocumentLoadFailedWithoutNotifiedRenderFrame) {
  // The render frame has never been notified about document loading before.
  ASSERT_FALSE(fake_plugin_.full_frame());
  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());

  ASSERT_EQ(PdfViewPluginBase::DocumentLoadState::kLoading,
            fake_plugin_.document_load_state_for_testing());
  EXPECT_CALL(fake_plugin_, UserMetricsRecordAction("PDF.LoadFailure"));
  EXPECT_CALL(fake_plugin_, PluginDidStopLoading()).Times(0);

  fake_plugin_.DocumentLoadFailed();
  EXPECT_EQ(PdfViewPluginBase::DocumentLoadState::kFailed,
            fake_plugin_.document_load_state_for_testing());
  EXPECT_FALSE(fake_plugin_.GetDidCallStartLoadingForTesting());
}

TEST_F(PdfViewPluginBaseTest, DocumentHasUnsupportedFeatureInFullFrame) {
  fake_plugin_.set_full_frame_for_testing(true);
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

TEST_F(PdfViewPluginBaseWithEngineTest, HandleInputEvent) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, HandleInputEvent)
      .WillRepeatedly([](const blink::WebInputEvent& event) {
        const auto& mouse_event =
            static_cast<const blink::WebMouseEvent&>(event);
        EXPECT_EQ(blink::WebInputEvent::Type::kMouseDown,
                  mouse_event.GetType());
        EXPECT_EQ(gfx::PointF(10.0f, 20.0f), mouse_event.PositionInWidget());
        return true;
      });

  blink::WebMouseEvent mouse_event;
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  mouse_event.SetPositionInWidget(10.0f, 20.0f);

  EXPECT_TRUE(fake_plugin_.HandleInputEvent(mouse_event));
}

TEST_F(PdfViewPluginBaseTest, EnteredEditMode) {
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));
  fake_plugin_.EnteredEditMode();

  base::Value::Dict expected_response;
  expected_response.Set("type", "setIsEditing");

  EXPECT_TRUE(fake_plugin_.edit_mode_for_testing());
  ASSERT_EQ(1u, fake_plugin_.sent_messages().size());
  EXPECT_EQ(base::Value(std::move(expected_response)),
            fake_plugin_.sent_messages()[0]);
}

using PdfViewPluginBaseSaveTest = PdfViewPluginBaseWithEngineTest;

#if BUILDFLAG(ENABLE_INK)
TEST_F(PdfViewPluginBaseSaveTest, SaveAnnotationInNonEditMode) {
  ASSERT_FALSE(fake_plugin_.edit_mode_for_testing());

  static constexpr char kSaveAnnotInNonEditModeToken[] =
      "save-annot-in-non-edit-mode-token";
  base::Value::Dict message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kAnnotation,
                               kSaveAnnotInNonEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveAnnotInNonEditModeToken,
                                         /*edit_mode=*/false);

  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));
  fake_plugin_.HandleMessage(message);
  ASSERT_FALSE(fake_plugin_.sent_messages().empty());
  EXPECT_EQ(expected_response, fake_plugin_.sent_messages().back());
}

TEST_F(PdfViewPluginBaseSaveTest, SaveAnnotationInEditMode) {
  fake_plugin_.EnteredEditMode();
  ASSERT_TRUE(fake_plugin_.edit_mode_for_testing());

  static constexpr char kSaveAnnotInEditModeToken[] =
      "save-annot-in-edit-mode-token";
  base::Value::Dict message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kAnnotation,
                               kSaveAnnotInEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveAnnotInEditModeToken,
                                         /*edit_mode=*/true);

  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));
  fake_plugin_.HandleMessage(message);
  ASSERT_FALSE(fake_plugin_.sent_messages().empty());
  EXPECT_EQ(expected_response, fake_plugin_.sent_messages().back());
}
#endif  // BUILDFLAG(ENABLE_INK)

TEST_F(PdfViewPluginBaseSaveTest, SaveOriginalInNonEditMode) {
  ASSERT_FALSE(fake_plugin_.edit_mode_for_testing());

  static constexpr char kSaveOriginalInNonEditModeToken[] =
      "save-original-in-non-edit-mode-token";
  base::Value::Dict message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kOriginal,
                               kSaveOriginalInNonEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToFileResponse(kSaveOriginalInNonEditModeToken);

  EXPECT_CALL(fake_plugin_, SaveAs());
  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(false)).Times(2);

  fake_plugin_.HandleMessage(message);
  ASSERT_FALSE(fake_plugin_.sent_messages().empty());
  EXPECT_EQ(expected_response, fake_plugin_.sent_messages().back());
}

TEST_F(PdfViewPluginBaseSaveTest, SaveOriginalInEditMode) {
  fake_plugin_.EnteredEditMode();
  ASSERT_TRUE(fake_plugin_.edit_mode_for_testing());

  static constexpr char kSaveOriginalInEditModeToken[] =
      "save-original-in-edit-mode-token";
  base::Value::Dict message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kOriginal,
                               kSaveOriginalInEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToFileResponse(kSaveOriginalInEditModeToken);

  EXPECT_CALL(fake_plugin_, SaveAs());
  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(false));
  EXPECT_CALL(fake_plugin_, SetPluginCanSave(true));

  fake_plugin_.HandleMessage(message);
  ASSERT_FALSE(fake_plugin_.sent_messages().empty());
  EXPECT_EQ(expected_response, fake_plugin_.sent_messages().back());
}

#if BUILDFLAG(ENABLE_INK)
TEST_F(PdfViewPluginBaseSaveTest, SaveEditedInNonEditMode) {
  ASSERT_FALSE(fake_plugin_.edit_mode_for_testing());

  static constexpr char kSaveEditedInNonEditModeToken[] =
      "save-edited-in-non-edit-mode";
  base::Value::Dict message =
      CreateSaveRequestMessage(PdfViewPluginBase::SaveRequestType::kEdited,
                               kSaveEditedInNonEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveEditedInNonEditModeToken,
                                         /*edit_mode=*/false);

  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  fake_plugin_.HandleMessage(message);
  ASSERT_FALSE(fake_plugin_.sent_messages().empty());
  EXPECT_EQ(expected_response, fake_plugin_.sent_messages().back());
}
#endif  // BUILDFLAG(ENABLE_INK)

TEST_F(PdfViewPluginBaseSaveTest, SaveEditedInEditMode) {
  fake_plugin_.EnteredEditMode();
  ASSERT_TRUE(fake_plugin_.edit_mode_for_testing());

  static constexpr char kSaveEditedInEditModeToken[] =
      "save-edited-in-edit-mode-token";
  base::Value::Dict message = CreateSaveRequestMessage(
      PdfViewPluginBase::SaveRequestType::kEdited, kSaveEditedInEditModeToken);
  base::Value expected_response =
      CreateExpectedSaveToBufferResponse(kSaveEditedInEditModeToken,
                                         /*edit_mode=*/true);

  EXPECT_CALL(fake_plugin_, SetFormTextFieldInFocus(false));
  fake_plugin_.HandleMessage(message);
  ASSERT_FALSE(fake_plugin_.sent_messages().empty());
  EXPECT_EQ(expected_response, fake_plugin_.sent_messages().back());
}

TEST_F(PdfViewPluginBaseTest, HandleSetBackgroundColorMessage) {
  const SkColor kNewBackgroundColor = SK_ColorGREEN;
  ASSERT_NE(kNewBackgroundColor, fake_plugin_.GetBackgroundColor());

  base::Value::Dict message;
  message.Set("type", "setBackgroundColor");
  message.Set("color", static_cast<double>(kNewBackgroundColor));

  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(kNewBackgroundColor, fake_plugin_.GetBackgroundColor());
}

TEST_F(PdfViewPluginBaseWithEngineTest,
       HandleViewportMessageBeforeDocumentLoadComplete) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ApplyDocumentLayout(DocumentLayout::Options()));

  base::Value message = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message.GetDict());

  EXPECT_THAT(fake_plugin_.sent_messages(), IsEmpty());
}

TEST_F(PdfViewPluginBaseWithEngineTest,
       HandleViewportMessageAfterDocumentLoadComplete) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ApplyDocumentLayout(DocumentLayout::Options()));

  fake_plugin_.DocumentLoadComplete();
  fake_plugin_.clear_sent_messages();

  base::Value message = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message.GetDict());

  EXPECT_THAT(fake_plugin_.sent_messages(), ElementsAre(base::test::IsJson(R"({
    "type": "loadProgress",
    "progress": 100.0,
  })")));
}

TEST_F(PdfViewPluginBaseWithEngineTest, HandleViewportMessageSubsequently) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());

  base::Value message1 = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message1.GetDict());
  fake_plugin_.clear_sent_messages();

  DocumentLayout::Options two_up_options;
  two_up_options.set_page_spread(DocumentLayout::PageSpread::kTwoUpOdd);
  EXPECT_CALL(*engine, ApplyDocumentLayout(two_up_options));

  base::Value message2 = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 0,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": true,
    },
    "xOffset": 0,
    "yOffset": 0,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message2.GetDict());

  EXPECT_THAT(fake_plugin_.sent_messages(), IsEmpty());
}

TEST_F(PdfViewPluginBaseWithEngineTest, HandleViewportMessageScroll) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  EXPECT_CALL(*engine, ScrolledToXPosition(2));
  EXPECT_CALL(*engine, ScrolledToYPosition(3));

  base::Value message = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 2,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 2,
    "yOffset": 3,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message.GetDict());
}

TEST_F(PdfViewPluginBaseWithEngineTest,
       HandleViewportMessageScrollRightToLeft) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  EXPECT_CALL(*engine, ScrolledToXPosition(2));
  EXPECT_CALL(*engine, ScrolledToYPosition(3));

  base::Value message = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 1,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": 2,
    "yOffset": 3,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message.GetDict());
}

TEST_F(PdfViewPluginBaseWithEngineTest,
       HandleViewportMessageScrollRightToLeftInPrintPreview) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  EXPECT_CALL(*engine, ScrolledToXPosition(14));
  EXPECT_CALL(*engine, ScrolledToYPosition(3));
  EXPECT_CALL(fake_plugin_, IsPrintPreview).WillRepeatedly(Return(true));

  base::Value message = base::test::ParseJson(R"({
    "type": "viewport",
    "userInitiated": false,
    "zoom": 1,
    "layoutOptions": {
      "direction": 1,
      "defaultPageOrientation": 0,
      "twoUpViewEnabled": false,
    },
    "xOffset": -2,
    "yOffset": 3,
    "pinchPhase": 0,
  })");
  fake_plugin_.HandleMessage(message.GetDict());
}

TEST_F(PdfViewPluginBaseWithEngineTest, UpdateScroll) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ScrolledToXPosition(0));
  EXPECT_CALL(*engine, ScrolledToYPosition(0));

  fake_plugin_.UpdateScroll({0, 0});
}

TEST_F(PdfViewPluginBaseWithEngineTest, UpdateScrollStopped) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ScrolledToXPosition).Times(0);
  EXPECT_CALL(*engine, ScrolledToYPosition).Times(0);

  base::Value message = base::test::ParseJson(R"({
    "type": "stopScrolling",
  })");
  fake_plugin_.HandleMessage(message.GetDict());
  fake_plugin_.UpdateScroll({0, 0});
}

TEST_F(PdfViewPluginBaseWithEngineTest, UpdateScrollUnderflow) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();
  EXPECT_CALL(*engine, ScrolledToXPosition(0));
  EXPECT_CALL(*engine, ScrolledToYPosition(0));

  fake_plugin_.UpdateScroll({-1, -1});
}

TEST_F(PdfViewPluginBaseWithEngineTest, UpdateScrollOverflow) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({3, 2}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, ScrolledToXPosition(13));
  EXPECT_CALL(*engine, ScrolledToYPosition(7));

  fake_plugin_.UpdateScroll({14, 8});
}

TEST_F(PdfViewPluginBaseWithEngineTest, UpdateScrollOverflowZoomed) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({3, 2}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();
  fake_plugin_.SetZoom(2.0);

  EXPECT_CALL(*engine, ScrolledToXPosition(29));
  EXPECT_CALL(*engine, ScrolledToYPosition(16));

  fake_plugin_.UpdateScroll({30, 17});
}

TEST_F(PdfViewPluginBaseWithEngineTest, UpdateScrollScaled) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({3, 2}, 2.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, ScrolledToXPosition(4));
  EXPECT_CALL(*engine, ScrolledToYPosition(2));

  fake_plugin_.UpdateScroll({2, 1});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPosition) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({300, 56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(2, 3)));
  fake_plugin_.SetCaretPosition({304.0f, 59.0f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPositionNegativeOrigin) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({-300, -56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(2, 3)));
  fake_plugin_.SetCaretPosition({-296.0f, -53.0f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPositionFractional) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({300, 56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(1, 2)));
  fake_plugin_.SetCaretPosition({303.9f, 58.9f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPositionScaled) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({600, 112, 40, 10}, 2.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(4, 6)));
  fake_plugin_.SetCaretPosition({304.0f, 59.0f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SelectionChanged) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.EnableAccessibility();
  fake_plugin_.DocumentLoadComplete();
  fake_plugin_.UpdateGeometryOnPluginRectChanged({300, 56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  AccessibilityViewportInfo viewport_info;
  EXPECT_CALL(fake_plugin_,
              NotifySelectionChanged(gfx::PointF(292.0f, 36.0f), 40,
                                     gfx::PointF(352.0f, 116.0f), 80));
  EXPECT_CALL(fake_plugin_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  fake_plugin_.SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});

  EXPECT_EQ(gfx::Point(-300, -56), viewport_info.scroll);
}

TEST_F(PdfViewPluginBaseWithEngineTest, SelectionChangedNegativeOrigin) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.EnableAccessibility();
  fake_plugin_.DocumentLoadComplete();
  fake_plugin_.UpdateGeometryOnPluginRectChanged({-300, -56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  AccessibilityViewportInfo viewport_info;
  EXPECT_CALL(fake_plugin_,
              NotifySelectionChanged(gfx::PointF(-308.0f, -76.0f), 40,
                                     gfx::PointF(-248.0f, 4.0f), 80));
  EXPECT_CALL(fake_plugin_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  fake_plugin_.SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});

  EXPECT_EQ(gfx::Point(300, 56), viewport_info.scroll);
}

TEST_F(PdfViewPluginBaseWithEngineTest, SelectionChangedScaled) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.EnableAccessibility();
  fake_plugin_.DocumentLoadComplete();
  fake_plugin_.UpdateGeometryOnPluginRectChanged({600, 112, 40, 10}, 2.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  AccessibilityViewportInfo viewport_info;
  EXPECT_CALL(fake_plugin_,
              NotifySelectionChanged(gfx::PointF(292.0f, 36.0f), 40,
                                     gfx::PointF(352.0f, 116.0f), 80));
  EXPECT_CALL(fake_plugin_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  fake_plugin_.SelectionChanged({-20, -40, 60, 80}, {100, 120, 140, 160});

  EXPECT_EQ(gfx::Point(-300, -56), viewport_info.scroll);
}

TEST_F(PdfViewPluginBaseTest, HandleResetPrintPreviewModeMessage) {
  EXPECT_CALL(fake_plugin_, IsPrintPreview).WillRepeatedly(Return(true));

  auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(&fake_plugin_);
  EXPECT_CALL(*engine, ZoomUpdated);
  EXPECT_CALL(*engine, PageOffsetUpdated);
  EXPECT_CALL(*engine, PluginSizeUpdated);
  EXPECT_CALL(*engine, SetGrayscale(false));
  EXPECT_CALL(fake_plugin_,
              CreateEngine(&fake_plugin_,
                           PDFiumFormFiller::ScriptOption::kNoJavaScript))
      .WillOnce(Return(ByMove(std::move(engine))));

  base::Value message = base::test::ParseJson(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/0/0/print.pdf",
    "grayscale": false,
    "pageCount": 1,
  })");
  fake_plugin_.HandleMessage(message.GetDict());
}

TEST_F(PdfViewPluginBaseTest, HandleResetPrintPreviewModeMessageSetGrayscale) {
  EXPECT_CALL(fake_plugin_, IsPrintPreview).WillRepeatedly(Return(true));

  auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(&fake_plugin_);
  EXPECT_CALL(*engine, SetGrayscale(true));
  EXPECT_CALL(fake_plugin_,
              CreateEngine(&fake_plugin_,
                           PDFiumFormFiller::ScriptOption::kNoJavaScript))
      .WillOnce(Return(ByMove(std::move(engine))));

  base::Value message = base::test::ParseJson(R"({
    "type": "resetPrintPreviewMode",
    "url": "chrome-untrusted://print/0/0/print.pdf",
    "grayscale": true,
    "pageCount": 1,
  })");
  fake_plugin_.HandleMessage(message.GetDict());
}

TEST_F(PdfViewPluginBaseWithEngineTest, NormalPrinting) {
  WebPrintParams params;
  const std::vector<int> kPageNumbers = {0};

  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  engine->SetPermissions({DocumentPermission::kPrintHighQuality,
                          DocumentPermission::kPrintLowQuality});

  InSequence sequence;
  EXPECT_CALL(*engine,
              PrintPages(kPageNumbers,
                         Field(&WebPrintParams::rasterize_pdf, IsFalse())))
      .Times(1);
  EXPECT_CALL(
      *engine,
      PrintPages(kPageNumbers, Field(&WebPrintParams::rasterize_pdf, IsTrue())))
      .Times(1);

  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            fake_plugin_.PrintBegin(params));
  fake_plugin_.PrintPages(kPageNumbers);
  fake_plugin_.PrintEnd();

  params.rasterize_pdf = true;
  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            fake_plugin_.PrintBegin(params));
  fake_plugin_.PrintPages(kPageNumbers);
  fake_plugin_.PrintEnd();
}

// Regression test for crbug.com/1307219.
TEST_F(PdfViewPluginBaseWithEngineTest, LowQualityPrinting) {
  WebPrintParams params;
  const std::vector<int> kPageNumbers = {0};

  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  engine->SetPermissions({DocumentPermission::kPrintLowQuality});

  EXPECT_CALL(
      *engine,
      PrintPages(kPageNumbers, Field(&WebPrintParams::rasterize_pdf, IsTrue())))
      .Times(2);

  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            fake_plugin_.PrintBegin(params));
  fake_plugin_.PrintPages(kPageNumbers);
  fake_plugin_.PrintEnd();

  params.rasterize_pdf = true;
  ASSERT_EQ(static_cast<int>(TestPDFiumEngine::kPageNumber),
            fake_plugin_.PrintBegin(params));
  fake_plugin_.PrintPages(kPageNumbers);
  fake_plugin_.PrintEnd();
}

TEST_F(PdfViewPluginBaseWithEngineTest, NoPrinting) {
  WebPrintParams params;

  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  engine->SetPermissions({});

  EXPECT_EQ(0, fake_plugin_.PrintBegin(params));

  params.rasterize_pdf = true;
  EXPECT_EQ(0, fake_plugin_.PrintBegin(params));
}

TEST_F(PdfViewPluginBaseWithEngineTest, GetContentRestrictions) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  static constexpr int kContentRestrictionCutPaste =
      kContentRestrictionCut | kContentRestrictionPaste;

  // Test engine without any permissions.
  engine->SetPermissions({});

  int content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionCopy |
                kContentRestrictionPrint,
            content_restrictions);

  // Test engine with only copy permission.
  engine->SetPermissions({DocumentPermission::kCopy});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionPrint,
            content_restrictions);

  // Test engine with only print low quality permission.
  engine->SetPermissions({DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionCopy,
            content_restrictions);

  // Test engine with both copy and print low quality permissions.
  engine->SetPermissions(
      {DocumentPermission::kCopy, DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste, content_restrictions);

  // Test engine with print high and low quality permissions.
  engine->SetPermissions({DocumentPermission::kPrintHighQuality,
                          DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionCopy,
            content_restrictions);

  // Test engine with copy, print high and low quality permissions.
  engine->SetPermissions({DocumentPermission::kCopy,
                          DocumentPermission::kPrintHighQuality,
                          DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste, content_restrictions);
}

TEST_F(PdfViewPluginBaseWithEngineTest, GetAccessibilityDocInfo) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());

  // Test engine without any permissions.
  engine->SetPermissions({});

  AccessibilityDocInfo doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_FALSE(doc_info.text_accessible);
  EXPECT_FALSE(doc_info.text_copyable);

  // Test engine with only copy permission.
  engine->SetPermissions({DocumentPermission::kCopy});

  doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_FALSE(doc_info.text_accessible);
  EXPECT_TRUE(doc_info.text_copyable);

  // Test engine with only copy accessible permission.
  engine->SetPermissions({DocumentPermission::kCopyAccessible});

  doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_TRUE(doc_info.text_accessible);
  EXPECT_FALSE(doc_info.text_copyable);

  // Test engine with both copy and copy accessible permission.
  engine->SetPermissions(
      {DocumentPermission::kCopy, DocumentPermission::kCopyAccessible});

  doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_TRUE(doc_info.text_accessible);
  EXPECT_TRUE(doc_info.text_copyable);
}

class PdfViewPluginBaseSubmitFormTest : public PdfViewPluginBaseTest {
 public:
  void SubmitForm(const std::string& url,
                  base::StringPiece form_data = "data") {
    EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal).WillOnce([this]() {
      auto mock_loader = std::make_unique<NiceMock<MockUrlLoader>>();
      EXPECT_CALL(*mock_loader, Open).WillOnce(testing::SaveArg<0>(&request_));
      return mock_loader;
    });

    fake_plugin_.SubmitForm(url, form_data.data(), form_data.size());
  }

  void SubmitFailingForm(const std::string& url) {
    EXPECT_CALL(fake_plugin_, CreateUrlLoaderInternal).Times(0);
    constexpr char kFormData[] = "form data";
    fake_plugin_.SubmitForm(url, kFormData, std::size(kFormData));
  }

 protected:
  UrlRequest request_;
};

TEST_F(PdfViewPluginBaseSubmitFormTest, RequestMethodAndBody) {
  EXPECT_CALL(fake_plugin_, GetURL)
      .WillOnce(Return("https://www.example.com/path/to/the.pdf"));
  constexpr char kFormData[] = "form data";
  SubmitForm(/*url=*/"", kFormData);
  EXPECT_EQ(request_.method, "POST");
  EXPECT_THAT(request_.body, StrEq(kFormData));
}

TEST_F(PdfViewPluginBaseSubmitFormTest, RelativeUrl) {
  EXPECT_CALL(fake_plugin_, GetURL)
      .WillOnce(Return("https://www.example.com/path/to/the.pdf"));
  SubmitForm("relative_endpoint");
  EXPECT_EQ(request_.url, "https://www.example.com/path/to/relative_endpoint");
}

TEST_F(PdfViewPluginBaseSubmitFormTest, NoRelativeUrl) {
  EXPECT_CALL(fake_plugin_, GetURL)
      .WillOnce(Return("https://www.example.com/path/to/the.pdf"));
  SubmitForm("");
  EXPECT_EQ(request_.url, "https://www.example.com/path/to/the.pdf");
}

TEST_F(PdfViewPluginBaseSubmitFormTest, AbsoluteUrl) {
  EXPECT_CALL(fake_plugin_, GetURL)
      .WillOnce(Return("https://a.example.com/path/to/the.pdf"));
  SubmitForm("https://b.example.com/relative_endpoint");
  EXPECT_EQ(request_.url, "https://b.example.com/relative_endpoint");
}

TEST_F(PdfViewPluginBaseSubmitFormTest, EmptyDocumentUrl) {
  EXPECT_CALL(fake_plugin_, GetURL).WillOnce(Return(std::string()));
  SubmitFailingForm("relative_endpoint");
}

TEST_F(PdfViewPluginBaseSubmitFormTest, RelativeUrlInvalidDocumentUrl) {
  EXPECT_CALL(fake_plugin_, GetURL)
      .WillOnce(Return(R"(https://www.%B%Ad.com/path/to/the.pdf)"));
  SubmitFailingForm("relative_endpoint");
}

TEST_F(PdfViewPluginBaseSubmitFormTest, AbsoluteUrlInvalidDocumentUrl) {
  EXPECT_CALL(fake_plugin_, GetURL)
      .WillOnce(Return(R"(https://www.%B%Ad.com/path/to/the.pdf)"));
  SubmitFailingForm("https://wwww.example.com");
}

}  // namespace chrome_pdf
