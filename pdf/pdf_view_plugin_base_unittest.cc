// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/icu_test_util.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/accessibility_structs.h"
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

using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

// TODO(crbug.com/1302059): Overhaul this when PdfViewPluginBase merges with
// PdfViewWebPlugin.
class FakePdfViewPluginBase : public PdfViewPluginBase {
 public:
  // Public for testing.
  using PdfViewPluginBase::engine;
  using PdfViewPluginBase::HandleInputEvent;
  using PdfViewPluginBase::HandleMessage;
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

  MOCK_METHOD(std::unique_ptr<UrlLoader>, CreateUrlLoader, (), (override));

  MOCK_METHOD(std::vector<PDFEngine::Client::SearchStringResult>,
              SearchString,
              (const char16_t*, const char16_t*, bool),
              (override));

  MOCK_METHOD(bool, IsPrintPreview, (), (const override));

  MOCK_METHOD(SkColor, GetBackgroundColor, (), (const override));

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

  MOCK_METHOD(void, LoadUrl, (base::StringPiece, LoadUrlCallback), (override));

  base::WeakPtr<PdfViewPluginBase> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  MOCK_METHOD(void, OnDocumentLoadComplete, (), (override));

  void SendMessage(base::Value::Dict message) override {
    sent_messages_.push_back(base::Value(std::move(message)));
  }

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

  MOCK_METHOD(void, DidStartLoading, (), (override));
  MOCK_METHOD(void, DidStopLoading, (), (override));

  MOCK_METHOD(void, InvokePrintDialog, (), (override));

  MOCK_METHOD(void,
              NotifySelectionChanged,
              (const gfx::PointF&, int, const gfx::PointF&, int),
              (override));

  MOCK_METHOD(void, UserMetricsRecordAction, (const std::string&), (override));

  MOCK_METHOD(bool, full_frame, (), (const override));

  void clear_sent_messages() { sent_messages_.clear(); }

  const std::vector<base::Value>& sent_messages() const {
    return sent_messages_;
  }

 private:
  std::vector<base::Value> sent_messages_;

  base::WeakPtrFactory<FakePdfViewPluginBase> weak_factory_{this};
};

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

  void SetEnginePermissions(
      const std::vector<DocumentPermission>& permissions) {
    TestPDFiumEngine& engine =
        *static_cast<TestPDFiumEngine*>(fake_plugin_.engine());

    // TODO(crbug.com/1323307): Break up tests instead of "resetting" mocks.
    for (DocumentPermission permission :
         {DocumentPermission::kCopy, DocumentPermission::kCopyAccessible,
          DocumentPermission::kPrintLowQuality,
          DocumentPermission::kPrintHighQuality}) {
      ON_CALL(engine, HasPermission(permission)).WillByDefault(Return(false));
    }

    for (DocumentPermission permission : permissions) {
      ON_CALL(engine, HasPermission(permission)).WillByDefault(Return(true));
    }
  }
};

class PdfViewPluginBaseWithScopedLocaleTest
    : public PdfViewPluginBaseWithEngineTest {
 protected:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_{"en_US"};
  base::test::ScopedRestoreDefaultTimezone la_time_{"America/Los_Angeles"};
};

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

TEST_F(PdfViewPluginBaseWithEngineTest, GetContentRestrictions) {
  static constexpr int kContentRestrictionCutPaste =
      kContentRestrictionCut | kContentRestrictionPaste;

  // Test engine without any permissions.
  SetEnginePermissions({});

  int content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionCopy |
                kContentRestrictionPrint,
            content_restrictions);

  // Test engine with only copy permission.
  SetEnginePermissions({DocumentPermission::kCopy});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionPrint,
            content_restrictions);

  // Test engine with only print low quality permission.
  SetEnginePermissions({DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionCopy,
            content_restrictions);

  // Test engine with both copy and print low quality permissions.
  SetEnginePermissions(
      {DocumentPermission::kCopy, DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste, content_restrictions);

  // Test engine with print high and low quality permissions.
  SetEnginePermissions({DocumentPermission::kPrintHighQuality,
                        DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste | kContentRestrictionCopy,
            content_restrictions);

  // Test engine with copy, print high and low quality permissions.
  SetEnginePermissions({DocumentPermission::kCopy,
                        DocumentPermission::kPrintHighQuality,
                        DocumentPermission::kPrintLowQuality});

  content_restrictions = fake_plugin_.GetContentRestrictions();
  EXPECT_EQ(kContentRestrictionCutPaste, content_restrictions);
}

TEST_F(PdfViewPluginBaseWithEngineTest, GetAccessibilityDocInfo) {
  // Test engine without any permissions.
  SetEnginePermissions({});

  AccessibilityDocInfo doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_FALSE(doc_info.text_accessible);
  EXPECT_FALSE(doc_info.text_copyable);

  // Test engine with only copy permission.
  SetEnginePermissions({DocumentPermission::kCopy});

  doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_FALSE(doc_info.text_accessible);
  EXPECT_TRUE(doc_info.text_copyable);

  // Test engine with only copy accessible permission.
  SetEnginePermissions({DocumentPermission::kCopyAccessible});

  doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_TRUE(doc_info.text_accessible);
  EXPECT_FALSE(doc_info.text_copyable);

  // Test engine with both copy and copy accessible permission.
  SetEnginePermissions(
      {DocumentPermission::kCopy, DocumentPermission::kCopyAccessible});

  doc_info = fake_plugin_.GetAccessibilityDocInfo();
  EXPECT_EQ(TestPDFiumEngine::kPageNumber, doc_info.page_count);
  EXPECT_TRUE(doc_info.text_accessible);
  EXPECT_TRUE(doc_info.text_copyable);
}

}  // namespace chrome_pdf
