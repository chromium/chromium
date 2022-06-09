// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdf_engine.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/test/test_pdfium_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

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
  using PdfViewPluginBase::SetCaretPosition;
  using PdfViewPluginBase::UpdateGeometryOnPluginRectChanged;

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

class PdfViewPluginBaseWithEngineTest : public testing::Test {
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

  NiceMock<FakePdfViewPluginBase> fake_plugin_;
};

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

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPosition) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({300, 56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(2, 3)));
  fake_plugin_.SetCaretPosition({4.0f, 3.0f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPositionNegativeOrigin) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({-300, -56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(2, 3)));
  fake_plugin_.SetCaretPosition({4.0f, 3.0f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPositionFractional) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({300, 56, 20, 5}, 1.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(1, 2)));
  fake_plugin_.SetCaretPosition({3.9f, 2.9f});
}

TEST_F(PdfViewPluginBaseWithEngineTest, SetCaretPositionScaled) {
  auto* engine = static_cast<TestPDFiumEngine*>(fake_plugin_.engine());
  fake_plugin_.UpdateGeometryOnPluginRectChanged({600, 112, 40, 10}, 2.0f);
  EXPECT_CALL(*engine, ApplyDocumentLayout)
      .WillRepeatedly(Return(gfx::Size(16, 9)));
  SendDefaultViewportMessage();

  EXPECT_CALL(*engine, SetCaretPosition(gfx::Point(4, 6)));
  fake_plugin_.SetCaretPosition({4.0f, 3.0f});
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
              NotifySelectionChanged(gfx::PointF(-8.0f, -20.0f), 40,
                                     gfx::PointF(52.0f, 60.0f), 80));
  EXPECT_CALL(fake_plugin_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  fake_plugin_.SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});

  EXPECT_EQ(gfx::Point(), viewport_info.scroll);
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
              NotifySelectionChanged(gfx::PointF(-8.0f, -20.0f), 40,
                                     gfx::PointF(52.0f, 60.0f), 80));
  EXPECT_CALL(fake_plugin_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  fake_plugin_.SelectionChanged({-10, -20, 30, 40}, {50, 60, 70, 80});

  EXPECT_EQ(gfx::Point(), viewport_info.scroll);
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
              NotifySelectionChanged(gfx::PointF(-8.0f, -20.0f), 40,
                                     gfx::PointF(52.0f, 60.0f), 80));
  EXPECT_CALL(fake_plugin_, SetAccessibilityViewportInfo)
      .WillOnce(SaveArg<0>(&viewport_info));
  fake_plugin_.SelectionChanged({-20, -40, 60, 80}, {100, 120, 140, 160});

  EXPECT_EQ(gfx::Point(), viewport_info.scroll);
}

}  // namespace chrome_pdf
